#include "chatwidget.h"
#include "thememanager.h"
#include "voicewaveform.h"
#include "../core/audiorecorder.h"
#include <QFileInfo>
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
#include <QPainter>
#include <QPainterPath>
#include <QEvent>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QMenu>
#include <QWidgetAction>
#include <QGridLayout>
#include <QLineEdit>
#include <QApplication>
#include <QClipboard>


#ifdef HAVE_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QAudioOutput>
#endif
#include <QUrl>

// ── Custom QTextEdit: Enter отправляет, Shift+Enter — перенос ────────────

class MsgInput : public QTextEdit {
    Q_OBJECT
public:
    explicit MsgInput(QWidget* p) : QTextEdit(p) {
        setFixedHeight(44);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    void setEnterSends(bool v) { m_enterSends = v; }
signals:
    void enterPressed();
protected:
    void keyPressEvent(QKeyEvent* e) override {
        const bool isReturn = e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter;
        const bool ctrl     = e->modifiers() & Qt::ControlModifier;
        const bool shift    = e->modifiers() & Qt::ShiftModifier;

        if (isReturn) {
            if (m_enterSends) {
                // Режим "Enter отправляет": Enter без Shift → отправить; Shift+Enter → новая строка
                if (!shift) {
                    emit enterPressed();
                    return;
                }
            } else {
                // Режим "Ctrl+Enter отправляет": Ctrl+Enter → отправить; Enter → новая строка
                if (ctrl) {
                    emit enterPressed();
                    return;
                }
            }
        }
        QTextEdit::keyPressEvent(e);
        const int h = qBound(44, static_cast<int>(document()->size().height()) + 14, 120);
        setFixedHeight(h);
    }
private:
    bool m_enterSends{true};
};
#include "chatwidget.moc"

// ── ChatWidget ────────────────────────────────────────────────────────────

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    showPlaceholder();

    // Инициализация записи голосовых
    m_recorder = std::make_unique<AudioRecorder>();
    {
        AudioRecorder::AudioRecorderEvent ev;
        ev.onRecorded = [this](const std::string& path, int durationMs) {
            QMetaObject::invokeMethod(this, [this, path, durationMs]() {
                onRecordingDone(QString::fromStdString(path), durationMs);
            }, Qt::QueuedConnection);
        };
        ev.onLevelChanged = [this](float level) {
            QMetaObject::invokeMethod(this, [this, level]() {
                onLevelChanged(level);
            }, Qt::QueuedConnection);
        };
        m_recorder->addListener(std::move(ev));
    }

    // Таймер счётчика секунд записи
    m_recSecTimer = new QTimer(this);
    m_recSecTimer->setInterval(1000);
    connect(m_recSecTimer, &QTimer::timeout, this, [this]() {
        ++m_recSeconds;
        const int m = m_recSeconds / 60;
        const int s = m_recSeconds % 60;
        m_recIndicator->setText(
            QString("🔴 %1:%2").arg(m).arg(s, 2, 10, QChar('0')));
    });

#ifdef HAVE_QT_MULTIMEDIA
    // Инициализация плеера
    m_audioOutput = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(1.0f);

    // Когда воспроизведение завершается — сбрасываем кнопку и волну
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState && m_activePlayBtn) {
            m_activePlayBtn->setIcon(ThemeManager::tintedIcon(QStringLiteral(":/icons/media_play.png")));
            m_activePlayBtn->setText({});
            m_activePlayBtn = nullptr;
            if (m_activeWaveform) {
                m_activeWaveform->setProgress(0.0f);
                m_activeWaveform = nullptr;
            }
        }
    });

    // Обновляем прогресс волновой формы во время воспроизведения
    connect(m_player, &QMediaPlayer::positionChanged,
            this, [this](qint64 pos) {
        if (m_activeWaveform) {
            const qint64 dur = m_player->duration();
            if (dur > 0)
                m_activeWaveform->setProgress(static_cast<float>(pos) / dur);
        }
    });
#endif

    // Debounce-таймер исходящего typing: 3 сек тишины → typingStopped
    m_typingOutTimer = new QTimer(this);
    m_typingOutTimer->setInterval(3000);
    m_typingOutTimer->setSingleShot(true);
    connect(m_typingOutTimer, &QTimer::timeout, this, [this]() {
        if (m_typingActive) {
            m_typingActive = false;
            emit typingStopped();
        }
    });

    connect(m_input, &QTextEdit::textChanged, this, [this]() {
        if (!m_typingActive && !m_input->toPlainText().isEmpty()) {
            m_typingActive = true;
            emit typingStarted();
        }
        if (!m_input->toPlainText().isEmpty())
            m_typingOutTimer->start();
        else {
            m_typingOutTimer->stop();
            if (m_typingActive) {
                m_typingActive = false;
                emit typingStopped();
            }
        }
    });

    // При смене темы обновляем виджет
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](Theme) {
                update();
            });
}

ChatWidget::~ChatWidget() = default;

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
        hl->setContentsMargins(16, 0, 10, 0);
        hl->setSpacing(10);

        m_peerAvatar = new QLabel();
        m_peerAvatar->setObjectName("peerAvatar");
        m_peerAvatar->setFixedSize(42, 42);
        m_peerAvatar->setAlignment(Qt::AlignCenter);
        m_peerAvatar->setCursor(Qt::PointingHandCursor);
        m_peerAvatar->installEventFilter(this);

        auto* infoCol = new QWidget();
        auto* infoLayout = new QVBoxLayout(infoCol);
        infoLayout->setContentsMargins(0,0,0,0);
        infoLayout->setSpacing(2);

        m_peerName   = new QLabel(QStringLiteral("—"));
        m_peerStatus = new QLabel(tr("offline"));
        m_peerName->setObjectName("chatPeerName");
        m_peerStatus->setObjectName("chatPeerStatus");

        infoLayout->addWidget(m_peerName);
        infoLayout->addWidget(m_peerStatus);

        // Кнопка поиска
        m_searchBtn = new QPushButton();
        m_searchBtn->setObjectName("iconBtn");
        m_searchBtn->setFixedSize(36, 36);
        m_searchBtn->setToolTip(tr("Поиск по сообщениям"));
        m_searchBtn->setCheckable(true);
        ThemeManager::applyIcon(m_searchBtn, QStringLiteral(":/icons/nav_search.png"), QSize(20, 20));
        connect(m_searchBtn, &QPushButton::toggled, this, [this](bool) { toggleSearchBar(); });

        // Кнопка звонка
        m_callBtn = new QPushButton();
        m_callBtn->setObjectName("iconBtn");
        m_callBtn->setFixedSize(36, 36);
        ThemeManager::applyIcon(m_callBtn, QStringLiteral(":/icons/nav_call.png"), QSize(20, 20));
#ifdef HAVE_OPUS
        m_callBtn->setToolTip(tr("Голосовой звонок"));
#else
        m_callBtn->setToolTip(tr("Голосовые звонки недоступны (libopus не найден)"));
        m_callBtn->setEnabled(false);
#endif
        connect(m_callBtn, &QPushButton::clicked, this, [this]() {
            if (!m_peerUuid.isNull()) emit callRequested(m_peerUuid);
        });

        // Кнопка "ещё" (⋮)
        m_menuBtn = new QPushButton();
        m_menuBtn->setObjectName("iconBtn");
        m_menuBtn->setFixedSize(36, 36);
        m_menuBtn->setToolTip(tr("Ещё"));
        ThemeManager::applyIcon(m_menuBtn, QStringLiteral(":/icons/ctx_more.png"), QSize(20, 20));
        connect(m_menuBtn, &QPushButton::clicked, this, [this]() {
            auto* menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            auto* profAct = menu->addAction(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/ctx_info.png")), tr("Профиль"));
            menu->addSeparator();
            auto* fileAct = menu->addAction(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/input_attach.png")), tr("Отправить файл"));
            connect(profAct, &QAction::triggered, this, [this]() {
                if (!m_peerUuid.isNull()) emit openProfileRequested(m_peerUuid);
            });
            connect(fileAct, &QAction::triggered, this, &ChatWidget::sendFileRequested);
            menu->popup(m_menuBtn->mapToGlobal(
                QPoint(m_menuBtn->width() / 2, m_menuBtn->height())));
        });

        hl->addWidget(m_peerAvatar);
        hl->addWidget(infoCol, 1);
        hl->addWidget(m_searchBtn);
        hl->addWidget(m_callBtn);
        hl->addWidget(m_menuBtn);
    }

    // ── Строка поиска (скрыта по умолчанию) ──────────────────────────────
    m_searchBar = new QWidget();
    m_searchBar->setObjectName("searchBar");
    m_searchBar->setFixedHeight(46);
    {
        auto* sl = new QHBoxLayout(m_searchBar);
        sl->setContentsMargins(12, 6, 12, 6);
        sl->setSpacing(6);

        auto* prevBtn = new QPushButton();
        prevBtn->setObjectName("iconBtn");
        prevBtn->setFixedSize(30, 30);
        prevBtn->setToolTip(tr("Предыдущее"));
        ThemeManager::applyIcon(prevBtn, QStringLiteral(":/icons/nav_back.png"), QSize(16, 16));
        m_searchPrev = prevBtn;

        m_searchInput = new QLineEdit();
        m_searchInput->setObjectName("searchBarInput");
        m_searchInput->setPlaceholderText(tr("Поиск в чате..."));
        connect(m_searchInput, &QLineEdit::textChanged, this, &ChatWidget::doSearch);

        m_searchCount = new QLabel();
        m_searchCount->setObjectName("searchCount");
        m_searchCount->setMinimumWidth(50);
        m_searchCount->setAlignment(Qt::AlignCenter);

        auto* nextBtn = new QPushButton();
        nextBtn->setObjectName("iconBtn");
        nextBtn->setFixedSize(30, 30);
        nextBtn->setToolTip(tr("Следующее"));
        ThemeManager::applyIcon(nextBtn, QStringLiteral(":/icons/nav_forward.png"), QSize(16, 16));
        m_searchNext = nextBtn;

        auto* closeBtn = new QPushButton();
        closeBtn->setObjectName("iconBtn");
        closeBtn->setFixedSize(30, 30);
        closeBtn->setToolTip(tr("Закрыть поиск"));
        ThemeManager::applyIcon(closeBtn, QStringLiteral(":/icons/nav_close.png"), QSize(14, 14));

        connect(prevBtn,  &QPushButton::clicked, this, [this]() { navigateSearch(-1); });
        connect(nextBtn,  &QPushButton::clicked, this, [this]() { navigateSearch(+1); });
        connect(closeBtn, &QPushButton::clicked, this, [this]() {
            m_searchBtn->setChecked(false);
        });

        sl->addWidget(prevBtn);
        sl->addWidget(m_searchInput, 1);
        sl->addWidget(m_searchCount);
        sl->addWidget(nextBtn);
        sl->addWidget(closeBtn);
    }
    m_searchBar->hide();

    // ── Placeholder ───────────────────────────────────────────────────────
    m_placeholder = new QWidget();
    {
        auto* pl = new QVBoxLayout(m_placeholder);
        pl->setAlignment(Qt::AlignCenter);
        pl->setSpacing(10);

        auto* icon = new QLabel(QStringLiteral("◈"));
        icon->setAlignment(Qt::AlignCenter);
        icon->setObjectName("placeholderIcon");
        icon->setStyleSheet("font-size: 44px; color: #3a3a5c;");

        auto* txt = new QLabel(tr("Select a contact"));
        txt->setAlignment(Qt::AlignCenter);
        txt->setObjectName("placeholderText");
        txt->setStyleSheet("font-size: 15px; font-weight: 600; color: #3a3a5c; letter-spacing: 0.5px;");

        auto* sub = new QLabel(tr("to start a conversation"));
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
    m_msgLayout->setContentsMargins(16, 16, 16, 16);
    m_msgLayout->setSpacing(2);
    m_msgLayout->addStretch();
    m_scrollArea->setWidget(m_msgContainer);

    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int val) {
        if (!m_loadingMore && val < 80) {
            m_loadingMore = true;
            emit loadMoreRequested();
        }
    });

    // ── Поле ввода ────────────────────────────────────────────────────────
    m_inputBar = new QWidget();
    m_inputBar->setObjectName("inputBar");
    {
        auto* il = new QHBoxLayout(m_inputBar);
        il->setContentsMargins(12, 8, 12, 8);
        il->setSpacing(6);

        // Кнопка прикрепить файл (слева)
        m_attachBtn = new QPushButton();
        m_attachBtn->setObjectName("iconBtn");
        m_attachBtn->setFixedSize(36, 36);
        m_attachBtn->setToolTip(tr("Прикрепить файл"));
        ThemeManager::applyIcon(m_attachBtn, QStringLiteral(":/icons/input_attach.png"), QSize(20, 20));
        connect(m_attachBtn, &QPushButton::clicked, this, &ChatWidget::sendFileRequested);

        auto* inp = new MsgInput(m_inputBar);
        inp->setObjectName("msgInput");
        inp->setPlaceholderText(tr("Сообщение..."));
        m_input = inp;

        // Индикатор записи
        m_recIndicator = new QLabel(QStringLiteral("🔴 0:00"));
        m_recIndicator->setStyleSheet(
            "font-size: 11px; color: #ff4d6d; font-weight: 600;");
        m_recIndicator->hide();

        // Кнопка эмодзи
        m_emojiBtn = new QPushButton();
        m_emojiBtn->setObjectName("iconBtn");
        m_emojiBtn->setFixedSize(36, 36);
        m_emojiBtn->setToolTip(tr("Эмодзи"));
        ThemeManager::applyIcon(m_emojiBtn, QStringLiteral(":/icons/input_emoji.png"), QSize(20, 20));
        connect(m_emojiBtn, &QPushButton::clicked, this, [this]() {
            // Быстрые эмодзи в меню
            static const QStringList kEmoji = {
                "😀","😂","😍","🥺","😭","😊","🙏","👍","❤️","🔥",
                "🎉","👀","😎","🤔","😅","🙄","💀","✅","🚀","💯"
            };
            auto* m = new QMenu(this);
            m->setAttribute(Qt::WA_DeleteOnClose);
            auto* grid = new QWidgetAction(m);
            auto* w = new QWidget();
            auto* gl = new QGridLayout(w);
            gl->setContentsMargins(6, 6, 6, 6);
            gl->setSpacing(2);
            for (int i = 0; i < kEmoji.size(); ++i) {
                auto* btn = new QPushButton(kEmoji[i]);
                btn->setFixedSize(36, 36);
                btn->setFlat(true);
                btn->setStyleSheet("font-size: 18px;");
                const QString e = kEmoji[i];
                connect(btn, &QPushButton::clicked, this, [this, e, m]() {
                    m_input->insertPlainText(e);
                    m->close();
                });
                gl->addWidget(btn, i / 10, i % 10);
            }
            grid->setDefaultWidget(w);
            m->addAction(grid);
            m->popup(m_emojiBtn->mapToGlobal(QPoint(0, -w->sizeHint().height() - 12)));
        });

        // Микрофон
        m_micBtn = new QPushButton();
        m_micBtn->setObjectName("iconBtn");
        m_micBtn->setFixedSize(36, 36);
        m_micBtn->setToolTip(tr("Записать голосовое сообщение"));
        ThemeManager::applyIcon(m_micBtn, QStringLiteral(":/icons/input_voice.png"), QSize(20, 20));
        connect(m_micBtn, &QPushButton::clicked, this, &ChatWidget::onMicClicked);

        // Отправить
        m_sendBtn = new QPushButton();
        m_sendBtn->setObjectName("sendBtn");
        m_sendBtn->setFixedSize(44, 44);
        ThemeManager::applyIconOnAccent(m_sendBtn, QStringLiteral(":/icons/input_send.png"), QSize(20, 20));

        connect(inp,       &MsgInput::enterPressed, this, &ChatWidget::onSendClicked);
        connect(m_sendBtn, &QPushButton::clicked,   this, &ChatWidget::onSendClicked);

        il->addWidget(m_attachBtn);
        il->addWidget(m_input, 1);
        il->addWidget(m_recIndicator);
        il->addWidget(m_emojiBtn);
        il->addWidget(m_micBtn);
        il->addWidget(m_sendBtn);
    }

    // ── Typing indicator ──────────────────────────────────────────────────
    m_typingLabel = new QLabel();
    m_typingLabel->setObjectName("typingIndicator");
    m_typingLabel->setContentsMargins(20, 2, 20, 2);
    m_typingLabel->setStyleSheet("font-size: 12px; font-style: italic; color: #8888aa;");
    m_typingLabel->hide();

    layout->addWidget(m_header);
    layout->addWidget(m_searchBar);
    layout->addWidget(m_placeholder, 1);
    layout->addWidget(m_scrollArea,  1);
    layout->addWidget(m_typingLabel);
    layout->addWidget(m_inputBar);

    m_header->hide();
    m_scrollArea->hide();
    m_inputBar->hide();
}

void ChatWidget::showPlaceholder() {
    m_header->hide();
    m_searchBar->hide();
    m_scrollArea->hide();
    m_inputBar->hide();
    m_placeholder->show();
    if (m_searchBtn) m_searchBtn->setChecked(false);
}

void ChatWidget::openConversation(const QString& peerName, bool isOnline) {
    m_placeholder->hide();
    m_header->show();
    m_scrollArea->show();
    m_inputBar->show();

    // Сбрасываем состояние lazy loading при открытии нового чата
    m_historyOffset = 0;
    m_loadingMore   = false;

    m_peerName->setText(peerName);
    setAvatar({});
    setPeerStatus(isOnline ? tr("online") : tr("offline"));

    // Сбрасываем поиск при открытии нового чата
    clearSearchHighlights();
    m_searchHits.clear();
    m_searchIdx = -1;
    if (m_searchInput) m_searchInput->clear();
    if (m_searchCount) m_searchCount->clear();
    m_searchBar->hide();
    if (m_searchBtn) m_searchBtn->setChecked(false);

    m_input->setFocus();
}

void ChatWidget::setPeerStatus(const QString& status) {
    m_peerStatus->setText(status);
}

void ChatWidget::setPeerName(const QString& name) {
    m_peerName->setText(name);
    // Обновляем букву аватара если нет загруженного изображения
    if (m_peerAvatar->pixmap().isNull())
        m_peerAvatar->setText(name.left(1).toUpper());
}

void ChatWidget::loadHistory(const QList<Message>& messages) {
    QLayoutItem* item;
    while ((item = m_msgLayout->takeAt(0))) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_msgLayout->addStretch();

    for (const auto& msg : messages) {
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
        if (msg.isVoice) {
            // filePath хранится в text при сохранении голосовых
            appendVoiceMessage(msg.outgoing, msg.voiceDurationMs,
                               ts, QString::fromStdString(msg.text));
        } else if (!msg.fileName.empty()) {
            appendMessage(QString("⊕ %1").arg(QString::fromStdString(msg.fileName)),
                          msg.outgoing, ts);
        } else {
            appendMessage(QString::fromStdString(msg.text), msg.outgoing, ts);
        }
    }
}

void ChatWidget::prependHistory(const QList<Message>& msgs) {
    m_loadingMore = false;
    if (msgs.isEmpty()) return;

    auto* bar = m_scrollArea->verticalScrollBar();
    const int oldMax = bar->maximum();

    // Вставляем старые сообщения сразу после stretch (idx=0) → перед существующими
    int idx = 1;
    for (const auto& msg : msgs) {
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(msg.timestamp);
        QWidget* w;
        if (msg.isVoice) {
            w = makeVoiceBubble(msg.outgoing, msg.voiceDurationMs, ts, QString::fromStdString(msg.text));
        } else if (!msg.fileName.empty()) {
            w = makeBubble(QString("⊕ %1").arg(QString::fromStdString(msg.fileName)), msg.outgoing, ts);
        } else {
            w = makeBubble(QString::fromStdString(msg.text), msg.outgoing, ts);
        }
        m_msgLayout->insertWidget(idx++, w);
    }

    // Компенсируем сдвиг полосы прокрутки — чтобы видимая часть не прыгала
    QTimer::singleShot(0, this, [bar, oldMax]() {
        bar->setValue(bar->value() + (bar->maximum() - oldMax));
    });
}

void ChatWidget::appendMessage(const QString& text, bool outgoing,
                                const QDateTime& ts, const QString& msgId) {
    m_msgLayout->addWidget(makeBubble(text, outgoing, ts, msgId));

    QTimer::singleShot(30, m_scrollArea, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

QWidget* ChatWidget::makeBubble(const QString& text, bool outgoing,
                                 const QDateTime& ts, const QString& msgId) {
    const auto& p = ThemeManager::instance().palette();

    auto* row = new QWidget();
    // Сохраняем текст для поиска
    row->setProperty("msgText", text);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 3, 0, 3);
    rowLayout->setSpacing(8);

    const QString timeStr = ts.toString("hh:mm");

    if (outgoing) {
        auto* bubble = new QLabel();
        bubble->setWordWrap(true);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bubble->setMaximumWidth(460);
        bubble->setMinimumWidth(60);
        bubble->setText(
            text + QString("  <span style='color:%1; font-size:10px;'>%2</span>")
                   .arg(p.textMuted, timeStr));
        bubble->setTextFormat(Qt::RichText);
        bubble->setStyleSheet(QString(R"(
            QLabel {
                background: %1;
                border-radius: 18px;
                border-bottom-right-radius: 6px;
                padding: 9px 14px;
                color: %2;
                font-size: 14px;
                line-height: 1.4;
            }
        )").arg(p.bgBubbleOut, p.textPrimary));

        bubble->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(bubble, &QWidget::customContextMenuRequested, bubble,
                [bubble, text](const QPoint& pos) {
            auto* m = new QMenu(bubble);
            m->setAttribute(Qt::WA_DeleteOnClose);
            auto* copyAct = m->addAction(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/ctx_copy.png")),
                QObject::tr("Копировать"));
            QObject::connect(copyAct, &QAction::triggered, [text]() {
                QApplication::clipboard()->setText(text);
            });
            m->popup(bubble->mapToGlobal(pos));
        });

        // Враппер: пузырь + иконка доставки
        auto* col = new QWidget();
        col->setMaximumWidth(460);
        auto* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(0, 0, 0, 0);
        colLayout->setSpacing(1);
        bubble->setMaximumWidth(460);
        colLayout->addWidget(bubble);

        auto* delivRow = new QHBoxLayout();
        delivRow->setContentsMargins(0, 0, 4, 0);
        auto* delivIcon = new QLabel();
        delivIcon->setPixmap(
            ThemeManager::tintedIcon(QStringLiteral(":/icons/msg_sent.png")).pixmap(12, 12));
        delivIcon->setVisible(!msgId.isEmpty());
        delivRow->addStretch();
        delivRow->addWidget(delivIcon);
        colLayout->addLayout(delivRow);

        if (!msgId.isEmpty())
            m_deliveryIcons[msgId] = delivIcon;

        rowLayout->addStretch();
        rowLayout->addWidget(col);

    } else {
        // ── Входящее: аватар слева + имя над пузырём ─────────────────────
        const int avSz = 30;
        auto* avLabel = new QLabel();
        avLabel->setFixedSize(avSz, avSz);
        avLabel->setAlignment(Qt::AlignCenter);
        avLabel->setStyleSheet(
            QString("font-size:12px; font-weight:700; background:%1; border-radius:%2px;")
                .arg(p.bgElevated).arg(avSz / 2));
        avLabel->setProperty("isBubbleAvatar", true);
        const QPixmap avPix = makeBubbleAvatar(avSz);
        if (!avPix.isNull())
            avLabel->setPixmap(avPix);
        else
            avLabel->setText(m_peerName->text().left(1).toUpper());

        // Колонка: имя + пузырь
        auto* col = new QWidget();
        col->setMaximumWidth(460);
        auto* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(0, 0, 0, 0);
        colLayout->setSpacing(2);

        auto* nameLbl = new QLabel(m_peerName->text());
        nameLbl->setStyleSheet(
            QString("font-size:11px; font-weight:600; color:%1; padding:0;").arg(p.accent));

        auto* bubble = new QLabel();
        bubble->setWordWrap(true);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bubble->setMaximumWidth(460);
        bubble->setMinimumWidth(60);
        bubble->setText(
            text + QString("  <span style='color:%1; font-size:10px;'>%2</span>")
                   .arg(p.textMuted, timeStr));
        bubble->setTextFormat(Qt::RichText);
        bubble->setStyleSheet(QString(R"(
            QLabel {
                background: %1;
                border: 1px solid %2;
                border-radius: 18px;
                border-bottom-left-radius: 6px;
                padding: 9px 14px;
                color: %3;
                font-size: 14px;
                line-height: 1.4;
            }
        )").arg(p.bgBubbleIn, p.border, p.textPrimary));

        bubble->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(bubble, &QWidget::customContextMenuRequested, bubble,
                [bubble, text](const QPoint& pos) {
            auto* m = new QMenu(bubble);
            m->setAttribute(Qt::WA_DeleteOnClose);
            auto* copyAct = m->addAction(
                ThemeManager::tintedIcon(QStringLiteral(":/icons/ctx_copy.png")),
                QObject::tr("Копировать"));
            QObject::connect(copyAct, &QAction::triggered, [text]() {
                QApplication::clipboard()->setText(text);
            });
            m->popup(bubble->mapToGlobal(pos));
        });

        colLayout->addWidget(nameLbl);
        colLayout->addWidget(bubble);

        rowLayout->addWidget(avLabel, 0, Qt::AlignBottom);
        rowLayout->addWidget(col);
        rowLayout->addStretch();
    }

    return row;
}

void ChatWidget::onSendClicked() {
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();
    m_input->setFixedHeight(44);

    // Сбрасываем typing-индикатор при отправке
    m_typingOutTimer->stop();
    if (m_typingActive) {
        m_typingActive = false;
        emit typingStopped();
    }

    emit sendMessage(text);
}

void ChatWidget::setPeerUuid(const QUuid& uuid) {
    m_peerUuid = uuid;
}

void ChatWidget::setAvatar(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        m_peerAvatarPix = {};
        const QPixmap fallback(QStringLiteral(":/icons/not-avatar.png"));
        if (!fallback.isNull()) { setAvatar(fallback); return; }
        m_peerAvatar->setPixmap({});
        const QString name = m_peerName->text();
        m_peerAvatar->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
        return;
    }

    m_peerAvatarPix = pixmap;

    const int sz = m_peerAvatar->width();
    const QPixmap scaled = pixmap.scaled(sz, sz,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Рисуем с эллиптической маской
    QPixmap rounded(sz, sz);
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, sz, sz);
    p.setClipPath(path);
    p.drawPixmap(0, 0, scaled);

    m_peerAvatar->setText({});
    m_peerAvatar->setPixmap(rounded);
}

bool ChatWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_peerAvatar
        && event->type() == QEvent::MouseButtonRelease
        && !m_peerUuid.isNull())
    {
        emit openProfileRequested(m_peerUuid);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// ── Запись голосовых ──────────────────────────────────────────────────────

void ChatWidget::onMicClicked() {
    if (!m_recorder->isRecording()) {
        // Начать запись
        m_recorder->startRecording();
        m_recSeconds = 0;
        m_recIndicator->setText("🔴 0:00");
        m_recIndicator->show();
        m_recSecTimer->start();
        m_micBtn->setIcon(ThemeManager::tintedIcon(QStringLiteral(":/icons/input_voice_active.png")));
        m_micBtn->setIconSize(QSize(20, 20));
        m_micBtn->setText({});
        m_micBtn->setToolTip(tr("Остановить запись"));
    } else {
        // Остановить запись → onRecordingDone вызовется через сигнал
        m_recorder->stopRecording();
    }
}

void ChatWidget::onRecordingDone(const QString& filePath, int durationMs) {
    // Сбрасываем UI записи
    m_recSecTimer->stop();
    m_recSeconds = 0;
    m_recIndicator->hide();
    m_recIndicator->setText("🔴 0:00");
    m_micBtn->setIcon(ThemeManager::tintedIcon(QStringLiteral(":/icons/input_voice.png")));
    m_micBtn->setIconSize(QSize(20, 20));
    m_micBtn->setText({});
    m_micBtn->setToolTip(tr("Записать голосовое сообщение"));
    m_micBtn->setStyleSheet({});

    if (filePath.isEmpty() || durationMs < 100) return;  // слишком короткое

    emit sendVoiceRequested(filePath, durationMs);
}

void ChatWidget::onLevelChanged(float level) {
    if (!m_recorder->isRecording()) {
        m_micBtn->setStyleSheet({});
        return;
    }
    // Пульсирующая подсветка кнопки при записи
    const int alpha = static_cast<int>(60 + level * 160);
    m_micBtn->setStyleSheet(
        QString("QPushButton#iconBtn { background: rgba(255,77,109,%1); }").arg(alpha));
}

// ── Голосовые пузыри ──────────────────────────────────────────────────────

void ChatWidget::appendVoiceMessage(bool outgoing, int durationMs,
                                     const QDateTime& ts,
                                     const QString& filePath) {
    m_msgLayout->addWidget(makeVoiceBubble(outgoing, durationMs, ts, filePath));

    QTimer::singleShot(30, m_scrollArea, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum());
    });
}

QWidget* ChatWidget::makeVoiceBubble(bool outgoing, int durationMs,
                                      const QDateTime& ts,
                                      const QString& filePath) {
    const auto& p = ThemeManager::instance().palette();

    auto* row = new QWidget();
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 2, 0, 2);
    rowLayout->setSpacing(0);

    // Пузырь
    auto* bubble = new QFrame();
    bubble->setObjectName("voiceBubble");
    bubble->setMaximumWidth(340);
    bubble->setMinimumWidth(220);
    auto* bLayout = new QVBoxLayout(bubble);
    bLayout->setContentsMargins(10, 8, 12, 8);
    bLayout->setSpacing(4);

    // ── Верхний ряд: кнопка + волна ──────────────────────────────────────
    auto* topRow = new QWidget();
    auto* topLay = new QHBoxLayout(topRow);
    topLay->setContentsMargins(0, 0, 0, 0);
    topLay->setSpacing(8);

    auto* playBtn = new QPushButton();
    playBtn->setFixedSize(36, 36);
    playBtn->setObjectName("voicePlayBtn");
    ThemeManager::applyIcon(playBtn, QStringLiteral(":/icons/media_play.png"), QSize(18, 18));

    // Волновая форма
    auto* waveform = new VoiceWaveform();
    const QColor colorPlayed   = outgoing ? p.textPrimary : p.accent;
    const QColor colorUnplayed = QColor(outgoing ? p.textMuted : p.textMuted);
    waveform->setColors(colorPlayed, colorUnplayed);
    waveform->loadFile(filePath);
    waveform->setCursor(Qt::PointingHandCursor);

    topLay->addWidget(playBtn);
    topLay->addWidget(waveform, 1);

    // ── Нижний ряд: длительность + размер + время ────────────────────────
    auto* botRow = new QWidget();
    auto* botLay = new QHBoxLayout(botRow);
    botLay->setContentsMargins(44, 0, 0, 0);  // выравнивание по правому краю кнопки
    botLay->setSpacing(0);

    const int totalSec = durationMs / 1000;
    const QString durStr = QString("%1:%2")
        .arg(totalSec / 60).arg(totalSec % 60, 2, 10, QChar('0'));

    QString sizeStr;
    if (!filePath.isEmpty()) {
        const qint64 bytes = QFileInfo(filePath).size();
        sizeStr = bytes < 1024 * 1024
            ? QString(", %1 KB").arg(bytes / 1024.0, 0, 'f', 1)
            : QString(", %1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
    }

    auto* durLabel  = new QLabel(durStr + sizeStr);
    durLabel->setObjectName("voiceDurLabel");
    auto* timeLabel = new QLabel(ts.toString("hh:mm"));
    timeLabel->setObjectName("voiceTimeLabel");

    botLay->addWidget(durLabel);
    botLay->addStretch();
    botLay->addWidget(timeLabel);

    bLayout->addWidget(topRow);
    bLayout->addWidget(botRow);

    // ── Стиль пузыря ─────────────────────────────────────────────────────
    const QString playBtnStyle =
        "QPushButton#voicePlayBtn { background: rgba(255,255,255,0.18); "
        "border-radius: 18px; border: none; }"
        "QPushButton#voicePlayBtn:hover { background: rgba(255,255,255,0.30); }";

    if (outgoing) {
        bubble->setStyleSheet(QString(R"(
            QFrame#voiceBubble {
                background: %1;
                border-radius: 18px;
                border-bottom-right-radius: 6px;
            }
            QWidget { background: transparent; }
            QLabel#voiceDurLabel  { color: %2; font-size: 11px; }
            QLabel#voiceTimeLabel { color: %3; font-size: 11px; }
        )").arg(p.bgBubbleOut, p.textPrimary, p.textMuted) + playBtnStyle);
        rowLayout->addStretch();
        rowLayout->addWidget(bubble);
    } else {
        bubble->setStyleSheet(QString(R"(
            QFrame#voiceBubble {
                background: %1;
                border: 1px solid %2;
                border-radius: 18px;
                border-bottom-left-radius: 6px;
            }
            QWidget { background: transparent; }
            QLabel#voiceDurLabel  { color: %3; font-size: 11px; }
            QLabel#voiceTimeLabel { color: %4; font-size: 11px; }
        )").arg(p.bgBubbleIn, p.border, p.textPrimary, p.textMuted) + playBtnStyle);

        const int avSz = 30;
        auto* avLabel = new QLabel();
        avLabel->setFixedSize(avSz, avSz);
        avLabel->setAlignment(Qt::AlignCenter);
        avLabel->setStyleSheet(
            QString("font-size:12px; font-weight:700; background:%1; border-radius:%2px;")
                .arg(p.bgElevated).arg(avSz / 2));
        const QPixmap avPix = makeBubbleAvatar(avSz);
        if (!avPix.isNull())
            avLabel->setPixmap(avPix);
        else
            avLabel->setText(m_peerName->text().left(1).toUpper());

        auto* col = new QWidget();
        auto* colLayout = new QVBoxLayout(col);
        colLayout->setContentsMargins(0, 0, 0, 0);
        colLayout->setSpacing(2);

        auto* nameLbl = new QLabel(m_peerName->text());
        nameLbl->setStyleSheet(
            QString("font-size:11px; font-weight:600; color:%1; padding:0;").arg(p.accent));
        colLayout->addWidget(nameLbl);
        colLayout->addWidget(bubble);

        rowLayout->setSpacing(8);
        rowLayout->addWidget(avLabel, 0, Qt::AlignBottom);
        rowLayout->addWidget(col);
        rowLayout->addStretch();
    }

    // ── Воспроизведение ───────────────────────────────────────────────────
    const bool hasFile = !filePath.isEmpty();
    if (hasFile) {
#ifdef HAVE_QT_MULTIMEDIA
        // Перемотка по клику на волну
        connect(waveform, &VoiceWaveform::seekRequested, this,
                [this, waveform](float pos) {
            if (m_activeWaveform == waveform) {
                m_player->setPosition(static_cast<qint64>(pos * m_player->duration()));
            }
        });

        connect(playBtn, &QPushButton::clicked, this,
                [this, playBtn, waveform, filePath]() {
            if (m_activePlayBtn == playBtn) {
                m_player->stop();
                ThemeManager::applyIcon(playBtn, ":/icons/media_play.png", QSize(18, 18));
                m_activePlayBtn  = nullptr;
                m_activeWaveform = nullptr;
            } else {
                if (m_activePlayBtn) {
                    ThemeManager::applyIcon(m_activePlayBtn, ":/icons/media_play.png", QSize(18, 18));
                    m_player->stop();
                }
                if (m_activeWaveform) m_activeWaveform->setProgress(0.0f);
                m_activePlayBtn  = playBtn;
                m_activeWaveform = waveform;
                ThemeManager::applyIcon(playBtn, ":/icons/media_pause.png", QSize(18, 18));
                m_player->setSource(QUrl::fromLocalFile(filePath));
                m_player->play();
            }
        });
#else
        playBtn->setEnabled(false);
        playBtn->setToolTip(tr("Воспроизведение недоступно (Qt6Multimedia не скомпилирован)"));
#endif
    } else {
        playBtn->setEnabled(false);
        playBtn->setToolTip(tr("Файл недоступен"));
    }

    return row;
}

// ── Typing indicator (входящий) ───────────────────────────────────────────

void ChatWidget::showTypingIndicator(const QString& peerName) {
    if (m_typingLabel) {
        m_typingLabel->setText(peerName + tr(" печатает..."));
        m_typingLabel->show();
    }
}

void ChatWidget::hideTypingIndicator() {
    if (m_typingLabel)
        m_typingLabel->hide();
}

// ── Статус доставки ───────────────────────────────────────────────────────

void ChatWidget::markDelivered(const QString& msgId) {
    if (msgId.isEmpty()) return;
    auto it = m_deliveryIcons.find(msgId);
    if (it != m_deliveryIcons.end() && it.value()) {
        it.value()->setPixmap(
            ThemeManager::tintedIcon(QStringLiteral(":/icons/msg_delivered.png"))
                .pixmap(12, 12));
        it.value()->show();
    }
}

// ── Enter sends setting ───────────────────────────────────────────────────

void ChatWidget::setEnterSends(bool on) {
    static_cast<MsgInput*>(m_input)->setEnterSends(on);
}

// ── Avatar helper ─────────────────────────────────────────────────────────

QPixmap ChatWidget::makeBubbleAvatar(int sz) const {
    if (m_peerAvatarPix.isNull())
        return {};

    const QPixmap scaled = m_peerAvatarPix.scaled(sz, sz,
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap rounded(sz, sz);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, sz, sz);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);
    return rounded;
}

// ── Search ────────────────────────────────────────────────────────────────

void ChatWidget::toggleSearchBar() {
    if (m_searchBtn->isChecked()) {
        m_searchBar->show();
        m_searchInput->setFocus();
        m_searchInput->selectAll();
    } else {
        m_searchBar->hide();
        clearSearchHighlights();
        m_searchHits.clear();
        m_searchIdx = -1;
        m_searchCount->clear();
    }
}

void ChatWidget::doSearch(const QString& query) {
    clearSearchHighlights();
    m_searchHits.clear();
    m_searchIdx = -1;

    if (query.trimmed().isEmpty()) {
        m_searchCount->clear();
        return;
    }

    const int n = m_msgLayout->count();
    for (int i = 0; i < n; ++i) {
        auto* item = m_msgLayout->itemAt(i);
        auto* w = item ? item->widget() : nullptr;
        if (!w) continue;
        if (w->property("msgText").toString().contains(query, Qt::CaseInsensitive)) {
            m_searchHits.append(w);
            QPalette pal = w->palette();
            pal.setColor(QPalette::Window, QColor(255, 200, 50, 55));
            w->setAutoFillBackground(true);
            w->setPalette(pal);
        }
    }

    if (m_searchHits.isEmpty()) {
        m_searchCount->setText(tr("0/0"));
        return;
    }

    m_searchIdx = 0;
    m_searchCount->setText(QString("1/%1").arg(m_searchHits.size()));
    m_scrollArea->ensureWidgetVisible(m_searchHits[0]);
}

void ChatWidget::navigateSearch(int delta) {
    if (m_searchHits.isEmpty()) return;
    m_searchIdx = (m_searchIdx + delta + m_searchHits.size()) % m_searchHits.size();
    m_searchCount->setText(
        QString("%1/%2").arg(m_searchIdx + 1).arg(m_searchHits.size()));
    m_scrollArea->ensureWidgetVisible(m_searchHits[m_searchIdx]);
}

void ChatWidget::clearSearchHighlights() {
    for (auto* w : std::as_const(m_searchHits)) {
        if (!w) continue;
        w->setAutoFillBackground(false);
        w->setPalette(QApplication::palette());
    }
}
