#include "shellmonitor.h"
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QCloseEvent>

// ── Конструктор ───────────────────────────────────────────────────────────────

ShellMonitor::ShellMonitor(const QString& sessionId,
                             const QString& peerName,
                             QWidget* parent)
    : QDialog(parent,
              Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint)
    , m_sessionId(sessionId)
{
    setWindowTitle(QString(">_ Монитор шелла: %1").arg(peerName));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(700, 450);
    resize(820, 540);
    setupUi(peerName);
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void ShellMonitor::setupUi(const QString& peerName) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Строка статуса
    m_statusLbl = new QLabel(
        tr("Активная шелл-сессия от %1  —  только просмотр").arg(peerName));
    m_statusLbl->setObjectName("settingsHint");
    root->addWidget(m_statusLbl);

    // Терминальный вывод: только чтение
    m_output = new QPlainTextEdit();
    m_output->setReadOnly(true);
    m_output->setObjectName("shellOutput");
    QFont mono("Monospace", 10);
    mono.setStyleHint(QFont::Monospace);
    m_output->setFont(mono);
    m_output->setWordWrapMode(QTextOption::WrapAnywhere);
    m_output->setMaximumBlockCount(5000);
    root->addWidget(m_output, 1);

    // Большая красная кнопка аварийного завершения
    m_terminateBtn = new QPushButton(tr("ЗАВЕРШИТЬ СЕССИЮ"));
    m_terminateBtn->setMinimumHeight(44);
    m_terminateBtn->setCursor(Qt::PointingHandCursor);
    m_terminateBtn->setStyleSheet(
        "QPushButton { background: #c0392b; color: white; "
        "border-radius: 6px; font-weight: bold; font-size: 13px; padding: 6px; }");
    root->addWidget(m_terminateBtn);

    connect(m_terminateBtn, &QPushButton::clicked,
            this, [this]() { emit terminateRequested(m_sessionId); });
}

// ── Добавить вывод шелла ─────────────────────────────────────────────────────

void ShellMonitor::appendData(const QByteArray& data) {
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(QString::fromUtf8(data));
    m_output->moveCursor(QTextCursor::End);
}

// ── Добавить команду инициатора (префикс "> ") ────────────────────────────────

void ShellMonitor::appendInput(const QByteArray& cmd) {
    const QString text = QString::fromUtf8(cmd).trimmed();
    if (text.isEmpty()) return;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText("> " + text + "\n");
    m_output->moveCursor(QTextCursor::End);
}

// ── Показать завершение сессии ────────────────────────────────────────────────

void ShellMonitor::showSessionEnded(const QString& reason) {
    m_ended = true;

    const QString msg =
        (reason == "privilege_escalation")
            ? tr("\n[СЕССИЯ УБИТА: попытка эскалации привилегий!]")
        : (reason == "process_exited")
            ? tr("\n[Шелл-процесс завершился]")
            : tr("\n[Сессия завершена: %1]").arg(reason);

    m_output->appendPlainText(msg);
    m_statusLbl->setText(tr("Сессия завершена"));
    m_terminateBtn->setEnabled(false);
}

// ── Закрытие окна (X) — завершить сессию если ещё активна ────────────────────

void ShellMonitor::closeEvent(QCloseEvent* event) {
    if (!m_ended)
        emit terminateRequested(m_sessionId);
    event->accept();
}
