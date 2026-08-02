#ifndef __SOCKETMGR_H__
#define __SOCKETMGR_H__

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>

#include "logmgr.h"

#define IP      "8.136.113.168"
#define PORT    8000

#define MAX_CONNECTIONS 1024
class SocketMgr {
public: 
    SocketMgr(const SocketMgr& other) = delete;
    void operator=(const SocketMgr& other) = delete;

    static SocketMgr& getInstance();

    void init();

    int getSocketFd();
private: 
    
    SocketMgr();
    ~SocketMgr();

    int m_socketFd;
};

#endif