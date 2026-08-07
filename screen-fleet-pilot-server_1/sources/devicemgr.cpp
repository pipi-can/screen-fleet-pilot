#include "../includes/devicemgr.h"
#include "logmgr.h"
#include "databasemgr.h"
#include <unordered_map>

static LogMgr* logger = &LogMgr::getInstance();

void DeviceMgr::updateOnlineEmbeddedInfo(int fd, EmbeddedHeartbeatBag bag) {
    if (bag.checkValid() == false) {
        logger->logMsg(ERROR, "embedded heartbeat bag invalid", true);
        return ;
    }

    if (isClientExists(fd)) {
        OnlineClientInfo* clientInfo = getClient(fd);
        clientInfo->lastCpuTemp = std::move(bag.cpuTemp);
        clientInfo->lastMemUsage = bag.memUsage;
        clientInfo->lastDiskFreeMb = bag.diskFreeMb;
        clientInfo->lastHeartbeatTimestamp = time(nullptr);
    } else {
        logger->logMsg(ERROR, "embedded heartbeat fd not exists", true);
    }
}

void DeviceMgr::addOnlineEmbeddedInfo(int fd, OnlineClientInfo info) {
    this->m_fd2ClientInfoMap.emplace(fd, std::move(info));
}

bool DeviceMgr::isClientExists(int clientFd) {
    return this->m_fd2ClientInfoMap.count(clientFd) > 0 ? true : false;
}

OnlineClientInfo* DeviceMgr::getClient(int clientFd) {
    // 返回非nullptr的内存是可以直接操作的，而不需要去再次赋值
    return isClientExists(clientFd) ? &m_fd2ClientInfoMap[clientFd] : nullptr;
}

void DeviceMgr::deleteOutlineDevice() {
    uint64_t current = time(NULL);
    std::vector<int> deleteFdList;
    for (auto it = m_fd2ClientInfoMap.begin(); it != m_fd2ClientInfoMap.end(); it++) {
        if (current - it->second.lastHeartbeatTimestamp >= OUTLINE_TOLERANCE) {
            deleteFdList.emplace_back(it->first);
        }
    }
    for (auto& fd: deleteFdList) {
        removeClient(fd);
        EpollMgr::getInstance().removeFd(fd);
    }
}

void DeviceMgr::removeClient(int clientFd) {
    this->m_fd2ClientInfoMap.erase(clientFd);
}

void DeviceMgr::loadAllDeviceInfo(std::vector<DeviceEntry>& devices) {
    devices.clear();

    std::unordered_map<std::string, const OnlineClientInfo*> uid2Online;
    for (const auto& pair : m_fd2ClientInfoMap) {
        if (pair.second.type != ClientType::Embedded || pair.second.deviceUid.empty()) {
            continue;
        }
        uid2Online[pair.second.deviceUid] = &pair.second;
    }

    std::vector<DeviceRecord> allDevices =
        DatabaseMgr::getInstance().queryAllByType("embedded");

    for (const auto& rec : allDevices) {
        DeviceEntry entry;
        entry.deviceUid = rec.uid;
        entry.name      = rec.name;
        entry.group     = rec.group;

        auto onlineIt = uid2Online.find(rec.uid);
        if (onlineIt != uid2Online.end()) {
            const OnlineClientInfo* info = onlineIt->second;
            entry.version     = info->version;
            entry.temperature = info->lastCpuTemp;
            entry.memUsage    = info->lastMemUsage;
            entry.diskFreeMb  = info->lastDiskFreeMb;
            entry.timestamp   = info->lastHeartbeatTimestamp;
            entry.online      = true;
        }

        devices.push_back(std::move(entry));
    }
}

int DeviceMgr::getEmbeddedFdByUid(const std::string& uid) const {
    for (const auto& pair : m_fd2ClientInfoMap) {
        if (pair.second.type == ClientType::Embedded && pair.second.deviceUid == uid) {
            return pair.first;
        }
    }
    return -1;
}

void DeviceMgr::updateOnlineEmbeddedMeta(int fd, const std::string& name, const std::string& group) {
    OnlineClientInfo* info = getClient(fd);
    if (!info) {
        return;
    }
    info->name  = name;
    info->group = group;
}