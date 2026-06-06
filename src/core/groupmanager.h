#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include <QByteArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include "types.h"

class StorageManager;

class GroupManager : public QObject {
    Q_OBJECT
public:
    explicit GroupManager(StorageManager* storage, QObject* parent = nullptr);
    ~GroupManager();

    // Вступить в группу/канал на сервере (HTTP join + WS)
    void joinGroup(const QString& serverUrl, const QString& username);

    // Покинуть группу (HTTP leave + закрыть WS)
    void leaveGroup(const QString& groupId);

    // Подключить WS для уже сохранённой группы
    void connectGroup(const Group& g);

    // Загрузить все сохранённые группы из БД и установить WS-соединения
    void loadSavedGroups();

    // Отправить зашифрованное сообщение в группу
    bool sendMessage(const QString& groupId, const QString& text);

    // Список всех загруженных групп
    [[nodiscard]] QList<Group> groups() const;
    [[nodiscard]] Group        groupById(const QString& id) const;

signals:
    void groupJoined(Group group);
    void groupLeft(QString groupId);
    void wsConnected(QString groupId);
    void wsDisconnected(QString groupId);
    void messageReceived(GroupMessage msg);
    void memberJoined(QString groupId, QString username, QString role);
    void memberLeft(QString groupId, QString username);
    void historyLoaded(QString groupId, QList<GroupMessage> messages);
    void joinError(QString serverUrl, QString error);

private slots:
    void onWsConnected(const QString& groupId);
    void onWsDisconnected(const QString& groupId);
    void onWsTextMessage(const QString& groupId, const QString& text);
    void onWsError(const QString& groupId);

private:
    struct Conn {
        Group        info;
        QWebSocket*  ws{nullptr};
        QTimer*      reconnTimer{nullptr};
        int          reconnAttempts{0};
    };

    // Encrypt plaintext with group AES key → base64(nonce+ct+tag)
    [[nodiscard]] QString encryptGroupMsg(const QByteArray& key, const QString& text);
    // Decrypt base64(nonce+ct+tag) → plaintext
    [[nodiscard]] QString decryptGroupMsg(const QByteArray& key, const QString& base64);

    void openWs(const QString& groupId);
    void scheduleReconnect(const QString& groupId);

    StorageManager*            m_storage;
    QNetworkAccessManager*     m_nam;
    QHash<QString, Conn>       m_conns;   // groupId → Conn
};
