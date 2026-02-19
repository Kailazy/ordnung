/****************************************************************************
** Meta object code from reading C++ file 'DownloadsView.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/views/DownloadsView.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DownloadsView.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_FolderNode_t {
    uint offsetsAndSizes[6];
    char stringdata0[11];
    char stringdata1[8];
    char stringdata2[1];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_FolderNode_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_FolderNode_t qt_meta_stringdata_FolderNode = {
    {
        QT_MOC_LITERAL(0, 10),  // "FolderNode"
        QT_MOC_LITERAL(11, 7),  // "clicked"
        QT_MOC_LITERAL(19, 0)   // ""
    },
    "FolderNode",
    "clicked",
    ""
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_FolderNode[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   20,    2, 0x06,    1 /* Public */,

 // signals: parameters
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject FolderNode::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_FolderNode.offsetsAndSizes,
    qt_meta_data_FolderNode,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_FolderNode_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<FolderNode, std::true_type>,
        // method 'clicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void FolderNode::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<FolderNode *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->clicked(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (FolderNode::*)();
            if (_t _q_method = &FolderNode::clicked; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    (void)_a;
}

const QMetaObject *FolderNode::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FolderNode::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_FolderNode.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FolderNode::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void FolderNode::clicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
namespace {
struct qt_meta_stringdata_DownloadsView_t {
    uint offsetsAndSizes[48];
    char stringdata0[14];
    char stringdata1[20];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[4];
    char stringdata5[14];
    char stringdata6[7];
    char stringdata7[20];
    char stringdata8[13];
    char stringdata9[23];
    char stringdata10[11];
    char stringdata11[11];
    char stringdata12[24];
    char stringdata13[3];
    char stringdata14[17];
    char stringdata15[17];
    char stringdata16[14];
    char stringdata17[14];
    char stringdata18[20];
    char stringdata19[19];
    char stringdata20[12];
    char stringdata21[6];
    char stringdata22[19];
    char stringdata23[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_DownloadsView_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_DownloadsView_t qt_meta_stringdata_DownloadsView = {
    {
        QT_MOC_LITERAL(0, 13),  // "DownloadsView"
        QT_MOC_LITERAL(14, 19),  // "saveConfigRequested"
        QT_MOC_LITERAL(34, 0),  // ""
        QT_MOC_LITERAL(35, 11),  // "WatchConfig"
        QT_MOC_LITERAL(47, 3),  // "cfg"
        QT_MOC_LITERAL(51, 13),  // "scanRequested"
        QT_MOC_LITERAL(65, 6),  // "folder"
        QT_MOC_LITERAL(72, 19),  // "convertAllRequested"
        QT_MOC_LITERAL(92, 12),  // "outputFolder"
        QT_MOC_LITERAL(105, 22),  // "convertSingleRequested"
        QT_MOC_LITERAL(128, 10),  // "downloadId"
        QT_MOC_LITERAL(139, 10),  // "sourcePath"
        QT_MOC_LITERAL(150, 23),  // "deleteDownloadRequested"
        QT_MOC_LITERAL(174, 2),  // "id"
        QT_MOC_LITERAL(177, 16),  // "onSrcNodeClicked"
        QT_MOC_LITERAL(194, 16),  // "onOutNodeClicked"
        QT_MOC_LITERAL(211, 13),  // "onSaveClicked"
        QT_MOC_LITERAL(225, 13),  // "onScanClicked"
        QT_MOC_LITERAL(239, 19),  // "onConvertAllClicked"
        QT_MOC_LITERAL(259, 18),  // "onConvertRequested"
        QT_MOC_LITERAL(278, 11),  // "QModelIndex"
        QT_MOC_LITERAL(290, 5),  // "index"
        QT_MOC_LITERAL(296, 18),  // "onTableContextMenu"
        QT_MOC_LITERAL(315, 3)   // "pos"
    },
    "DownloadsView",
    "saveConfigRequested",
    "",
    "WatchConfig",
    "cfg",
    "scanRequested",
    "folder",
    "convertAllRequested",
    "outputFolder",
    "convertSingleRequested",
    "downloadId",
    "sourcePath",
    "deleteDownloadRequested",
    "id",
    "onSrcNodeClicked",
    "onOutNodeClicked",
    "onSaveClicked",
    "onScanClicked",
    "onConvertAllClicked",
    "onConvertRequested",
    "QModelIndex",
    "index",
    "onTableContextMenu",
    "pos"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_DownloadsView[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   86,    2, 0x06,    1 /* Public */,
       5,    1,   89,    2, 0x06,    3 /* Public */,
       7,    1,   92,    2, 0x06,    5 /* Public */,
       9,    3,   95,    2, 0x06,    7 /* Public */,
      12,    1,  102,    2, 0x06,   11 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      14,    0,  105,    2, 0x08,   13 /* Private */,
      15,    0,  106,    2, 0x08,   14 /* Private */,
      16,    0,  107,    2, 0x08,   15 /* Private */,
      17,    0,  108,    2, 0x08,   16 /* Private */,
      18,    0,  109,    2, 0x08,   17 /* Private */,
      19,    1,  110,    2, 0x08,   18 /* Private */,
      22,    1,  113,    2, 0x08,   20 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void, QMetaType::LongLong, QMetaType::QString, QMetaType::QString,   10,   11,    8,
    QMetaType::Void, QMetaType::LongLong,   13,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 20,   21,
    QMetaType::Void, QMetaType::QPoint,   23,

       0        // eod
};

Q_CONSTINIT const QMetaObject DownloadsView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_DownloadsView.offsetsAndSizes,
    qt_meta_data_DownloadsView,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_DownloadsView_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<DownloadsView, std::true_type>,
        // method 'saveConfigRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const WatchConfig &, std::false_type>,
        // method 'scanRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'convertAllRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'convertSingleRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<long long, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'deleteDownloadRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<long long, std::false_type>,
        // method 'onSrcNodeClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOutNodeClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSaveClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onScanClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConvertAllClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConvertRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QModelIndex &, std::false_type>,
        // method 'onTableContextMenu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>
    >,
    nullptr
} };

void DownloadsView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DownloadsView *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->saveConfigRequested((*reinterpret_cast< std::add_pointer_t<WatchConfig>>(_a[1]))); break;
        case 1: _t->scanRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->convertAllRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->convertSingleRequested((*reinterpret_cast< std::add_pointer_t<qlonglong>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 4: _t->deleteDownloadRequested((*reinterpret_cast< std::add_pointer_t<qlonglong>>(_a[1]))); break;
        case 5: _t->onSrcNodeClicked(); break;
        case 6: _t->onOutNodeClicked(); break;
        case 7: _t->onSaveClicked(); break;
        case 8: _t->onScanClicked(); break;
        case 9: _t->onConvertAllClicked(); break;
        case 10: _t->onConvertRequested((*reinterpret_cast< std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 11: _t->onTableContextMenu((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DownloadsView::*)(const WatchConfig & );
            if (_t _q_method = &DownloadsView::saveConfigRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (DownloadsView::*)(const QString & );
            if (_t _q_method = &DownloadsView::scanRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (DownloadsView::*)(const QString & );
            if (_t _q_method = &DownloadsView::convertAllRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (DownloadsView::*)(long long , const QString & , const QString & );
            if (_t _q_method = &DownloadsView::convertSingleRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (DownloadsView::*)(long long );
            if (_t _q_method = &DownloadsView::deleteDownloadRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
}

const QMetaObject *DownloadsView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DownloadsView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DownloadsView.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int DownloadsView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void DownloadsView::saveConfigRequested(const WatchConfig & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void DownloadsView::scanRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void DownloadsView::convertAllRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void DownloadsView::convertSingleRequested(long long _t1, const QString & _t2, const QString & _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void DownloadsView::deleteDownloadRequested(long long _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
