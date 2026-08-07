#ifndef DEVICEMGR_H
#define DEVICEMGR_H

#include <string>
#include <sys/types.h>
#include <cstdint>
#include <map>
#include "json_bags.h"
#include "global_def.h"
#include <vector>
#include "epollmgr.h"

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

class DeviceMgr {
public: 
    DeviceMgr(const DeviceMgr&) = delete;
    void operator=(const DeviceMgr& ) = delete;

    static DeviceMgr& getInstance() {
        static DeviceMgr instance;
        return instance;
    }

    void updateOnlineEmbeddedInfo(int fd, EmbeddedHeartbeatBag bag);
    void addOnlineEmbeddedInfo(int fd, OnlineClientInfo info);

    bool isClientExists(int clientFd);
    OnlineClientInfo* getClient(int clientFd);

    void deleteOutlineDevice();
    void removeClient(int clientFd);

    void loadAllDeviceInfo(std::vector<DeviceEntry>& devices);

    int getEmbeddedFdByUid(const std::string& uid) const;
    void updateOnlineEmbeddedMeta(int fd, const std::string& name, const std::string& group);
    
private:
    DeviceMgr() = default;
    ~DeviceMgr() = default;
    std::map<int, OnlineClientInfo> m_fd2ClientInfoMap;
};


#endif 