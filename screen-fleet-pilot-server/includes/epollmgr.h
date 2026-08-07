#ifndef __EPOLLMGR_H__
#define __EPOLLMGR_H__

#include "logmgr.h"
#include "socketmgr.h"
#include <sys/epoll.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <queue>
#include <unordered_map>
#include <cstdint>

extern "C" {
#include <json-c/json.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
}
#include "socketmgr.h"

enum DeviceType {
    EMBEDDED = 1,
    CLIENT
};

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

class DeviceInfo {
public: 
    DeviceInfo();
    DeviceInfo(std::string name, std::string group, std::string version, DeviceType type);

    std::string name();
    std::string group();
    std::string version();
    time_t lastUploadTime();
    DeviceType type();
    std::string temperature();
    int memUsage();
    int diskFreeMb();
    std::string deviceUid();
    std::set<std::string> maskUidList();
    time_t deviceTimestamp();

    int id();

    void setId(int id);
    void setGroup(const char* group);
    void setName(const char* name);
    void updateLastUploadTime();

    void setTemperature(const std::string& temperature);
    void setMemUsage(int memUsage);
    void setDiskFreeMb(int diskFreeMb);
    void setDeviceUid(const std::string& uid);
    void setDeviceTimestamp(time_t ts);
    
    void addAdvToList(const std::string& adv);
    void addAdvsToList(std::vector<std::string> advs);

    void addMask(const std::string& uid);
private: 

    int m_id;
    std::string m_name;
    std::string m_group;
    std::string m_version;
    DeviceType  m_type;

    std::string m_temperature;
    int m_memUsage;
    int m_diskFreeMb;
    std::string m_deviceUid;

    std::vector<std::string> m_advList;
    std::set<std::string> m_maskUidList;

    time_t m_lastUploadTime;
    time_t m_deviceTimestamp;
};

struct FileEntry {
    std::string filePath;
    std::string fileName;
    int fileSize;
};

struct FirmwareFileInfo {
    std::string name;
    long long size;
};

struct FirmwareEntry {
    std::string filePath;
    std::string fileName;
    long long fileSize;
    std::string version;
    std::string packTime;
    std::string changelog;
    std::vector<std::string> executables;
    std::vector<FirmwareFileInfo> files;
};

class EpollManager {
public: 

    EpollManager(const EpollManager& other) = delete;
    void operator=(const EpollManager& other) = delete;

    static EpollManager& getInstance();

    void init();

    void add(int fd, uint32_t events);

    int getEpollFd();

    void wait();

    void sendJson(int fd, struct json_object* obj);

    void parseMessage(int fd, char* message);

    void handleEmbeddedMessage(int fd, struct json_object* root);

    void handleClientMessage(int fd, struct json_object* root);

    void handleNewClient();

    void recycleClient(int fd);

    void handleEmbeddedRegister(int fd, struct json_object* paramsObj);

    void handleEmbeddedHeartbeat(int fd, struct json_object* root, struct json_object* paramsObj);

    void handleEmbeddedUpdateInfoAck(int fd, struct json_object* paramsObj);

    void handleEmbeddedScreenshotData(int fd, struct json_object* paramsObj);

    void handleEmbeddedOtaUpdateAck(int fd, struct json_object* paramsObj);

    void handleClientRegister(int fd, struct json_object* paramsObj);

    void handleClientFetchDevices(int fd);

    void handleClientHeartbeat(int fd, struct json_object* root);

    void handleClientRequestUpdateEmbedded(int fd, struct json_object* paramsObj);

    void handleClientRequestFileList(int fd, struct json_object* paramsObj);

    void handleClientRequestFirmwareList(int fd, struct json_object* paramsObj);

    void handleClientMaskDevice(int fd, struct json_object* paramsObj);

    void handleClientRequestPushContentToEmbedded(int fd, struct json_object* paramsObj);

    void handleClientRequestScreenshot(int fd, struct json_object* paramsObj);

    void handleClientRequestSchedulePush(int fd, struct json_object* paramsObj);

    void handleClientRequestOTAUpdate(int fd, struct json_object* paramsObj);

    void handleClientRequestCheckFirmware(int fd, struct json_object* paramsObj);

    bool pushResourcesToEmbedded(const std::string& deviceUid,
                                 const std::vector<std::string>& relativePaths);

    void checkTimeout();
private: 

    EpollManager();
    ~EpollManager();

    int m_epollFd;

    int m_deviceCnt;

    std::unordered_map<int, DeviceInfo> m_fd2DeviceMap;

    std::unordered_map<int, int> m_id2fdMap;
    std::unordered_map<std::string, int> m_uid2IdMap;

    std::unordered_map<int, FdBuffer> m_fd2Buffer;

   
};

#endif 
