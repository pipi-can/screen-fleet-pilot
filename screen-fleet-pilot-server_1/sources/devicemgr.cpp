#include "../includes/devicemgr.h"
#include "logmgr.h"

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
        OnlineClientInfo clientInfo;
        clientInfo.type        = ClientType::Embedded;
        clientInfo.lastCpuTemp = std::move(bag.cpuTemp);
        clientInfo.lastMemUsage = bag.memUsage;
        clientInfo.lastDiskFreeMb = bag.diskFreeMb;
        clientInfo.lastHeartbeatTimestamp = time(nullptr);
        this->addOnlineEmbeddedInfo(fd, clientInfo);
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
        if (current - it->second.lastHeartbeatTimestamp > OUTLINE_TOLERANCE) {
            deleteFdList.emplace_back(it->first);
        }
    }
    for (auto& fd: deleteFdList) {
        removeClient(fd);
        close(fd);
    }
}

void DeviceMgr::removeClient(int clientFd) {
    this->m_fd2ClientInfoMap.erase(clientFd);
}