#include "groupmanager.h"
#include "storage.h"
#include "../crypto/x3dh.h"
#include "../crypto/qt_bridge.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>
#include <QDateTime>
#include <openssl/evp.h>
#include <openssl/rand.h>

static constexpr int kMaxReconnAttempts = 10;
static constexpr int kReconnBaseMs = 2000;

GroupManager::GroupManager(StorageManager* storage, QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setProxy(QNetworkProxy::NoProxy);
}

GroupManager::~GroupManager() {}

// ── Crypto helpers ────────────────────────────────────────────────────────────

QString GroupManager::encryptGroupMsg(const QByteArray& key, const QString& text) {
    if (key.size() != 32) return {};
    const QByteArray plain = text.toUtf8();

    QByteArray nonce(12, '\0');
    RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()), 12);

    // AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    QByteArray ct(plain.size(), '\0');
    QByteArray tag(16, '\0');
    int len = 0;

    bool ok = true;
    ok = ok && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) > 0;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) > 0;
    ok = ok && EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                   reinterpret_cast<const unsigned char*>(key.constData()),
                   reinterpret_cast<const unsigned char*>(nonce.constData())) > 0;
    ok = ok && EVP_EncryptUpdate(ctx,
                   reinterpret_cast<unsigned char*>(ct.data()), &len,
                   reinterpret_cast<const unsigned char*>(plain.constData()), plain.size()) > 0;
    int fin = 0;
    ok = ok && EVP_EncryptFinal_ex(ctx,
                   reinterpret_cast<unsigned char*>(ct.data()) + len, &fin) > 0;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16,
                   reinterpret_cast<unsigned char*>(tag.data())) > 0;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    ct.resize(len + fin);

    // Формат: nonce(12) + ciphertext + tag(16)
    QByteArray blob = nonce + ct + tag;
    return QString::fromLatin1(blob.toBase64());
}

QString GroupManager::decryptGroupMsg(const QByteArray& key, const QString& base64) {
    if (key.size() != 32) return {};
    const QByteArray blob = QByteArray::fromBase64(base64.toLatin1());
    if (blob.size() < 12 + 16) return {};

    const QByteArray nonce = blob.left(12);
    const QByteArray tag   = blob.right(16);
    const QByteArray ct    = blob.mid(12, blob.size() - 12 - 16);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    QByteArray plain(ct.size(), '\0');
    int len = 0, fin = 0;
    bool ok = true;

    ok = ok && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) > 0;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) > 0;
    ok = ok && EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                   reinterpret_cast<const unsigned char*>(key.constData()),
                   reinterpret_cast<const unsigned char*>(nonce.constData())) > 0;
    ok = ok && EVP_DecryptUpdate(ctx,
                   reinterpret_cast<unsigned char*>(plain.data()), &len,
                   reinterpret_cast<const unsigned char*>(ct.constData()), ct.size()) > 0;
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                   const_cast<char*>(tag.constData())) > 0;
    ok = ok && EVP_DecryptFinal_ex(ctx,
                   reinterpret_cast<unsigned char*>(plain.data()) + len, &fin) > 0;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) { qWarning("[GroupManager] decryptGroupMsg: GCM auth failed"); return {}; }
    plain.resize(len + fin);
    return QString::fromUtf8(plain);
}

// ── Join ──────────────────────────────────────────────────────────────────────

void GroupManager::joinGroup(const QString& serverUrl, const QString& username) {
    // Генерируем ephemeral X25519 keypair для этой группы
    Bytes privKeyB, pubKeyB;
    if (!X3DH::generateX25519(privKeyB, pubKeyB)) {
        emit joinError(serverUrl, "keygen failed");
        return;
    }

    // POST /group/join
    QUrl url(serverUrl + "/group/join");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const QJsonObject body {
        {"username", username},
        {"pubkey",   QString::fromLatin1(bridge::toQBA(pubKeyB).toBase64())}
    };

    const QByteArray privKey = bridge::toQBA(privKeyB);
    const QByteArray pubKey  = bridge::toQBA(pubKeyB);
    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, serverUrl, username, privKey, pubKey]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning("[GroupManager] join error: %s", qPrintable(reply->errorString()));
            emit joinError(serverUrl, reply->errorString());
            return;
        }
        const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token     = obj["token"].toString();
        const QString keyEncB64 = obj["group_key_enc"].toString();
        const QString role      = obj["role"].toString("member");

        // ECIES расшифровка группового ключа
        const QByteArray encBlob = QByteArray::fromBase64(keyEncB64.toLatin1());
        const QByteArray groupKey = bridge::toQBA(
            X3DH::eciesDecrypt(bridge::fromQBA(privKey), bridge::fromQBA(encBlob)));
        if (groupKey.size() != 32) {
            qWarning("[GroupManager] ECIES decrypt failed for group_key_enc");
            emit joinError(serverUrl, "key decryption failed");
            return;
        }

        // Получить имя и тип группы из /info
        QUrl infoUrl(serverUrl + "/info");
        QNetworkReply* infoReply = m_nam->get(QNetworkRequest(infoUrl));
        connect(infoReply, &QNetworkReply::finished, this,
                [this, infoReply, serverUrl, username, token, groupKey, privKey, pubKey, role]() {
            infoReply->deleteLater();
            const auto info = QJsonDocument::fromJson(infoReply->readAll()).object();

            Group g;
            g.id           = serverUrl.toStdString();
            g.name         = info["name"].toString(serverUrl).toStdString();
            g.type         = info["broadcast_only"].toBool() ? GroupType::Channel : GroupType::Group;
            g.serverUrl    = serverUrl.toStdString();
            g.username     = username.toStdString();
            g.token        = token.toStdString();
            g.groupKey     = bridge::fromQBA(groupKey);
            g.localPrivKey = bridge::fromQBA(privKey);
            g.localPubKey  = bridge::fromQBA(pubKey);
            g.isAdmin      = (role == "owner" || role == "admin");
            g.joinedAt     = QDateTime::currentSecsSinceEpoch() * 1000LL;

            m_storage->saveGroup(g);

            qDebug("[GroupManager] Вступили в %s как %s (%s)",
                   qPrintable(serverUrl), qPrintable(username), qPrintable(role));
            emit groupJoined(g);
            connectGroup(g);
        });
    });
}

// ── Leave ─────────────────────────────────────────────────────────────────────

void GroupManager::leaveGroup(const QString& groupId) {
    auto it = m_conns.find(groupId);
    if (it == m_conns.end()) return;

    const Group& g = it->info;

    // HTTP DELETE /group/leave
    QUrl url(QString::fromStdString(g.serverUrl) + "/group/leave");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QByteArray("Bearer ") + QString::fromStdString(g.token).toUtf8());
    m_nam->deleteResource(req);

    if (it->ws) {
        it->ws->close();
        it->ws->deleteLater();
    }
    if (it->reconnTimer) {
        it->reconnTimer->stop();
        it->reconnTimer->deleteLater();
    }
    m_conns.erase(it);

    m_storage->deleteGroup(groupId.toStdString());
    emit groupLeft(groupId);
}

// ── Connect WS ───────────────────────────────────────────────────────────────

void GroupManager::connectGroup(const Group& g) {
    const QString qid = QString::fromStdString(g.id);
    if (m_conns.contains(qid) && m_conns[qid].ws) {
        m_conns[qid].info = g;
    } else {
        Conn c;
        c.info = g;
        m_conns[qid] = std::move(c);
    }
    openWs(qid);
}

void GroupManager::openWs(const QString& groupId) {
    auto& c = m_conns[groupId];
    if (c.ws) {
        c.ws->close();
        c.ws->deleteLater();
        c.ws = nullptr;
    }

    const QString wsUrl = QString::fromStdString(c.info.serverUrl)
                             .replace("http://", "ws://")
                             .replace("https://", "wss://")
                          + "/group/ws?token=" + QString::fromStdString(c.info.token);

    c.ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    const QString gid = groupId;
    connect(c.ws, &QWebSocket::connected, this, [this, gid]() { onWsConnected(gid); });
    connect(c.ws, &QWebSocket::disconnected, this, [this, gid]() { onWsDisconnected(gid); });
    connect(c.ws, &QWebSocket::textMessageReceived, this, [this, gid](const QString& msg) {
        onWsTextMessage(gid, msg);
    });
    connect(c.ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, [this, gid](QAbstractSocket::SocketError) { onWsError(gid); });

    c.ws->open(QUrl(wsUrl));
}

void GroupManager::onWsConnected(const QString& groupId) {
    if (!m_conns.contains(groupId)) return;
    m_conns[groupId].reconnAttempts = 0;
    qDebug("[GroupManager] WS подключён: %s", qPrintable(groupId));
    emit wsConnected(groupId);
}

void GroupManager::onWsDisconnected(const QString& groupId) {
    emit wsDisconnected(groupId);
    scheduleReconnect(groupId);
}

void GroupManager::onWsError(const QString& groupId) {
    if (!m_conns.contains(groupId)) return;
    qWarning("[GroupManager] WS ошибка: %s", qPrintable(groupId));
    scheduleReconnect(groupId);
}

void GroupManager::scheduleReconnect(const QString& groupId) {
    auto it = m_conns.find(groupId);
    if (it == m_conns.end()) return;
    if (it->reconnAttempts >= kMaxReconnAttempts) {
        qWarning("[GroupManager] Превышено число попыток реконнекта для %s", qPrintable(groupId));
        return;
    }
    const int delay = kReconnBaseMs * (1 << qMin(it->reconnAttempts, 5));
    it->reconnAttempts++;

    if (!it->reconnTimer) {
        it->reconnTimer = new QTimer(this);
        it->reconnTimer->setSingleShot(true);
        connect(it->reconnTimer, &QTimer::timeout, this, [this, groupId]() {
            if (m_conns.contains(groupId)) openWs(groupId);
        });
    }
    it->reconnTimer->start(delay);
}

// ── WS message handler ────────────────────────────────────────────────────────

void GroupManager::onWsTextMessage(const QString& groupId, const QString& raw) {
    const auto obj = QJsonDocument::fromJson(raw.toUtf8()).object();
    const QString type = obj["type"].toString();

    auto it = m_conns.find(groupId);
    if (it == m_conns.end()) return;
    const QByteArray key = bridge::toQBA(it->info.groupKey);

    if (type == "history") {
        QList<GroupMessage> hist;
        const auto msgs = obj["messages"].toArray();
        for (const auto& mv : msgs) {
            const auto mo = mv.toObject();
            const QString decrypted = decryptGroupMsg(key, mo["data"].toString());
            if (decrypted.isEmpty()) continue;
            const QString sender = mo["sender"].toString();
            const bool outgoing = (sender.toStdString() == it->info.username);
            GroupMessage gm;
            gm.groupId  = groupId.toStdString();
            gm.sender   = sender.toStdString();
            gm.text     = decrypted.toStdString();
            gm.ts       = mo["ts"].toInteger();
            gm.outgoing = outgoing;
            // Сохраняем если ещё нет в БД (история — нет дублей)
            m_storage->saveGroupMessage(gm);
            hist.append(gm);
        }
        emit historyLoaded(groupId, hist);

    } else if (type == "msg") {
        const QString sender    = obj["sender"].toString();
        const QString decrypted = decryptGroupMsg(key, obj["data"].toString());
        if (decrypted.isEmpty()) return;
        const bool outgoing = (sender.toStdString() == it->info.username);

        GroupMessage gm;
        gm.groupId  = groupId.toStdString();
        gm.sender   = sender.toStdString();
        gm.text     = decrypted.toStdString();
        gm.ts       = obj["ts"].toInteger(QDateTime::currentSecsSinceEpoch());
        gm.outgoing = outgoing;
        m_storage->saveGroupMessage(gm);
        emit messageReceived(gm);

    } else if (type == "join") {
        emit memberJoined(groupId, obj["username"].toString(), obj["role"].toString("member"));
    } else if (type == "leave") {
        emit memberLeft(groupId, obj["username"].toString());
    }
}

// ── Send ──────────────────────────────────────────────────────────────────────

bool GroupManager::sendMessage(const QString& groupId, const QString& text) {
    auto it = m_conns.find(groupId);
    if (it == m_conns.end() || !it->ws) return false;
    if (it->ws->state() != QAbstractSocket::ConnectedState) return false;

    const QString encrypted = encryptGroupMsg(bridge::toQBA(it->info.groupKey), text);
    if (encrypted.isEmpty()) return false;

    const QJsonObject frame { {"type", "msg"}, {"data", encrypted} };
    it->ws->sendTextMessage(QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact)));
    return true;
}

// ── Load saved groups ─────────────────────────────────────────────────────────

void GroupManager::loadSavedGroups() {
    const auto saved = m_storage->allGroups();
    for (const Group& g : saved) {
        if (g.token.empty() || g.groupKey.size() != 32) continue;
        m_conns[QString::fromStdString(g.id)].info = g;
        openWs(QString::fromStdString(g.id));
        qDebug("[GroupManager] Загружена группа %s", g.name.c_str());
    }
}

QList<Group> GroupManager::groups() const {
    QList<Group> list;
    list.reserve(m_conns.size());
    for (const auto& c : m_conns) list.append(c.info);
    return list;
}

Group GroupManager::groupById(const QString& id) const {
    return m_conns.contains(id) ? m_conns[id].info : Group{};
}
