#ifndef SOCKETMGR_H
#define SOCKETMGR_H

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>    
#include <netinet/in.h>
}
#include <string>
#include "global_def.h"

class SocketMgr {
public:
    SocketMgr(const SocketMgr& other) = delete;
    void operator=(const SocketMgr& other) = delete;

    static SocketMgr& getInstance() {
        static SocketMgr instance;
        return instance;
    }

    void init();
    int getSocketFd() { return m_socketFd; }
    static bool sendMessage(int fd, const std::string& json);

private:
    SocketMgr() = default;
    ~SocketMgr();

    int m_socketFd = -1;
};

#endif