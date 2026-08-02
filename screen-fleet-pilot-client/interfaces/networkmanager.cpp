#include "networkmanager.h"
#include "devicelistmodel.h"
#include <QUuid>
#include <QSettings>

NetworkManager::NetworkManager(QObject *parent)
    : QObject{parent}
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_connectTimeoutTimer->stop();
        qDebug() << "[client]: connect to server success";
        m_connectionStatus = QStringLiteral("已连接");
        emit connectionStatusChanged();
        emit connected();
        sendRegisterRequest();
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        qDebug() << "[client]: disconnect from server";
        m_connectionStatus = QStringLiteral("连接断开");
        emit connectionStatusChanged();
        emit disconnected();
        m_reconnectTimer->start(5000);
    });

    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        m_buffer.append(m_socket->readAll());
        processBuffer();
    });

    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        m_connectTimeoutTimer->stop();
        qDebug() << "[client]: tcp socket error:" << m_socket->errorString();
        m_connectionStatus = QStringLiteral("连接错误");
        emit connectionStatusChanged();
        emit errorOccurred(m_socket->errorString());
        m_reconnectTimer->start(5000);
    });

    m_connectTimeoutTimer = new QTimer(this);
    m_connectTimeoutTimer->setSingleShot(true);
    connect(m_connectTimeoutTimer, &QTimer::timeout, this, [this]() {
        m_socket->abort();
        qDebug() << "[client]: connect to server timeout";
        m_connectionStatus = QStringLiteral("连接超时");
        emit connectionStatusChanged();
        emit errorOccurred("connection timeout");
        m_reconnectTimer->start(5000);
    });

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "[client]: try to reconnect to server";
        m_connectionStatus = QStringLiteral("重连中...");
        emit connectionStatusChanged();
        m_socket->connectToHost(m_serverHost, m_serverPort);
    });

    connect(this, &NetworkManager::messageReceived,
            this, &NetworkManager::handleMessage);

    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &NetworkManager::sendHeartBeat);

    connect(this, &NetworkManager::registered,
            this, &NetworkManager::onRegistered);

    m_fetchEmbeddedDeviceTimer = new QTimer(this);
    m_fetchEmbeddedDeviceTimer->setSingleShot(false);
    connect(m_fetchEmbeddedDeviceTimer, &QTimer::timeout, this, [this]() {
        fetchEmbeddedDevices();
    });

    // ── DeviceListModel ──
    m_deviceModel = &DeviceListModel::getInstance();
    connect(m_deviceModel, &DeviceListModel::countsChanged, this, [this]() {
        m_onlineCount  = m_deviceModel->onlineCount();
        m_warningCount = m_deviceModel->warningCount();
        m_offlineCount = m_deviceModel->offlineCount();
        m_totalCount   = m_deviceModel->totalCount();
        emit deviceCountsChanged();
    });

    // ── 初始化管理后台唯一 ID ──
    initDeviceUid();
}

NetworkManager::~NetworkManager()
{
    m_reconnectTimer->stop();
    m_connectTimeoutTimer->stop();
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
    }
}

void NetworkManager::connectToServer(const QString& host, int port)
{
    m_serverHost = host;
    m_serverPort = port;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_connectionStatus = QStringLiteral("连接中...");
    emit connectionStatusChanged();

    m_socket->connectToHost(host, port);
    m_connectTimeoutTimer->start(5000);
}

void NetworkManager::sendRegisterRequest()
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("register");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    params["name"]    = QStringLiteral("管理后台");
    params["group"]   = QStringLiteral("admin");
    params["version"] = QStringLiteral("1.0");
    params["device_uid"] = m_deviceUid;
    msg["params"] = params;

    sendJson(msg);
    qDebug() << "[client]: register request sent, uid:" << m_deviceUid;
}

void NetworkManager::fetchEmbeddedDevices()
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("fetch_devices");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    msg["params"]    = QJsonObject();

    sendJson(msg);
    qDebug() << "[client]: fetch devices request sent";
}

void NetworkManager::requestScreenshot(const QString &deviceUid)
{
    if (deviceUid.isEmpty())
        return;

    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_screenshot");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    params["device_uid"] = deviceUid;
    msg["params"] = params;

    sendJson(msg);
    m_pendingScreenshotUid = deviceUid;
    qDebug() << "[client]: screenshot request sent for" << deviceUid;
}

void NetworkManager::requestPushToDevice(const QString &deviceName)
{
    // 通过信号通知 QML 切换到推送页面
    emit pushToDeviceRequested(deviceName);
}

void NetworkManager::requestEditEmbeddedMessage(const QString &deviceId, const QString &group, const QString &name)
{
    if (m_deviceModel->isUniqueInGroup(group, name, deviceId) == false) {
        qDebug() << "[client]: the name exists";
        return ;
    }

    // 暂存，ack 时立即本地更新
    m_pendingEditId    = deviceId;
    m_pendingEditName  = name;
    m_pendingEditGroup = group;

    QJsonObject msg;
    msg["source"] = QStringLiteral("client");
    msg["cmd"] = QStringLiteral("request_update_embedded");
    msg["seq"] = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    params["device_uid"] = deviceId;
    params["group"] = group;
    params["name"] = name;

    msg["params"] = params;
    sendJson(msg);

    qDebug() << "[client]: request edit embedded message to server";
}

void NetworkManager::requestMaskDevice(const QString &deviceUid)
{
    if (deviceUid.isEmpty())
        return;

    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("mask_device");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    params["device_uid"] = deviceUid;
    msg["params"] = params;

    sendJson(msg);
    qDebug() << "[client]: mask device request sent for" << deviceUid;
}

void NetworkManager::requestServerFileList()
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_file_list");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    msg["params"] = params;
    sendJson(msg);

    qDebug() << "[client]: request file list to server";
}

void NetworkManager::requestServerFirmwareList()
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_firmware_list");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    msg["params"] = params;
    sendJson(msg);

    qDebug() << "[client]: request firmware list from server";
}

void NetworkManager::requestPushContentsToEmbedded(const QStringList &uidList, const QStringList &pathList)
{
    QJsonObject msg;
    msg["source"]       = QStringLiteral("client");
    msg["cmd"]          = QStringLiteral("request_push_content_to_embedded");
    msg["seq"]          = ++m_cmdSeq;
    msg["timestamp"]    = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    QJsonArray  device_uids, paths;
    for (QString uid: uidList) {
        device_uids.append(uid);
    }
    for (QString path: pathList) {
        paths.append(path);
    }
    params["device_uids"] = device_uids;
    params["paths"] = paths;
    msg["params"] = params;
    sendJson(msg);
    qDebug() << "[client]: request push contents to embedded";
}

void NetworkManager::requestSchedulePushToEmbedded(const QStringList &uidList,
                                                   const QStringList &pathList,
                                                   const QString &scheduleDate,
                                                   const QString &scheduleTime,
                                                   int durationSec)
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_schedule_push");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    QJsonArray device_uids, paths;
    for (const QString &uid : uidList)
        device_uids.append(uid);
    for (const QString &path : pathList)
        paths.append(path);
    params["device_uids"]   = device_uids;
    params["paths"]         = paths;
    params["schedule_date"] = scheduleDate;
    params["schedule_time"] = scheduleTime;
    params["duration_sec"]  = durationSec;
    msg["params"] = params;
    sendJson(msg);
    qDebug() << "[client]: request schedule push, date:" << scheduleDate
             << "time:" << scheduleTime << "duration:" << durationSec;
}

void NetworkManager::requestOTAUpdateToEmbedded(const QStringList &uidList, const QString &otaFilePath)
{
    if (uidList.isEmpty() || otaFilePath.isEmpty()) {
        qDebug() << "[client]: request OTA update skipped, empty uid list or path";
        return;
    }

    QString path = otaFilePath;
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));

    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_ota_update");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    QJsonArray device_uids;
    for (const QString &uid : uidList)
        device_uids.append(uid);
    params["device_uids"] = device_uids;
    params["path"]        = path;
    params["device_uid"]  = m_deviceUid;
    msg["params"] = params;
    sendJson(msg);
    qDebug() << "[client]: request OTA update to" << uidList.size() << "devices, path:" << path
             << "client_uid:" << m_deviceUid;
}

void NetworkManager::requestCheckFirmware(const QString &path, const QString &md5)
{
    if (path.isEmpty() || md5.isEmpty()) {
        qDebug() << "[client]: request check firmware skipped, empty path or md5";
        return;
    }

    QString serverPath = path;
    if (!serverPath.startsWith(QLatin1Char('/')))
        serverPath.prepend(QLatin1Char('/'));

    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("request_check_firmware");
    msg["seq"]       = ++m_cmdSeq;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QJsonObject params;
    params["path"] = serverPath;
    params["md5"]  = md5;
    msg["params"] = params;
    sendJson(msg);
    qDebug() << "[client]: request check firmware, path:" << serverPath << "md5:" << md5;
}

QString NetworkManager::screenshotUrlFromPath(const QString &path)
{
    if (path.isEmpty())
        return {};
    if (path.startsWith(QStringLiteral("http://")) || path.startsWith(QStringLiteral("https://")))
        return path;
    QString normalized = path;
    if (!normalized.startsWith(QLatin1Char('/')))
        normalized.prepend(QLatin1Char('/'));
    return QStringLiteral("http://8.136.113.168") + normalized;
}

void NetworkManager::handleMessage(const QJsonObject& msg)
{
    QString source = msg["source"].toString();
    QString cmd    = msg["cmd"].toString();
    QJsonObject params = msg["params"].toObject();

    if (source == "server") {
        if (cmd == "register_ack") {
            int code = params["code"].toInt(-1);
            if (code == 0) {
                m_deviceId   = params["device_id"].toInt();
                m_registered = true;
                qDebug() << "[client]: register success, device_id:" << m_deviceId;
                emit registered();
            } else {
                qDebug() << "[client]: register failed:" << params["msg"].toString();
            }
        } else if (cmd == "fetch_devices_ack") {
            QJsonArray devices = params["devices"].toArray();
            m_deviceModel->setDevices(devices);

            qDebug() << "[client]: embedded devices (" << devices.size() << "): ";
            for (const QJsonValue& val : devices) {
                QJsonObject dev = val.toObject();
                qDebug() << "id:"           << dev["id"].toInt()
                         << "device_uid:"   << dev["device_uid"].toString()
                         << "name:"         << dev["name"].toString()
                         << "group:"        << dev["group"].toString()
                         << "version:"      << dev["version"].toString()
                         << "temperature:"  << dev["temperature"].toString()
                         << "mem_usage"     << dev["mem_usage"].toInt()
                         << "disk_free_mb"  << dev["disk_free_mb"].toInt() << "\n";
            }
        } else if (cmd == "request_screenshot_ack") {
            const QString path = params["path"].toString();
            if (path.isEmpty()) {
                qDebug() << "[client]: screenshot ack missing path";
                return;
            }
            const QString imageUrl = screenshotUrlFromPath(path);
            const QString deviceName = m_deviceModel->deviceNameByUid(m_pendingScreenshotUid);
            qDebug() << "[client]: screenshot ready" << deviceName << imageUrl;
            emit screenshotUrlReceived(deviceName, imageUrl);
            m_pendingScreenshotUid.clear();
        } else if (cmd == "update_embedded_info_result") {
            if (params["status"] != "ok") {
                qDebug() << "[client]: update embedded failed";
                return ;
            }
            qDebug() << "[client]: update embedded success";
            // 本地立即更新（不等服务器二次查询，避免被未落地的旧数据覆盖）
            if (m_pendingEditId.isEmpty() == false) {
                m_deviceModel->updateDeviceLocal(m_pendingEditId, m_pendingEditName, m_pendingEditGroup);
                m_pendingEditId = "";
            }
            emit updateEmbeddedMessageFinished();
        } else if (cmd == "request_filelist_ack") {
            QJsonArray files = params["files"].toArray();
            const int count = params["count"].toInt(files.size());
            qDebug() << "[client]: server file list (" << count << "):";
            for (const QJsonValue& val : files) {
                QJsonObject f = val.toObject();
                qDebug() << "  path:" << f["path"].toString()
                         << "name:" << f["name"].toString()
                         << "size:" << f["size"].toVariant().toLongLong();
            }
            emit serverFileListReceived(files);
        } else if (cmd == QStringLiteral("request_firmware_list_ack")
                   || cmd == QStringLiteral("request_firmwarelist_ack")) {
            QJsonArray firmwares = params[QStringLiteral("firmwares")].toArray();
            if (firmwares.isEmpty())
                firmwares = params[QStringLiteral("files")].toArray();
            const int count = params[QStringLiteral("count")].toInt(firmwares.size());
            qDebug() << "[client]: server firmware list (" << count << "):";
            for (const QJsonValue &val : firmwares) {
                QJsonObject f = val.toObject();
                qDebug() << "  path:" << f[QStringLiteral("path")].toString()
                         << "name:" << f[QStringLiteral("name")].toString()
                         << "version:" << f[QStringLiteral("version")].toString()
                         << "size:" << f[QStringLiteral("size")].toVariant().toLongLong();
            }
            emit serverFirmwareListReceived(firmwares);
        } else if (cmd == QStringLiteral("check_firmware_ack")) {
            const int result = params[QStringLiteral("result")].toInt(0);
            qDebug() << "[client]: check firmware ack, result:" << result;
            emit checkFirmwareAck(result);
        } else if (cmd == QStringLiteral("ota_update_ack")) {
            const int result = params[QStringLiteral("result")].toInt(0);
            qDebug() << "[client]: ota update ack, result:" << result;
            emit otaUpdateAck(result);
        }
    }

}

void NetworkManager::onRegistered()
{
    fetchEmbeddedDevices();
    m_fetchEmbeddedDeviceTimer->start(5000);
    m_heartbeatTimer->start(5000);
}

void NetworkManager::sendHeartBeat()
{
    QJsonObject msg;
    msg["source"]    = QStringLiteral("client");
    msg["cmd"]       = QStringLiteral("heartbeat");
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();
    msg["device_id"] = m_deviceId;

    // ── 采集健康数据 ──
    QJsonObject params;

    msg["params"] = params;

    sendJson(msg);
    qDebug() << "[client]: sent heartbeat";
}

// ═══════════════════════════════════════════════════════════════
// 管理后台唯一 ID：QSettings 持久化，首次生成后不再变
// ═══════════════════════════════════════════════════════════════

void NetworkManager::initDeviceUid()
{
    QSettings settings("ScreenFleetPilot", "Client");
    m_deviceUid = settings.value("device_uid").toString();

    if (m_deviceUid.isEmpty()) {
        // 首次运行：生成 UUID 并持久化
        m_deviceUid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("device_uid", m_deviceUid);
        settings.sync();
        qDebug() << "[client] generated new device_uid:" << m_deviceUid;
    } else {
        qDebug() << "[client] loaded device_uid:" << m_deviceUid;
    }
}

void NetworkManager::sendJson(const QJsonObject& obj)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void NetworkManager::processBuffer()
{
    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx < 0) break;

        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        if (!doc.isObject()) continue;

        emit messageReceived(doc.object());
    }
}
