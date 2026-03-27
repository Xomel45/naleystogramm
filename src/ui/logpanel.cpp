#include "logpanel.h"
#include "thememanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QTextStream>
#include <QScrollBar>

// ── LogPanel ────────────────────────────────────────────────────────────────

LogPanel::LogPanel(QWidget* parent) : QWidget(parent) {
    setupUi();

    // Подключаемся к Logger
    connect(&Logger::instance(), &Logger::logEntry,
            this, &LogPanel::appendEntry);

    connect(&Logger::instance(), &Logger::logCleared,
            this, &LogPanel::clear);

    // Загружаем последние записи из буфера
    const auto entries = Logger::instance().recentEntries(100);
    for (const auto& entry : entries) {
        appendEntry(entry);
    }
}

void LogPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Заголовок
    auto* header = new QHBoxLayout();
    header->setSpacing(8);

    // Чекбокс подробного режима
    m_verboseCheck = new QCheckBox(tr("Verbose mode"));
    m_verboseCheck->setObjectName("logVerboseCheck");
    m_verboseCheck->setChecked(Logger::instance().isVerbose());
    connect(m_verboseCheck, &QCheckBox::toggled,
            this, &LogPanel::onVerboseToggled);

    // Кнопки
    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setObjectName("dlgCancelBtn");
    m_clearBtn->setFixedHeight(28);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &LogPanel::onClearClicked);

    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setObjectName("dlgCancelBtn");
    m_exportBtn->setFixedHeight(28);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &LogPanel::onExportClicked);

    header->addWidget(m_verboseCheck);
    header->addStretch();
    header->addWidget(m_clearBtn);
    header->addWidget(m_exportBtn);

    // Область логов
    m_logView = new QPlainTextEdit();
    m_logView->setObjectName("logView");
    m_logView->setReadOnly(true);
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logView->setMaximumBlockCount(kMaxLines);

    // Моноширинный шрифт для логов
    QFont monoFont("Monospace", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_logView->setFont(monoFont);

    // Стиль для тёмной темы
    const auto& p = ThemeManager::instance().palette();
    m_logView->setStyleSheet(QString(R"(
        QPlainTextEdit {
            background: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 8px;
        }
    )").arg(p.bgInput, p.textPrimary, p.border));

    layout->addLayout(header);
    layout->addWidget(m_logView, 1);
}

void LogPanel::appendEntry(const LogEntry& entry) {
    // Формируем строку с цветом уровня
    const QString color = levelColor(entry.level);
    const QString levelStr = [&]() {
        switch (entry.level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
        }
        return "?";
    }();

    const QString compStr = [&]() {
        switch (entry.component) {
            case LogComponent::Network:      return "NET";
            case LogComponent::FileTransfer: return "FILE";
            case LogComponent::Crypto:       return "CRYPT";
            case LogComponent::Storage:      return "DB";
            case LogComponent::UI:           return "UI";
            case LogComponent::General:      return "GEN";
        }
        return "?";
    }();

    const QString timeStr = entry.timestamp.toString("hh:mm:ss.zzz");

    // Формируем текст записи
    const QString line = QString("[%1] [%2] [%3] %4")
        .arg(timeStr, levelStr, compStr, entry.message);

    // Добавляем в виджет
    m_logView->appendPlainText(line);

    // Прокручиваем вниз
    QScrollBar* sb = m_logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void LogPanel::clear() {
    m_logView->clear();
}

void LogPanel::setVerbose(bool enabled) {
    m_verboseCheck->setChecked(enabled);
}

void LogPanel::onClearClicked() {
    Logger::instance().clearLog();
}

void LogPanel::onExportClicked() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save log file"),
        "naleystogramm_log.txt",
        tr("Log files (*.log *.txt)")
    );

    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream << m_logView->toPlainText();
    file.close();
}

void LogPanel::onVerboseToggled(bool checked) {
    Logger::instance().setVerbose(checked);
}

QString LogPanel::levelColor(LogLevel level) const {
    const auto& p = ThemeManager::instance().palette();
    switch (level) {
        case LogLevel::Debug:   return p.textMuted;
        case LogLevel::Info:    return p.textPrimary;
        case LogLevel::Warning: return "#f0ad4e";  // Жёлтый
        case LogLevel::Error:   return p.danger;
    }
    return p.textPrimary;
}

QString LogPanel::formatEntryHtml(const LogEntry& entry) const {
    Q_UNUSED(entry);
    // Для будущего использования с HTML форматированием
    return QString();
}
