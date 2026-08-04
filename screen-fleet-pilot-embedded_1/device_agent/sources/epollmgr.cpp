#include "../includes/epollmgr.h"
#include "../includes/client.h"
#include "logmgr.h"
#include <cerrno>
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
                    return;
                }
            }
        }
    }
}

bool EpollMgr::handleServerMessage(int serverFd) {
    char buffer[4096];
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

    logger->logMsg(DEBUG, "received data from server, bytes: "
        + std::to_string(bytesRead), true);
    return true;
}

void EpollMgr::closeServerFd(int serverFd) {
    epoll_ctl(m_epollFd, EPOLL_CTL_DEL, serverFd, nullptr);
    close(serverFd);
}
