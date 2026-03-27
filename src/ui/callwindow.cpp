#include "callwindow.h"
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ── CallWindow ────────────────────────────────────────────────────────────────

CallWindow::CallWindow(QWidget* parent)
    : QDialog(parent, Qt::Window | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint)
{
    setWindowTitle("Голосовой звонок");
    setFixedSize(320, 220);
    setAttribute(Qt::WA_DeleteOnClose, false);  // владелец управляет временем жизни
    setupUi();
}

CallWindow::~CallWindow() {
    if (m_durationTimer) {
        m_durationTimer->stop();
        delete m_durationTimer;
    }
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void CallWindow::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // Имя собеседника
    m_peerLabel = new QLabel("—", this);
    m_peerLabel->setAlignment(Qt::AlignCenter);
    QFont bigFont = m_peerLabel->font();
    bigFont.setPointSize(14);
    bigFont.setBold(true);
    m_peerLabel->setFont(bigFont);
    root->addWidget(m_peerLabel);

    // Строка статуса / таймер
    m_statusLabel = new QLabel("Исходящий звонок...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_statusLabel);

    // Индикатор уровня звука
    m_levelBar = new QProgressBar(this);
    m_levelBar->setRange(0, 100);
    m_levelBar->setValue(0);
    m_levelBar->setTextVisible(false);
    m_levelBar->setMaximumHeight(8);
    root->addWidget(m_levelBar);

    root->addStretch();

    // Кнопки: принять / отклонить (только при Ringing)
    auto* callRow = new QHBoxLayout;
    m_acceptBtn = new QPushButton("✅ Принять", this);
    m_rejectBtn = new QPushButton("❌ Отклонить", this);
    m_acceptBtn->setFixedHeight(36);
    m_rejectBtn->setFixedHeight(36);
    callRow->addWidget(m_acceptBtn);
    callRow->addWidget(m_rejectBtn);
    root->addLayout(callRow);

    // Кнопки: заглушить / завершить
    auto* ctrlRow = new QHBoxLayout;
    m_muteBtn   = new QPushButton("🎙 Выкл. микр.", this);
    m_hangupBtn = new QPushButton("📵 Завершить", this);
    m_muteBtn->setFixedHeight(36);
    m_hangupBtn->setFixedHeight(36);
    m_hangupBtn->setStyleSheet("QPushButton { background: #c0392b; color: white; "
                               "border-radius: 6px; font-weight: bold; }");
    ctrlRow->addWidget(m_muteBtn);
    ctrlRow->addWidget(m_hangupBtn);
    root->addLayout(ctrlRow);

    // Связи
    connect(m_muteBtn, &QPushButton::clicked, this, [this]() {
        m_muted = !m_muted;
        m_muteBtn->setText(m_muted ? "🔇 Вкл. микр." : "🎙 Выкл. микр.");
        emit muteToggled(m_muted);
    });
    connect(m_hangupBtn, &QPushButton::clicked, this, &CallWindow::hangupClicked);
    connect(m_acceptBtn, &QPushButton::clicked, this, &CallWindow::acceptClicked);
    connect(m_rejectBtn, &QPushButton::clicked, this, &CallWindow::rejectClicked);

    // Таймер длительности разговора
    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(1000);
    connect(m_durationTimer, &QTimer::timeout, this, &CallWindow::onDurationTick);

    applyState();
}

// ── setPeerName ───────────────────────────────────────────────────────────────

void CallWindow::setPeerName(const QString& name) {
    if (m_peerLabel)
        m_peerLabel->setText(name.isEmpty() ? "—" : name);
}

// ── setState ─────────────────────────────────────────────────────────────────

void CallWindow::setState(State s) {
    m_state = s;
    applyState();
}

void CallWindow::applyState() {
    switch (m_state) {
    case State::Calling:
        m_statusLabel->setText("Исходящий звонок...");
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        m_muteBtn->setEnabled(false);
        m_durationTimer->stop();
        break;
    case State::Ringing:
        m_statusLabel->setText("Входящий звонок!");
        m_acceptBtn->show();
        m_rejectBtn->show();
        m_muteBtn->setEnabled(false);
        m_durationTimer->stop();
        break;
    case State::InCall:
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        m_muteBtn->setEnabled(true);
        m_callTimer.start();
        m_durationTimer->start();
        m_statusLabel->setText("Разговор: 0:00");
        break;
    }
}

// ── setAudioLevel ─────────────────────────────────────────────────────────────

void CallWindow::setAudioLevel(float level) {
    if (m_levelBar)
        m_levelBar->setValue(static_cast<int>(level * 100.0f));
}

// ── onDurationTick ────────────────────────────────────────────────────────────

void CallWindow::onDurationTick() {
    const qint64 elapsed = m_callTimer.elapsed() / 1000;
    const int minutes    = static_cast<int>(elapsed / 60);
    const int seconds    = static_cast<int>(elapsed % 60);
    m_statusLabel->setText(QString("Разговор: %1:%2")
                           .arg(minutes)
                           .arg(seconds, 2, 10, QChar('0')));
}
