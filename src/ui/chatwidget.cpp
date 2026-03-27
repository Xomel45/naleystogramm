#include "chatwidget.h"
#include "thememanager.h"
#include "../core/audiorecorder.h"
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

    // Когда воспроизведение завершается — сбрасываем кнопку
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState && m_activePlayBtn) {
            m_activePlayBtn->setText("▶");
            m_activePlayBtn = nullptr;
        }
    });
#endif

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
        m_peerAvatar->setFixedSize(38, 38);
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

        m_fileBtn = new QPushButton("⊕");
        m_fileBtn->setObjectName("iconBtn");
        m_fileBtn->setFixedSize(36, 36);
        m_fileBtn->setToolTip(tr("Send file"));
        connect(m_fileBtn, &QPushButton::clicked,
                this, &ChatWidget::sendFileRequested);

        m_callBtn = new QPushButton("📞");
        m_callBtn->setObjectName("iconBtn");
        m_callBtn->setFixedSize(36, 36);
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
        m_micBtn = new QPushButton("🎤");
        m_micBtn->setObjectName("iconBtn");
        m_micBtn->setFixedSize(36, 36);
        m_micBtn->setToolTip(tr("Записать голосовое сообщение"));
        connect(m_micBtn, &QPushButton::clicked, this, &ChatWidget::onMicClicked);

        // Индикатор записи — скрыт по умолчанию
        m_recIndicator = new QLabel("🔴 0:00");
        m_recIndicator->setStyleSheet(
            "font-size: 11px; color: #ff4d6d; font-weight: 600;");
        m_recIndicator->hide();

        m_sendBtn = new QPushButton("↑");
        m_sendBtn->setObjectName("sendBtn");
        m_sendBtn->setFixedSize(44, 44);

        connect(inp,      &MsgInput::enterPressed,    this, &ChatWidget::onSendClicked);
        connect(m_sendBtn, &QPushButton::clicked,     this, &ChatWidget::onSendClicked);

        il->addWidget(m_input, 1);
        il->addWidget(m_recIndicator);
        il->addWidget(m_micBtn);
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

    // Сбрасываем состояние lazy loading при открытии нового чата
    m_historyOffset = 0;
    m_loadingMore   = false;

    m_peerName->setText(peerName);
    // Сбрасываем аватар до буквы — реальное изображение загрузит MainWindow если есть
    m_peerAvatar->setPixmap({});
    m_peerAvatar->setText(peerName.left(1).toUpper());
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

void ChatWidget::setPeerUuid(const QUuid& uuid) {
    m_peerUuid = uuid;
}

void ChatWidget::setAvatar(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        // Показываем букву имени
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
        m_micBtn->setText("⏹");
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
    m_micBtn->setText("🎤");
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

    // Контейнер пузыря
    auto* bubble = new QFrame();
    bubble->setMaximumWidth(280);
    bubble->setMinimumWidth(160);
    auto* bLayout = new QHBoxLayout(bubble);
    bLayout->setContentsMargins(10, 8, 12, 8);
    bLayout->setSpacing(8);

    // Кнопка воспроизведения
    auto* playBtn = new QPushButton("▶");
    playBtn->setFixedSize(32, 32);
    playBtn->setObjectName("voicePlayBtn");

    // Форматируем длительность
    const int totalSec = durationMs / 1000;
    const int mm = totalSec / 60;
    const int ss = totalSec % 60;
    const QString durStr = QString("%1:%2").arg(mm).arg(ss, 2, 10, QChar('0'));
    const QString timeStr = ts.toString("hh:mm");

    auto* infoLabel = new QLabel(
        QString("🎤 %1  <span style='color:%2; font-size:10px;'>%3</span>")
            .arg(durStr, p.textMuted, timeStr));
    infoLabel->setTextFormat(Qt::RichText);

    bLayout->addWidget(playBtn);
    bLayout->addWidget(infoLabel, 1);

    // Стиль пузыря
    const QString playBtnBase = outgoing
        ? "QPushButton { background: rgba(255,255,255,0.18); border-radius:16px; "
          "color: %1; font-size:12px; } QPushButton:hover { background: rgba(255,255,255,0.28); }"
        : "QPushButton { background: rgba(255,255,255,0.10); border-radius:16px; "
          "color: %1; font-size:12px; } QPushButton:hover { background: rgba(255,255,255,0.18); }";

    playBtn->setStyleSheet(QString(playBtnBase).arg(p.textPrimary));

    if (outgoing) {
        bubble->setStyleSheet(QString(R"(
            QFrame {
                background: %1;
                border-radius: 16px;
                border-bottom-right-radius: 4px;
            }
            QLabel { color: %2; font-size: 13px; background: transparent; }
        )").arg(p.bgBubbleOut, p.textPrimary));
        rowLayout->addStretch();
        rowLayout->addWidget(bubble);
    } else {
        bubble->setStyleSheet(QString(R"(
            QFrame {
                background: %1;
                border: 1px solid %2;
                border-radius: 16px;
                border-bottom-left-radius: 4px;
            }
            QLabel { color: %3; font-size: 13px; background: transparent; }
        )").arg(p.bgBubbleIn, p.border, p.textPrimary));
        rowLayout->addWidget(bubble);
        rowLayout->addStretch();
    }

    // Воспроизведение через общий m_player (только при наличии Qt6Multimedia)
    if (!filePath.isEmpty()) {
#ifdef HAVE_QT_MULTIMEDIA
        connect(playBtn, &QPushButton::clicked, this,
                [this, playBtn, filePath]() {
            if (m_activePlayBtn == playBtn) {
                // Повторное нажатие — стоп
                m_player->stop();
                playBtn->setText("▶");
                m_activePlayBtn = nullptr;
            } else {
                // Остановить предыдущий
                if (m_activePlayBtn) {
                    m_activePlayBtn->setText("▶");
                    m_player->stop();
                }
                m_activePlayBtn = playBtn;
                playBtn->setText("⏸");
                m_player->setSource(QUrl::fromLocalFile(filePath));
                m_player->play();
            }
        });
#else
        // Qt6Multimedia недоступен — воспроизведение отключено
        playBtn->setEnabled(false);
        playBtn->setToolTip(tr("Воспроизведение недоступно (Qt6Multimedia не скомпилирован)"));
#endif
    } else {
        playBtn->setEnabled(false);
        playBtn->setToolTip(tr("Файл недоступен"));
    }

    return row;
}
