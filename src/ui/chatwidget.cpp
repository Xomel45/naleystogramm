#include "chatwidget.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>
#include <QFrame>
#include <QTimer>
#include <QDateTime>

// ── Custom QTextEdit: Enter отправляет, Shift+Enter — перенос ────────────

class MsgInput : public QTextEdit {
    Q_OBJECT
public:
    explicit MsgInput(QWidget* p) : QTextEdit(p) {
        setFixedHeight(44);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
signals:
    void enterPressed();
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Return && !(e->modifiers() & Qt::ShiftModifier)) {
            emit enterPressed();
            return;
        }
        QTextEdit::keyPressEvent(e);
        const int h = qBound(44, static_cast<int>(document()->size().height()) + 14, 120);
        setFixedHeight(h);
    }
};
#include "chatwidget.moc"

// ── ChatWidget ────────────────────────────────────────────────────────────

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    showPlaceholder();

    // При смене темы — перерисовать пузыри не нужно (QSS применится сам)
    // но placeholder иконку обновим
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) {
                // QSS обновился глобально, принудительно обновляем виджет
                update();
            });
}

void ChatWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    // ── Хедер ────────────────────────────────────────────────────────────
    m_header = new QWidget();
    m_header->setObjectName("chatHeader");
    m_header->setFixedHeight(62);
    {
        auto* hl = new QHBoxLayout(m_header);
        hl->setContentsMargins(16, 0, 14, 0);
        hl->setSpacing(12);

        m_peerAvatar = new QLabel();
        m_peerAvatar->setObjectName("peerAvatar");
        m_peerAvatar->setFixedSize(38, 38);
        m_peerAvatar->setAlignment(Qt::AlignCenter);

        auto* infoCol = new QWidget();
        auto* infoLayout = new QVBoxLayout(infoCol);
        infoLayout->setContentsMargins(0,0,0,0);
        infoLayout->setSpacing(2);

        m_peerName   = new QLabel("—");
        m_peerStatus = new QLabel("оффлайн");
        m_peerName->setObjectName("chatPeerName");
        m_peerStatus->setObjectName("chatPeerStatus");

        infoLayout->addWidget(m_peerName);
        infoLayout->addWidget(m_peerStatus);

        m_fileBtn = new QPushButton("⊕");
        m_fileBtn->setObjectName("iconBtn");
        m_fileBtn->setFixedSize(36, 36);
        m_fileBtn->setToolTip("Отправить файл");
        connect(m_fileBtn, &QPushButton::clicked,
                this, &ChatWidget::sendFileRequested);

        hl->addWidget(m_peerAvatar);
        hl->addWidget(infoCol, 1);
        hl->addWidget(m_fileBtn);
    }

    // ── Placeholder ───────────────────────────────────────────────────────
    m_placeholder = new QWidget();
    {
        auto* pl = new QVBoxLayout(m_placeholder);
        pl->setAlignment(Qt::AlignCenter);
        pl->setSpacing(10);

        auto* icon = new QLabel("◈");
        icon->setAlignment(Qt::AlignCenter);
        icon->setObjectName("placeholderIcon");
        icon->setStyleSheet("font-size: 44px; color: #3a3a5c;");

        auto* txt = new QLabel("Выбери контакт");
        txt->setAlignment(Qt::AlignCenter);
        txt->setObjectName("placeholderText");
        txt->setStyleSheet("font-size: 15px; font-weight: 600; color: #3a3a5c; letter-spacing: 0.5px;");

        auto* sub = new QLabel("чтобы начать разговор");
        sub->setAlignment(Qt::AlignCenter);
        sub->setStyleSheet("font-size: 12px; color: #2a2a48;");

        pl->addWidget(icon);
        pl->addWidget(txt);
        pl->addWidget(sub);
    }

    // ── Сообщения ─────────────────────────────────────────────────────────
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_msgContainer = new QWidget();
    m_msgLayout = new QVBoxLayout(m_msgContainer);
    m_msgLayout->setContentsMargins(20, 16, 20, 16);
    m_msgLayout->setSpacing(4);
    m_msgLayout->addStretch();
    m_scrollArea->setWidget(m_msgContainer);

    // ── Поле ввода ────────────────────────────────────────────────────────
    m_inputBar = new QWidget();
    m_inputBar->setObjectName("inputBar");
    {
        auto* il = new QHBoxLayout(m_inputBar);
        il->setContentsMargins(14, 10, 14, 10);
        il->setSpacing(10);

        auto* inp = new MsgInput(m_inputBar);
        inp->setObjectName("msgInput");
        inp->setPlaceholderText("Сообщение...");
        m_input = inp;

        m_sendBtn = new QPushButton("↑");
        m_sendBtn->setObjectName("sendBtn");
        m_sendBtn->setFixedSize(44, 44);

        connect(inp,      &MsgInput::enterPressed,    this, &ChatWidget::onSendClicked);
        connect(m_sendBtn, &QPushButton::clicked,     this, &ChatWidget::onSendClicked);

        il->addWidget(m_input, 1);
        il->addWidget(m_sendBtn);
    }

    layout->addWidget(m_header);
    layout->addWidget(m_placeholder, 1);
    layout->addWidget(m_scrollArea,  1);
    layout->addWidget(m_inputBar);

    m_header->hide();
    m_scrollArea->hide();
    m_inputBar->hide();
}

void ChatWidget::showPlaceholder() {
    m_header->hide();
    m_scrollArea->hide();
    m_inputBar->hide();
    m_placeholder->show();
}

void ChatWidget::openConversation(const QString& peerName, bool isOnline) {
    m_placeholder->hide();
    m_header->show();
    m_scrollArea->show();
    m_inputBar->show();

    m_peerName->setText(peerName);
    m_peerAvatar->setText(peerName.left(1).toUpper());
    setPeerStatus(isOnline ? "● онлайн" : "○ оффлайн");
    m_input->setFocus();
}

void ChatWidget::setPeerStatus(const QString& status) {
    m_peerStatus->setText(status);
}

void ChatWidget::loadHistory(const QList<Message>& messages) {
    QLayoutItem* item;
    while ((item = m_msgLayout->takeAt(0))) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_msgLayout->addStretch();

    for (const auto& msg : messages) {
        if (!msg.fileName.isEmpty())
            appendMessage(QString("⊕ %1").arg(msg.fileName),
                          msg.outgoing, msg.timestamp);
        else
            appendMessage(msg.text, msg.outgoing, msg.timestamp);
    }
}

void ChatWidget::appendMessage(const QString& text, bool outgoing,
                                const QDateTime& ts) {
    // Группировка по отправителю — небольшой отступ если тот же
    m_msgLayout->addWidget(makeBubble(text, outgoing, ts));

    QTimer::singleShot(30, m_scrollArea, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

QWidget* ChatWidget::makeBubble(const QString& text, bool outgoing,
                                 const QDateTime& ts) {
    const auto& p = ThemeManager::instance().palette();

    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(0);

    auto* bubble = new QLabel();
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setMaximumWidth(460);
    bubble->setMinimumWidth(60);

    const QString timeStr = ts.toString("hh:mm");
    bubble->setText(
        text + QString("  <span style='color:%1; font-size:10px;'>%2</span>")
               .arg(p.textMuted, timeStr)
    );
    bubble->setTextFormat(Qt::RichText);

    if (outgoing) {
        bubble->setStyleSheet(QString(R"(
            QLabel {
                background: %1;
                border-radius: 16px;
                border-bottom-right-radius: 4px;
                padding: 9px 14px;
                color: %2;
                font-size: 14px;
                line-height: 1.4;
            }
        )").arg(p.bgBubbleOut, p.textPrimary));
        rowLayout->addStretch();
        rowLayout->addWidget(bubble);
    } else {
        bubble->setStyleSheet(QString(R"(
            QLabel {
                background: %1;
                border: 1px solid %2;
                border-radius: 16px;
                border-bottom-left-radius: 4px;
                padding: 9px 14px;
                color: %3;
                font-size: 14px;
                line-height: 1.4;
            }
        )").arg(p.bgBubbleIn, p.border, p.textPrimary));
        rowLayout->addWidget(bubble);
        rowLayout->addStretch();
    }

    return row;
}

void ChatWidget::onSendClicked() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();
    m_input->setFixedHeight(44);
    emit sendMessage(text);
}
