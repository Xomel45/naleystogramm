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
        "fileOffer",
        "",
        "QUuid",
        "from",
        "name",
        "size",
        "offerId",
        "durationMs",
        "transferStarted",
        "TransferProgress",
        "progress",
        "transferProgress",
        "transferCompleted",
        "id",
        "filePath",
        "outgoing",
        "transferFailed",
        "error",
        "transferCancelled",
        "fileReceived",
        "path",
        "acceptOffer",
        "rejectOffer",
        "cancelTransfer",
        "transferId",
        "pauseTransfer",
        "resumeTransfer"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'fileOffer'
        QtMocHelpers::SignalData<void(QUuid, QString, qint64, QString, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 5 }, { QMetaType::LongLong, 6 }, { QMetaType::QString, 7 },
            { QMetaType::Int, 8 },
        }}),
        // Signal 'transferStarted'
        QtMocHelpers::SignalData<void(TransferProgress)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'transferProgress'
        QtMocHelpers::SignalData<void(TransferProgress)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'transferCompleted'
        QtMocHelpers::SignalData<void(QString, QString, bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::QString, 15 }, { QMetaType::Bool, 16 },
        }}),
        // Signal 'transferFailed'
        QtMocHelpers::SignalData<void(QString, QString)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 }, { QMetaType::QString, 18 },
        }}),
        // Signal 'transferCancelled'
        QtMocHelpers::SignalData<void(QString)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'fileReceived'
        QtMocHelpers::SignalData<void(QUuid, QString, QString)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 21 }, { QMetaType::QString, 5 },
        }}),
        // Slot 'acceptOffer'
        QtMocHelpers::SlotData<void(const QUuid &, const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 7 },
        }}),
        // Slot 'rejectOffer'
        QtMocHelpers::SlotData<void(const QUuid &, const QString &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QString, 7 },
        }}),
        // Slot 'cancelTransfer'
        QtMocHelpers::SlotData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Slot 'pauseTransfer'
        QtMocHelpers::SlotData<void(const QString &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
        }}),
        // Slot 'resumeTransfer'
        QtMocHelpers::SlotData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 25 },
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
        case 0: _t->fileOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5]))); break;
        case 1: _t->transferStarted((*reinterpret_cast<std::add_pointer_t<TransferProgress>>(_a[1]))); break;
        case 2: _t->transferProgress((*reinterpret_cast<std::add_pointer_t<TransferProgress>>(_a[1]))); break;
        case 3: _t->transferCompleted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: _t->transferFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->transferCancelled((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->fileReceived((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 7: _t->acceptOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->rejectOffer((*reinterpret_cast<std::add_pointer_t<QUuid>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->cancelTransfer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->pauseTransfer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->resumeTransfer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QUuid , QString , qint64 , QString , int )>(_a, &FileTransfer::fileOffer, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(TransferProgress )>(_a, &FileTransfer::transferStarted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(TransferProgress )>(_a, &FileTransfer::transferProgress, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QString , QString , bool )>(_a, &FileTransfer::transferCompleted, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QString , QString )>(_a, &FileTransfer::transferFailed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QString )>(_a, &FileTransfer::transferCancelled, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileTransfer::*)(QUuid , QString , QString )>(_a, &FileTransfer::fileReceived, 6))
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
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void FileTransfer::fileOffer(QUuid _t1, QString _t2, qint64 _t3, QString _t4, int _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 1
void FileTransfer::transferStarted(TransferProgress _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void FileTransfer::transferProgress(TransferProgress _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void FileTransfer::transferCompleted(QString _t1, QString _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void FileTransfer::transferFailed(QString _t1, QString _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void FileTransfer::transferCancelled(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void FileTransfer::fileReceived(QUuid _t1, QString _t2, QString _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
