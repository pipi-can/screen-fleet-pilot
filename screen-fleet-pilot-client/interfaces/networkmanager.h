#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

#include "devicelistmodel.h"

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    Q_INVOKABLE void connectToServer(const QString& host, int port);
    Q_INVOKABLE void sendRegisterRequest();

    Q_INVOKABLE void fetchEmbeddedDevices();
    Q_INVOKABLE void requestScreenshot(const QString &deviceUid);
    Q_INVOKABLE void requestPushToDevice(const QString &deviceName);
    Q_INVOKABLE void requestEditEmbeddedMessage(const QString& deviceId, const QString& group, const QString& name);
    Q_INVOKABLE void requestMaskDevice(const QString& deviceUid);

    Q_INVOKABLE void requestServerFileList();

    Q_INVOKABLE void requestServerFirmwareList();

    Q_INVOKABLE void requestPushContentsToEmbedded(const QStringList& uidList, const QStringList& pathList);

    Q_INVOKABLE void requestSchedulePushToEmbedded(const QStringList& uidList,
                                                   const QStringList& pathList,
                                                   const QString& scheduleDate,
                                                   const QString& scheduleTime,
                                                   int durationSec);


    Q_INVOKABLE void requestOTAUpdateToEmbedded(const QStringList& uidList, const QString& otaFilePath);

    Q_INVOKABLE void requestCheckFirmware(const QString& path, const QString& md5);

    Q_PROPERTY(int deviceId READ deviceId NOTIFY registered)
    int deviceId() const { return m_deviceId; }

    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
    QString connectionStatus() const { return m_connectionStatus; }

    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY deviceCountsChanged)
    int onlineCount() const { return m_onlineCount; }

    Q_PROPERTY(int warningCount READ warningCount NOTIFY deviceCountsChanged)
    int warningCount() const { return m_warningCount; }

    Q_PROPERTY(int offlineCount READ offlineCount NOTIFY deviceCountsChanged)
    int offlineCount() const { return m_offlineCount; }

    Q_PROPERTY(int totalCount READ totalCount NOTIFY deviceCountsChanged)
    int totalCount() const { return m_totalCount; }

    Q_PROPERTY(QObject* deviceModel READ deviceModel CONSTANT)
    QObject* deviceModel() const { return m_deviceModel; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);
    void registered();
    void messageReceived(const QJsonObject& msg);
    void connectionStatusChanged();
    void deviceCountsChanged();
    void screenshotUrlReceived(const QString &deviceName, const QString &imageUrl);
    void pushToDeviceRequested(const QString &deviceName);
    void updateEmbeddedMessageFinished();
    void serverFileListReceived(const QJsonArray& files);
    void serverFirmwareListReceived(const QJsonArray& files);
    void checkFirmwareAck(int result);
    void otaUpdateAck(int result);

private slots:
    void handleMessage(const QJsonObject& msg);

    void onRegistered() ;

    void sendHeartBeat();
private:
    void sendJson(const QJsonObject& obj);
    void processBuffer();
    void initDeviceUid();

    QTcpSocket* m_socket = nullptr;
    QTimer*     m_reconnectTimer = nullptr;
    QTimer*     m_connectTimeoutTimer = nullptr;
    QTimer*     m_fetchEmbeddedDeviceTimer = nullptr;
    QTimer*     m_heartbeatTimer = nullptr;

    QByteArray  m_buffer;
    QString     m_serverHost;
    int         m_serverPort = 0;
    int         m_cmdSeq = 0;
    int         m_deviceId = -1;
    bool        m_registered = false;
    QString     m_deviceUid;
    QString     m_connectionStatus = QStringLiteral("未连接");

    int         m_onlineCount = 0;
    int         m_warningCount = 0;
    int         m_offlineCount = 0;
    int         m_totalCount = 0;

    DeviceListModel* m_deviceModel = nullptr;

    // 待编辑暂存（ack 时本地更新用）
    QString m_pendingEditId ;
    QString m_pendingEditName;
    QString m_pendingEditGroup;
    QString m_pendingScreenshotUid;

    static QString screenshotUrlFromPath(const QString &path);
};

#endif // NETWORKMANAGER_H
