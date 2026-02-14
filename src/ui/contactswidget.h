#pragma once
#include <QWidget>
#include <QUuid>
#include <QList>
#include "../core/storage.h"

class QListWidget;
class QListWidgetItem;
class QLineEdit;

class ContactsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ContactsWidget(QWidget* parent = nullptr);
    void setContacts(const QList<Contact>& contacts);
    void setPeerOnline(const QUuid& uuid, bool online);
    void updateLastMessage(const QUuid& uuid, const QString& text);

signals:
    void contactSelected(QUuid uuid);

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onSearchChanged(const QString& text);

private:
    void rebuildList(const QString& filter = {});

    QLineEdit*   m_search{nullptr};
    QListWidget* m_list{nullptr};
    QList<Contact> m_contacts;
    QMap<QUuid, bool>    m_online;
    QMap<QUuid, QString> m_lastMsg;
};
