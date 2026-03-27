/****************************************************************************
** Meta object code from reading C++ file 'contactswidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/contactswidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'contactswidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14ContactsWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto ContactsWidget::qt_create_metaobjectdata<qt_meta_tag_ZN14ContactsWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ContactsWidget",
        "contactSelected",
        "",
        "QUuid",
        "uuid",
        "profileRequested",
        "blockRequested",
        "deleteChatRequested",
        "contactDeleteRequested",
        "onItemClicked",
        "QListWidgetItem*",
        "item",
        "onSearchChanged",
        "text",
        "onContextMenuRequested",
        "QPoint",
        "pos"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'contactSelected'
        QtMocHelpers::SignalData<void(QUuid)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'profileRequested'
        QtMocHelpers::SignalData<void(QUuid)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'blockRequested'
        QtMocHelpers::SignalData<void(QUuid)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'deleteChatRequested'
        QtMocHelpers::SignalData<void(QUuid)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'contactDeleteRequested'
        QtMocHelpers::SignalData<void(QUuid)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'onItemClicked'
        QtMocHelpers::SlotData<void(QListWidgetItem *)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Slot 'onSearchChanged'
        QtMocHelpers::SlotData<void(const QString &)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Slot 'onContextMenuRequested'
        QtMocHelpers::SlotData<void(const QPoint &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 15, 16 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ContactsWidget, qt_meta_tag_ZN14ContactsWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ContactsWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ContactsWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ContactsWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14ContactsWidgetE_t>.metaTypes,
    nullptr
} };

void ContactsWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ContactsWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->contactSelected((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 1: _t->profileRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 2: _t->blockRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 3: _t->deleteChatRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 4: _t->contactDeleteRequested((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1]))); break;
        case 5: _t->onItemClicked((*reinterpret_cast<std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 6: _t->onSearchChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onContextMenuRequested((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ContactsWidget::*)(QUuid )>(_a, &ContactsWidget::contactSelected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ContactsWidget::*)(QUuid )>(_a, &ContactsWidget::profileRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ContactsWidget::*)(QUuid )>(_a, &ContactsWidget::blockRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ContactsWidget::*)(QUuid )>(_a, &ContactsWidget::deleteChatRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ContactsWidget::*)(QUuid )>(_a, &ContactsWidget::contactDeleteRequested, 4))
            return;
    }
}

const QMetaObject *ContactsWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ContactsWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14ContactsWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ContactsWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void ContactsWidget::contactSelected(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ContactsWidget::profileRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ContactsWidget::blockRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ContactsWidget::deleteChatRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void ContactsWidget::contactDeleteRequested(QUuid _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
