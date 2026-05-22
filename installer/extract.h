#pragma once
#include "installer.h"
#include <stdint.h>

/*
 * Коллбек прогресса. Вызывается после каждого извлечённого файла.
 * current — сколько файлов уже извлечено, total — всего файлов.
 */
typedef void (*extract_progress_fn)(int current, int total, const wchar_t *filename);

/*
 * Извлечь ZIP из буфера памяти в директорию dest_dir.
 * Возвращает 0 при успехе, -1 при ошибке.
 * Путь dest_dir должен уже существовать.
 */
int extract_zip_from_memory(const void *zip_data, size_t zip_size,
                             const wchar_t *dest_dir,
                             extract_progress_fn progress_cb);

/*
 * Извлечь ZIP из файла в директорию dest_dir.
 * Возвращает 0 при успехе, -1 при ошибке.
 */
int extract_zip_from_file(const wchar_t *zip_path,
                           const wchar_t *dest_dir,
                           extract_progress_fn progress_cb);

/* Создать директорию и все родительские, аналог mkdir -p */
bool makedirs(const wchar_t *path);
