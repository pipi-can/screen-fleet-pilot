#include "../includes/socketmgr.h"
#include <fcntl.h>

static LogMgr* logger = &LogMgr::getInstance();

SocketMgr::SocketMgr() {

}

SocketMgr::~SocketMgr() {

}

SocketMgr& SocketMgr::getInstance() {
    static SocketMgr instance;
    return instance;
}

void SocketMgr::init() {
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        logger->logMsg(ERROR, "socket failed", true);
        return ;
    }

    int reuse = 1;
    setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in serverInfo;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(PORT);
    serverInfo.sin_family = AF_INET;
    socklen_t sockLen = sizeof(serverInfo);

    int ret = bind(m_socketFd, (struct sockaddr*)(&serverInfo), sockLen);
    if (ret < 0) {
        logger->logMsg(ERROR, "bind failed", true);
        return ;
    }

    ret = listen(m_socketFd, MAX_CONNECTIONS);
    if (ret < 0) {
        logger->logMsg(ERROR, "listen failed", true);
        return ;
    }

    logger->logMsg(DEBUG, "socket init success", true);
}

int SocketMgr::getSocketFd() {
    return m_socketFd;
}

bool SocketMgr::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}