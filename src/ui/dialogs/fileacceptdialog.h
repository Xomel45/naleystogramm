#pragma once
#include <QDialog>
#include <QUuid>

class QLabel;
class QPushButton;

// ── FileAcceptDialog ─────────────────────────────────────────────────────────
// Диалог подтверждения входящего файла.
// Показывает информацию об отправителе, имя файла, размер.
// Пользователь может принять или отклонить передачу.
//
class FileAcceptDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileAcceptDialog(const QString& senderName,
                              const QString& fileName,
                              qint64 fileSize,
                              const QString& offerId,
                              QWidget* parent = nullptr);

    // Получить ID предложения
    [[nodiscard]] QString offerId() const { return m_offerId; }

signals:
    // Пользователь принял файл
    void accepted(QString offerId);

    // Пользователь отклонил файл
    void rejected(QString offerId);

private:
    void setupUi(const QString& senderName,
                 const QString& fileName,
                 qint64 fileSize);

    // Форматирование размера файла (человекочитаемый формат)
    QString formatFileSize(qint64 bytes) const;

    QString m_offerId;

    QLabel*      m_titleLabel    {nullptr};
    QLabel*      m_senderLabel   {nullptr};
    QLabel*      m_fileNameLabel {nullptr};
    QLabel*      m_fileSizeLabel {nullptr};
    QPushButton* m_acceptBtn     {nullptr};
    QPushButton* m_rejectBtn     {nullptr};
};
