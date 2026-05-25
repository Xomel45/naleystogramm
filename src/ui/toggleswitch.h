#pragma once
#include <QAbstractButton>

class QPropertyAnimation;

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal t READ t WRITE setT)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override;

    qreal t() const { return m_t; }
    void  setT(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    qreal               m_t    {0.0};
    QPropertyAnimation* m_anim {nullptr};
    void animateTo(qreal target);
};
