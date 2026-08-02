/****************************************************************************
** Meta object code from reading C++ file 'devicelistmodel.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../interfaces/devicelistmodel.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'devicelistmodel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.3. It"
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

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSDeviceListModelENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSDeviceListModelENDCLASS = QtMocHelpers::stringData(
    "DeviceListModel",
    "filterGroupChanged",
    "",
    "searchTextChanged",
    "groupsChanged",
    "countsChanged",
    "groupNames",
    "groupDeviceCount",
    "group",
    "filterGroup",
    "searchText",
    "onlineCount",
    "warningCount",
    "offlineCount",
    "totalCount",
    "DeviceRoles",
    "DeviceIdRole",
    "DeviceNameRole",
    "GroupRole",
    "VersionRole",
    "TemperatureRole",
    "MemUsageRole",
    "DiskFreeRole",
    "StatusRole",
    "StatusColorRole",
    "StatusTextRole"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSDeviceListModelENDCLASS_t {
    uint offsetsAndSizes[52];
    char stringdata0[16];
    char stringdata1[19];
    char stringdata2[1];
    char stringdata3[18];
    char stringdata4[14];
    char stringdata5[14];
    char stringdata6[11];
    char stringdata7[17];
    char stringdata8[6];
    char stringdata9[12];
    char stringdata10[11];
    char stringdata11[12];
    char stringdata12[13];
    char stringdata13[13];
    char stringdata14[11];
    char stringdata15[12];
    char stringdata16[13];
    char stringdata17[15];
    char stringdata18[10];
    char stringdata19[12];
    char stringdata20[16];
    char stringdata21[13];
    char stringdata22[13];
    char stringdata23[11];
    char stringdata24[16];
    char stringdata25[15];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSDeviceListModelENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSDeviceListModelENDCLASS_t qt_meta_stringdata_CLASSDeviceListModelENDCLASS = {
    {
        QT_MOC_LITERAL(0, 15),  // "DeviceListModel"
        QT_MOC_LITERAL(16, 18),  // "filterGroupChanged"
        QT_MOC_LITERAL(35, 0),  // ""
        QT_MOC_LITERAL(36, 17),  // "searchTextChanged"
        QT_MOC_LITERAL(54, 13),  // "groupsChanged"
        QT_MOC_LITERAL(68, 13),  // "countsChanged"
        QT_MOC_LITERAL(82, 10),  // "groupNames"
        QT_MOC_LITERAL(93, 16),  // "groupDeviceCount"
        QT_MOC_LITERAL(110, 5),  // "group"
        QT_MOC_LITERAL(116, 11),  // "filterGroup"
        QT_MOC_LITERAL(128, 10),  // "searchText"
        QT_MOC_LITERAL(139, 11),  // "onlineCount"
        QT_MOC_LITERAL(151, 12),  // "warningCount"
        QT_MOC_LITERAL(164, 12),  // "offlineCount"
        QT_MOC_LITERAL(177, 10),  // "totalCount"
        QT_MOC_LITERAL(188, 11),  // "DeviceRoles"
        QT_MOC_LITERAL(200, 12),  // "DeviceIdRole"
        QT_MOC_LITERAL(213, 14),  // "DeviceNameRole"
        QT_MOC_LITERAL(228, 9),  // "GroupRole"
        QT_MOC_LITERAL(238, 11),  // "VersionRole"
        QT_MOC_LITERAL(250, 15),  // "TemperatureRole"
        QT_MOC_LITERAL(266, 12),  // "MemUsageRole"
        QT_MOC_LITERAL(279, 12),  // "DiskFreeRole"
        QT_MOC_LITERAL(292, 10),  // "StatusRole"
        QT_MOC_LITERAL(303, 15),  // "StatusColorRole"
        QT_MOC_LITERAL(319, 14)   // "StatusTextRole"
    },
    "DeviceListModel",
    "filterGroupChanged",
    "",
    "searchTextChanged",
    "groupsChanged",
    "countsChanged",
    "groupNames",
    "groupDeviceCount",
    "group",
    "filterGroup",
    "searchText",
    "onlineCount",
    "warningCount",
    "offlineCount",
    "totalCount",
    "DeviceRoles",
    "DeviceIdRole",
    "DeviceNameRole",
    "GroupRole",
    "VersionRole",
    "TemperatureRole",
    "MemUsageRole",
    "DiskFreeRole",
    "StatusRole",
    "StatusColorRole",
    "StatusTextRole"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSDeviceListModelENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       7,   58, // properties
       1,   93, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    8 /* Public */,
       3,    0,   51,    2, 0x06,    9 /* Public */,
       4,    0,   52,    2, 0x06,   10 /* Public */,
       5,    0,   53,    2, 0x06,   11 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       6,    0,   54,    2, 0x102,   12 /* Public | MethodIsConst  */,
       7,    1,   55,    2, 0x102,   13 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::QStringList,
    QMetaType::Int, QMetaType::QString,    8,

 // properties: name, type, flags
       9, QMetaType::QString, 0x00015103, uint(0), 0,
      10, QMetaType::QString, 0x00015103, uint(1), 0,
       6, QMetaType::QStringList, 0x00015001, uint(2), 0,
      11, QMetaType::Int, 0x00015001, uint(3), 0,
      12, QMetaType::Int, 0x00015001, uint(3), 0,
      13, QMetaType::Int, 0x00015001, uint(3), 0,
      14, QMetaType::Int, 0x00015001, uint(3), 0,

 // enums: name, alias, flags, count, data
      15,   15, 0x0,   10,   98,

 // enum data: key, value
      16, uint(DeviceListModel::DeviceIdRole),
      17, uint(DeviceListModel::DeviceNameRole),
      18, uint(DeviceListModel::GroupRole),
      19, uint(DeviceListModel::VersionRole),
      20, uint(DeviceListModel::TemperatureRole),
      21, uint(DeviceListModel::MemUsageRole),
      22, uint(DeviceListModel::DiskFreeRole),
      23, uint(DeviceListModel::StatusRole),
      24, uint(DeviceListModel::StatusColorRole),
      25, uint(DeviceListModel::StatusTextRole),

       0        // eod
};

Q_CONSTINIT const QMetaObject DeviceListModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractListModel::staticMetaObject>(),
    qt_meta_stringdata_CLASSDeviceListModelENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSDeviceListModelENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSDeviceListModelENDCLASS_t,
        // property 'filterGroup'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'searchText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'groupNames'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'onlineCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'warningCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'offlineCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'totalCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<DeviceListModel, std::true_type>,
        // method 'filterGroupChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'searchTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'groupsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'countsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'groupNames'
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        // method 'groupDeviceCount'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void DeviceListModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DeviceListModel *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->filterGroupChanged(); break;
        case 1: _t->searchTextChanged(); break;
        case 2: _t->groupsChanged(); break;
        case 3: _t->countsChanged(); break;
        case 4: { QStringList _r = _t->groupNames();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = std::move(_r); }  break;
        case 5: { int _r = _t->groupDeviceCount((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DeviceListModel::*)();
            if (_t _q_method = &DeviceListModel::filterGroupChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (DeviceListModel::*)();
            if (_t _q_method = &DeviceListModel::searchTextChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (DeviceListModel::*)();
            if (_t _q_method = &DeviceListModel::groupsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (DeviceListModel::*)();
            if (_t _q_method = &DeviceListModel::countsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<DeviceListModel *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->filterGroup(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->searchText(); break;
        case 2: *reinterpret_cast< QStringList*>(_v) = _t->groupNames(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->onlineCount(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->warningCount(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->offlineCount(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->totalCount(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<DeviceListModel *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setFilterGroup(*reinterpret_cast< QString*>(_v)); break;
        case 1: _t->setSearchText(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *DeviceListModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DeviceListModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSDeviceListModelENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QAbstractListModel::qt_metacast(_clname);
}

int DeviceListModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractListModel::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void DeviceListModel::filterGroupChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DeviceListModel::searchTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void DeviceListModel::groupsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void DeviceListModel::countsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
