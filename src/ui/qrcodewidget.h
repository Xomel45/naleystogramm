#pragma once
#include <QWidget>
#include <QPixmap>

// Виджет, рисующий QR-код из строки.
// Требует libqrencode (HAVE_QRENCODE). Без него показывает заглушку.
class QrCodeWidget : public QWidget {
    Q_OBJECT
public:
    explicit QrCodeWidget(int sizePx = 200, QWidget* parent = nullptr);

    void setContent(const QString& text);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_pixmap;
    int     m_sizePx;
    bool    m_hasContent {false};
};
