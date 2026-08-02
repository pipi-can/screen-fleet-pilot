/****************************************************************************
** Meta object code from reading C++ file 'networkmanager.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../interfaces/networkmanager.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'networkmanager.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSNetworkManagerENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSNetworkManagerENDCLASS = QtMocHelpers::stringData(
    "NetworkManager",
    "connected",
    "",
    "disconnected",
    "errorOccurred",
    "error",
    "registered",
    "messageReceived",
    "msg",
    "connectionStatusChanged",
    "deviceCountsChanged",
    "screenshotReceived",
    "deviceName",
    "imageBase64",
    "pushToDeviceRequested",
    "updateEmbeddedMessageFinished",
    "handleMessage",
    "onRegistered",
    "sendHeartBeat",
    "connectToServer",
    "host",
    "port",
    "sendRegisterRequest",
    "fetchEmbeddedDevices",
    "requestScreenshot",
    "requestPushToDevice",
    "requestEditEmbeddedMessage",
    "deviceId",
    "group",
    "name",
    "connectionStatus",
    "onlineCount",
    "warningCount",
    "offlineCount",
    "totalCount",
    "deviceModel"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSNetworkManagerENDCLASS_t {
    uint offsetsAndSizes[72];
    char stringdata0[15];
    char stringdata1[10];
    char stringdata2[1];
    char stringdata3[13];
    char stringdata4[14];
    char stringdata5[6];
    char stringdata6[11];
    char stringdata7[16];
    char stringdata8[4];
    char stringdata9[24];
    char stringdata10[20];
    char stringdata11[19];
    char stringdata12[11];
    char stringdata13[12];
    char stringdata14[22];
    char stringdata15[30];
    char stringdata16[14];
    char stringdata17[13];
    char stringdata18[14];
    char stringdata19[16];
    char stringdata20[5];
    char stringdata21[5];
    char stringdata22[20];
    char stringdata23[21];
    char stringdata24[18];
    char stringdata25[20];
    char stringdata26[27];
    char stringdata27[9];
    char stringdata28[6];
    char stringdata29[5];
    char stringdata30[17];
    char stringdata31[12];
    char stringdata32[13];
    char stringdata33[13];
    char stringdata34[11];
    char stringdata35[12];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSNetworkManagerENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSNetworkManagerENDCLASS_t qt_meta_stringdata_CLASSNetworkManagerENDCLASS = {
    {
        QT_MOC_LITERAL(0, 14),  // "NetworkManager"
        QT_MOC_LITERAL(15, 9),  // "connected"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 12),  // "disconnected"
        QT_MOC_LITERAL(39, 13),  // "errorOccurred"
        QT_MOC_LITERAL(53, 5),  // "error"
        QT_MOC_LITERAL(59, 10),  // "registered"
        QT_MOC_LITERAL(70, 15),  // "messageReceived"
        QT_MOC_LITERAL(86, 3),  // "msg"
        QT_MOC_LITERAL(90, 23),  // "connectionStatusChanged"
        QT_MOC_LITERAL(114, 19),  // "deviceCountsChanged"
        QT_MOC_LITERAL(134, 18),  // "screenshotReceived"
        QT_MOC_LITERAL(153, 10),  // "deviceName"
        QT_MOC_LITERAL(164, 11),  // "imageBase64"
        QT_MOC_LITERAL(176, 21),  // "pushToDeviceRequested"
        QT_MOC_LITERAL(198, 29),  // "updateEmbeddedMessageFinished"
        QT_MOC_LITERAL(228, 13),  // "handleMessage"
        QT_MOC_LITERAL(242, 12),  // "onRegistered"
        QT_MOC_LITERAL(255, 13),  // "sendHeartBeat"
        QT_MOC_LITERAL(269, 15),  // "connectToServer"
        QT_MOC_LITERAL(285, 4),  // "host"
        QT_MOC_LITERAL(290, 4),  // "port"
        QT_MOC_LITERAL(295, 19),  // "sendRegisterRequest"
        QT_MOC_LITERAL(315, 20),  // "fetchEmbeddedDevices"
        QT_MOC_LITERAL(336, 17),  // "requestScreenshot"
        QT_MOC_LITERAL(354, 19),  // "requestPushToDevice"
        QT_MOC_LITERAL(374, 26),  // "requestEditEmbeddedMessage"
        QT_MOC_LITERAL(401, 8),  // "deviceId"
        QT_MOC_LITERAL(410, 5),  // "group"
        QT_MOC_LITERAL(416, 4),  // "name"
        QT_MOC_LITERAL(421, 16),  // "connectionStatus"
        QT_MOC_LITERAL(438, 11),  // "onlineCount"
        QT_MOC_LITERAL(450, 12),  // "warningCount"
        QT_MOC_LITERAL(463, 12),  // "offlineCount"
        QT_MOC_LITERAL(476, 10),  // "totalCount"
        QT_MOC_LITERAL(487, 11)   // "deviceModel"
    },
    "NetworkManager",
    "connected",
    "",
    "disconnected",
    "errorOccurred",
    "error",
    "registered",
    "messageReceived",
    "msg",
    "connectionStatusChanged",
    "deviceCountsChanged",
    "screenshotReceived",
    "deviceName",
    "imageBase64",
    "pushToDeviceRequested",
    "updateEmbeddedMessageFinished",
    "handleMessage",
    "onRegistered",
    "sendHeartBeat",
    "connectToServer",
    "host",
    "port",
    "sendRegisterRequest",
    "fetchEmbeddedDevices",
    "requestScreenshot",
    "requestPushToDevice",
    "requestEditEmbeddedMessage",
    "deviceId",
    "group",
    "name",
    "connectionStatus",
    "onlineCount",
    "warningCount",
    "offlineCount",
    "totalCount",
    "deviceModel"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSNetworkManagerENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
      19,   14, // methods
       7,  173, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  128,    2, 0x06,    8 /* Public */,
       3,    0,  129,    2, 0x06,    9 /* Public */,
       4,    1,  130,    2, 0x06,   10 /* Public */,
       6,    0,  133,    2, 0x06,   12 /* Public */,
       7,    1,  134,    2, 0x06,   13 /* Public */,
       9,    0,  137,    2, 0x06,   15 /* Public */,
      10,    0,  138,    2, 0x06,   16 /* Public */,
      11,    2,  139,    2, 0x06,   17 /* Public */,
      14,    1,  144,    2, 0x06,   20 /* Public */,
      15,    0,  147,    2, 0x06,   22 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      16,    1,  148,    2, 0x08,   23 /* Private */,
      17,    0,  151,    2, 0x08,   25 /* Private */,
      18,    0,  152,    2, 0x08,   26 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      19,    2,  153,    2, 0x02,   27 /* Public */,
      22,    0,  158,    2, 0x02,   30 /* Public */,
      23,    0,  159,    2, 0x02,   31 /* Public */,
      24,    1,  160,    2, 0x02,   32 /* Public */,
      25,    1,  163,    2, 0x02,   34 /* Public */,
      26,    3,  166,    2, 0x02,   36 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QJsonObject,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   12,   13,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QJsonObject,    8,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   20,   21,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::QString, QMetaType::QString,   27,   28,   29,

 // properties: name, type, flags
      27, QMetaType::Int, 0x00015001, uint(3), 0,
      30, QMetaType::QString, 0x00015001, uint(5), 0,
      31, QMetaType::Int, 0x00015001, uint(6), 0,
      32, QMetaType::Int, 0x00015001, uint(6), 0,
      33, QMetaType::Int, 0x00015001, uint(6), 0,
      34, QMetaType::Int, 0x00015001, uint(6), 0,
      35, QMetaType::QObjectStar, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject NetworkManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSNetworkManagerENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSNetworkManagerENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSNetworkManagerENDCLASS_t,
        // property 'deviceId'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'connectionStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'onlineCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'warningCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'offlineCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'totalCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'deviceModel'
        QtPrivate::TypeAndForceComplete<QObject*, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<NetworkManager, std::true_type>,
        // method 'connected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'disconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'errorOccurred'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'registered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'messageReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>,
        // method 'connectionStatusChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deviceCountsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'screenshotReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'pushToDeviceRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'updateEmbeddedMessageFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>,
        // method 'onRegistered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sendHeartBeat'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'connectToServer'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'sendRegisterRequest'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'fetchEmbeddedDevices'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestScreenshot'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'requestPushToDevice'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'requestEditEmbeddedMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void NetworkManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<NetworkManager *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connected(); break;
        case 1: _t->disconnected(); break;
        case 2: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->registered(); break;
        case 4: _t->messageReceived((*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 5: _t->connectionStatusChanged(); break;
        case 6: _t->deviceCountsChanged(); break;
        case 7: _t->screenshotReceived((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->pushToDeviceRequested((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->updateEmbeddedMessageFinished(); break;
        case 10: _t->handleMessage((*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 11: _t->onRegistered(); break;
        case 12: _t->sendHeartBeat(); break;
        case 13: _t->connectToServer((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 14: _t->sendRegisterRequest(); break;
        case 15: _t->fetchEmbeddedDevices(); break;
        case 16: _t->requestScreenshot((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->requestPushToDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->requestEditEmbeddedMessage((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::connected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::disconnected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)(const QString & );
            if (_t _q_method = &NetworkManager::errorOccurred; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::registered; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)(const QJsonObject & );
            if (_t _q_method = &NetworkManager::messageReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::connectionStatusChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::deviceCountsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)(const QString & , const QString & );
            if (_t _q_method = &NetworkManager::screenshotReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)(const QString & );
            if (_t _q_method = &NetworkManager::pushToDeviceRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (NetworkManager::*)();
            if (_t _q_method = &NetworkManager::updateEmbeddedMessageFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<NetworkManager *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->deviceId(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->connectionStatus(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->onlineCount(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->warningCount(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->offlineCount(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->totalCount(); break;
        case 6: *reinterpret_cast< QObject**>(_v) = _t->deviceModel(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *NetworkManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *NetworkManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSNetworkManagerENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int NetworkManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 19;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void NetworkManager::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void NetworkManager::disconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void NetworkManager::errorOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void NetworkManager::registered()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void NetworkManager::messageReceived(const QJsonObject & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void NetworkManager::connectionStatusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void NetworkManager::deviceCountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void NetworkManager::screenshotReceived(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void NetworkManager::pushToDeviceRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void NetworkManager::updateEmbeddedMessageFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}
QT_WARNING_POP
