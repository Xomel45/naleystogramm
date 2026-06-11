#include "groupchatwidget.h"
#include "thememanager.h"
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QDateTime>
#include <QApplication>
#include <QKeyEvent>

GroupChatWidget::GroupChatWidget(QWidget* parent) : QWidget(parent) {
    setupUi();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) { applyTheme(); });
}

void GroupChatWidget::setupUi() {
    const ThemePalette& p = ThemeManager::instance().palette();

    setObjectName("groupChatWidget");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Шапка ─────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setObjectName("chatHeader");
    header->setFixedHeight(56);
    auto* headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(12, 0, 8, 0);
    headerLay->setSpacing(8);

    m_nameLabel   = new QLabel;
    m_nameLabel->setStyleSheet(QString("font-weight:600;font-size:14px;color:%1;").arg(p.textPrimary));
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;").arg(p.textSecondary));

    auto* nameBlock = new QVBoxLayout;
    nameBlock->setSpacing(0);
    nameBlock->addWidget(m_nameLabel);
    nameBlock->addWidget(m_statusLabel);

    m_membersBtn = new QPushButton("Участники");
    m_membersBtn->setFixedHeight(28);
    m_membersBtn->setObjectName("headerBtn");
    m_leaveBtn   = new QPushButton("Покинуть");
    m_leaveBtn->setFixedHeight(28);
    m_leaveBtn->setObjectName("headerBtn");
    m_leaveBtn->setStyleSheet(QString("QPushButton{color:%1;}").arg(p.danger));

    headerLay->addLayout(nameBlock, 1);
    headerLay->addWidget(m_membersBtn);
    headerLay->addWidget(m_leaveBtn);

    connect(m_membersBtn, &QPushButton::clicked, this, &GroupChatWidget::membersRequested);
    connect(m_leaveBtn,   &QPushButton::clicked, this, &GroupChatWidget::leaveRequested);

    // ── Сообщения ──────────────────────────────────────────────────────────
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setObjectName("chatScrollArea");
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_msgContainer = new QWidget;
    m_msgContainer->setObjectName("msgContainer");
    m_msgLayout    = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(8, 8, 8, 8);
    m_msgLayout->setSpacing(4);
    m_msgLayout->addStretch();
    m_scrollArea->setWidget(m_msgContainer);

    // ── Строка ввода ───────────────────────────────────────────────────────
    m_inputBar = new QWidget;
    m_inputBar->setObjectName("inputBar");
    m_inputBar->setFixedHeight(56);
    auto* inputLay = new QHBoxLayout(m_inputBar);
    inputLay->setContentsMargins(12, 8, 12, 8);
    inputLay->setSpacing(8);

    m_input = new QTextEdit;
    m_input->setObjectName("messageInput");
    m_input->setFixedHeight(36);
    m_input->setPlaceholderText("Написать сообщение…");
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_input->installEventFilter(this);

    m_sendBtn = new QPushButton("→");
    m_sendBtn->setFixedSize(36, 36);
    m_sendBtn->setObjectName("sendBtn");

    inputLay->addWidget(m_input, 1);
    inputLay->addWidget(m_sendBtn);

    connect(m_sendBtn, &QPushButton::clicked, this, &GroupChatWidget::onSendClicked);

    root->addWidget(header);
    root->addWidget(m_scrollArea, 1);
    root->addWidget(m_inputBar);
}

bool GroupChatWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return && !(ke->modifiers() & Qt::ShiftModifier)) {
            onSendClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void GroupChatWidget::openGroup(const Group& g) {
    m_group = g;
    m_nameLabel->setText(g.name.empty()
        ? QString::fromStdString(g.serverUrl)
        : QString::fromStdString(g.name));
    const QString typeStr = g.type == GroupType::Channel ? "Канал" : "Группа";
    m_statusLabel->setText(typeStr + " · подключение…");

    // Очистить сообщения
    while (m_msgLayout->count() > 1)
        delete m_msgLayout->takeAt(0)->widget();

    // Для канала: если мы не admin — скрыть строку ввода
    bool canSend = (g.type == GroupType::Group || g.isAdmin);
    setCanSend(canSend);
}

void GroupChatWidget::loadHistory(const QList<GroupMessage>& messages) {
    for (const GroupMessage& msg : messages)
        appendMessage(msg);
    scrollToBottom();
}

void GroupChatWidget::appendMessage(const GroupMessage& msg) {
    QWidget* bubble = makeBubble(msg);
    m_msgLayout->insertWidget(m_msgLayout->count() - 1, bubble);
    scrollToBottom();
}

void GroupChatWidget::appendSystemMsg(const QString& text) {
    const ThemePalette& p = ThemeManager::instance().palette();
    auto* lbl = new QLabel(text);
    lbl->setObjectName("systemMsg");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color:%1;font-size:11px;padding:4px 0;").arg(p.textSecondary));
    m_msgLayout->insertWidget(m_msgLayout->count() - 1, lbl);
    scrollToBottom();
}

void GroupChatWidget::setCanSend(bool canSend) {
    m_canSend = canSend;
    m_inputBar->setVisible(canSend);
}

void GroupChatWidget::setConnected(bool connected) {
    if (!m_statusLabel) return;
    m_connected = connected;
    const ThemePalette& p = ThemeManager::instance().palette();
    const QString typeStr = m_group.type == GroupType::Channel ? "Канал" : "Группа";
    if (connected) {
        m_statusLabel->setText(typeStr + " · онлайн");
        m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;").arg(p.online));
    } else {
        m_statusLabel->setText(typeStr + " · нет соединения");
        m_statusLabel->setStyleSheet(QString("font-size:11px;color:%1;").arg(p.textSecondary));
    }
}

void GroupChatWidget::applyTheme() {
    const ThemePalette& p = ThemeManager::instance().palette();

    m_nameLabel->setStyleSheet(QString("font-weight:600;font-size:14px;color:%1;").arg(p.textPrimary));
    m_leaveBtn->setStyleSheet(QString("QPushButton{color:%1;}").arg(p.danger));
    setConnected(m_connected);

    // Перекрасить уже отрисованную историю сообщений
    for (int i = 0; i < m_msgLayout->count() - 1; ++i) {
        QWidget* w = m_msgLayout->itemAt(i)->widget();
        if (!w) continue;

        if (auto* sysLbl = qobject_cast<QLabel*>(w)) {
            sysLbl->setStyleSheet(QString("color:%1;font-size:11px;padding:4px 0;").arg(p.textSecondary));
            continue;
        }

        if (auto* senderLbl = w->findChild<QLabel*>("senderLbl"))
            senderLbl->setStyleSheet(QString("font-size:11px;font-weight:600;color:%1;padding-left:8px;")
                                     .arg(p.accent));

        if (auto* bubble = w->findChild<QLabel*>("bubbleLbl")) {
            const bool outgoing = w->property("outgoing").toBool();
            if (outgoing)
                bubble->setStyleSheet(QString(
                    "QLabel{background:%1;color:%2;border-radius:12px;border-bottom-right-radius:4px;"
                    "padding:8px 10px;font-size:13px;}")
                    .arg(p.bgBubbleOut, p.textPrimary));
            else
                bubble->setStyleSheet(QString(
                    "QLabel{background:%1;color:%2;border-radius:12px;border-bottom-left-radius:4px;"
                    "padding:8px 10px;font-size:13px;}")
                    .arg(p.bgBubbleIn, p.textPrimary));
        }

        for (auto* timeLbl : w->findChildren<QLabel*>("timeLbl"))
            timeLbl->setStyleSheet(QString("font-size:10px;color:%1;").arg(p.textSecondary));
    }
}

void GroupChatWidget::onSendClicked() {
    if (!m_canSend) return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();
    emit sendMessage(text);
}

QWidget* GroupChatWidget::makeBubble(const GroupMessage& msg) {
    const ThemePalette& p = ThemeManager::instance().palette();

    auto* row = new QWidget;
    row->setProperty("outgoing", msg.outgoing);
    auto* rowLay = new QVBoxLayout(row);
    rowLay->setContentsMargins(4, 2, 4, 2);
    rowLay->setSpacing(2);

    if (!msg.outgoing) {
        // Имя отправителя над пузырём (как в TG-группах)
        auto* senderLbl = new QLabel(QString::fromStdString(msg.sender));
        senderLbl->setObjectName("senderLbl");
        senderLbl->setStyleSheet(QString("font-size:11px;font-weight:600;color:%1;padding-left:8px;")
                                 .arg(p.accent));
        rowLay->addWidget(senderLbl);
    }

    auto* bubble = new QLabel(QString::fromStdString(msg.text).toHtmlEscaped().replace("\n", "<br>"));
    bubble->setObjectName("bubbleLbl");
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setMaximumWidth(480);

    const QString ts = QDateTime::fromSecsSinceEpoch(msg.ts).toString("HH:mm");

    if (msg.outgoing) {
        bubble->setStyleSheet(QString(
            "QLabel{background:%1;color:%2;border-radius:12px;border-bottom-right-radius:4px;"
            "padding:8px 10px;font-size:13px;}")
            .arg(p.bgBubbleOut, p.textPrimary));
        // Время
        auto* timeLbl = new QLabel(ts);
        timeLbl->setObjectName("timeLbl");
        timeLbl->setStyleSheet(QString("font-size:10px;color:%1;").arg(p.textSecondary));
        auto* hrow = new QHBoxLayout;
        hrow->setContentsMargins(0, 0, 0, 0);
        hrow->addStretch();
        hrow->addWidget(bubble);
        auto* vbox = new QVBoxLayout;
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(1);
        vbox->addLayout(hrow);
        auto* tsRow = new QHBoxLayout;
        tsRow->addStretch();
        tsRow->addWidget(timeLbl);
        vbox->addLayout(tsRow);
        auto* wrapper = new QWidget;
        wrapper->setLayout(vbox);
        rowLay->addWidget(wrapper);
    } else {
        bubble->setStyleSheet(QString(
            "QLabel{background:%1;color:%2;border-radius:12px;border-bottom-left-radius:4px;"
            "padding:8px 10px;font-size:13px;}")
            .arg(p.bgBubbleIn, p.textPrimary));
        auto* timeLbl = new QLabel(ts);
        timeLbl->setObjectName("timeLbl");
        timeLbl->setStyleSheet(QString("font-size:10px;color:%1;").arg(p.textSecondary));
        auto* hrow = new QHBoxLayout;
        hrow->setContentsMargins(0, 0, 0, 0);
        hrow->addWidget(bubble);
        hrow->addStretch();
        auto* vbox = new QVBoxLayout;
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(1);
        vbox->addLayout(hrow);
        auto* tsRow = new QHBoxLayout;
        tsRow->addWidget(timeLbl);
        tsRow->addStretch();
        vbox->addLayout(tsRow);
        auto* wrapper = new QWidget;
        wrapper->setLayout(vbox);
        rowLay->addWidget(wrapper);
    }

    return row;
}

void GroupChatWidget::scrollToBottom() {
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_scrollArea->verticalScrollBar()->setValue(
        m_scrollArea->verticalScrollBar()->maximum());
}
