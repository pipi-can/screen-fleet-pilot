#include "../includes/epollmgr.h"
#include "logmgr.h"
#include "socketmgr.h"
#include <cstdio>
#include <cstring>

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
    } else {
        logger->logMsg(DEBUG, "epoll_create1 success", true);
    }
}

void EpollMgr::addFd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        logger->logMsg(ERROR, "epoll_ctl add fd failed", true);
        perror("\t\tepoll_ctl error");
    } else {
        logger->logMsg(DEBUG, "epoll_ctl add fd success", true);
    }
}

void EpollMgr::setNonBlock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        logger->logMsg(ERROR, "fcntl get flags failed", true);
        perror("\t\tfcntl get error");
        return;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        logger->logMsg(ERROR, "fcntl set non-blocking failed", true);
        perror("\t\tfcntl set error");
    } else {
        logger->logMsg(DEBUG, "fcntl set non-blocking success", true);
    }
}

void EpollMgr::wait() {
    const int MAX_EVENTS = MAX_CONNECTIONS;
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int readyFds = epoll_wait(m_epollFd, events, MAX_EVENTS, 1000);
        if (readyFds == -1) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            } else {
                logger->logMsg(ERROR, "epoll_wait failed", true);
                perror("\t\tepoll_wait error");
                break;
            }
        } else {
            logger->logMsg(DEBUG, "epoll_wait returned " + std::to_string(readyFds) + " ready fds", true);
            for (int i = 0; i < readyFds; i++) {
                struct epoll_event& ev = events[i];
                int fd = ev.data.fd;
                if (fd == -1) {
                    logger->logMsg(ERROR, "epoll_wait returned invalid fd", true);
                    continue;
                } else if (fd == SocketMgr::getInstance().getSocketFd()) {
                    // new client come 
                    handleNewClient(fd);
                } else {
                    // handle message sent by client
                    handleClientMessage(fd);
                }
            }
        }
    }
}
void EpollMgr::handleNewClient(int socketFd) {
    struct sockaddr_in clientInfo;
    while (true) {
        memset(&clientInfo, 0, sizeof(clientInfo));
        socklen_t sockLen = sizeof(clientInfo);
        int newClientFd = accept(socketFd, (struct sockaddr*)(&clientInfo), &sockLen);
        if (newClientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                logger->logMsg(ERROR, "accept new client failed", true);
                perror("\t\taccept error");
                return;
            }
        } else {
            logger->logMsg(DEBUG, "accept new client success, fd: " + std::to_string(newClientFd), true);
            setNonBlock(newClientFd); // must set nonblock when use et mode
            addFd(newClientFd, EPOLLIN | EPOLLET);
        }
    }
}

void EpollMgr::handleClientMessage(int clientFd) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    while (true) {
        ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // recv end 
                break;
            } else {
                logger->logMsg(ERROR, "recv error", true);
                perror("\t\trecv error");
                epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
                close(clientFd);
                break;
            }
        } else if (bytesRead == 0) {
            logger->logMsg(DEBUG, "client closed connection", true);
            epoll_ctl(m_epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
            close(clientFd);
            break;
        } else {
            FdBuffer& fdbuf = m_fd2BufferMap[clientFd];
            fdbuf.append(buffer, bytesRead);

            while (true) {
                char* nl = (char*)memchr(fdbuf.data, '\n', fdbuf.len);
                if (!nl) break;
                int frameLen = nl - fdbuf.data;
                if (frameLen > 0 && fdbuf.data[frameLen - 1] == '\r') {
                    frameLen--;  // 兼容 \r\n
                }
                if (frameLen > 0) {
                    fdbuf.data[frameLen] = '\0';
                    // 先只打日志，验证切帧对不对
                    logger->logMsg(DEBUG, fdbuf.data, true);
                    parseMessage(clientFd, fdbuf.data);
                }
                fdbuf.consume(frameLen + 1);
            }
        }
    }
}

void EpollMgr::parseMessage(int clientFd, char* message) {
    JsonBagBasic basic = JsonParser::parseBasic(message);
    ServerParser::getInstance().parseMessage(ParserContext(clientFd, basic, message));
}
