#ifndef SOCKETMGR_H
#define SOCKETMGR_H

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>  
#include <time.h>  
#include <netinet/in.h>
}
#include <map>
#include <string>
#include "json_bags.h"
#include "global_def.h"

enum ClientType {
    Client, 
    Embedded
};

struct OnlineClientInfo {
    ClientType  type;
    std::string name;
    std::string group;
    std::string version;
    std::string deviceUid;
    
    std::string lastCpuTemp;
    int         lastMemUsage;
    int         lastDiskFreeMb;

    uint64_t    lastHeartbeatTimestamp;
};

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

    void updateOnlineEmbeddedInfo(int fd, EmbeddedHeartbeatBag bag);
    void addOnlineEmbeddedInfo(int fd, OnlineClientInfo info);

    bool isClientExists(int clientFd);
    OnlineClientInfo* getClient(int clientFd);


private:
    SocketMgr() = default;
    ~SocketMgr();

    int m_socketFd = -1;

    std::map<int, OnlineClientInfo> m_fd2ClientInfoMap;
};

#endif