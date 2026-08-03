#ifndef EPOLLMGR_H
#define EPOLLMGR_H
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "global_def.h"
#include <errno.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <map>

// ── Per-FD 读缓冲区（解决逐字节 recv + 半包丢弃）──
struct FdBuffer {
    char data[8192];
    int  len;

    FdBuffer() : len(0) {}

    int append(const char* src, int srcLen) {
        if (len + srcLen > (int)sizeof(data)) {
            len = 0;
            return -1;
        }
        memcpy(data + len, src, srcLen);
        len += srcLen;
        data[len] = 0;
        return len;
    }

    void consume(int consumed) {
        if (consumed >= len) {
            len = 0;
        } else {
            memmove(data, data + consumed, len - consumed);
            len -= consumed;
        }
    }
};

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
    
    int getEpollFd() const { return m_epollFd; }

    void handleNewClient(int socketFd);
    void handleClientMessage(int clientFd);

private: 
    EpollMgr() = default;
    ~EpollMgr();

    int m_epollFd;

    std::map<int, FdBuffer> m_fd2BufferMap; // Map to store buffers for each file descriptor
};

#endif 