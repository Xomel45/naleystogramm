#pragma once
#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QAbstractScrollArea>
#include <QCoreApplication>

// Блокирует прокрутку колёсиком на виджете (QComboBox, QSpinBox и т.п.),
// но пробрасывает событие ближайшему QAbstractScrollArea — страница продолжает скроллиться.
inline void noScrollWheel(QWidget* w) {
    struct Filter : QObject {
        using QObject::QObject;
        bool eventFilter(QObject* obj, QEvent* e) override {
            if (e->type() != QEvent::Wheel)
                return false;
            // Ищем ближайший scroll area вверх по иерархии
            auto* widget = qobject_cast<QWidget*>(obj);
            for (auto* p = widget ? widget->parentWidget() : nullptr; p; p = p->parentWidget()) {
                if (auto* sa = qobject_cast<QAbstractScrollArea*>(p)) {
                    QCoreApplication::sendEvent(sa->viewport(), e);
                    return true;
                }
            }
            return true; // scroll area не найден — просто блокируем
        }
    };
    w->installEventFilter(new Filter(w));
}
