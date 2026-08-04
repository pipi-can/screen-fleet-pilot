#include "../includes/client.h"
#include "logmgr.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>

static LogMgr* logger = &LogMgr::getInstance();

struct ClientMetaMessage {
    std::string deviceName;
    std::string deviceGroup;
    std::string deviceVersion;
    std::string deviceUid;
    ClientMetaMessage(): deviceName(""), deviceGroup(""), deviceVersion(""), deviceUid("") {}
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
                logger->logMsg(ERROR, "open device message file failed", true);
                perror("\t\topen error");
                return;
            }
            ssize_t bytesRead = read(fp, buf, sizeof(buf) - 1);
            if (bytesRead < 0) {
                logger->logMsg(ERROR, "read device message file failed", true);
                perror("\t\tread error");
                close(fp);
                return;
            }
            buf[bytesRead] = '\0';
            // 解析 JSON
            struct json_object* root = json_tokener_parse(buf);
            if (!root) {
                logger->logMsg(ERROR, "parse device message JSON failed", true);
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
            logger->logMsg(ERROR, "open device message file failed", true);
            perror("\t\topen error");
            return;
        }
        const char *jsonStr = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        int len = strlen(jsonStr);
        if (write(fd, jsonStr, len) != len) {
            logger->logMsg(ERROR, "write device message file failed", true);
            perror("\t\twrite error");
        }
        json_object_put(root);
        close(fd);
    }
};

Client::~Client() {
    if (m_socketFd >= 0) {
        close(m_socketFd);
    }
}

void Client::connectToServer() {
    bool flag = false; // 链接标志
    int  time = MAX_CONNECT_TIME;    // 链接最大次数
    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        logger->logMsg(ERROR, "socket creation failed", true);
        perror("\t\tsocket error");
        return;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_HOST, &serverAddr.sin_addr) <= 0) {
        logger->logMsg(ERROR, "invalid server address", true);
        close(m_socketFd);
        m_socketFd = -1;
        return;
    }

    while (time--) {
        if (connect(m_socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            logger->logMsg(ERROR, "connect to server failed, left time: " + std::to_string(time), true);
            perror("\t\tconnect error");
            sleep(5);
        } else {
            flag = true;
            break;
        }
    }

    if (!flag) {
        close(m_socketFd);
        m_socketFd = -1;
        return;
    }

    logger->logMsg(DEBUG, "connect to server success", true);
}

static int readFromMacIface(const char* iface, char* buf, int bufsize) {
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

void Client::requestRegisterToServer() {
    RegisterBag registerBag;
    registerBag.deviceUid = m_metaMessage.deviceUid;
    registerBag.name = m_metaMessage.deviceName;
    registerBag.group = m_metaMessage.deviceGroup;
    registerBag.version = m_metaMessage.deviceVersion;
    const char* jsonStr = registerBag.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "failed to create register JSON", true);
        return;
    }
    if (!sendMessageToServer(jsonStr)) {
        logger->logMsg(ERROR, "failed to send register message to server", true);
    } else {
        logger->logMsg(DEBUG, "register message sent to server: " + std::string(jsonStr), true);
    }
}

bool Client::sendMessageToServer(const char* message) {
    if (m_socketFd < 0) {
        logger->logMsg(ERROR, "socket is not connected", true);
        return false;
    }

    // 多发送一个换行符，作为粘包解决方案
    ssize_t totalSent = 0;
    char* msgWithNewline = new char[strlen(message) + 1]; memset(msgWithNewline, 0, strlen(message) + 1);
    strcpy(msgWithNewline, message);
    msgWithNewline[strlen(message)] = '\n';
    size_t messageLen = strlen(msgWithNewline);
    while (totalSent < messageLen) {
        ssize_t sent = send(m_socketFd, msgWithNewline + totalSent, messageLen - totalSent, MSG_NOSIGNAL);
        if (sent < 0) {
            logger->logMsg(ERROR, "send to server failed", true);
            perror("\t\tsend error");
            return false;
        }
        totalSent += sent;
    }
    free(msgWithNewline);

    logger->logMsg(DEBUG, "sent message to server: " + std::string(message), true);
    return true;
}