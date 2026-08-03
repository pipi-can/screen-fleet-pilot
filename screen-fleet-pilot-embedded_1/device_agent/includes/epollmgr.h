#ifndef EPOLLMGR_H
#define EPOLLMGR_H

#include <sys/epoll.h>
#include "global_def.h"

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
    void wait();

private:
    EpollMgr() = default;
    ~EpollMgr();

    bool handleServerMessage(int serverFd);
    void closeServerFd(int serverFd);

    int m_epollFd = -1;
};

#endif
