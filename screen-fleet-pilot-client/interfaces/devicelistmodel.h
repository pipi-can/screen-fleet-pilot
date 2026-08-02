#ifndef DEVICELISTMODEL_H
#define DEVICELISTMODEL_H

#include <QAbstractListModel>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QVariant>

class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString filterGroup READ filterGroup WRITE setFilterGroup NOTIFY filterGroupChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QStringList groupNames READ groupNames NOTIFY groupsChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY countsChanged)
    Q_PROPERTY(int offlineCount READ offlineCount NOTIFY countsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)

public:
    enum DeviceRoles {
        DeviceIdRole    = Qt::UserRole + 1,
        DeviceUIdRole,
        DeviceNameRole,
        GroupRole,
        VersionRole,
        TemperatureRole,
        MemUsageRole,
        DiskFreeRole,
        StatusRole,
        StatusColorRole,
        StatusTextRole
    };
    Q_ENUM(DeviceRoles)

    static DeviceListModel& getInstance();

    DeviceListModel(const DeviceListModel& other) = delete;
    void operator=(const DeviceListModel& other) = delete;

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // ── 数据加载（增量合并，不跳顶部） ──
    void setDevices(const QJsonArray &devices);
    void clear();

    // ── 分组过滤 ──
    QString filterGroup() const { return m_filterGroup; }
    void setFilterGroup(const QString &group);
    bool isUniqueInGroup(const QString& group, const QString& name, const QString& excludeId);
    void updateDeviceLocal(const QString& uid, const QString &name, const QString &group);

    // ── 搜索过滤 ──
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    // ── 分组信息 ──
    Q_INVOKABLE QStringList groupNames() const;
    Q_INVOKABLE int groupDeviceCount(const QString &group) const;
    Q_INVOKABLE int onlineCountInGroup(const QString &group) const;
    Q_INVOKABLE int onlineDeviceCount() const;
    Q_INVOKABLE QVariantList onlineDeviceList() const;
    Q_INVOKABLE QString deviceNameByUid(const QString &uid) const;

    // ── 统计 ──
    int onlineCount()  const { return m_onlineCount; }
    int warningCount() const { return m_warningCount; }
    int offlineCount() const { return m_offlineCount; }
    int totalCount()   const { return m_totalCount; }

signals:
    void filterGroupChanged();
    void searchTextChanged();
    void groupsChanged();
    void countsChanged();

private:
    explicit DeviceListModel(QObject *parent = nullptr);
    // ── 核心数据结构 ──
    // m_allDevices: 全量设备数据，按 uid 索引，更新不丢滚动位置
    QHash<QString, QJsonObject> m_allDevices;
    // m_displayIds: 当前筛选后的显示顺序（模型暴露的就是这个）
    QVector<QString>            m_displayUIds;

    QString m_filterGroup;  // "" = 全部
    QString m_searchText;   // "" = 全部

    // ── 内部方法 ──
    void rebuildDisplayFromAll();  // 从 m_allDevices 重建 m_displayIds（切换筛选时用）
    void recalcCountsAndGroups();

    static QString statusFromData(const QJsonObject &dev);
    static QString statusColorFromStatus(const QString &status);
    static QString statusTextFromStatus(const QString &status);

    int m_onlineCount  = 0;
    int m_warningCount = 0;
    int m_offlineCount = 0;
    int m_totalCount   = 0;
};

#endif // DEVICELISTMODEL_H
