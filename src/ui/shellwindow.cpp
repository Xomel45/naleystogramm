#include "shellwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QTextCursor>
#include <QCloseEvent>

// ── Конструктор ───────────────────────────────────────────────────────────────

ShellWindow::ShellWindow(const QString& sessionId,
                          const QString& peerName,
                          QWidget* parent)
    : QDialog(parent,
              Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint)
    , m_sessionId(sessionId)
{
    setWindowTitle(QString(">_ Shell: %1").arg(peerName));
    // WA_DeleteOnClose=false: временем жизни управляет MainWindow
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(700, 450);
    resize(820, 540);
    setupUi(peerName);
}

// ── Построение UI ─────────────────────────────────────────────────────────────

void ShellWindow::setupUi(const QString& /*peerName*/) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Строка статуса сессии
    m_statusLbl = new QLabel(tr("Сессия активна"));
    m_statusLbl->setObjectName("settingsHint");
    root->addWidget(m_statusLbl);

    // Терминальный вывод: моноширинный шрифт, только чтение
    m_output = new QPlainTextEdit();
    m_output->setReadOnly(true);
    m_output->setObjectName("shellOutput");
    QFont mono("Monospace", 10);
    mono.setStyleHint(QFont::Monospace);
    m_output->setFont(mono);
    m_output->setWordWrapMode(QTextOption::WrapAnywhere);
    // Ограничиваем буфер — не даём вырасти до бесконечности
    m_output->setMaximumBlockCount(5000);
    root->addWidget(m_output, 1);

    // Строка ввода команд
    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(6);

    m_input = new QLineEdit();
    m_input->setPlaceholderText(tr("Введите команду..."));
    m_input->setFont(mono);

    m_sendBtn = new QPushButton(tr("Enter"));
    m_sendBtn->setFixedWidth(56);
    m_sendBtn->setToolTip(tr("Отправить команду"));

    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendBtn);
    root->addLayout(inputRow);

    // Красная кнопка завершения
    m_terminateBtn = new QPushButton(tr("Завершить сессию"));
    m_terminateBtn->setStyleSheet(
        "QPushButton { background: #c0392b; color: white; "
        "border-radius: 6px; font-weight: bold; padding: 6px; }");
    m_terminateBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(m_terminateBtn);

    connect(m_input,        &QLineEdit::returnPressed,
            this,           &ShellWindow::onInputEnter);
    connect(m_sendBtn,      &QPushButton::clicked,
            this,           &ShellWindow::onInputEnter);
    connect(m_terminateBtn, &QPushButton::clicked,
            this, [this]() { emit terminateRequested(m_sessionId); });
}

// ── Ввод команды ─────────────────────────────────────────────────────────────

void ShellWindow::onInputEnter() {
    if (m_ended) return;
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    // Эхо команды в терминал (до ответа шелла)
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText("> " + text + "\n");
    m_output->moveCursor(QTextCursor::End);

    m_input->clear();
    // Отправляем с завершающим переводом строки — bash выполнит команду
    emit inputSubmitted(m_sessionId, (text + "\n").toUtf8());
}

// ── Добавить вывод удалённого шелла ───────────────────────────────────────────

void ShellWindow::appendOutput(const QByteArray& data) {
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(QString::fromUtf8(data));
    m_output->moveCursor(QTextCursor::End);
}

// ── Показать завершение сессии ────────────────────────────────────────────────

void ShellWindow::showSessionEnded(const QString& reason) {
    m_ended = true;

    const QString msg =
        (reason == "privilege_escalation")
            ? tr("\n[СЕССИЯ УБИТА: попытка эскалации привилегий]")
        : (reason == "process_exited")
            ? tr("\n[Шелл-процесс завершился]")
        : (reason == "spawn_failed")
            ? tr("\n[Не удалось запустить шелл на удалённой машине]")
            : tr("\n[Сессия завершена: %1]").arg(reason);

    m_output->appendPlainText(msg);
    m_statusLbl->setText(tr("Сессия завершена"));
    m_input->setEnabled(false);
    m_sendBtn->setEnabled(false);
    m_terminateBtn->setEnabled(false);
}

// ── Закрытие окна (X) — завершить сессию если ещё активна ────────────────────

void ShellWindow::closeEvent(QCloseEvent* event) {
    if (!m_ended)
        emit terminateRequested(m_sessionId);
    event->accept();
}
