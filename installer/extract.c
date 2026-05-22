#include "extract.h"
#include "installer.h"
#include "inflate.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── ZIP-структуры (little-endian, packed) ───────────────────────────────── */
#pragma pack(push, 1)

typedef struct {
    uint32_t signature;        /* 0x04034b50 */
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;      /* 0=stored, 8=deflate */
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
    /* filename и extra следуют далее */
} ZipLocalHeader;

typedef struct {
    uint32_t signature;       /* 0x02014b50 — central dir */
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t int_attrs;
    uint32_t ext_attrs;
    uint32_t local_header_offset;
} ZipCentralHeader;

typedef struct {
    uint32_t signature;       /* 0x06054b50 — end of central dir */
    uint16_t disk_num;
    uint16_t cd_disk;
    uint16_t cd_entries_disk;
    uint16_t cd_entries_total;
    uint32_t cd_size;
    uint32_t cd_offset;
    uint16_t comment_len;
} ZipEndRecord;

#pragma pack(pop)

#define ZIP_LOCAL_SIG   0x04034b50u
#define ZIP_CENTRAL_SIG 0x02014b50u
#define ZIP_END_SIG     0x06054b50u
#define COMP_STORED     0
#define COMP_DEFLATE    8

/* ── Вспомогательные функции ─────────────────────────────────────────────── */

bool makedirs(const wchar_t *path) {
    wchar_t tmp[MAX_PATH];
    wcsncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = L'\0';
    size_t len = wcslen(tmp);
    if (len == 0) return false;
    if (tmp[len - 1] == L'\\' || tmp[len - 1] == L'/')
        tmp[--len] = L'\0';

    for (wchar_t *p = tmp + 1; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            *p = L'\0';
            CreateDirectoryW(tmp, NULL);
            *p = L'\\';
        }
    }
    BOOL ok = CreateDirectoryW(tmp, NULL);
    return ok || GetLastError() == ERROR_ALREADY_EXISTS;
}

/* UTF-8 → UTF-16 */
static int utf8_to_wide(const char *src, int src_len, wchar_t *dst, int dst_len) {
    return MultiByteToWideChar(CP_UTF8, 0, src, src_len, dst, dst_len);
}

/* Записать блок данных в файл (создаёт/перезаписывает) */
static bool write_file_data(const wchar_t *path, const uint8_t *data, uint32_t size) {
    HANDLE hf = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD written;
    BOOL ok = WriteFile(hf, data, size, &written, NULL);
    CloseHandle(hf);
    return ok && written == size;
}

/* DEFLATE inflate из src_buf в динамически выделенный буфер.
 * Использует встроенный tinflate() из inflate.h — нет внешних зависимостей. */
static uint8_t *inflate_buf(const uint8_t *src, uint32_t src_size, uint32_t uncomp_size) {
    if (uncomp_size == 0) return (uint8_t *)malloc(1);   /* пустой файл */
    uint8_t *out = (uint8_t *)malloc(uncomp_size);
    if (!out) return NULL;
    if (tinflate(src, src_size, out, uncomp_size) != 0) { free(out); return NULL; }
    return out;
}

/* ── Счётчик файлов в ZIP (обходим central directory) ───────────────────── */
static int count_zip_entries(const uint8_t *data, size_t size) {
    if (size < sizeof(ZipEndRecord)) return -1;
    /* Ищем конец central directory с конца */
    for (int i = (int)size - (int)sizeof(ZipEndRecord); i >= 0; i--) {
        const ZipEndRecord *end = (const ZipEndRecord *)(data + i);
        if (end->signature == ZIP_END_SIG)
            return (int)end->cd_entries_total;
    }
    return -1;
}

/* ── Основная функция распаковки из буфера ───────────────────────────────── */
int extract_zip_from_memory(const void *zip_data, size_t zip_size,
                             const wchar_t *dest_dir,
                             extract_progress_fn progress_cb) {
    const uint8_t *data = (const uint8_t *)zip_data;
    size_t pos = 0;
    int total = count_zip_entries(data, zip_size);
    int current = 0;

    while (pos + sizeof(ZipLocalHeader) <= zip_size) {
        const ZipLocalHeader *lh = (const ZipLocalHeader *)(data + pos);

        if (lh->signature == ZIP_CENTRAL_SIG || lh->signature == ZIP_END_SIG)
            break;

        if (lh->signature != ZIP_LOCAL_SIG)
            return -1;   /* повреждённый архив */

        pos += sizeof(ZipLocalHeader);

        /* Имя файла (UTF-8 в ZIP) */
        if (pos + lh->filename_len > zip_size) return -1;
        char fname_u8[MAX_PATH] = {0};
        int fnlen = lh->filename_len < MAX_PATH - 1 ? lh->filename_len : MAX_PATH - 1;
        memcpy(fname_u8, data + pos, fnlen);
        fname_u8[fnlen] = '\0';

        /* Преобразуем '/' → '\\' и имя в wide */
        for (int i = 0; fname_u8[i]; i++)
            if (fname_u8[i] == '/') fname_u8[i] = '\\';

        wchar_t fname_w[MAX_PATH] = {0};
        utf8_to_wide(fname_u8, -1, fname_w, MAX_PATH);

        pos += lh->filename_len + lh->extra_len;

        const uint8_t *file_data = data + pos;
        uint32_t comp_size   = lh->compressed_size;
        uint32_t uncomp_size = lh->uncompressed_size;

        if (pos + comp_size > zip_size) return -1;
        pos += comp_size;

        /* Пропускаем data descriptor если установлен флаг 0x08 */
        if (lh->flags & 0x08) {
            /* data descriptor: crc32 + comp + uncomp = 12 байт, может быть сигнатура 4 */
            if (pos + 4 <= zip_size) {
                uint32_t sig;
                memcpy(&sig, data + pos, 4);
                if (sig == 0x08074b50u) pos += 4;
            }
            pos += 12;
        }

        /* Директория — только создаём */
        bool is_dir = (fnlen > 0 && fname_u8[fnlen - 1] == '\\');

        /* Полный путь назначения */
        wchar_t out_path[MAX_PATH];
        _snwprintf(out_path, MAX_PATH, L"%s\\%s", dest_dir, fname_w);

        if (is_dir) {
            makedirs(out_path);
            continue;
        }

        /* Создаём родительские директории */
        wchar_t parent[MAX_PATH];
        wcsncpy(parent, out_path, MAX_PATH - 1);
        wchar_t *last_sep = wcsrchr(parent, L'\\');
        if (last_sep) { *last_sep = L'\0'; makedirs(parent); }

        /* Распаковываем / копируем */
        uint8_t *out_buf = NULL;
        bool alloc = false;

        if (lh->compression == COMP_STORED) {
            out_buf = (uint8_t *)file_data;
        } else if (lh->compression == COMP_DEFLATE) {
            out_buf = inflate_buf(file_data, comp_size, uncomp_size);
            if (!out_buf) return -1;
            alloc = true;
        } else {
            return -1;   /* неизвестный метод сжатия */
        }

        bool ok = write_file_data(out_path, out_buf, uncomp_size);
        if (alloc) free(out_buf);
        if (!ok) return -1;

        current++;
        if (progress_cb) progress_cb(current, total > 0 ? total : current + 1, fname_w);
    }

    return 0;
}

/* ── Распаковка из файла ─────────────────────────────────────────────────── */
int extract_zip_from_file(const wchar_t *zip_path,
                           const wchar_t *dest_dir,
                           extract_progress_fn progress_cb) {
    HANDLE hf = CreateFileW(zip_path, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return -1;

    DWORD size_hi = 0;
    DWORD size_lo = GetFileSize(hf, &size_hi);
    if (size_lo == INVALID_FILE_SIZE || size_hi != 0) { CloseHandle(hf); return -1; }

    uint8_t *buf = (uint8_t *)malloc(size_lo);
    if (!buf) { CloseHandle(hf); return -1; }

    DWORD read = 0;
    BOOL ok = ReadFile(hf, buf, size_lo, &read, NULL);
    CloseHandle(hf);

    if (!ok || read != size_lo) { free(buf); return -1; }

    int ret = extract_zip_from_memory(buf, size_lo, dest_dir, progress_cb);
    free(buf);
    return ret;
}
