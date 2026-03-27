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
        "upnpMappingResult",
        "ok",
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
        "contactNameUpdated",
        "peerInfoUpdated",
        "connectionStateChanged",
        "ConnectionState",
        "state",
        "connectionLog",
        "message",
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
        // Signal 'upnpMappingResult'
        QtMocHelpers::SignalData<void(bool)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 9 },
        }}),
        // Signal 'incomingRequest'
        QtMocHelpers::SignalData<void(QUuid, QString, QString)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 12 }, { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'messageReceived'
        QtMocHelpers::SignalData<void(QUuid, QJsonObject)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 16 }, { 0x80000000 | 17, 18 },
        }}),
        // Signal 'peerConnected'
        QtMocHelpers::SignalData<void(QUuid, QString)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 20 }, { QMetaType::QString, 21 },
        }}),
        // Signal 'peerDisconnected'
        QtMocHelpers::SignalData<void(QUuid)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 20 },
        }}),
        // Signal 'contactNameUpdated'
        QtMocHelpers::SignalData<void(QUuid, QString)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 20 }, { QMetaType::QString, 21 },
        }}),
        // Signal 'peerInfoUpdated'
        QtMocHelpers::SignalData<void(QUuid)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 20 },
        }}),
        // Signal 'connectionStateChanged'
        QtMocHelpers::SignalData<void(QUuid, ConnectionState)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 11, 20 }, { 0x80000000 | 26, 27 },
        }}),
        // Signal 'connectionLog'
        QtMocHelpers::SignalData<void(const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 29 },
        }}),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'onNewConnection'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketReadyRead'
        QtMocHelpers::SlotData<void()>(32, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketDisconnected'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
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
        case 2: _t->upnpMappingResult((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->incomingRequest((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 4: _t->messageReceived((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 5: _t->peerConnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->peerDisconnected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 7: _t->contactNameUpdated((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->peerInfoUpdated((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 9: _t->connectionStateChanged((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<ConnectionState>>(_a[2]))); break;
        case 10: _t->connectionLog((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->error((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->onNewConnection(); break;
        case 13: _t->onSocketReadyRead(); break;
        case 14: _t->onSocketDisconnected(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & , quint16 , bool )>(_a, &NetworkManager::ready, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & )>(_a, &NetworkManager::externalIpDiscovered, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(bool )>(_a, &NetworkManager::upnpMappingResult, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QString , QString )>(_a, &NetworkManager::incomingRequest, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QJsonObject )>(_a, &NetworkManager::messageReceived, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QString )>(_a, &NetworkManager::peerConnected, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid )>(_a, &NetworkManager::peerDisconnected, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , QString )>(_a, &NetworkManager::contactNameUpdated, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid )>(_a, &NetworkManager::peerInfoUpdated, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(QUuid , ConnectionState )>(_a, &NetworkManager::connectionStateChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & )>(_a, &NetworkManager::connectionLog, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (NetworkManager::*)(const QString & )>(_a, &NetworkManager::error, 11))
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
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
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
void NetworkManager::upnpMappingResult(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void NetworkManager::incomingRequest(QUuid _t1, QString _t2, QString _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void NetworkManager::messageReceived(QUuid _t1, QJsonObject _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void NetworkManager::peerConnected(QUuid _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void NetworkManager::peerDisconnected(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void NetworkManager::contactNameUpdated(QUuid _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void NetworkManager::peerInfoUpdated(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void NetworkManager::connectionStateChanged(QUuid _t1, ConnectionState _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}

// SIGNAL 10
void NetworkManager::connectionLog(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void NetworkManager::error(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1);
}
QT_WARNING_POP
