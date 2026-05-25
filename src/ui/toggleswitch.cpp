#include "toggleswitch.h"
#include "thememanager.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QMouseEvent>

static constexpr int kW   = 46;
static constexpr int kH   = 26;
static constexpr int kR   = 13;
static constexpr int kD   = 20;
static constexpr int kPad = 3;

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(kW, kH);

    m_t    = 0.0;
    m_anim = new QPropertyAnimation(this, "t", this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);

    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        animateTo(on ? 1.0 : 0.0);
    });
}

QSize ToggleSwitch::sizeHint() const { return {kW, kH}; }

void ToggleSwitch::setT(qreal v) { m_t = v; update(); }

void ToggleSwitch::animateTo(qreal target) {
    m_anim->stop();
    m_anim->setStartValue(m_t);
    m_anim->setEndValue(target);
    m_anim->start();
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        toggle();
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor trackOff(38, 38, 42);
    const QColor trackOn = QColor(ThemeManager::instance().palette().accent);

    // Интерполируем цвет трека
    QColor track;
    track.setRgbF(
        trackOff.redF()   + m_t * (trackOn.redF()   - trackOff.redF()),
        trackOff.greenF() + m_t * (trackOn.greenF() - trackOff.greenF()),
        trackOff.blueF()  + m_t * (trackOn.blueF()  - trackOff.blueF())
    );

    p.setBrush(track);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(0, 0, kW, kH), kR, kR);

    // Ползунок
    const qreal tx = kPad + m_t * (kW - kD - 2 * kPad);
    const qreal ty = (kH - kD) / 2.0;
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(tx, ty, kD, kD));
}
