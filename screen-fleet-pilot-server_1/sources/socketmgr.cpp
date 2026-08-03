#include "../includes/socketmgr.h"
#include "logmgr.h"

static LogMgr* logger = &LogMgr::getInstance();

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