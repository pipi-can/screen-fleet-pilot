#ifndef EPOLLMGR_H
#define EPOLLMGR_H

#include <cstdint>
#include <map>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include "global_def.h"
#include "fdbuffer.h"
#include "serverparser.h"

class EpollMgr {
public:
    EpollMgr(const EpollMgr& other) = delete;
    void operator=(const EpollMgr& other) = delete;

    static EpollMgr& getInstance() {
        static EpollMgr instance;
        return instance;
    }

    void init();
    void addFd(int fd, uint32_t events);
    static void setNonBlock(int fd);
    void wait();
    void handleNewClient(int socketFd);
    void handleClientMessage(int clientFd);
    void parseMessage(int clientFd, char* message);

    int getEpollFd() const { return m_epollFd; }


private: 
    EpollMgr() = default;
    ~EpollMgr();

    int m_epollFd;

    std::map<int, FdBuffer> m_fd2BufferMap; // Map to store buffers for each file descriptor
};

#endif 