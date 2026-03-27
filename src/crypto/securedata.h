#pragma once
#include <QByteArray>
#include <openssl/crypto.h>

// Безопасное обнуление ключевого материала.
//
// Обычный QByteArray::clear() освобождает память, но не гарантирует
// перезапись байт — данные могут остаться в куче и быть считаны через
// memory-dump или swap. OPENSSL_cleanse() выполняет гарантированную
// запись нулей, которую компилятор не оптимизирует.
//
// Использовать для всех долгоживущих приватных ключей (IK, SPK, OTPK).

// Безопасно обнуляет содержимое QByteArray и очищает буфер.
inline void secureZero(QByteArray& data) {
    if (!data.isEmpty())
        OPENSSL_cleanse(data.data(), static_cast<size_t>(data.size()));
    data.clear();
}
