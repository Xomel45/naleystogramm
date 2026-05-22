/*
 * inflate.h — минимальный DEFLATE inflate (RFC 1951), single-header.
 * Покрывает: type-0 stored, type-1 fixed Huffman, type-2 dynamic Huffman.
 * Нет внешних зависимостей кроме <stdint.h> и <string.h>.
 *
 * API:
 *   int tinflate(const void *src, uint32_t src_len,
 *                void       *dst, uint32_t dst_len);
 *   Возвращает 0 при успехе, -1 при ошибке.
 */
#pragma once
#include <stdint.h>
#include <string.h>

/* ── Таблицы Хаффмана ──────────────────────────────────────────────────── */
#define TINF_MAX_BITS  15
#define TINF_MAX_SYM   288

typedef struct {
    uint16_t counts[TINF_MAX_BITS + 1];  /* сколько кодов каждой длины */
    uint16_t symbols[TINF_MAX_SYM];      /* символы в канонич. порядке */
    int      max_sym;
} TinfTree;

/* ── Состояние декодера ────────────────────────────────────────────────── */
typedef struct {
    const uint8_t *src;
    uint32_t       src_pos;
    uint32_t       src_len;
    uint8_t       *dst;
    uint32_t       dst_pos;
    uint32_t       dst_len;
    uint32_t       bits;      /* буфер битов */
    int            num_bits;  /* сколько битов в буфере */
} TinfState;

/* ── Чтение битов ──────────────────────────────────────────────────────── */
static int tinf_refill(TinfState *s, int n) {
    while (s->num_bits < n) {
        if (s->src_pos >= s->src_len) return -1;
        s->bits |= (uint32_t)s->src[s->src_pos++] << s->num_bits;
        s->num_bits += 8;
    }
    return 0;
}

static int tinf_bits(TinfState *s, int n) {
    if (tinf_refill(s, n)) return -1;
    int val = s->bits & ((1u << n) - 1);
    s->bits >>= n;
    s->num_bits -= n;
    return val;
}

/* Выровнять на байтовую границу */
static void tinf_align(TinfState *s) {
    s->bits    >>= s->num_bits & 7;
    s->num_bits  -= s->num_bits & 7;
}

/* ── Построение дерева Хаффмана ────────────────────────────────────────── */
static int tinf_build_tree(TinfTree *t, const uint8_t *lengths, int num) {
    uint16_t offs[TINF_MAX_BITS + 1] = {0};
    memset(t->counts, 0, sizeof(t->counts));

    for (int i = 0; i < num; i++) {
        if (lengths[i] > TINF_MAX_BITS) return -1;
        t->counts[lengths[i]]++;
    }
    t->counts[0] = 0;
    t->max_sym = num;

    uint16_t avail = 1;
    for (int i = 1; i <= TINF_MAX_BITS; i++) {
        avail = (uint16_t)(avail * 2);
        if (avail < t->counts[i]) return -1;
        avail = (uint16_t)(avail - t->counts[i]);
        offs[i] = (uint16_t)(i > 1 ? offs[i-1] + t->counts[i-1] : 0);
    }

    for (int i = 0; i < num; i++) {
        if (lengths[i])
            t->symbols[offs[lengths[i]]++] = (uint16_t)i;
    }
    return 0;
}

/* ── Декодирование символа по дереву ───────────────────────────────────── */
static int tinf_decode(TinfState *s, const TinfTree *t) {
    int cur = 0, len = 0, cnt = 0;
    do {
        int bit = tinf_bits(s, 1);
        if (bit < 0) return -1;
        cur = cur * 2 + bit;
        len++;
        cnt += t->counts[len];
        cur -= t->counts[len];
    } while (cur >= 0 && len < TINF_MAX_BITS);
    if (cur < 0) {
        int idx = cnt + cur;
        if (idx < 0 || idx >= t->max_sym) return -1;
        return t->symbols[idx];
    }
    return -1;
}

/* ── Фиксированные таблицы Хаффмана (тип 1) ───────────────────────────── */
static void tinf_build_fixed(TinfTree *lt, TinfTree *dt) {
    uint8_t l[320];
    int i;
    for (i =   0; i < 144; i++) l[i] = 8;
    for (i = 144; i < 256; i++) l[i] = 9;
    for (i = 256; i < 280; i++) l[i] = 7;
    for (i = 280; i < 288; i++) l[i] = 8;
    tinf_build_tree(lt, l, 288);

    for (i = 0; i < 32; i++) l[i] = 5;
    tinf_build_tree(dt, l, 32);
}

/* ── Базы и экстра-биты для длин/дистанций ─────────────────────────────── */
static const uint16_t s_len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t s_len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t s_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,
    8193,12289,16385,24577
};
static const uint8_t s_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* ── Декодирование блока (литералы + длины/дистанции) ─────────────────── */
static int tinf_inflate_block(TinfState *s, const TinfTree *lt, const TinfTree *dt) {
    for (;;) {
        int sym = tinf_decode(s, lt);
        if (sym < 0) return -1;

        if (sym < 256) {
            /* Литерал */
            if (s->dst_pos >= s->dst_len) return -1;
            s->dst[s->dst_pos++] = (uint8_t)sym;
        } else if (sym == 256) {
            /* Конец блока */
            break;
        } else {
            /* Длина */
            int idx = sym - 257;
            if (idx >= 29) return -1;
            int extra = tinf_bits(s, s_len_extra[idx]);
            if (extra < 0) return -1;
            int length = s_len_base[idx] + extra;

            /* Дистанция */
            int dsym = tinf_decode(s, dt);
            if (dsym < 0 || dsym >= 30) return -1;
            extra = tinf_bits(s, s_dist_extra[dsym]);
            if (extra < 0) return -1;
            uint32_t dist = s_dist_base[dsym] + (uint32_t)extra;

            /* Копирование из скользящего окна */
            if (s->dst_pos < dist) return -1;
            if (s->dst_pos + (uint32_t)length > s->dst_len) return -1;
            uint32_t from = s->dst_pos - dist;
            for (int k = 0; k < length; k++)
                s->dst[s->dst_pos++] = s->dst[from++ % s->dst_len];
        }
    }
    return 0;
}

/* ── Динамические таблицы (тип 2) ──────────────────────────────────────── */
static const uint8_t s_clen_order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

static int tinf_inflate_dynamic(TinfState *s) {
    int hlit  = tinf_bits(s, 5); if (hlit  < 0) return -1; hlit  += 257;
    int hdist = tinf_bits(s, 5); if (hdist < 0) return -1; hdist += 1;
    int hclen = tinf_bits(s, 4); if (hclen < 0) return -1; hclen += 4;

    /* Code length alphabet */
    uint8_t cl[19] = {0};
    for (int i = 0; i < hclen; i++) {
        int v = tinf_bits(s, 3); if (v < 0) return -1;
        cl[s_clen_order[i]] = (uint8_t)v;
    }
    TinfTree clt;
    if (tinf_build_tree(&clt, cl, 19)) return -1;

    /* Literal/length + distance code lengths */
    uint8_t lens[320];
    int total = hlit + hdist;
    for (int i = 0; i < total; ) {
        int sym = tinf_decode(s, &clt);
        if (sym < 0) return -1;
        if (sym < 16) {
            lens[i++] = (uint8_t)sym;
        } else if (sym == 16) {
            int rep = tinf_bits(s, 2); if (rep < 0) return -1; rep += 3;
            if (i == 0) return -1;
            uint8_t prev = lens[i-1];
            while (rep-- && i < total) lens[i++] = prev;
        } else if (sym == 17) {
            int rep = tinf_bits(s, 3); if (rep < 0) return -1; rep += 3;
            while (rep-- && i < total) lens[i++] = 0;
        } else { /* sym == 18 */
            int rep = tinf_bits(s, 7); if (rep < 0) return -1; rep += 11;
            while (rep-- && i < total) lens[i++] = 0;
        }
    }

    TinfTree lt, dt;
    if (tinf_build_tree(&lt, lens,        hlit))  return -1;
    if (tinf_build_tree(&dt, lens + hlit, hdist)) return -1;
    return tinf_inflate_block(s, &lt, &dt);
}

/* ── Публичный API ─────────────────────────────────────────────────────── */
static int tinflate(const void *src, uint32_t src_len,
                    void *dst,       uint32_t dst_len) {
    TinfState s;
    s.src      = (const uint8_t *)src;
    s.src_pos  = 0;
    s.src_len  = src_len;
    s.dst      = (uint8_t *)dst;
    s.dst_pos  = 0;
    s.dst_len  = dst_len;
    s.bits     = 0;
    s.num_bits = 0;

    int bfinal;
    do {
        bfinal = tinf_bits(&s, 1); if (bfinal < 0) return -1;
        int btype = tinf_bits(&s, 2); if (btype < 0) return -1;

        if (btype == 0) {
            /* Stored block */
            tinf_align(&s);
            int len  = tinf_bits(&s, 16); if (len  < 0) return -1;
            int nlen = tinf_bits(&s, 16); if (nlen < 0) return -1;
            if ((len ^ nlen) != 0xFFFF) return -1;
            if (s.dst_pos + (uint32_t)len > s.dst_len) return -1;
            if (s.src_pos + (uint32_t)len > s.src_len) return -1;
            memcpy(s.dst + s.dst_pos, s.src + s.src_pos, (uint32_t)len);
            s.dst_pos += (uint32_t)len;
            s.src_pos += (uint32_t)len;
        } else if (btype == 1) {
            /* Fixed Huffman */
            TinfTree lt, dt;
            tinf_build_fixed(&lt, &dt);
            if (tinf_inflate_block(&s, &lt, &dt)) return -1;
        } else if (btype == 2) {
            /* Dynamic Huffman */
            if (tinf_inflate_dynamic(&s)) return -1;
        } else {
            return -1;
        }
    } while (!bfinal);

    return (s.dst_pos == dst_len) ? 0 : -1;
}
