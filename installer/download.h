#pragma once
#include "installer.h"

/* Коллбек прогресса загрузки: bytes_done, bytes_total (0 если неизвестно) */
typedef void (*download_progress_fn)(uint64_t bytes_done, uint64_t bytes_total);

/*
 * Скачать последний Windows-релиз с GitHub.
 * Запрашивает api.github.com/repos/xomel45/naleystogramm/releases/latest,
 * ищет asset с именем вида *-windows*.zip, скачивает во временный файл.
 *
 * out_path  — буфер MAX_PATH, заполняется путём к скачанному файлу.
 * Возвращает 0 при успехе, -1 при ошибке.
 */
int download_latest_release(wchar_t *out_path, download_progress_fn progress_cb);

/* Удалить временный файл после завершения установки */
void download_cleanup(const wchar_t *tmp_path);
