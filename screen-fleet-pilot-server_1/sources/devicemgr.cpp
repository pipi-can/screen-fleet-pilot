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

void DeviceMgr::updateOnlineClientInfo(int fd, ClientHeartbeatBag bag) {
    if (bag.checkValid() == false) {
        logger->logMsg(ERROR, "client heartbeat bag invalid", true);
        return ;
    }

    if (isClientExists(fd)) {
        OnlineClientInfo* clientInfo = getClient(fd);
        if (clientInfo->type != ClientType::Client) {
            logger->logMsg(ERROR, "client heartbeat fd is not client type", true);
            return ;
        }
        clientInfo->lastHeartbeatTimestamp = time(nullptr);
    } else {
        logger->logMsg(ERROR, "client heartbeat fd not exists", true);
    }
}

void DeviceMgr::addOnlineEmbeddedInfo(int fd, OnlineClientInfo info) {
    m_fd2ClientInfoMap.insert_or_assign(fd, std::move(info));
}

void DeviceMgr::addOnlineClientInfo(int fd, OnlineClientInfo info) {
    m_fd2ClientInfoMap.insert_or_assign(fd, std::move(info));
}

void DeviceMgr::kickOtherFdByUid(const std::string& uid, ClientType type, int keepFd) {
    if (uid.empty()) {
        return;
    }

    std::vector<int> kickFdList;
    for (const auto& pair : m_fd2ClientInfoMap) {
        if (pair.first != keepFd
            && pair.second.type == type
            && pair.second.deviceUid == uid) {
            kickFdList.emplace_back(pair.first);
        }
    }

    for (int fd : kickFdList) {
        EpollMgr::getInstance().recycleFd(fd);
    }
}

void DeviceMgr::loadClientMaskList(int clientFd) {
    OnlineClientInfo* info = getClient(clientFd);
    if (!info || info->type != ClientType::Client || info->deviceUid.empty()) {
        return;
    }

    info->maskedDeviceUids.clear();
    std::vector<std::string> maskedUids =
        DatabaseMgr::getInstance().queryMasksByClient(info->deviceUid);
    for (const std::string& uid : maskedUids) {
        info->maskedDeviceUids.insert(uid);
    }
}

void DeviceMgr::addClientMaskedDevice(int clientFd, const std::string& embeddedUid) {
    OnlineClientInfo* info = getClient(clientFd);
    if (!info || info->type != ClientType::Client || info->deviceUid.empty()) {
        return;
    }

    if (!DatabaseMgr::getInstance().insertMask(info->deviceUid, embeddedUid)) {
        return;
    }
    info->maskedDeviceUids.insert(embeddedUid);
}

bool DeviceMgr::isClientExists(int clientFd) {
    return this->m_fd2ClientInfoMap.count(clientFd) > 0 ? true : false;
}

bool DeviceMgr::isRegisteredClient(int clientFd) const {
    auto it = m_fd2ClientInfoMap.find(clientFd);
    return it != m_fd2ClientInfoMap.end() && it->second.type == ClientType::Client;
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

void DeviceMgr::loadAllDeviceInfo(std::vector<DeviceEntry>& devices, int clientFd) {
    devices.clear();

    const std::set<std::string>* maskedUids = nullptr;
    if (isRegisteredClient(clientFd)) {
        maskedUids = &m_fd2ClientInfoMap[clientFd].maskedDeviceUids;
    }

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
        if (maskedUids && maskedUids->count(rec.uid) > 0) {
            continue;
        }

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