/****************************************************************************
** Meta object code from reading C++ file 'LibraryView.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/views/LibraryView.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LibraryView.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_LibraryView_t {
    uint offsetsAndSizes[26];
    char stringdata0[12];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[6];
    char stringdata4[24];
    char stringdata5[3];
    char stringdata6[19];
    char stringdata7[12];
    char stringdata8[16];
    char stringdata9[5];
    char stringdata10[17];
    char stringdata11[16];
    char stringdata12[10];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_LibraryView_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_LibraryView_t qt_meta_stringdata_LibraryView = {
    {
        QT_MOC_LITERAL(0, 11),  // "LibraryView"
        QT_MOC_LITERAL(12, 15),  // "importRequested"
        QT_MOC_LITERAL(28, 0),  // ""
        QT_MOC_LITERAL(29, 5),  // "paths"
        QT_MOC_LITERAL(35, 23),  // "deletePlaylistRequested"
        QT_MOC_LITERAL(59, 2),  // "id"
        QT_MOC_LITERAL(62, 18),  // "onPlaylistSelected"
        QT_MOC_LITERAL(81, 11),  // "onBulkApply"
        QT_MOC_LITERAL(93, 15),  // "onTrackExpanded"
        QT_MOC_LITERAL(109, 4),  // "data"
        QT_MOC_LITERAL(114, 16),  // "onTrackCollapsed"
        QT_MOC_LITERAL(131, 15),  // "onUndoAvailable"
        QT_MOC_LITERAL(147, 9)   // "available"
    },
    "LibraryView",
    "importRequested",
    "",
    "paths",
    "deletePlaylistRequested",
    "id",
    "onPlaylistSelected",
    "onBulkApply",
    "onTrackExpanded",
    "data",
    "onTrackCollapsed",
    "onUndoAvailable",
    "available"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_LibraryView[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   56,    2, 0x06,    1 /* Public */,
       4,    1,   59,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       6,    1,   62,    2, 0x08,    5 /* Private */,
       7,    0,   65,    2, 0x08,    7 /* Private */,
       8,    1,   66,    2, 0x08,    8 /* Private */,
      10,    0,   69,    2, 0x08,   10 /* Private */,
      11,    1,   70,    2, 0x08,   11 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QStringList,    3,
    QMetaType::Void, QMetaType::LongLong,    5,

 // slots: parameters
    QMetaType::Void, QMetaType::LongLong,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QVariantMap,    9,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   12,

       0        // eod
};

Q_CONSTINIT const QMetaObject LibraryView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_LibraryView.offsetsAndSizes,
    qt_meta_data_LibraryView,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_LibraryView_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<LibraryView, std::true_type>,
        // method 'importRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'deletePlaylistRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<long long, std::false_type>,
        // method 'onPlaylistSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<long long, std::false_type>,
        // method 'onBulkApply'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTrackExpanded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'onTrackCollapsed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onUndoAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void LibraryView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<LibraryView *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->importRequested((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 1: _t->deletePlaylistRequested((*reinterpret_cast< std::add_pointer_t<qlonglong>>(_a[1]))); break;
        case 2: _t->onPlaylistSelected((*reinterpret_cast< std::add_pointer_t<qlonglong>>(_a[1]))); break;
        case 3: _t->onBulkApply(); break;
        case 4: _t->onTrackExpanded((*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[1]))); break;
        case 5: _t->onTrackCollapsed(); break;
        case 6: _t->onUndoAvailable((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (LibraryView::*)(const QStringList & );
            if (_t _q_method = &LibraryView::importRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (LibraryView::*)(long long );
            if (_t _q_method = &LibraryView::deletePlaylistRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *LibraryView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LibraryView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_LibraryView.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int LibraryView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void LibraryView::importRequested(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void LibraryView::deletePlaylistRequested(long long _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
