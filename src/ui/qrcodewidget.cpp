#include "qrcodewidget.h"
#include <QPainter>
#include <QImage>

#ifdef HAVE_QRENCODE
#include <qrencode.h>
#endif

QrCodeWidget::QrCodeWidget(int sizePx, QWidget* parent)
    : QWidget(parent), m_sizePx(sizePx)
{
    setFixedSize(sizePx, sizePx);
}

void QrCodeWidget::setContent(const QString& text) {
    m_hasContent = false;
    m_pixmap     = QPixmap();

#ifdef HAVE_QRENCODE
    QRcode* qr = QRcode_encodeString(
        text.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (qr) {
        const int modules = qr->width;
        // Масштаб: вписываем QR в (sizePx - 2*margin) с целым числом пикселей на модуль
        const int margin = 8;
        const int scale  = qMax(1, (m_sizePx - 2 * margin) / modules);
        const int qrPx   = modules * scale;
        const int offset = (m_sizePx - qrPx) / 2;

        QImage img(m_sizePx, m_sizePx, QImage::Format_RGB32);
        img.fill(Qt::white);

        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (qr->data[y * modules + x] & 1)
                    p.drawRect(offset + x * scale, offset + y * scale, scale, scale);
            }
        }

        QRcode_free(qr);
        m_pixmap     = QPixmap::fromImage(img);
        m_hasContent = true;
    }
#else
    Q_UNUSED(text)
#endif

    update();
}

void QrCodeWidget::clear() {
    m_pixmap     = QPixmap();
    m_hasContent = false;
    update();
}

void QrCodeWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    if (m_hasContent) {
        p.drawPixmap(0, 0, m_pixmap);
    } else {
#ifndef HAVE_QRENCODE
        p.setPen(palette().mid().color());
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("Установите\nlibqrencode\nдля QR-кода"));
#endif
    }
}
