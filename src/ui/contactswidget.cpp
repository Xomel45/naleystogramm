#include "contactswidget.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>

ContactsWidget::ContactsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 0);
    layout->setSpacing(4);

    m_search = new QLineEdit();
    m_search->setPlaceholderText("Поиск...");
    m_search->setObjectName("searchInput");
    m_search->setStyleSheet(R"(
        QLineEdit {
            background: #1e1e36; border: 1px solid #2a2a4a;
            border-radius: 16px; padding: 6px 14px;
            color: #e8e6ff; font-size: 13px;
        }
        QLineEdit:focus { border-color: #7c6aff; }
    )");

    m_list = new QListWidget();
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSpacing(2);
    m_list->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item {
            background: transparent;
            border-radius: 10px;
            padding: 8px 6px;
            color: #e8e6ff;
        }
        QListWidget::item:hover    { background: #1e1e36; }
        QListWidget::item:selected { background: #1e1e36; border: 1px solid #2a2a4a; }
    )");

    connect(m_search, &QLineEdit::textChanged, this, &ContactsWidget::onSearchChanged);
    connect(m_list, &QListWidget::itemClicked, this, &ContactsWidget::onItemClicked);

    layout->addWidget(m_search);
    layout->addWidget(m_list, 1);
}

void ContactsWidget::setContacts(const QList<Contact>& contacts) {
    m_contacts = contacts;
    rebuildList(m_search->text());
}

void ContactsWidget::setPeerOnline(const QUuid& uuid, bool online) {
    m_online[uuid] = online;
    rebuildList(m_search->text());
}

void ContactsWidget::updateLastMessage(const QUuid& uuid, const QString& text) {
    m_lastMsg[uuid] = text;
    rebuildList(m_search->text());
}

void ContactsWidget::rebuildList(const QString& filter) {
    m_list->clear();
    for (const auto& c : m_contacts) {
        if (!filter.isEmpty() &&
            !c.name.contains(filter, Qt::CaseInsensitive)) continue;

        const bool online = m_online.value(c.uuid, false);
        const QString last = m_lastMsg.value(c.uuid, "");

        auto* item = new QListWidgetItem();
        const QString bullet = online ? "🟢 " : "⚫ ";
        const QString preview = last.isEmpty() ? "" :
            QString("\n  %1").arg(last.left(40));
        item->setText(bullet + c.name + preview);
        item->setData(Qt::UserRole, c.uuid.toString());
        m_list->addItem(item);
    }
}

void ContactsWidget::onItemClicked(QListWidgetItem* item) {
    if (!item) return;
    const QUuid uuid(item->data(Qt::UserRole).toString());
    if (!uuid.isNull()) emit contactSelected(uuid);
}

void ContactsWidget::onSearchChanged(const QString& text) {
    rebuildList(text);
}
