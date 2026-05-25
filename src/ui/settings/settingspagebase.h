#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QFrame>

// Базовый класс для всех страниц настроек.
// Сам является QScrollArea; дочерний контент в m_lay.
class SettingsPageBase : public QScrollArea {
    Q_OBJECT
public:
    virtual void reload() {}
    virtual bool save()   { return true; }

protected:
    explicit SettingsPageBase(QWidget* parent = nullptr) : QScrollArea(parent) {
        setFrameShape(QFrame::NoFrame);
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setObjectName("settingsScroll");

        auto* content = new QWidget();
        content->setObjectName("settingsContent");
        m_lay = new QVBoxLayout(content);
        m_lay->setContentsMargins(16, 16, 16, 24);
        m_lay->setSpacing(4);
        setWidget(content);
    }

    QVBoxLayout* m_lay {nullptr};
};
