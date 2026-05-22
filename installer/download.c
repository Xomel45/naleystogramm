#include "download.h"
#include "installer.h"
#include <wininet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define GITHUB_HOST    "api.github.com"
#define GITHUB_PATH    "/repos/xomel45/naleystogramm/releases/latest"
#define USER_AGENT     "Naleystogramm-Installer/1.0"
#define READ_CHUNK     65536u

/* ── Простой HTTP-GET в буфер ──────────────────────────────────────────── */
typedef struct { char *data; size_t size; } HttpBuf;

static void http_buf_free(HttpBuf *b) { free(b->data); b->data = NULL; b->size = 0; }

static int http_get_to_buf(const char *host, const char *path,
                            bool https, HttpBuf *out) {
    HINTERNET hNet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG,
                                    NULL, NULL, 0);
    if (!hNet) return -1;

    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT
                                : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConn = InternetConnectA(hNet, host, port,
                                        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return -1; }

    DWORD flags = (https ? INTERNET_FLAG_SECURE : 0)
                  | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path, NULL, NULL,
                                       NULL, flags, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return -1; }

    /* Заголовок Accept — GitHub API требует */
    HttpAddRequestHeadersA(hReq,
        "Accept: application/vnd.github+json\r\n"
        "X-GitHub-Api-Version: 2022-11-28\r\n",
        (DWORD)-1, HTTP_ADDREQ_FLAG_ADD);

    if (!HttpSendRequestA(hReq, NULL, 0, NULL, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        return -1;
    }

    /* Проверяем HTTP-статус */
    char status_buf[8] = {0};
    DWORD status_len = sizeof(status_buf);
    DWORD idx = 0;
    HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE, status_buf, &status_len, &idx);
    if (atoi(status_buf) != 200) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        return -1;
    }

    /* Читаем ответ */
    size_t cap = READ_CHUNK;
    out->data = (char *)malloc(cap);
    out->size = 0;
    if (!out->data) goto fail;

    char chunk[READ_CHUNK];
    DWORD bytes_read = 0;
    while (InternetReadFile(hReq, chunk, sizeof(chunk), &bytes_read) && bytes_read > 0) {
        if (out->size + bytes_read + 1 > cap) {
            cap = (out->size + bytes_read + 1) * 2;
            char *tmp = (char *)realloc(out->data, cap);
            if (!tmp) goto fail;
            out->data = tmp;
        }
        memcpy(out->data + out->size, chunk, bytes_read);
        out->size += bytes_read;
    }
    out->data[out->size] = '\0';

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return 0;

fail:
    http_buf_free(out);
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return -1;
}

/* ── Минимальный JSON-поиск ────────────────────────────────────────────── */

/*
 * Найти значение строкового поля в JSON вида: "key":"value"
 * Копирует до out_size-1 символов. Возвращает true если нашёл.
 */
static bool json_find_str(const char *json, const char *key,
                           char *out, int out_size) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ' || *p == ':' || *p == ' ') p++;
    if (*p != '"') return false;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_size - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return *p == '"';
}

/*
 * В массиве assets[] найти объект, у которого "name" содержит substr.
 * Вернуть значение поля field этого объекта.
 */
static bool json_find_asset(const char *json, const char *name_substr,
                             const char *field, char *out, int out_size) {
    const char *p = json;
    while ((p = strstr(p, "\"assets\"")) != NULL) {
        p += 8;
        break;
    }
    if (!p) return false;

    /* Итерируемся по объектам внутри assets [] */
    const char *obj = p;
    while ((obj = strchr(obj, '{')) != NULL) {
        /* Ищем конец объекта — упрощённо: до следующего }, учитывая вложенность */
        int depth = 1;
        const char *e = obj + 1;
        while (*e && depth > 0) {
            if (*e == '{') depth++;
            else if (*e == '}') depth--;
            e++;
        }
        /* Скопируем объект во временный буфер для поиска */
        int obj_len = (int)(e - obj);
        if (obj_len < 2 || obj_len > 4096) { obj = e; continue; }

        char tmp[4096];
        int copy_len = obj_len < (int)sizeof(tmp) - 1 ? obj_len : (int)sizeof(tmp) - 1;
        memcpy(tmp, obj, copy_len);
        tmp[copy_len] = '\0';

        /* Проверяем, что "name" содержит name_substr */
        char name_val[512] = {0};
        if (json_find_str(tmp, "name", name_val, sizeof(name_val))) {
            if (strstr(name_val, name_substr)) {
                if (json_find_str(tmp, field, out, out_size))
                    return true;
            }
        }
        obj = e;
    }
    return false;
}

/* ── Скачать файл по URL во временный файл ─────────────────────────────── */
static int download_url_to_file(const char *url,
                                 wchar_t *out_path,
                                 download_progress_fn progress_cb) {
    /* Парсим URL: https://HOST/PATH */
    char host[256] = {0}, path[1024] = {0};
    bool https = false;

    if (strncmp(url, "https://", 8) == 0) {
        https = true;
        const char *rest = url + 8;
        const char *slash = strchr(rest, '/');
        if (!slash) return -1;
        int hlen = (int)(slash - rest);
        memcpy(host, rest, hlen < 255 ? hlen : 255);
        strncpy(path, slash, sizeof(path) - 1);
    } else if (strncmp(url, "http://", 7) == 0) {
        const char *rest = url + 7;
        const char *slash = strchr(rest, '/');
        if (!slash) return -1;
        int hlen = (int)(slash - rest);
        memcpy(host, rest, hlen < 255 ? hlen : 255);
        strncpy(path, slash, sizeof(path) - 1);
    } else {
        return -1;
    }

    /* Временный файл */
    wchar_t tmp_dir[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp_dir);
    GetTempFileNameW(tmp_dir, L"nal", 0, out_path);
    /* Меняем расширение на .zip */
    wchar_t *dot = wcsrchr(out_path, L'.');
    if (dot) wcscpy(dot, L".zip");

    HINTERNET hNet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG,
                                    NULL, NULL, 0);
    if (!hNet) return -1;

    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT
                                : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConn = InternetConnectA(hNet, host, port,
                                        NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return -1; }

    DWORD flags = (https ? INTERNET_FLAG_SECURE : 0)
                  | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE
                  | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path, NULL, NULL,
                                       NULL, flags, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return -1; }

    HttpAddRequestHeadersA(hReq,
        "Accept: application/octet-stream\r\n",
        (DWORD)-1, HTTP_ADDREQ_FLAG_ADD);

    if (!HttpSendRequestA(hReq, NULL, 0, NULL, 0)) goto fail;

    /* Content-Length для прогресса */
    uint64_t total = 0;
    {
        char cl_buf[32] = {0};
        DWORD cl_len = sizeof(cl_buf);
        DWORD cl_idx = 0;
        if (HttpQueryInfoA(hReq, HTTP_QUERY_CONTENT_LENGTH,
                            cl_buf, &cl_len, &cl_idx))
            total = (uint64_t)_atoi64(cl_buf);
    }

    HANDLE hf = CreateFileW(out_path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) goto fail;

    char buf[READ_CHUNK];
    DWORD bytes_read = 0;
    uint64_t done = 0;
    while (InternetReadFile(hReq, buf, sizeof(buf), &bytes_read) && bytes_read > 0) {
        DWORD written;
        if (!WriteFile(hf, buf, bytes_read, &written, NULL) || written != bytes_read) {
            CloseHandle(hf);
            goto fail;
        }
        done += bytes_read;
        if (progress_cb) progress_cb(done, total);
    }
    CloseHandle(hf);

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return 0;

fail:
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return -1;
}

/* ── Публичный API ─────────────────────────────────────────────────────── */

int download_latest_release(wchar_t *out_path, download_progress_fn progress_cb) {
    /* 1. Получаем JSON с информацией о последнем релизе */
    HttpBuf json = {0};
    if (http_get_to_buf(GITHUB_HOST, GITHUB_PATH, true, &json) != 0)
        return -1;

    /* 2. Ищем asset с "-windows" в имени */
    char dl_url[1024] = {0};
    bool found = json_find_asset(json.data, "-windows", "browser_download_url",
                                  dl_url, sizeof(dl_url));
    http_buf_free(&json);

    if (!found || dl_url[0] == '\0') return -1;

    /* 3. Скачиваем ZIP */
    return download_url_to_file(dl_url, out_path, progress_cb);
}

void download_cleanup(const wchar_t *tmp_path) {
    if (tmp_path && tmp_path[0])
        DeleteFileW(tmp_path);
}
