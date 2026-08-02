#include "devicelistmodel.h"
#include <QDebug>
#include <QJsonDocument>
#include <algorithm>

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// ═══════════════════════════════════════════
// QAbstractListModel 接口
// ═══════════════════════════════════════════

DeviceListModel &DeviceListModel::getInstance()
{
    static DeviceListModel instance;
    return instance;
}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_displayUIds.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_displayUIds.size())
        return {};

    QString id = m_displayUIds.at(index.row());
    const QJsonObject &dev = m_allDevices[id];
    QString status = statusFromData(dev);

    switch (role) {
    case DeviceIdRole:    return dev["id"].toInt();
    case DeviceUIdRole:   return dev["device_uid"].toString();
    case DeviceNameRole:  return dev["name"].toString();
    case GroupRole:       return dev["group"].toString();
    case VersionRole:     return dev["version"].toString();
    case TemperatureRole: return dev["temperature"].toString();
    case MemUsageRole:    return dev["mem_usage"].toInt();
    case DiskFreeRole:    return dev["disk_free_mb"].toInt();
    case StatusRole:      return status;
    case StatusColorRole: return statusColorFromStatus(status);
    case StatusTextRole:  return statusTextFromStatus(status);
    case Qt::DisplayRole: return dev["name"].toString();
    }

    return {};
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        { DeviceIdRole,    "deviceId"    },
        { DeviceUIdRole,   "deviceUId"   },
        { DeviceNameRole,  "deviceName"  },
        { GroupRole,       "deviceGroup" },
        { VersionRole,     "deviceVersion" },
        { TemperatureRole, "temperature" },
        { MemUsageRole,    "memUsage"    },
        { DiskFreeRole,    "diskFree"    },
        { StatusRole,      "status"      },
        { StatusColorRole, "statusColor" },
        { StatusTextRole,  "statusText"  },
    };
}

// ═══════════════════════════════════════════
// 辅助：把设备的关键字段序列化成可比字符串
// ═══════════════════════════════════════════
static QByteArray deviceFingerprint(const QJsonObject &dev)
{
    // 只比较会影响 UI 显示的字段
    return QJsonDocument(QJsonObject{
        {"name",        dev["name"].toString()},
        {"group",       dev["group"].toString()},
        {"version",     dev["version"].toString()},
        {"temperature", dev["temperature"].toString()},
        {"mem_usage",   dev["mem_usage"].toInt()},
        {"disk_free_mb",dev["disk_free_mb"].toInt()},
    }).toJson(QJsonDocument::Compact);
}

// ═══════════════════════════════════════════
// 设备是否匹配当前筛选（分组 + 搜索）
// ═══════════════════════════════════════════
bool deviceMatchesFilter(const QJsonObject &dev, const QString &filterGroup, const QString &searchText)
{
    if (!filterGroup.isEmpty() && dev["group"].toString() != filterGroup)
        return false;
    if (!searchText.isEmpty() && !dev["name"].toString().contains(searchText, Qt::CaseInsensitive))
        return false;
    return true;
}

// ═══════════════════════════════════════════
// 数据加载 — 核心：增量合并，不跳顶部
// ═══════════════════════════════════════════
void DeviceListModel::setDevices(const QJsonArray &devices)
{
    // 1. 解析输入
    QHash<QString, QJsonObject> incoming;
    for (const QJsonValue &val : devices) {
        QJsonObject obj = val.toObject();
        QString uid = obj["device_uid"].toString();
        incoming[uid] = obj;
    }

    // 2. 找出三类变化
    QSet<QString> incomingIds;   // 设备列表所有id
    for (auto it = incoming.begin(); it != incoming.end(); ++it)
        incomingIds.insert(it.key());

    QSet<QString> staleIds;       // 在旧数据但不在新数据，这次数据包没有这个设备，可能断开了
    QVector<QString> updatedIds;  // 在两边且数据变了，这次数据包包含这个设备，且更新了
    QSet<QString> newIds;         // 在新数据但不在旧数据，旧的数据包没有这个设备，是新来的

    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        QString uid = it.key();
        if (!incomingIds.contains(uid)) { // 这次数据包没有
            staleIds.insert(uid);
        }
    }

    for (auto it = incoming.begin(); it != incoming.end(); ++it) {
        QString id = it.key();
        if (!m_allDevices.contains(id)) {
            newIds.insert(id);  // 新来到的设备，旧数据没有新数据有
        } else {
            // 比较指纹，看是否发生变化
            if (deviceFingerprint(incoming[id]) != deviceFingerprint(m_allDevices[id])) {
                updatedIds.append(id);
            }
        }
    }

    // 3. 如果没有任何变化，直接返回
    if (staleIds.isEmpty() && updatedIds.isEmpty() && newIds.isEmpty()) {
        // 连 counts 都不用刷新
        return;
    }

    // 4. 构建 display 中旧 id → 行号的映射
    QHash<QString, int> displayPos;
    for (int i = 0; i < m_displayUIds.size(); ++i)
        displayPos[m_displayUIds[i]] = i;

    // 5. 处理 stale（从后往前删，保持行号有效）
    QVector<int> staleRows;
    for (QString uid : staleIds) { // 将断开连接的设备加入staleRows里面
        if (displayPos.contains(uid))
            staleRows.append(displayPos[uid]);
        // 同时从全量数据中移除，否则 recalcCountsAndGroups 仍会计入已断开的设备
        m_allDevices.remove(uid);
    }
    std::sort(staleRows.begin(), staleRows.end(), std::greater<int>()); // 降序排列，从后到前删除，不然会乱序
    for (int row : staleRows) {
        beginRemoveRows(QModelIndex(), row, row);
        m_displayUIds.removeAt(row);
        endRemoveRows();
    }

    // 6. 处理 updated（增量合并——只覆盖服务器返回的字段，本地已有字段保留）
    QVector<int> updatedRows;
    for (QString uid : updatedIds) {
        // 合并而非替换：服务器可能只返回部分字段
        QJsonObject &existing = m_allDevices[uid];
        const QJsonObject &incomingObj = incoming[uid];
        for (auto it = incomingObj.begin(); it != incomingObj.end(); ++it) {
            existing[it.key()] = it.value();
        }
        if (displayPos.contains(uid))
            updatedRows.append(displayPos[uid]);
    }
    // 由于删除了 stale 行，更新行号可能偏移 — 但 updated 和 stale 不应该重叠
    // 安全起见：重新计算 updated 在 m_displayIds 中的位置
    if (!updatedRows.isEmpty()) {
        // rebuild displayPos after removals
        displayPos.clear();
        for (int i = 0; i < m_displayUIds.size(); ++i)
            displayPos[m_displayUIds[i]] = i;

        QVector<int> topLeft, bottomRight;
        for (QString uid : updatedIds) {
            if (displayPos.contains(uid)) {
                int row = displayPos[uid];
                topLeft.append(row);
                bottomRight.append(row);
            }
        }
        if (!topLeft.isEmpty()) {
            // 排序并合并连续区间
            std::sort(topLeft.begin(), topLeft.end());
            std::sort(bottomRight.begin(), bottomRight.end());
            // 分段 emit dataChanged
            if (topLeft.size() == 1) {
                emit dataChanged(index(topLeft[0]), index(bottomRight[0]));
            } else {
                // 合并成 [min, max]
                emit dataChanged(index(topLeft.first()), index(bottomRight.last()));
            }
        }
    }

    // 7. 处理 new（追加到末尾，如果匹配筛选条件）
    QVector<QString> sortedNewUIds = newIds.values();
    std::sort(sortedNewUIds.begin(), sortedNewUIds.end());
    for (QString uid : sortedNewUIds) {
        m_allDevices[uid] = incoming[uid];
        if (deviceMatchesFilter(incoming[uid], m_filterGroup, m_searchText)) {
            int pos = m_displayUIds.size();
            beginInsertRows(QModelIndex(), pos, pos);
            m_displayUIds.append(uid);
            endInsertRows();
        }
    }

    // 8. 刷新统计 & 分组
    recalcCountsAndGroups();
}

void DeviceListModel::clear()
{
    beginResetModel();
    m_allDevices.clear();
    m_displayUIds.clear();
    endResetModel();
    recalcCountsAndGroups();
}

// ═══════════════════════════════════════════
// 分组过滤 — 切换筛选时全量重建（可接受）
// ═══════════════════════════════════════════
void DeviceListModel::setFilterGroup(const QString &group)
{
    if (m_filterGroup == group) return;
    m_filterGroup = group;
    rebuildDisplayFromAll();
    emit filterGroupChanged();
}

bool DeviceListModel::isUniqueInGroup(const QString &group, const QString &name, const QString& excludeId)
{
    if (name.isEmpty() || group.isEmpty()) {
        qDebug() << "[client]: param error";
        return false;
    }
    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        if (it.key() == excludeId) continue;  // 编辑自己时不检查
        QJsonObject obj = it.value();
        if (obj["group"].toString() == group && obj["name"].toString() == name) {
            return false;
        }
    }
    return true;
}

void DeviceListModel::updateDeviceLocal(const QString &uid, const QString &name, const QString &group)
{
    if (!m_allDevices.contains(uid)) return;

    // 更新全量数据
    m_allDevices[uid]["name"] = name;
    m_allDevices[uid]["group"] = group;

    // 找到在 display 中的位置 → dataChanged
    for (int row = 0; row < m_displayUIds.size(); ++row) {
        if (m_displayUIds[row] == uid) {
            emit dataChanged(index(row), index(row));
            break;
        }
    }

    // 如果当前筛选组变了，该设备可能不再匹配 → 重建显示列表
    if (!m_filterGroup.isEmpty() && group != m_filterGroup) {
        rebuildDisplayFromAll();
    }

    recalcCountsAndGroups();
}

void DeviceListModel::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    rebuildDisplayFromAll();
    emit searchTextChanged();
}

void DeviceListModel::rebuildDisplayFromAll()
{
    beginResetModel();

    m_displayUIds.clear();
    // 按 id 排序保证显示稳定
    QVector<QString> allIds = m_allDevices.keys().toVector();
    std::sort(allIds.begin(), allIds.end());

    for (QString id : allIds) {
        if (deviceMatchesFilter(m_allDevices[id], m_filterGroup, m_searchText))
            m_displayUIds.append(id);
    }

    endResetModel();
}

// ═══════════════════════════════════════════
// 分组信息
// ═══════════════════════════════════════════

QStringList DeviceListModel::groupNames() const
{
    QSet<QString> set;
    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        QString g = it.value()["group"].toString();
        if (!g.isEmpty()) set.insert(g);
    }

    QStringList list = set.values();
    std::sort(list.begin(), list.end());
    return list;
}

int DeviceListModel::groupDeviceCount(const QString &group) const
{
    if (group.isEmpty()) return m_allDevices.size();

    int cnt = 0;
    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        if (it.value()["group"].toString() == group) cnt++;
    }
    return cnt;
}

int DeviceListModel::onlineCountInGroup(const QString &group) const
{
    int cnt = 0;
    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        if (!group.isEmpty() && it.value()["group"].toString() != group)
            continue;
        if (statusFromData(it.value()) == QStringLiteral("offline"))
            continue;
        cnt++;
    }
    return cnt;
}

int DeviceListModel::onlineDeviceCount() const
{
    return onlineCountInGroup(QString());
}

QVariantList DeviceListModel::onlineDeviceList() const
{
    QVariantList list;
    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        if (statusFromData(it.value()) == QStringLiteral("offline"))
            continue;

        const QJsonObject &dev = it.value();
        QVariantMap item;
        item[QStringLiteral("deviceUId")]     = dev[QStringLiteral("device_uid")].toString();
        item[QStringLiteral("deviceName")]    = dev[QStringLiteral("name")].toString();
        item[QStringLiteral("deviceGroup")]   = dev[QStringLiteral("group")].toString();
        item[QStringLiteral("deviceVersion")] = dev[QStringLiteral("version")].toString();
        item[QStringLiteral("status")]        = statusFromData(dev);
        list.append(item);
    }
    return list;
}

QString DeviceListModel::deviceNameByUid(const QString &uid) const
{
    auto it = m_allDevices.find(uid);
    if (it == m_allDevices.end())
        return uid;
    const QString name = it.value()[QStringLiteral("name")].toString();
    return name.isEmpty() ? uid : name;
}

// ═══════════════════════════════════════════
// 统计
// ═══════════════════════════════════════════

void DeviceListModel::recalcCountsAndGroups()
{
    int online = 0, warning = 0, offline = 0;
    qDebug() << "[client]: recalc counts and groups";

    for (auto it = m_allDevices.begin(); it != m_allDevices.end(); ++it) {
        QString s = statusFromData(it.value());
        if (s == "warning")       warning++;
        else if (s == "offline")  offline++;
        else                      online++;
    }

    bool changed = (m_onlineCount != online || m_warningCount != warning ||
                    m_offlineCount != offline || m_totalCount != m_allDevices.size());

    m_onlineCount  = online;
    m_warningCount = warning;
    m_offlineCount = offline;
    m_totalCount   = m_allDevices.size();

    if (changed) emit countsChanged();
    emit groupsChanged();
}

// ═══════════════════════════════════════════
// 状态判定
// ═══════════════════════════════════════════

QString DeviceListModel::statusFromData(const QJsonObject &dev)
{
    // 优先检查服务器的 online 标记（false = 离线设备，id = -1）
    if (!dev["online"].toBool(true))
        return QStringLiteral("offline");

    double temp = dev["temperature"].toString().toDouble();
    int    mem  = dev["mem_usage"].toInt();

    if (temp > 70.0 || mem > 60)
        return QStringLiteral("warning");
    return QStringLiteral("online");
}

QString DeviceListModel::statusColorFromStatus(const QString &status)
{
    if (status == "warning")  return QStringLiteral("#eab308");
    if (status == "offline")  return QStringLiteral("#ef4444");
    return QStringLiteral("#22c55e");
}

QString DeviceListModel::statusTextFromStatus(const QString &status)
{
    if (status == "warning")  return QStringLiteral("告警");
    if (status == "offline")  return QStringLiteral("离线");
    return QStringLiteral("在线");
}
