/****************************************************************************
** Meta object code from reading C++ file 'main_window.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/main_window.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'main_window.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
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
struct qt_meta_tag_ZN6poetry10MainWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN6poetry10MainWindowE = QtMocHelpers::stringData(
    "poetry::MainWindow",
    "addImageDirectory",
    "",
    "removeImageDirectory",
    "rescanImages",
    "importPoetry",
    "deleteSelectedPoem",
    "poemItemChanged",
    "QTableWidgetItem*",
    "item",
    "requestManualSwitch",
    "timerTimeout",
    "scanFinished",
    "switchFinished",
    "chooseTextColor",
    "choosePanelColor",
    "saveSettings",
    "toggleSchedule",
    "enabled",
    "openCacheDirectory",
    "showWindow",
    "quitFromTray"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN6poetry10MainWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  116,    2, 0x08,    1 /* Private */,
       3,    0,  117,    2, 0x08,    2 /* Private */,
       4,    0,  118,    2, 0x08,    3 /* Private */,
       5,    0,  119,    2, 0x08,    4 /* Private */,
       6,    0,  120,    2, 0x08,    5 /* Private */,
       7,    1,  121,    2, 0x08,    6 /* Private */,
      10,    0,  124,    2, 0x08,    8 /* Private */,
      11,    0,  125,    2, 0x08,    9 /* Private */,
      12,    0,  126,    2, 0x08,   10 /* Private */,
      13,    0,  127,    2, 0x08,   11 /* Private */,
      14,    0,  128,    2, 0x08,   12 /* Private */,
      15,    0,  129,    2, 0x08,   13 /* Private */,
      16,    0,  130,    2, 0x08,   14 /* Private */,
      17,    1,  131,    2, 0x08,   15 /* Private */,
      19,    0,  134,    2, 0x08,   17 /* Private */,
      20,    0,  135,    2, 0x08,   18 /* Private */,
      21,    0,  136,    2, 0x08,   19 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   18,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject poetry::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_ZN6poetry10MainWindowE.offsetsAndSizes,
    qt_meta_data_ZN6poetry10MainWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN6poetry10MainWindowE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'addImageDirectory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeImageDirectory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'rescanImages'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'importPoetry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteSelectedPoem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'poemItemChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTableWidgetItem *, std::false_type>,
        // method 'requestManualSwitch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'timerTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'scanFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'switchFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'chooseTextColor'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'choosePanelColor'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleSchedule'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'openCacheDirectory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showWindow'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'quitFromTray'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void poetry::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->addImageDirectory(); break;
        case 1: _t->removeImageDirectory(); break;
        case 2: _t->rescanImages(); break;
        case 3: _t->importPoetry(); break;
        case 4: _t->deleteSelectedPoem(); break;
        case 5: _t->poemItemChanged((*reinterpret_cast< std::add_pointer_t<QTableWidgetItem*>>(_a[1]))); break;
        case 6: _t->requestManualSwitch(); break;
        case 7: _t->timerTimeout(); break;
        case 8: _t->scanFinished(); break;
        case 9: _t->switchFinished(); break;
        case 10: _t->chooseTextColor(); break;
        case 11: _t->choosePanelColor(); break;
        case 12: _t->saveSettings(); break;
        case 13: _t->toggleSchedule((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->openCacheDirectory(); break;
        case 15: _t->showWindow(); break;
        case 16: _t->quitFromTray(); break;
        default: ;
        }
    }
}

const QMetaObject *poetry::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *poetry::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN6poetry10MainWindowE.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int poetry::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
