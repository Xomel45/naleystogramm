#pragma once
#include <QObject>
#include <QUuid>
#include <QJsonObject>
#include "network.h"

// Forward declaration чтобы не тянуть весь e2e.h
class E2EManager;

// Chunked file transfer over the existing P2P connection.
// Ключ шифрования файла передаётся через E2E-сессию (не в открытую!).
// Protocol:
//   FILE_OFFER  → receiver shows accept/reject dialog
//   FILE_ACCEPT → sender starts sending FILE_CHUNK messages
//   FILE_CHUNK  → base64 chunk of encrypted data
//   FILE_DONE   → transfer complete
class FileTransfer : public QObject {
    Q_OBJECT
public:
    // FIX: передаём E2EManager чтобы шифровать ключи файлов
    explicit FileTransfer(NetworkManager* network, E2EManager* e2e,
                          QObject* parent = nullptr);

    void sendFile(const QUuid& peerUuid, const QString& filePath);
    void handleMessage(const QUuid& from, const QJsonObject& msg);

signals:
    void fileReceived(QUuid from, QString path, QString name);
    void fileOffer(QUuid from, QString name, qint64 size, QString offerId);
    void transferProgress(QString offerId, int percent);

public slots:
    void acceptOffer(const QUuid& from, const QString& offerId);
    void rejectOffer(const QUuid& from, const QString& offerId);

private:
    struct OutTransfer {
        QUuid   peerUuid;
        QString filePath;
        QByteArray key;
        QByteArray iv;
    };

    struct InTransfer {
        QUuid      peerUuid;
        QString    name;
        qint64     size{0};
        QByteArray key;
        QByteArray iv;
        QList<QByteArray> chunks;
    };

    void startSending(const QString& offerId);
    void finishReceiving(const QString& offerId, InTransfer& t);

    NetworkManager* m_net{nullptr};
    E2EManager*     m_e2e{nullptr};  // FIX: для шифрования ключей
    QMap<QString, OutTransfer> m_out;
    QMap<QString, InTransfer>  m_in;
};
