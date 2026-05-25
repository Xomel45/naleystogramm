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
    m_recorder = new AudioRecorder(this);
    connect(m_recorder, &AudioRecorder::recorded,
            this, &ChatWidget::onRecordingDone);
    connect(m_recorder, &AudioRecorder::levelChanged,
            this, &ChatWidget::onLevelChanged);

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
        m_peerAvatar->setFixedSize(42, 42);
        m_peerAvatar->setAlignment(Qt::AlignCenter);
        // Клик по аватару → сигнал openProfileRequested
        m_peerAvatar->setCursor(Qt::PointingHandCursor);
        m_peerAvatar->installEventFilter(this);

        auto* infoCol = new QWidget();
        auto* infoLayout = new QVBoxLayout(infoCol);
        infoLayout->setContentsMargins(0,0,0,0);
        infoLayout->setSpacing(2);

        m_peerName   = new QLabel("—");
        m_peerStatus = new QLabel(tr("offline"));
        m_peerName->setObjectName("chatPeerName");
        m_peerStatus->setObjectName("chatPeerStatus");

        infoLayout->addWidget(m_peerName);
        infoLayout->addWidget(m_peerStatus);

        m_fileBtn = new QPushButton();
        m_fileBtn->setObjectName("iconBtn");
        m_fileBtn->setFixedSize(36, 36);
        m_fileBtn->setToolTip(tr("Send file"));
        ThemeManager::applyIcon(m_fileBtn, QStringLiteral(":/icons/input_attach.png"), QSize(20, 20));
        connect(m_fileBtn, &QPushButton::clicked,
                this, &ChatWidget::sendFileRequested);

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
            if (!m_peerUuid.isNull())
                emit callRequested(m_peerUuid);
        });

        hl->addWidget(m_peerAvatar);
        hl->addWidget(infoCol, 1);
        hl->addWidget(m_callBtn);
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
    m_msgLayout->setContentsMargins(20, 16, 20, 16);
    m_msgLayout->setSpacing(4);
    m_msgLayout->addStretch();
    m_scrollArea->setWidget(m_msgContainer);

    // Прокрутка к верху → запросить ещё историю (lazy loading)
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
        il->setContentsMargins(14, 10, 14, 10);
        il->setSpacing(8);

        auto* inp = new MsgInput(m_inputBar);
        inp->setObjectName("msgInput");
        inp->setPlaceholderText(tr("Message..."));
        m_input = inp;

        // Кнопка записи голосового сообщения
        m_micBtn = new QPushButton();
        m_micBtn->setObjectName("iconBtn");
        m_micBtn->setFixedSize(36, 36);
        m_micBtn->setToolTip(tr("Записать голосовое сообщение"));
        ThemeManager::applyIcon(m_micBtn, QStringLiteral(":/icons/input_voice.png"), QSize(20, 20));
        connect(m_micBtn, &QPushButton::clicked, this, &ChatWidget::onMicClicked);

        // Индикатор записи — скрыт по умолчанию
        m_recIndicator = new QLabel("🔴 0:00");
        m_recIndicator->setStyleSheet(
            "font-size: 11px; color: #ff4d6d; font-weight: 600;");
        m_recIndicator->hide();

        m_sendBtn = new QPushButton();
        m_sendBtn->setObjectName("sendBtn");
        m_sendBtn->setFixedSize(44, 44);
        ThemeManager::applyIconOnAccent(m_sendBtn, QStringLiteral(":/icons/input_send.png"), QSize(20, 20));

        connect(inp,      &MsgInput::enterPressed,    this, &ChatWidget::onSendClicked);
        connect(m_sendBtn, &QPushButton::clicked,     this, &ChatWidget::onSendClicked);

        il->addWidget(m_input, 1);
        il->addWidget(m_recIndicator);
        il->addWidget(m_micBtn);
        il->addWidget(m_sendBtn);
    }

    // ── Индикатор "пишет..." ──────────────────────────────────────────────
    m_typingLabel = new QLabel();
    m_typingLabel->setObjectName("typingIndicator");
    m_typingLabel->setContentsMargins(20, 2, 20, 2);
    m_typingLabel->setStyleSheet("font-size: 12px; font-style: italic; color: #8888aa;");
    m_typingLabel->hide();

    layout->addWidget(m_header);
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
    m_scrollArea->hide();
    m_inputBar->hide();
    m_placeholder->show();
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
    // Сбрасываем до заглушки — реальное изображение загрузит MainWindow если есть
    setAvatar({});
    setPeerStatus(isOnline ? tr("online") : tr("offline"));
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
        if (msg.isVoice) {
            // filePath хранится в text при сохранении голосовых
            appendVoiceMessage(msg.outgoing, msg.voiceDurationMs,
                               msg.timestamp, msg.text);
        } else if (!msg.fileName.isEmpty()) {
            appendMessage(QString("⊕ %1").arg(msg.fileName),
                          msg.outgoing, msg.timestamp);
        } else {
            appendMessage(msg.text, msg.outgoing, msg.timestamp);
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
        QWidget* w;
        if (msg.isVoice) {
            w = makeVoiceBubble(msg.outgoing, msg.voiceDurationMs, msg.timestamp, msg.text);
        } else if (!msg.fileName.isEmpty()) {
            w = makeBubble(QString("⊕ %1").arg(msg.fileName), msg.outgoing, msg.timestamp);
        } else {
            w = makeBubble(msg.text, msg.outgoing, msg.timestamp);
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

    // Правый клик — копировать текст сообщения
    bubble->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bubble, &QWidget::customContextMenuRequested, bubble,
            [bubble, text](const QPoint& pos) {
        auto* m = new QMenu(bubble);
        m->setAttribute(Qt::WA_DeleteOnClose);
        auto* copyAct = m->addAction(
            ThemeManager::tintedIcon(QStringLiteral(":/icons/ctx_copy.png")), QObject::tr("Копировать"));
        QObject::connect(copyAct, &QAction::triggered, [text]() {
            QApplication::clipboard()->setText(text);
        });
        m->popup(bubble->mapToGlobal(pos));
    });

    if (outgoing) {
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

        // Враппер: пузырь + иконка доставки под ним справа
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
            ThemeManager::tintedIcon(QStringLiteral(":/icons/msg_sent.png"))
                .pixmap(12, 12));
        // Показываем иконку «отправлено» только для новых сообщений (не из истории)
        delivIcon->setVisible(!msgId.isEmpty());
        delivRow->addStretch();
        delivRow->addWidget(delivIcon);
        colLayout->addLayout(delivRow);

        if (!msgId.isEmpty())
            m_deliveryIcons[msgId] = delivIcon;

        rowLayout->addStretch();
        rowLayout->addWidget(col);
    } else {
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
        const QPixmap fallback(QStringLiteral(":/icons/not-avatar.png"));
        if (!fallback.isNull()) { setAvatar(fallback); return; }
        // Ресурс не найден — буква имени как последний вариант
        m_peerAvatar->setPixmap({});
        const QString name = m_peerName->text();
        m_peerAvatar->setText(name.isEmpty() ? "?" : name.left(1).toUpper());
        return;
    }

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
        rowLayout->addWidget(bubble);
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
