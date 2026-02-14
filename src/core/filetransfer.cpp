#include "filetransfer.h"
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QJsonObject>
#include <QUuid>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <numeric>    // std::accumulate
#include <ranges>     // C++20
#include "../crypto/e2e.h"

static constexpr int kChunkSize = 65536; // 64 KB

// FIX: принимаем E2EManager для шифрования ключей файлов
FileTransfer::FileTransfer(NetworkManager* network, E2EManager* e2e,
                           QObject* parent)
    : QObject(parent), m_net(network), m_e2e(e2e) {}

void FileTransfer::sendFile(const QUuid& peerUuid, const QString& filePath) {
    QFileInfo fi(filePath);
    if (!fi.exists()) return;

    const QString offerId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    OutTransfer t;
    t.peerUuid = peerUuid;
    t.filePath = filePath;
    t.key.resize(32); t.iv.resize(16);
    RAND_bytes(reinterpret_cast<unsigned char*>(t.key.data()), 32);
    RAND_bytes(reinterpret_cast<unsigned char*>(t.iv.data()), 16);
    m_out[offerId] = t;

    // FIX: шифруем ключ+iv через E2E перед отправкой, а не в открытую
    QByteArray keyMaterial = t.key + t.iv; // 48 байт
    QJsonObject offer;
    offer["type"]    = "FILE_OFFER";
    offer["id"]      = offerId;
    offer["name"]    = fi.fileName();
    offer["size"]    = fi.size();

    if (m_e2e && m_e2e->hasSession(peerUuid)) {
        // Шифруем keyMaterial через Double Ratchet
        const QJsonObject encKeyEnv = m_e2e->encrypt(peerUuid, keyMaterial);
        offer["enc_key_env"] = encKeyEnv; // зашифрованный конверт с ключом
    } else {
        // Fallback: открытый ключ (предупреждаем в лог)
        qWarning("[FileTransfer] No E2E session — sending file key unencrypted!");
        offer["enc_key"] = QString::fromLatin1(t.key.toHex());
        offer["enc_iv"]  = QString::fromLatin1(t.iv.toHex());
    }

    m_net->sendJson(peerUuid, offer);
}

void FileTransfer::acceptOffer(const QUuid& from, const QString& offerId) {
    QJsonObject msg;
    msg["type"] = "FILE_ACCEPT";
    msg["id"]   = offerId;
    m_net->sendJson(from, msg);
}

void FileTransfer::rejectOffer(const QUuid& from, const QString& offerId) {
    QJsonObject msg;
    msg["type"] = "FILE_REJECT";
    msg["id"]   = offerId;
    m_net->sendJson(from, msg);
    m_in.remove(offerId);
}

void FileTransfer::startSending(const QString& offerId) {
    if (!m_out.contains(offerId)) return;
    auto& t = m_out[offerId];

    QFile f(t.filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray plain = f.readAll();

    // AES-256-CBC encrypt
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    QByteArray enc(plain.size() + 16, '\0');
    int len = 0, total = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(t.key.constData()),
        reinterpret_cast<const unsigned char*>(t.iv.constData()));
    EVP_EncryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(enc.data()), &len,
        reinterpret_cast<const unsigned char*>(plain.constData()),
        plain.size());
    total += len;
    EVP_EncryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(enc.data()) + total, &len);
    total += len;
    EVP_CIPHER_CTX_free(ctx);
    enc.resize(total);

    // Send in chunks
    int offset = 0, seq = 0;
    while (offset < enc.size()) {
        const QByteArray chunk = enc.mid(offset, kChunkSize);
        const bool isLast = (offset + kChunkSize) >= enc.size();

        QJsonObject msg;
        msg["type"] = "FILE_CHUNK";
        msg["id"]   = offerId;
        msg["seq"]  = seq++;
        msg["data"] = QString::fromLatin1(chunk.toBase64());
        msg["last"] = isLast;
        m_net->sendJson(t.peerUuid, msg);

        offset += kChunkSize;
    }
    m_out.remove(offerId);
}

void FileTransfer::finishReceiving(const QString& offerId, InTransfer& t) {
    // C++20: ranges + std::accumulate для сборки чанков
    const QByteArray enc = std::accumulate(
        t.chunks.cbegin(), t.chunks.cend(), QByteArray{},
        [](QByteArray acc, const QByteArray& chunk) {
            return acc + chunk;
        });

    // Decrypt
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    QByteArray plain(enc.size(), '\0');
    int len = 0, total = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
        reinterpret_cast<const unsigned char*>(t.key.constData()),
        reinterpret_cast<const unsigned char*>(t.iv.constData()));
    EVP_DecryptUpdate(ctx,
        reinterpret_cast<unsigned char*>(plain.data()), &len,
        reinterpret_cast<const unsigned char*>(enc.constData()), enc.size());
    total += len;
    if (EVP_DecryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(plain.data()) + total, &len) > 0)
        total += len;
    EVP_CIPHER_CTX_free(ctx);
    plain.resize(total);

    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::DownloadLocation) + "/naleystogramm";
    QDir().mkpath(dir);
    const QString outPath = dir + "/" + t.name;

    QFile out(outPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(plain);
        emit fileReceived(t.peerUuid, outPath, t.name);
    }
    m_in.remove(offerId);
}

void FileTransfer::handleMessage(const QUuid& from, const QJsonObject& msg) {
    const QString type = msg["type"].toString();
    const QString id   = msg["id"].toString();

    if (type == "FILE_OFFER") {
        InTransfer t;
        t.peerUuid = from;
        t.name     = msg["name"].toString();
        t.size     = static_cast<qint64>(msg["size"].toDouble());

        // FIX: пробуем расшифровать ключ через E2E
        if (msg.contains("enc_key_env") && m_e2e && m_e2e->hasSession(from)) {
            const QByteArray keyMaterial = m_e2e->decrypt(
                from, msg["enc_key_env"].toObject());
            if (keyMaterial.size() >= 48) {
                t.key = keyMaterial.left(32);
                t.iv  = keyMaterial.mid(32, 16);
            } else {
                qWarning("[FileTransfer] Failed to decrypt file key via E2E");
                return;
            }
        } else {
            // Fallback: открытый ключ (старый формат)
            t.key = QByteArray::fromHex(msg["enc_key"].toString().toLatin1());
            t.iv  = QByteArray::fromHex(msg["enc_iv"].toString().toLatin1());
        }

        m_in[id] = t;
        emit fileOffer(from, t.name, t.size, id);

    } else if (type == "FILE_ACCEPT") {
        startSending(id);

    } else if (type == "FILE_REJECT") {
        m_out.remove(id);

    } else if (type == "FILE_CHUNK") {
        if (!m_in.contains(id)) return;
        auto& t = m_in[id];
        t.chunks.append(QByteArray::fromBase64(
            msg["data"].toString().toLatin1()));
        if (msg["last"].toBool())
            finishReceiving(id, t);
    }
}
