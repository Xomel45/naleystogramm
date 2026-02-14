/****************************************************************************
** Meta object code from reading C++ file 'network.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/network.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'network.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14NetworkManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto NetworkManager::qt_create_metaobjectdata<qt_meta_tag_ZN14NetworkManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "NetworkManager",
        "ready",
        "",
        "externalIp",
        "port",
        "upnpOk",
        "externalIpDiscovered",
        "ip",
        "incomingRequest",
        "QUuid",
        "peerUuid",
        "peerName",
        "peerIp",
        "messageReceived",
        "fromUuid",
        "QJsonObject",
        "msg",
        "peerConnected",
        "uuid",
        "name",
        "peerDisconnected",
        "error",
        "onNewConnection",
        "onSocketReadyRead",
        "onSocketDisconnected"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'ready'
        QtMocHelpers::SignalData<void(const QString &, quint16, bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::UShort, 4 }, { QMetaType::Bool, 5 },
        }}),
        // Signal 'externalIpDiscovered'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Signal 'incomingRequest'
        QtMocHelpers::SignalData<void(QUuid, QString, QString)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { QMetaType::QString, 11 }, { QMetaType::QString, 12 },
        }}),
        // Signal 'messageReceived'
        QtMocHelpers::SignalData<void(QUuid, QJsonObject)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 14 }, { 0x80000000 | 15, 16 },
        }}),
        // Signal 'peerConnected'
        QtMocHelpers::SignalData<void(QUuid, QString)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 18 }, { QMetaType::QString, 19 },
        }}),
        // Signal 'peerDisconnected'
        QtMocHelpers::SignalData<void(QUuid)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 18 },
        }}),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 16 },
        }}),
        // Slot 'onNewConnection'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketReadyRead'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketDisconnected'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<NetworkManager, qt_meta_tag_ZN14NetworkManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject NetworkManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14NetworkManagerE_t>.metaTypes,
    nullptr
} };

void NetworkManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NetworkManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->ready((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint16>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 1: _t->externalIpDiscovered((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->incomingRequest((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 3: _t->messageReceived((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 4: _t->peerConnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->peerDisconnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 6: _t->error((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onNewConnection(); break;
        case 8: _t->onSocketReadyRead(); break;
        case 9: _t->onSocketDisconnected(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & , quint16 , bool )>(_a, &NetworkManager::ready, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & )>(_a, &NetworkManager::externalIpDiscovered, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QString , QString )>(_a, &NetworkManager::incomingRequest, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QJsonObject )>(_a, &NetworkManager::messageReceived, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QString )>(_a, &NetworkManager::peerConnected, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid )>(_a, &NetworkManager::peerDisconnected, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & )>(_a, &NetworkManager::error, 6))
            return;
    }
}

const QMetaObject *NetworkManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetworkManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14NetworkManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetworkManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
void NetworkManager::ready(const QString & _t1, quint16 _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void NetworkManager::externalIpDiscovered(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void NetworkManager::incomingRequest(QUuid _t1, QString _t2, QString _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void NetworkManager::messageReceived(QUuid _t1, QJsonObject _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void NetworkManager::peerConnected(QUuid _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void NetworkManager::peerDisconnected(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void NetworkManager::error(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}
QT_WARNING_POP
