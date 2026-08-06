#include "../includes/socketmgr.h"
#include "logmgr.h"
#include <cerrno>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>

static LogMgr* logger = &LogMgr::getInstance();

bool SocketMgr::sendMessage(int fd, const std::string& json) {
    if (fd < 0) {
        logger->logMsg(ERROR, "sendMessage: invalid fd", true);
        return false;
    }

    std::string packet = json;
    packet.push_back('\n');

    ssize_t totalSent = 0;
    const char* ptr = packet.c_str();
    const size_t packetLen = packet.size();
    while (totalSent < static_cast<ssize_t>(packetLen)) {
        ssize_t sent = send(fd, ptr + totalSent,
                            packetLen - totalSent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            logger->logMsg(ERROR, "sendMessage: send failed", true);
            perror("\t\tsend error");
            return false;
        }
        totalSent += sent;
    }
    return true;
}

SocketMgr::~SocketMgr() {
    if (m_socketFd >= 0) {
        close(m_socketFd);
    }
}

void SocketMgr::init() {
    // Implementation for initializing the socket
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        logger->logMsg(ERROR, "socket creation failed", true);
        perror("\t\tsocket error");
        return;
    } else {
        logger->logMsg(DEBUG, "socket creation success", true);
    }

    int reuse = 1;
    setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in serverInfo;
    serverInfo.sin_addr.s_addr  = INADDR_ANY;
    serverInfo.sin_port         = htons(PORT);
    serverInfo.sin_family       = AF_INET;
    socklen_t sockLen           = sizeof(serverInfo);

    int ret = bind(m_socketFd, (struct sockaddr*)(&serverInfo), sockLen);
    if (ret < 0) {
        logger->logMsg(ERROR, "bind failed", true);
        perror("\t\tbind error");
        close(m_socketFd);
        return;
    } else {
        logger->logMsg(DEBUG, "bind success", true);
    }

    ret = listen(m_socketFd, MAX_CONNECTIONS);
    if (ret < 0) {
        logger->logMsg(ERROR, "listen failed", true);
        perror("\t\tlisten error");
        close(m_socketFd);
        return;
    } else {
        logger->logMsg(DEBUG, "listen success", true);
    }

    logger->logMsg(DEBUG, "socket init success", true);
}

void SocketMgr::updateOnlineEmbeddedInfo(int fd, EmbeddedHeartbeatBag bag) {
    if (bag.checkValid() == false) {
        logger->logMsg(ERROR, "embedded heartbeat bag invalid", true);
        return ;
    }

    if (isClientExists(fd)) {
        OnlineClientInfo* clientInfo = getClient(fd);
        clientInfo->lastCpuTemp = std::move(bag.cpuTemp);
        clientInfo->lastMemUsage = bag.memUsage;
        clientInfo->lastDiskFreeMb = bag.diskFreeMb;
        clientInfo->lastHeartbeatTimeStamp = time(nullptr);
    } else {
        OnlineClientInfo clientInfo;
        clientInfo.type        = ClientType::Embedded;
        clientInfo.name        = std::move(bag.name);
        clientInfo.group       = std::move(bag.group);
        clientInfo.version     = std::move(bag.version);
        clientInfo.deviceUid   = std::move(bag.deviceUid);
        clientInfo.lastCpuTemp = std::move(bag.cpuTemp);
        clientInfo.lastMemUsage = bag.memUsage;
        clientInfo.lastDiskFreeMb = bag.diskFreeMb;
        clientInfo.lastHeartbeatTimeStamp = time(nullptr);
        this->addOnlineEmbeddedInfo(fd, clientInfo);
    }
}

void SocketMgr::addOnlineEmbeddedInfo(int fd, OnlineClientInfo info) {
    this->m_fd2ClientInfoMap.emplace(fd, std::move(info));
}

bool SocketMgr::isClientExists(int clientFd) {
    return this->m_fd2ClientInfoMap.contains(clientFd);
}

OnlineClientInfo* SocketMgr::getClient(int clientFd) {
    // 返回非nullptr的内存是可以直接操作的，而不需要去再次赋值
    return isClientExists(clientFd) ? &m_fd2ClientInfomap[clientFd] : nullptr;
}