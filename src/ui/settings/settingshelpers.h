#pragma once
#include <QLabel>
#include <QFrame>
#include <QString>

inline QLabel* spFieldLabel(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsFieldLabel");
    return lbl;
}

inline QLabel* spHint(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setObjectName("settingsHint");
    lbl->setWordWrap(true);
    return lbl;
}

inline QFrame* spSeparator() {
    auto* f = new QFrame();
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("settingsSeparator");
    return f;
}
