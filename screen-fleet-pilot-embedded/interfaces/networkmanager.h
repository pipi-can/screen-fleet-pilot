#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>

// ── 服务器连接配置 ──────────────────────────────────
#define PORT    8000
#define IP      "8.136.113.168"

// ── NetworkManager：设备端网络通信核心 ───────────────
// 使用 QTcpSocket 事件驱动，替代原始 POSIX socket
// Q_PROPERTY 暴露给 QML 做数据绑定

class NetworkManager : public QObject
{
    Q_OBJECT

    // ── QML 绑定属性 ──
    Q_PROPERTY(QString deviceName   READ deviceName   CONSTANT)
    Q_PROPERTY(QString deviceGroup  READ deviceGroup  CONSTANT)
    Q_PROPERTY(QString currentImage READ currentImage NOTIFY currentImageChanged)
    Q_PROPERTY(QString currentText  READ currentText  NOTIFY currentTextChanged)
    Q_PROPERTY(QString statusInfo   READ statusInfo   NOTIFY statusInfoChanged)
    Q_PROPERTY(QString cpuTemp      READ cpuTemp      NOTIFY healthChanged)
    Q_PROPERTY(bool    connected    READ connected    NOTIFY connectedChanged)

public:
    // 单例
    static NetworkManager& getInstance();

    // 禁止拷贝
    NetworkManager(const NetworkManager&) = delete;
    void operator=(const NetworkManager&) = delete;

    // ── 公共接口 ──
    Q_INVOKABLE void connectToServer(const QString& host = IP, int port = PORT);
    Q_INVOKABLE void disconnectFromServer();

    // ── 属性访问器 ──
    QString deviceName()   const { return m_deviceName; }
    QString deviceGroup()  const { return m_deviceGroup; }
    QString currentImage() const { return m_currentImage; }
    QString currentText()  const { return m_currentText; }
    QString statusInfo()   const { return m_statusInfo; }
    QString cpuTemp()      const { return m_cpuTemp; }
    bool    connected()    const { return m_connected; }

    // 设置设备信息
    void setDeviceInfo(const QString& name, const QString& group);

signals:
    void currentImageChanged();
    void currentTextChanged();
    void statusInfoChanged();
    void healthChanged();
    void connectedChanged();

    // QML 端抓图信号
    void screenshotRequested();

private slots:
    // QTcpSocket 信号
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

    // 心跳定时器
    void onHeartbeat();

    // 重连定时器
    void onReconnect();

private:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();

    // ── 协议处理 ──
    void sendJson(const QJsonObject& msg);
    void handleCommand(const QString& source, const QString& cmd, const QJsonObject& params);

    // ── 硬件读取 ──
    QString readCpuTemp();
    int     readMemUsage();
    int     readDiskFreeMb();
    void    initDeviceUid();

    // ── 网络组件 ──
    QTcpSocket* m_socket   = nullptr;
    QTimer*     m_heartbeatTimer = nullptr;
    QTimer*     m_reconnectTimer = nullptr;

    // ── 协议缓冲 ──
    QByteArray m_buffer;

    // ── 设备信息 ──
    QString m_deviceName;
    QString m_deviceGroup;
    QString m_deviceUid;
    int     m_deviceId;       // 服务器分配
    bool    m_registered = false;
    bool    m_connected  = false;

    // ── 当前内容 ──
    QString m_currentImage;
    QString m_currentText;

    // ── 健康数据 ──
    QString m_cpuTemp;
    int     m_memUsage    = 0;
    int     m_diskFreeMb  = 0;

    // ── 序列号 ──
    int m_heartbeatSeq = 0;
    int m_cmdSeq       = 0;

    // ── 状态信息 ──
    QString m_statusInfo;
    QString m_serverHost;
    int     m_serverPort = PORT;
};

#endif // NETWORKMANAGER_H
