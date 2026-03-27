#include "filetransfer.h"
#include "logger.h"
#include "../crypto/e2e.h"
#include "../crypto/keyprotector.h"

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

#include <openssl/evp.h>
#include <openssl/rand.h>

// ── Конструктор/деструктор ───────────────────────────────────────────────────

FileTransfer::FileTransfer(NetworkManager* network, E2EManager* e2e,
                           QObject* parent)
    : QObject(parent)
    , m_net(network)
    , m_e2e(e2e)
{
    LOG_INFO(FileTransfer, "FileTransfer initialized");
}

FileTransfer::~FileTransfer() {
    // L-2: отменяем все активные вычисления хеша перед уничтожением объекта.
    // Без этого lambda-колбэк watcher::finished может обратиться к уже удалённому this.
    // QFutureWatcher<T> не имеет Q_OBJECT → используем базовый QFutureWatcherBase.
    for (auto* w : findChildren<QFutureWatcherBase*>()) {
        w->cancel();
        w->waitForFinished();
    }

    // Закрываем все открытые файлы
    for (auto& t : m_outgoing) {
        if (t.file) {
            t.file->close();
            delete t.file;
        }
    }
    for (auto& t : m_incoming) {
        if (t.file) {
            t.file->close();
            delete t.file;
        }
        delete t.hasher;
    }
}

// ── Отправка файла ───────────────────────────────────────────────────────────

void FileTransfer::sendFile(const QUuid& peerUuid, const QString& filePath, int durationMs) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        LOG_ERROR(FileTransfer, "File Not Found");
        return;
    }

    // Генерируем уникальный ID передачи
    const QString offerId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    LOG_INFO(FileTransfer, "Preparing File Offer");

    // Создаём структуру передачи (fileHash пока пустой — заполним асинхронно)
    OutgoingTransfer t;
    t.id         = offerId;
    t.peerUuid   = peerUuid;
    t.filePath   = filePath;
    t.fileName   = fi.fileName();
    t.fileSize   = fi.size();
    t.bytesSent  = 0;
    t.chunksSent = 0;
    t.file       = nullptr;
    t.state      = TransferState::Pending;
    t.lastSpeedCalcBytes = 0;
    t.lastSpeedCalcTime  = 0;
    t.currentSpeed       = 0.0;
    t.durationMs         = durationMs;

    // Генерируем ключ AES-256 (32 байта) и nonce (12 байт для GCM)
    t.key.resize(kAesKeySize);
    t.nonce.resize(kGcmNonceSize);
    RAND_bytes(reinterpret_cast<unsigned char*>(t.key.data()), kAesKeySize);
    RAND_bytes(reinterpret_cast<unsigned char*>(t.nonce.data()), kGcmNonceSize);

    m_outgoing[offerId] = t;

    // Вычисляем SHA-256 в фоновом потоке — не блокируем UI на 150–600ms
    auto* watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
            [this, watcher, offerId]() {
        const QByteArray hash = watcher->result();
        watcher->deleteLater();

        if (!m_outgoing.contains(offerId)) return;  // передача отменена пока считался хеш

        if (hash.isEmpty()) {
            LOG_ERROR(FileTransfer, "File Hash Error");
            emit transferFailed(offerId, tr("Ошибка вычисления хеша файла"));
            m_outgoing.remove(offerId);
            return;
        }

        m_outgoing[offerId].fileHash = hash;
        sendFileOffer(offerId);
    });

    watcher->setFuture(QtConcurrent::run(
        [filePath]() { return FileTransfer::computeFileHashStatic(filePath); }));
}

// ── Вычисление хеша файла (статический, запускается в пуле потоков) ──────────

QByteArray FileTransfer::computeFileHashStatic(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("[FileTransfer] Cannot Open File For Hashing");
        return {};
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);

    // Читаем файл чанками для экономии памяти
    constexpr qint64 hashChunkSize = 1024 * 1024;  // 1 MB
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(hashChunkSize);
        hasher.addData(chunk);
    }

    file.close();
    return hasher.result();
}

// ── Формирование и отправка FILE_OFFER ───────────────────────────────────────
// Вызывается из watcher::finished после асинхронного вычисления хеша.

void FileTransfer::sendFileOffer(const QString& offerId) {
    if (!m_outgoing.contains(offerId)) return;
    const auto& t = m_outgoing[offerId];

    // Формируем FILE_OFFER сообщение
    QJsonObject offer;
    offer["type"] = "FILE_OFFER";
    offer["id"]   = offerId;
    offer["name"] = t.fileName;
    offer["size"] = t.fileSize;
    offer["hash"] = QString::fromLatin1(t.fileHash.toHex());
    // Поле duration_ms > 0 сигнализирует получателю что это голосовое сообщение
    if (t.durationMs > 0)
        offer["duration_ms"] = t.durationMs;

    // Шифруем ключ + nonce через E2E сессию.
    // Без активной сессии отправка НЕВОЗМОЖНА: ключ файла не должен идти по сети в открытом виде.
    // Передача завершается с ошибкой — пользователь должен дождаться установки E2E-сессии.
    if (!m_e2e || !m_e2e->hasSession(t.peerUuid)) {
        LOG_ERROR(FileTransfer, "Offer Failed (No Active Session)");
        emit transferFailed(offerId,
                            tr("Нет E2E-сессии — ключ файла нельзя передать безопасно"));
        m_outgoing.remove(offerId);
        return;
    }

    const QByteArray keyMaterial = t.key + t.nonce;  // 32 + 12 = 44 байта
    const QJsonObject encKeyEnv = m_e2e->encrypt(t.peerUuid, keyMaterial);
    offer["enc_key_env"] = encKeyEnv;
    LOG_DEBUG(FileTransfer, "Ключ файла зашифрован через E2E-сессию");

    m_net->sendJson(t.peerUuid, offer);
    LOG_INFO(FileTransfer, "File Offer Sent");
}

// ── Стриминг отправки ────────────────────────────────────────────────────────

void FileTransfer::startStreaming(const QString& offerId) {
    if (!m_outgoing.contains(offerId)) {
        LOG_WARNING(FileTransfer, "Unknown Offer");
        return;
    }

    auto& t = m_outgoing[offerId];

    // Открываем файл для стриминга
    t.file = new QFile(t.filePath);
    if (!t.file->open(QIODevice::ReadOnly)) {
        LOG_ERROR(FileTransfer, "File Open Failed");
        t.state = TransferState::Failed;
        emit transferFailed(offerId, tr("Cannot open file"));
        delete t.file;          // предотвращаем утечку QFile
        t.file = nullptr;
        m_outgoing.remove(offerId);
        return;
    }

    t.state = TransferState::Active;
    t.timer.start();
    t.lastSpeedCalcTime = 0;
    t.lastSpeedCalcBytes = 0;

    LOG_INFO(FileTransfer, "Streaming Started");

    // Эмитим начало передачи
    TransferProgress progress;
    progress.id               = offerId;
    progress.fileName         = t.fileName;
    progress.bytesTransferred = 0;
    progress.totalBytes       = t.fileSize;
    progress.speedBytesPerSec = 0;
    progress.etaSeconds       = 0;
    progress.percent          = 0;
    progress.outgoing         = true;
    emit transferStarted(progress);

    // Отправляем первый чанк
    sendNextChunk(offerId);
}

void FileTransfer::sendNextChunk(const QString& offerId) {
    if (!m_outgoing.contains(offerId)) return;

    auto& t = m_outgoing[offerId];

    // Проверяем состояние (пауза/отмена)
    if (t.state == TransferState::Paused || t.state == TransferState::Cancelled) {
        return;
    }

    if (!t.file || t.file->atEnd()) {
        // Все данные отправлены — отправляем FILE_COMPLETE
        LOG_INFO(FileTransfer, "Streaming Completed");

        QJsonObject complete;
        complete["type"]    = "FILE_COMPLETE";
        complete["id"]      = offerId;
        complete["hash"]    = QString::fromLatin1(t.fileHash.toHex());
        complete["success"] = true;
        m_net->sendJson(t.peerUuid, complete);

        t.state = TransferState::Completed;
        emit transferCompleted(offerId, t.filePath, true);

        // Очищаем ресурсы
        t.file->close();
        delete t.file;
        removeTransferState(offerId);
        m_outgoing.remove(offerId);
        return;
    }

    // Читаем следующий чанк
    const QByteArray plainChunk = t.file->read(kChunkSize);
    if (plainChunk.isEmpty()) {
        LOG_WARNING(FileTransfer, "Empty Chunk");
        return;
    }

    // Шифруем чанк с AES-256-GCM
    QByteArray authTag;
    const QByteArray encryptedChunk = encryptChunk(
        plainChunk, t.key, t.nonce, t.chunksSent, authTag);

    if (encryptedChunk.isEmpty()) {
        LOG_ERROR(FileTransfer, "Chunk Encryption Failed");
        t.state = TransferState::Failed;
        emit transferFailed(offerId, tr("Encryption failed"));
        // Закрываем и освобождаем файл, иначе дескриптор зависнет до конца сессии
        t.file->close();
        delete t.file;
        t.file = nullptr;
        m_outgoing.remove(offerId);
        return;
    }

    // Формируем сообщение FILE_CHUNK
    const bool isLast = t.file->atEnd();
    QJsonObject chunk;
    chunk["type"]  = "FILE_CHUNK";
    chunk["id"]    = offerId;
    chunk["seq"]   = t.chunksSent;
    chunk["data"]  = QString::fromLatin1(encryptedChunk.toBase64());
    chunk["tag"]   = QString::fromLatin1(authTag.toBase64());
    chunk["last"]  = isLast;

    m_net->sendJson(t.peerUuid, chunk);

    // Обновляем статистику
    t.bytesSent += plainChunk.size();
    t.chunksSent++;

    // Эмитим прогресс
    emitProgress(t);

    // Планируем отправку следующего чанка асинхронно (не блокируем UI)
    QTimer::singleShot(0, this, [this, offerId]() {
        sendNextChunk(offerId);
    });
}

// ── AES-256-GCM шифрование ───────────────────────────────────────────────────

QByteArray FileTransfer::encryptChunk(const QByteArray& plaintext,
                                       const QByteArray& key,
                                       const QByteArray& baseNonce,
                                       qint64 chunkSeq,
                                       QByteArray& authTagOut) {
    // Формируем уникальный nonce для этого чанка: baseNonce XOR chunkSeq
    QByteArray nonce = baseNonce;
    for (int i = 0; i < 8 && i < nonce.size(); ++i) {
        nonce[nonce.size() - 1 - i] ^= static_cast<char>((chunkSeq >> (i * 8)) & 0xFF);
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        LOG_ERROR(FileTransfer, "Failed to create EVP_CIPHER_CTX");
        return {};
    }

    // Инициализируем AES-256-GCM
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Устанавливаем длину nonce (12 байт для GCM)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmNonceSize, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Устанавливаем ключ и nonce
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Шифруем данные
    QByteArray ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int outLen = 0;
    int totalLen = 0;

    if (EVP_EncryptUpdate(ctx,
            reinterpret_cast<unsigned char*>(ciphertext.data()),
            &outLen,
            reinterpret_cast<const unsigned char*>(plaintext.constData()),
            plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = outLen;

    // Финализируем
    if (EVP_EncryptFinal_ex(ctx,
            reinterpret_cast<unsigned char*>(ciphertext.data()) + totalLen,
            &outLen) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen += outLen;
    ciphertext.resize(totalLen);

    // Получаем auth tag (16 байт)
    authTagOut.resize(kGcmTagSize);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kGcmTagSize,
            authTagOut.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

// ── AES-256-GCM расшифровка ──────────────────────────────────────────────────

QByteArray FileTransfer::decryptChunk(const QByteArray& ciphertext,
                                       const QByteArray& authTag,
                                       const QByteArray& key,
                                       const QByteArray& baseNonce,
                                       qint64 chunkSeq) {
    // Формируем тот же nonce что при шифровании
    QByteArray nonce = baseNonce;
    for (int i = 0; i < 8 && i < nonce.size(); ++i) {
        nonce[nonce.size() - 1 - i] ^= static_cast<char>((chunkSeq >> (i * 8)) & 0xFF);
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        LOG_ERROR(FileTransfer, "Failed to create EVP_CIPHER_CTX for decryption");
        return {};
    }

    // Инициализируем AES-256-GCM
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Устанавливаем длину nonce
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kGcmNonceSize, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Устанавливаем ключ и nonce
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Расшифровываем данные
    QByteArray plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH, '\0');
    int outLen = 0;
    int totalLen = 0;

    if (EVP_DecryptUpdate(ctx,
            reinterpret_cast<unsigned char*>(plaintext.data()),
            &outLen,
            reinterpret_cast<const unsigned char*>(ciphertext.constData()),
            ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    totalLen = outLen;

    // Устанавливаем auth tag для проверки
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kGcmTagSize,
            const_cast<char*>(authTag.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Финализируем и проверяем auth tag
    if (EVP_DecryptFinal_ex(ctx,
            reinterpret_cast<unsigned char*>(plaintext.data()) + totalLen,
            &outLen) != 1) {
        LOG_ERROR(FileTransfer, "GCM auth tag verification failed — data tampered!");
        EVP_CIPHER_CTX_free(ctx);
        return {};  // Auth tag не совпал — данные повреждены/подменены
    }
    totalLen += outLen;
    plaintext.resize(totalLen);

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

// ── Обработка входящих сообщений ─────────────────────────────────────────────

void FileTransfer::handleMessage(const QUuid& from, const QJsonObject& msg) {
    const QString type = msg["type"].toString();
    const QString id   = msg["id"].toString();

    if (type == "FILE_OFFER") {
        // Получили предложение файла
        IncomingTransfer t;
        t.id            = id;
        t.peerUuid      = from;
        t.fileName      = sanitizeFileName(msg["name"].toString());
        t.fileSize      = static_cast<qint64>(msg["size"].toDouble());
        t.expectedHash  = QByteArray::fromHex(msg["hash"].toString().toLatin1());
        t.bytesReceived = 0;
        t.chunksReceived = 0;
        t.file          = nullptr;
        t.hasher        = nullptr;
        t.state         = TransferState::Pending;
        t.lastSpeedCalcBytes = 0;
        t.lastSpeedCalcTime  = 0;
        t.currentSpeed       = 0.0;
        // Читаем длительность голосового сообщения (0 если не голосовое)
        t.durationMs         = msg["duration_ms"].toInt(0);

        // Расшифровываем ключ
        if (msg.contains("enc_key_env") && m_e2e && m_e2e->hasSession(from)) {
            const QByteArray keyMaterial = m_e2e->decrypt(
                from, msg["enc_key_env"].toObject());
            if (keyMaterial.size() >= kAesKeySize + kGcmNonceSize) {
                t.key   = keyMaterial.left(kAesKeySize);
                t.nonce = keyMaterial.mid(kAesKeySize, kGcmNonceSize);
                LOG_DEBUG(FileTransfer, "File key decrypted via E2E session");
            } else {
                LOG_ERROR(FileTransfer, "Failed to decrypt file key via E2E");
                return;
            }
        } else {
            // Fallback: открытый ключ
            t.key   = QByteArray::fromHex(msg["enc_key"].toString().toLatin1());
            t.nonce = QByteArray::fromHex(msg["enc_nonce"].toString().toLatin1());
            LOG_WARNING(FileTransfer, "Received file key unencrypted");
        }

        // Определяем пути
        t.tempFilePath  = tempFilePath(id);
        t.finalFilePath = safeDownloadPath(t.fileName);

        m_incoming[id] = t;

        LOG_INFO(FileTransfer, "File Offer Received");

        emit fileOffer(from, t.fileName, t.fileSize, id, t.durationMs);

    } else if (type == "FILE_ACCEPT") {
        // Получатель принял — начинаем стриминг
        LOG_INFO(FileTransfer, "File Accept Received");
        startStreaming(id);

    } else if (type == "FILE_REJECT") {
        // Получатель отклонил
        LOG_INFO(FileTransfer, "File Reject Received");
        if (m_outgoing.contains(id)) {
            auto& t = m_outgoing[id];
            if (t.file) {
                t.file->close();
                delete t.file;
            }
            m_outgoing.remove(id);
        }

    } else if (type == "FILE_CHUNK") {
        handleFileChunk(from, msg);

    } else if (type == "FILE_COMPLETE") {
        // Отправитель сообщает о завершении (дублирующая проверка)
        LOG_DEBUG(FileTransfer, "File Complete Received");

    } else if (type == "FILE_CANCEL") {
        // Отмена передачи
        LOG_INFO(FileTransfer, "File Cancel Received");
        if (m_outgoing.contains(id)) {
            auto& t = m_outgoing[id];
            t.state = TransferState::Cancelled;
            if (t.file) {
                t.file->close();
                delete t.file;
            }
            emit transferCancelled(id);
            m_outgoing.remove(id);
        }
        if (m_incoming.contains(id)) {
            auto& t = m_incoming[id];
            t.state = TransferState::Cancelled;
            if (t.file) {
                t.file->close();
                delete t.file;
                QFile::remove(t.tempFilePath);
            }
            delete t.hasher;
            emit transferCancelled(id);
            m_incoming.remove(id);
        }
        removeTransferState(id);

    } else if (type == "FILE_PAUSE") {
        // Приостановка передачи
        LOG_INFO(FileTransfer, "File Pause Received");
        if (m_outgoing.contains(id)) {
            m_outgoing[id].state = TransferState::Paused;
        }

    } else if (type == "FILE_RESUME_REQUEST") {
        handleResumeRequest(from, msg);

    } else if (type == "FILE_RESUME_ACK") {
        // Отправитель подтвердил возобновление
        const qint64 resumeFrom = static_cast<qint64>(msg["resume_from"].toDouble());
        LOG_INFO(FileTransfer, "File Resume ACK Received");
        if (m_incoming.contains(id)) {
            m_incoming[id].state = TransferState::Active;
            m_incoming[id].chunksReceived = resumeFrom;
        }
    }
}

// ── Обработка входящего чанка ────────────────────────────────────────────────

void FileTransfer::handleFileChunk(const QUuid& /*from*/, const QJsonObject& msg) {
    const QString id  = msg["id"].toString();
    const qint64  seq = static_cast<qint64>(msg["seq"].toDouble());

    if (!m_incoming.contains(id)) {
        LOG_WARNING(FileTransfer, "Unknown Transfer");
        return;
    }

    auto& t = m_incoming[id];

    // Проверяем состояние
    if (t.state == TransferState::Paused || t.state == TransferState::Cancelled) {
        return;
    }

    // Проверяем последовательность
    if (seq != t.chunksReceived) {
        LOG_WARNING(FileTransfer, "Chunk Sequence Mismatch");
        // Можно запросить повтор или отменить передачу
        return;
    }

    // Открываем файл при первом чанке
    if (!t.file) {
        t.file = new QFile(t.tempFilePath);
        if (!t.file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            LOG_ERROR(FileTransfer, "Temp File Creation Failed");
            t.state = TransferState::Failed;
            emit transferFailed(id, tr("Cannot create temp file"));
            delete t.file;          // предотвращаем утечку QFile
            t.file = nullptr;
            m_incoming.remove(id);
            return;
        }

        t.hasher = new QCryptographicHash(QCryptographicHash::Sha256);
        t.timer.start();
        t.state = TransferState::Active;

        // Эмитим начало передачи
        TransferProgress progress;
        progress.id               = id;
        progress.fileName         = t.fileName;
        progress.bytesTransferred = 0;
        progress.totalBytes       = t.fileSize;
        progress.speedBytesPerSec = 0;
        progress.etaSeconds       = 0;
        progress.percent          = 0;
        progress.outgoing         = false;
        emit transferStarted(progress);
    }

    // Расшифровываем чанк
    const QByteArray ciphertext = QByteArray::fromBase64(
        msg["data"].toString().toLatin1());
    const QByteArray authTag = QByteArray::fromBase64(
        msg["tag"].toString().toLatin1());

    const QByteArray plaintext = decryptChunk(ciphertext, authTag, t.key, t.nonce, seq);

    if (plaintext.isEmpty()) {
        LOG_ERROR(FileTransfer, "Chunk Decryption Failed");
        t.state = TransferState::Failed;
        emit transferFailed(id, tr("Decryption failed — data corrupted"));

        // Очищаем ресурсы
        t.file->close();
        delete t.file;
        delete t.hasher;
        QFile::remove(t.tempFilePath);
        m_incoming.remove(id);
        return;
    }

    // Записываем в файл
    t.file->write(plaintext);

    // Обновляем хеш
    t.hasher->addData(plaintext);

    // Обновляем статистику
    t.bytesReceived += plaintext.size();
    t.chunksReceived++;

    // Эмитим прогресс
    emitProgress(t);

    // Проверяем завершение
    if (msg["last"].toBool()) {
        finishReceiving(id);
    }
}

// ── Завершение приёма файла ──────────────────────────────────────────────────

void FileTransfer::finishReceiving(const QString& offerId) {
    if (!m_incoming.contains(offerId)) return;

    auto& t = m_incoming[offerId];

    // Закрываем файл
    t.file->close();

    // Проверяем хеш
    const QByteArray computedHash = t.hasher->result();

    if (computedHash != t.expectedHash) {
        LOG_ERROR(FileTransfer, "Hash Mismatch");

        t.state = TransferState::Failed;
        emit transferFailed(offerId, tr("File hash mismatch — corrupted"));

        QFile::remove(t.tempFilePath);
        delete t.file;
        delete t.hasher;
        m_incoming.remove(offerId);
        return;
    }

    LOG_INFO(FileTransfer, "Hash Verified");

    // Перемещаем из временного файла в финальный
    // Если файл уже существует — добавляем суффикс
    QString finalPath = t.finalFilePath;
    int counter = 1;
    while (QFile::exists(finalPath)) {
        QFileInfo fi(t.finalFilePath);
        finalPath = fi.path() + "/" + fi.completeBaseName() +
                    QString(" (%1).").arg(counter++) + fi.suffix();
    }

    if (!QFile::rename(t.tempFilePath, finalPath)) {
        LOG_ERROR(FileTransfer, "File Save Failed");
        t.state = TransferState::Failed;
        emit transferFailed(offerId, tr("Cannot save file"));

        QFile::remove(t.tempFilePath);
        delete t.file;
        delete t.hasher;
        m_incoming.remove(offerId);
        return;
    }

    LOG_INFO(FileTransfer, "File Received");

    t.state = TransferState::Completed;
    emit transferCompleted(offerId, finalPath, false);
    emit fileReceived(t.peerUuid, finalPath, t.fileName);

    // Очищаем ресурсы
    delete t.file;
    delete t.hasher;
    removeTransferState(offerId);
    m_incoming.remove(offerId);
}

// ── Принятие/отклонение предложения ──────────────────────────────────────────

void FileTransfer::acceptOffer(const QUuid& from, const QString& offerId) {
    if (!m_incoming.contains(offerId)) {
        LOG_WARNING(FileTransfer, "Accept: Unknown Offer");
        return;
    }

    LOG_INFO(FileTransfer, "Accepting File Offer");

    QJsonObject msg;
    msg["type"] = "FILE_ACCEPT";
    msg["id"]   = offerId;
    m_net->sendJson(from, msg);
}

void FileTransfer::rejectOffer(const QUuid& from, const QString& offerId) {
    LOG_INFO(FileTransfer, "Rejecting File Offer");

    QJsonObject msg;
    msg["type"] = "FILE_REJECT";
    msg["id"]   = offerId;
    m_net->sendJson(from, msg);

    m_incoming.remove(offerId);
}

// ── Отмена передачи ──────────────────────────────────────────────────────────

void FileTransfer::cancelTransfer(const QString& transferId) {
    LOG_INFO(FileTransfer, "Cancelling Transfer");

    QJsonObject msg;
    msg["type"]   = "FILE_CANCEL";
    msg["id"]     = transferId;
    msg["reason"] = "user cancelled";

    if (m_outgoing.contains(transferId)) {
        auto& t = m_outgoing[transferId];
        t.state = TransferState::Cancelled;
        m_net->sendJson(t.peerUuid, msg);

        if (t.file) {
            t.file->close();
            delete t.file;
        }
        emit transferCancelled(transferId);
        m_outgoing.remove(transferId);
    }

    if (m_incoming.contains(transferId)) {
        auto& t = m_incoming[transferId];
        t.state = TransferState::Cancelled;
        m_net->sendJson(t.peerUuid, msg);

        if (t.file) {
            t.file->close();
            delete t.file;
            QFile::remove(t.tempFilePath);
        }
        delete t.hasher;
        emit transferCancelled(transferId);
        m_incoming.remove(transferId);
    }

    removeTransferState(transferId);
}

// ── Пауза/возобновление ──────────────────────────────────────────────────────

void FileTransfer::pauseTransfer(const QString& transferId) {
    LOG_INFO(FileTransfer, "Pausing Transfer");

    if (m_outgoing.contains(transferId)) {
        auto& t = m_outgoing[transferId];
        t.state = TransferState::Paused;

        // Сохраняем состояние на диск
        TransferResumeInfo info;
        info.id               = transferId;
        info.peerUuid         = t.peerUuid;
        info.fileName         = t.fileName;
        info.tempFilePath     = t.filePath;
        info.totalSize        = t.fileSize;
        info.lastConfirmedChunk = t.chunksSent;
        info.key              = t.key;
        info.nonce            = t.nonce;
        info.outgoing         = true;
        saveTransferState(info);

        QJsonObject msg;
        msg["type"] = "FILE_PAUSE";
        msg["id"]   = transferId;
        m_net->sendJson(t.peerUuid, msg);
    }

    if (m_incoming.contains(transferId)) {
        auto& t = m_incoming[transferId];
        t.state = TransferState::Paused;

        // Сохраняем состояние на диск
        TransferResumeInfo info;
        info.id               = transferId;
        info.peerUuid         = t.peerUuid;
        info.fileName         = t.fileName;
        info.tempFilePath     = t.tempFilePath;
        info.totalSize        = t.fileSize;
        info.lastConfirmedChunk = t.chunksReceived;
        info.key              = t.key;
        info.nonce            = t.nonce;
        info.outgoing         = false;
        saveTransferState(info);

        QJsonObject msg;
        msg["type"] = "FILE_PAUSE";
        msg["id"]   = transferId;
        m_net->sendJson(t.peerUuid, msg);
    }
}

void FileTransfer::resumeTransfer(const QString& transferId) {
    LOG_INFO(FileTransfer, "Resuming Transfer");

    if (m_incoming.contains(transferId)) {
        auto& t = m_incoming[transferId];

        // Отправляем запрос на возобновление отправителю
        QJsonObject msg;
        msg["type"]       = "FILE_RESUME_REQUEST";
        msg["id"]         = transferId;
        msg["last_chunk"] = t.chunksReceived;
        m_net->sendJson(t.peerUuid, msg);
    }
}

void FileTransfer::handleResumeRequest(const QUuid& from, const QJsonObject& msg) {
    const QString id        = msg["id"].toString();
    const qint64  lastChunk = static_cast<qint64>(msg["last_chunk"].toDouble());

    LOG_INFO(FileTransfer, "Resume Request Received");

    if (!m_outgoing.contains(id)) {
        // Попробуем загрузить сохранённое состояние
        TransferResumeInfo info;
        if (!loadTransferState(id, info)) {
            LOG_WARNING(FileTransfer, "Resume Failed: No Saved State");
            return;
        }

        // Восстанавливаем передачу
        OutgoingTransfer t;
        t.id         = id;
        t.peerUuid   = info.peerUuid;
        t.filePath   = info.tempFilePath;
        t.fileName   = info.fileName;
        t.fileSize   = info.totalSize;
        t.key        = info.key;
        t.nonce      = info.nonce;
        t.bytesSent  = lastChunk * kChunkSize;
        t.chunksSent = lastChunk + 1;  // Начинаем со следующего чанка
        t.file       = new QFile(t.filePath);
        t.state      = TransferState::Active;

        if (!t.file->open(QIODevice::ReadOnly)) {
            LOG_ERROR(FileTransfer, "File Reopen Failed");
            delete t.file;
            return;
        }

        // Перематываем к нужной позиции
        t.file->seek(t.chunksSent * kChunkSize);
        t.timer.start();

        m_outgoing[id] = t;
    } else {
        auto& t = m_outgoing[id];
        t.state = TransferState::Active;
        t.chunksSent = lastChunk + 1;
        if (t.file) {
            t.file->seek(t.chunksSent * kChunkSize);
        }
    }

    // Подтверждаем возобновление
    QJsonObject ack;
    ack["type"]        = "FILE_RESUME_ACK";
    ack["id"]          = id;
    ack["resume_from"] = lastChunk + 1;
    m_net->sendJson(from, ack);

    // Продолжаем отправку
    sendNextChunk(id);
}

// ── Сохранение/загрузка состояния передачи ───────────────────────────────────

QString FileTransfer::transferStateDir() {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + "/transfers";
    QDir().mkpath(dir);
    return dir;
}

void FileTransfer::saveTransferState(const TransferResumeInfo& info) {
    QJsonObject obj;
    obj["id"]                 = info.id;
    obj["peerUuid"]           = info.peerUuid.toString(QUuid::WithoutBraces);
    obj["fileName"]           = info.fileName;
    obj["tempFilePath"]       = info.tempFilePath;
    obj["totalSize"]          = info.totalSize;
    obj["lastConfirmedChunk"] = info.lastConfirmedChunk;
    obj["key"]                = QString::fromLatin1(info.key.toHex());
    obj["nonce"]              = QString::fromLatin1(info.nonce.toHex());
    obj["outgoing"]           = info.outgoing;

    const QByteArray jsonBytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    // M-3: шифруем состояние передачи перед записью — AES-ключ файла не должен
    // лежать на диске открытым текстом. Формат файла: зашифрованный блоб (KeyProtector)
    // или fallback plaintext, если KeyProtector не инициализирован.
    const QString path = transferStateDir() + "/" + info.id + ".json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(FileTransfer, "Transfer State Save Failed");
        return;
    }

    if (KeyProtector::instance().isReady()) {
        const QByteArray encrypted = KeyProtector::instance().encrypt(jsonBytes);
        if (!encrypted.isEmpty()) {
            file.write(encrypted);
            LOG_DEBUG(FileTransfer, "Transfer State Saved (Encrypted)");
        } else {
            // Шифрование провалилось — не сохраняем ключевой материал открытым текстом
            LOG_ERROR(FileTransfer, "Transfer State Encryption Failed");
        }
    } else {
        // KeyProtector не готов — plaintext (деградация, логируем предупреждение)
        LOG_WARNING(FileTransfer, "Transfer State Saved Unencrypted");
        file.write(jsonBytes);
    }
    file.close();
}

bool FileTransfer::loadTransferState(const QString& transferId,
                                      TransferResumeInfo& info) {
    const QString path = transferStateDir() + "/" + transferId + ".json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray raw = file.readAll();
    file.close();

    // M-3: определяем формат блоба (зашифрованный или legacy plaintext).
    // Plaintext JSON начинается с '{' (0x7B); зашифрованный блоб — случайный nonce.
    QByteArray jsonBytes;
    const bool looksLikeJson = !raw.isEmpty() && raw[0] == '{';
    if (!looksLikeJson && KeyProtector::instance().isReady()) {
        jsonBytes = KeyProtector::instance().decrypt(raw);
        if (jsonBytes.isEmpty()) {
            LOG_ERROR(FileTransfer, "Transfer State Decryption Failed");
            return false;
        }
    } else {
        jsonBytes = raw;  // legacy plaintext или KeyProtector не готов
    }

    const QJsonObject obj = QJsonDocument::fromJson(jsonBytes).object();

    info.id                 = obj["id"].toString();
    info.peerUuid           = QUuid(obj["peerUuid"].toString());
    info.fileName           = obj["fileName"].toString();
    info.tempFilePath       = obj["tempFilePath"].toString();
    info.totalSize          = static_cast<qint64>(obj["totalSize"].toDouble());
    info.lastConfirmedChunk = static_cast<qint64>(obj["lastConfirmedChunk"].toDouble());
    info.key                = QByteArray::fromHex(obj["key"].toString().toLatin1());
    info.nonce              = QByteArray::fromHex(obj["nonce"].toString().toLatin1());
    info.outgoing           = obj["outgoing"].toBool();

    LOG_DEBUG(FileTransfer, "Transfer State Loaded");
    return true;
}

void FileTransfer::removeTransferState(const QString& transferId) {
    const QString path = transferStateDir() + "/" + transferId + ".json";
    if (QFile::exists(path)) {
        QFile::remove(path);
        LOG_DEBUG(FileTransfer, "Transfer State Removed");
    }
}

// ── Утилиты ──────────────────────────────────────────────────────────────────

QString FileTransfer::sanitizeFileName(const QString& name) {
    // Берём только последний компонент пути — убираем любые директории
    QString safe = QFileInfo(name).fileName();

    // Дополнительно удаляем разделители путей и нулевые байты
    safe.remove(QRegularExpression("[/\\\\\\x00]"));

    // Запрещаем имена вида "..": полностью убираем любое вхождение ".."
    safe.replace("..", "");

    // Ограничиваем длину
    if (safe.length() > 200)
        safe = safe.left(200);

    // Пустое или только точки — дефолтное имя
    {
        QString stripped = safe;
        stripped.remove('.');
        if (safe.isEmpty() || stripped.isEmpty())
            safe = "unnamed_file";
    }

    return safe;
}

QString FileTransfer::safeDownloadPath(const QString& fileName) {
    const QString baseDir = QStandardPaths::writableLocation(
        QStandardPaths::DownloadLocation) + "/naleystogramm";
    QDir().mkpath(baseDir);

    const QString safe = sanitizeFileName(fileName);
    const QString candidate = QDir::cleanPath(baseDir + "/" + safe);

    // Жёсткая проверка: финальный путь ОБЯЗАН начинаться с baseDir.
    // Это исключает любые path-traversal обходы через symlinks и canonicalization.
    if (!candidate.startsWith(QDir::cleanPath(baseDir) + "/")) {
        qWarning("[FileTransfer] Path Traversal Blocked");
        return baseDir + "/unnamed_file";
    }
    return candidate;
}

QString FileTransfer::tempFilePath(const QString& transferId) {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + "/transfers";
    QDir().mkpath(dir);
    return dir + "/" + transferId + ".tmp";
}

// ── Прогресс и скорость ──────────────────────────────────────────────────────

double FileTransfer::calculateSpeed(qint64 currentBytes, qint64& lastBytes,
                                     qint64& lastTimeMs, QElapsedTimer& timer) {
    const qint64 now = timer.elapsed();
    const qint64 timeDelta = now - lastTimeMs;

    if (timeDelta < kSpeedCalcInterval) {
        return -1.0;  // Слишком рано для пересчёта
    }

    const qint64 bytesDelta = currentBytes - lastBytes;
    const double speed = (bytesDelta * 1000.0) / timeDelta;

    lastBytes = currentBytes;
    lastTimeMs = now;

    return speed;
}

void FileTransfer::emitProgress(OutgoingTransfer& t) {
    const double newSpeed = calculateSpeed(
        t.bytesSent, t.lastSpeedCalcBytes, t.lastSpeedCalcTime, t.timer);

    if (newSpeed >= 0) {
        t.currentSpeed = newSpeed;
    }

    TransferProgress progress;
    progress.id               = t.id;
    progress.fileName         = t.fileName;
    progress.bytesTransferred = t.bytesSent;
    progress.totalBytes       = t.fileSize;
    progress.speedBytesPerSec = t.currentSpeed;
    progress.percent          = t.fileSize > 0
        ? static_cast<int>(100 * t.bytesSent / t.fileSize) : 0;
    progress.etaSeconds       = t.currentSpeed > 0
        ? static_cast<int>((t.fileSize - t.bytesSent) / t.currentSpeed) : 0;
    progress.outgoing         = true;

    emit transferProgress(progress);
}

void FileTransfer::emitProgress(IncomingTransfer& t) {
    const double newSpeed = calculateSpeed(
        t.bytesReceived, t.lastSpeedCalcBytes, t.lastSpeedCalcTime, t.timer);

    if (newSpeed >= 0) {
        t.currentSpeed = newSpeed;
    }

    TransferProgress progress;
    progress.id               = t.id;
    progress.fileName         = t.fileName;
    progress.bytesTransferred = t.bytesReceived;
    progress.totalBytes       = t.fileSize;
    progress.speedBytesPerSec = t.currentSpeed;
    progress.percent          = t.fileSize > 0
        ? static_cast<int>(100 * t.bytesReceived / t.fileSize) : 0;
    progress.etaSeconds       = t.currentSpeed > 0
        ? static_cast<int>((t.fileSize - t.bytesReceived) / t.currentSpeed) : 0;
    progress.outgoing         = false;

    emit transferProgress(progress);
}

// ── Получение прогресса ──────────────────────────────────────────────────────

TransferProgress FileTransfer::getProgress(const QString& transferId) const {
    TransferProgress progress = {};
    progress.id = transferId;

    if (m_outgoing.contains(transferId)) {
        const auto& t = m_outgoing[transferId];
        progress.fileName         = t.fileName;
        progress.bytesTransferred = t.bytesSent;
        progress.totalBytes       = t.fileSize;
        progress.speedBytesPerSec = t.currentSpeed;
        progress.percent          = t.fileSize > 0
            ? static_cast<int>(100 * t.bytesSent / t.fileSize) : 0;
        progress.etaSeconds       = t.currentSpeed > 0
            ? static_cast<int>((t.fileSize - t.bytesSent) / t.currentSpeed) : 0;
        progress.outgoing         = true;
    } else if (m_incoming.contains(transferId)) {
        const auto& t = m_incoming[transferId];
        progress.fileName         = t.fileName;
        progress.bytesTransferred = t.bytesReceived;
        progress.totalBytes       = t.fileSize;
        progress.speedBytesPerSec = t.currentSpeed;
        progress.percent          = t.fileSize > 0
            ? static_cast<int>(100 * t.bytesReceived / t.fileSize) : 0;
        progress.etaSeconds       = t.currentSpeed > 0
            ? static_cast<int>((t.fileSize - t.bytesReceived) / t.currentSpeed) : 0;
        progress.outgoing         = false;
    }

    return progress;
}

bool FileTransfer::hasPendingTransfers(const QUuid& peerUuid) const {
    for (const auto& t : m_outgoing) {
        if (t.peerUuid == peerUuid &&
            (t.state == TransferState::Active || t.state == TransferState::Pending)) {
            return true;
        }
    }
    for (const auto& t : m_incoming) {
        if (t.peerUuid == peerUuid &&
            (t.state == TransferState::Active || t.state == TransferState::Pending)) {
            return true;
        }
    }
    return false;
}
