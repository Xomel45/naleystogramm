/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onAppReady",
        "",
        "ip",
        "port",
        "upnp",
        "onIncomingRequest",
        "QUuid",
        "uuid",
        "name",
        "onPeerConnected",
        "onPeerDisconnected",
        "onMessageReceived",
        "from",
        "QJsonObject",
        "msg",
        "onSessionEstablished",
        "peerUuid",
        "onAddContactClicked",
        "onContactSelected",
        "onSendMessage",
        "text",
        "onSendFile",
        "onShowMyId",
        "onEditName",
        "onCycleTheme",
        "openSettings",
        "closeSettings",
        "refreshOwnDisplay"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onAppReady'
        QtMocHelpers::SlotData<void(const QString &, quint16, bool)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::UShort, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Slot 'onIncomingRequest'
        QtMocHelpers::SlotData<void(QUuid, QString, QString)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 }, { QMetaType::QString, 9 }, { QMetaType::QString, 3 },
        }}),
        // Slot 'onPeerConnected'
        QtMocHelpers::SlotData<void(QUuid, QString)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 }, { QMetaType::QString, 9 },
        }}),
        // Slot 'onPeerDisconnected'
        QtMocHelpers::SlotData<void(QUuid)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Slot 'onMessageReceived'
        QtMocHelpers::SlotData<void(QUuid, QJsonObject)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 13 }, { 0x80000000 | 14, 15 },
        }}),
        // Slot 'onSessionEstablished'
        QtMocHelpers::SlotData<void(QUuid)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 17 },
        }}),
        // Slot 'onAddContactClicked'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onContactSelected'
        QtMocHelpers::SlotData<void(QUuid)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Slot 'onSendMessage'
        QtMocHelpers::SlotData<void(const QString &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Slot 'onSendFile'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onShowMyId'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEditName'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCycleTheme'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openSettings'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'closeSettings'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshOwnDisplay'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onAppReady((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 1: _t->onIncomingRequest((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 2: _t->onPeerConnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->onPeerDisconnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 4: _t->onMessageReceived((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 5: _t->onSessionEstablished((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 6: _t->onAddContactClicked(); break;
        case 7: _t->onContactSelected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 8: _t->onSendMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->onSendFile(); break;
        case 10: _t->onShowMyId(); break;
        case 11: _t->onEditName(); break;
        case 12: _t->onCycleTheme(); break;
        case 13: _t->openSettings(); break;
        case 14: _t->closeSettings(); break;
        case 15: _t->refreshOwnDisplay(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    return _id;
}
QT_WARNING_POP
