#include "../includes/epollmgr.h"
#include "../includes/client.h"
#include "../includes/embeddedparser.h"
#include "logmgr.h"
#include "jsonparser.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/socket.h>

static LogMgr* logger = &LogMgr::getInstance();

EpollMgr::~EpollMgr() {
    if (m_epollFd >= 0) {
        close(m_epollFd);
    }
}

void EpollMgr::init() {
    m_epollFd = epoll_create1(0);
    if (m_epollFd == -1) {
        logger->logMsg(ERROR, "epoll_create1 failed", true);
        perror("\t\tepoll_create1 error");
        return;
    }
    logger->logMsg(DEBUG, "epoll_create1 success", true);
}

void EpollMgr::addFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        logger->logMsg(ERROR, "epoll_ctl add fd failed", true);
        perror("\t\tepoll_ctl error");
        return;
    }
    logger->logMsg(DEBUG, "epoll_ctl add fd success", true);
}

void EpollMgr::wait() {
    struct epoll_event events[16];

    while (true) {
        int readyFds = epoll_wait(m_epollFd, events, 16, -1);
        if (readyFds == -1) {
            if (errno == EINTR) {
                continue;
            }
            logger->logMsg(ERROR, "epoll_wait failed", true);
            perror("\t\tepoll_wait error");
            break;
        }

        for (int i = 0; i < readyFds; i++) {
            int fd = events[i].data.fd;
            if (fd == Client::getInstance().getSocketFd()) {
                if (!handleServerMessage(fd)) {
                    logger->logMsg(ERROR, "handle server message failed", true);
                }
            } else if (fd == Client::getInstance().getHeartbeatTimerFd()) {

            }
        }
    }
}

bool EpollMgr::handleServerMessage(int serverFd) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesRead = recv(serverFd, buffer, sizeof(buffer), 0);
    if (bytesRead < 0) {
        logger->logMsg(ERROR, "recv from server failed", true);
        perror("\t\trecv error");
        closeServerFd(serverFd);
        return false;
    }
    if (bytesRead == 0) {
        logger->logMsg(DEBUG, "server closed connection", true);
        closeServerFd(serverFd);
        return false;
    }

    FdBuffer& fdbuf = m_fd2BufferMap[serverFd];
    if (fdbuf.append(buffer, static_cast<int>(bytesRead)) < 0) {
        logger->logMsg(ERROR, "server message buffer overflow", true);
        return false;
    }

    while (true) {
        char* nl = static_cast<char*>(memchr(fdbuf.data, '\n', fdbuf.len));
        if (!nl) {
            break;
        }
        int frameLen = nl - fdbuf.data;
        if (frameLen > 0 && fdbuf.data[frameLen - 1] == '\r') {
            frameLen--;
        }
        if (frameLen > 0) {
            fdbuf.data[frameLen] = '\0';
            logger->logMsg(DEBUG, fdbuf.data, true);
            parseMessage(serverFd, fdbuf.data);
        }
        fdbuf.consume(frameLen + 1);
    }
    return true;
}

void EpollMgr::handleHeartBeatTimer(int timerFd) {
    uint64_t exp;
    ssize_t readBytes = read(timerFd, &exp, sizeof(exp));
    if (readBytes < 0) {
        logger->logMsg(ERROR, "read heartbeat timer failed", true);
        perror("\t\tread heartbeat timer");
        return ;
    }
}

void EpollMgr::parseMessage(int serverFd, char* message) {
    JsonBagBasic basic = JsonParser::parseBasic(message);
    EmbeddedParser::getInstance().parseMessage(ParserContext(serverFd, basic, message));
}

void EpollMgr::closeServerFd(int serverFd) {
    epoll_ctl(m_epollFd, EPOLL_CTL_DEL, serverFd, nullptr);
    close(serverFd);
    m_fd2BufferMap.erase(serverFd);
}
