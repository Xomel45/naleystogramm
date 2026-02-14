/****************************************************************************
** Meta object code from reading C++ file 'filetransfer.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/filetransfer.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'filetransfer.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12FileTransferE_t {};
} // unnamed namespace

template <> constexpr inline auto FileTransfer::qt_create_metaobjectdata<qt_meta_tag_ZN12FileTransferE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FileTransfer",
        "fileReceived",
        "",
        "QUuid",
        "from",
        "path",
        "name",
        "fileOffer",
        "size",
        "offerId",
        "transferProgress",
        "percent",
        "acceptOffer",
        "rejectOffer"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'fileReceived'
        QtMocHelpers::SignalData<void(QUuid, QString, QString)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'fileOffer'
        QtMocHelpers::SignalData<void(QUuid, QString, qint64, QString)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 6 }, { QMetaType::LongLong, 8 }, { QMetaType::QString, 9 },
        }}),
        // Signal 'transferProgress'
        QtMocHelpers::SignalData<void(QString, int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'acceptOffer'
        QtMocHelpers::SlotData<void(const QUuid &, const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 9 },
        }}),
        // Slot 'rejectOffer'
        QtMocHelpers::SlotData<void(const QUuid &, const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FileTransfer, qt_meta_tag_ZN12FileTransferE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FileTransfer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12FileTransferE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12FileTransferE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12FileTransferE_t>.metaTypes,
    nullptr
} };

void FileTransfer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FileTransfer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->fileReceived((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 1: _t->fileOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 2: _t->transferProgress((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->acceptOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->rejectOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QUuid , QString , QString )>(_a, &FileTransfer::fileReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QUuid , QString , qint64 , QString )>(_a, &FileTransfer::fileOffer, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QString , int )>(_a, &FileTransfer::transferProgress, 2))
            return;
    }
}

const QMetaObject *FileTransfer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FileTransfer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12FileTransferE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int FileTransfer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void FileTransfer::fileReceived(QUuid _t1, QString _t2, QString _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void FileTransfer::fileOffer(QUuid _t1, QString _t2, qint64 _t3, QString _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 2
void FileTransfer::transferProgress(QString _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}
QT_WARNING_POP
