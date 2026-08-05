#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstring>
#include <cstdio>

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
}
#include "global_def.h"
#include "logmgr.h"
#include "json_bags.h"

struct ClientMetaMessage {
    std::string deviceName;
    std::string deviceGroup;
    std::string deviceVersion;
    std::string deviceUid;
    ClientMetaMessage(): deviceName(""), deviceGroup(""), deviceVersion(""), deviceUid("") {}
        
    int readFromMacIface(const char* iface, char* buf, int bufsize) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", iface);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            return -1;
        }

        char raw[32];
        memset(raw, 0, sizeof(raw));
        int ret = read(fd, raw, sizeof(raw) - 1);
        close(fd);
        if (ret <= 0) {
            return -1;
        }

        int j = 0;
        for (int i = 0; raw[i] && j < bufsize - 1; i++) {
            if (raw[i] == ':' || raw[i] == '\n' || raw[i] == '\r') {
                continue;
            }
            buf[j++] = raw[i];
        }
        buf[j] = '\0';
        return j > 0 ? 0 : -1;
    }
    std::string getMacAddress() {
        char buf[128];
        const char* ifaces[] = {"eth0", "eth1", "wlan0", NULL};

        memset(buf, 0, 128);
        for (int i = 0; ifaces[i]; i++) {
            if (readFromMacIface(ifaces[i], buf, 128) == 0) {
                return std::string(buf);
            }
        }
        return std::string();
    }
    void loadMetaMessage() {
        if (access(DEVICE_MESSAGE_PATH, F_OK) != 0) {
            // 创建文件写入默认值，默认用mac地址作为deviceUid和deviceName和deviceGroup，version默认1.0.0
            this->deviceUid     = getMacAddress();
            this->deviceName    = this->deviceUid;
            this->deviceGroup   = this->deviceUid;
            this->deviceVersion = "1.0.0";
            writeMetaMessageToFile();
        } else {
            // 使用系统调用读取设备信息
            char buf[256] = {0};
            int fp = open(DEVICE_MESSAGE_PATH, O_RDONLY);
            if (fp < 0) {
                LogMgr::getInstance().logMsg(ERROR, "open device message file failed", true);
                perror("\t\topen error");
                return;
            }
            ssize_t bytesRead = read(fp, buf, sizeof(buf) - 1);
            if (bytesRead < 0) {
                LogMgr::getInstance().logMsg(ERROR, "read device message file failed", true);
                perror("\t\tread error");
                close(fp);
                return;
            }
            buf[bytesRead] = '\0';
            // 解析 JSON
            struct json_object* root = json_tokener_parse(buf);
            if (!root) {
                LogMgr::getInstance().logMsg(ERROR, "parse device message JSON failed", true);
                close(fp);
                return;
            }
            struct json_object* nameObj = nullptr;
            struct json_object* groupObj = nullptr;
            struct json_object* versionObj = nullptr;
            struct json_object* uidObj = nullptr;
            // 开始解析所有字段
            if (json_object_object_get_ex(root, "name", &nameObj) && json_object_is_type(nameObj, json_type_string)) {
                deviceName = json_object_get_string(nameObj);
            }
            if (json_object_object_get_ex(root, "group", &groupObj) && json_object_is_type(groupObj, json_type_string)) {
                deviceGroup = json_object_get_string(groupObj);
            }
            if (json_object_object_get_ex(root, "version", &versionObj) && json_object_is_type(versionObj, json_type_string)) {
                deviceVersion = json_object_get_string(versionObj);
            }
            if (json_object_object_get_ex(root, "device_uid", &uidObj) && json_object_is_type(uidObj, json_type_string)) {
                deviceUid = json_object_get_string(uidObj);
            }
            // 释放资源
            json_object_put(root);
            close(fp);
        }
    }

    void writeMetaMessageToFile() {
        struct json_object *root = json_object_new_object();
        json_object_object_add(root, "device_uid", json_object_new_string(this->deviceUid.c_str()));
        json_object_object_add(root, "name", json_object_new_string(this->deviceName.c_str()));
        json_object_object_add(root, "group", json_object_new_string(this->deviceGroup.c_str()));
        json_object_object_add(root, "version", json_object_new_string(this->deviceVersion.c_str()));
        int fd = open(DEVICE_MESSAGE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            LogMgr::getInstance().logMsg(ERROR, "open device message file failed", true);
            perror("\t\topen error");
            return;
        }
        const char *jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        int len = strlen(jsonStr);
        if (write(fd, jsonStr, len) != len) {
            LogMgr::getInstance().logMsg(ERROR, "write device message file failed", true);
            perror("\t\twrite error");
        }
        json_object_put(root);
        close(fd);
    }
};

class Client {
public:
    Client(const Client& other) = delete;
    void operator=(const Client& other) = delete;

    static Client& getInstance() {
        static Client instance;
        return instance;
    }

    void connectToServer();

    void requestRegisterToServer();

    bool sendMessageToServer(const char* message);

    void setName(const std::string& name) { 
        m_metaMessage.deviceName = name;
        m_metaMessage.writeMetaMessageToFile();
    }
    void setGroup(const std::string& group) { 
        m_metaMessage.deviceGroup = group;
        m_metaMessage.writeMetaMessageToFile();
    }
    void setVersion(const std::string& version) { 
        m_metaMessage.deviceVersion = version;
        m_metaMessage.writeMetaMessageToFile();
    }

    int getSocketFd() const { return m_socketFd; }
    std::string getName() const { return m_metaMessage.deviceName; }
    std::string getGroup() const { return m_metaMessage.deviceGroup; }
    std::string getVersion() const { return m_metaMessage.deviceVersion; }
    std::string getDeviceUid() const { return m_metaMessage.deviceUid; }

private:
    Client() {
        m_metaMessage.loadMetaMessage();
    }
    ~Client();

    int     m_socketFd = -1;
    bool    m_registered = false;
    int     m_seqToServer = 0;
    int     m_seqToQt     = 0;
    ClientMetaMessage m_metaMessage;
};

#endif
