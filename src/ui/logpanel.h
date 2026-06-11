#pragma once
#include <QWidget>
#include "../core/diag/logger.h"

class QPlainTextEdit;
class QPushButton;
class QCheckBox;

// ── LogPanel ────────────────────────────────────────────────────────────────
// Панель отображения логов в настройках.
// Показывает последние записи из Logger с цветовой кодировкой по уровню.
//
class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);
    ~LogPanel();

    // Добавить запись в отображение
    void appendEntry(const LogEntry& entry);

    // Очистить отображение
    void clear();

    // Включить/выключить подробный режим
    void setVerbose(bool enabled);

signals:
    void verboseChanged(bool enabled);

private slots:
    void onClearClicked();
    void onExportClicked();
    void onVerboseToggled(bool checked);

private:
    void setupUi();
    QString formatEntryHtml(const LogEntry& entry) const;
    QString levelColor(LogLevel level) const;

    QPlainTextEdit* m_logView   {nullptr};
    QPushButton*    m_clearBtn  {nullptr};
    QPushButton*    m_exportBtn {nullptr};
    QCheckBox*      m_verboseCheck {nullptr};

    Logger::Token m_logToken  {0};
    Logger::Token m_clearToken{0};

    static constexpr int kMaxLines = 1000;
};
