/****************************************************************************
** Meta object code from reading C++ file 'chatwidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/chatwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'chatwidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10ChatWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto ChatWidget::qt_create_metaobjectdata<qt_meta_tag_ZN10ChatWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ChatWidget",
        "sendMessage",
        "",
        "text",
        "sendFileRequested",
        "openProfileRequested",
        "QUuid",
        "uuid",
        "sendVoiceRequested",
        "filePath",
        "durationMs",
        "loadMoreRequested",
        "callRequested",
        "peerUuid",
        "onSendClicked",
        "onMicClicked",
        "onRecordingDone",
        "onLevelChanged",
        "level"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'sendMessage'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'sendFileRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'openProfileRequested'
        QtMocHelpers::SignalData<void(QUuid)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'sendVoiceRequested'
        QtMocHelpers::SignalData<void(const QString &, int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 10 },
        }}),
        // Signal 'loadMoreRequested'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'callRequested'
        QtMocHelpers::SignalData<void(QUuid)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 13 },
        }}),
        // Slot 'onSendClicked'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMicClicked'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRecordingDone'
        QtMocHelpers::SlotData<void(const QString &, int)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 10 },
        }}),
        // Slot 'onLevelChanged'
        QtMocHelpers::SlotData<void(float)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Float, 18 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ChatWidget, qt_meta_tag_ZN10ChatWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ChatWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10ChatWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10ChatWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10ChatWidgetE_t>.metaTypes,
    nullptr
} };

void ChatWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ChatWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sendMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->sendFileRequested(); break;
        case 2: _t->openProfileRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 3: _t->sendVoiceRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->loadMoreRequested(); break;
        case 5: _t->callRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 6: _t->onSendClicked(); break;
        case 7: _t->onMicClicked(); break;
        case 8: _t->onRecordingDone((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->onLevelChanged((*reinterpret_cast<std::add_pointer_t<float>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)(const QString & )>(_a, &ChatWidget::sendMessage, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)()>(_a, &ChatWidget::sendFileRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)(QUuid )>(_a, &ChatWidget::openProfileRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)(const QString & , int )>(_a, &ChatWidget::sendVoiceRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)()>(_a, &ChatWidget::loadMoreRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ChatWidget::*)(QUuid )>(_a, &ChatWidget::callRequested, 5))
            return;
    }
}

const QMetaObject *ChatWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ChatWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10ChatWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ChatWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void ChatWidget::sendMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ChatWidget::sendFileRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ChatWidget::openProfileRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ChatWidget::sendVoiceRequested(const QString & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void ChatWidget::loadMoreRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void ChatWidget::callRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
