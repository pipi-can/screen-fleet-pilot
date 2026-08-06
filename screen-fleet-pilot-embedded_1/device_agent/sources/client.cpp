#include "../includes/client.h"
#include "logmgr.h"
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>

static LogMgr* logger = &LogMgr::getInstance();


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

int Client::initHeartbeatTimer() {
    if (this->m_heartbeatFd != -1) return -1; // 意味着已经存在不需要初始化了额
    this->m_heartbeatFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_heartbeatFd < 0) {
        perror("\t\tcreate timerfd failed");
        return -2; // 意味着创建timerfd失败
    }
    EpollMgr::getInstance().addFd(m_heartbeatFd, EPOLLIN);
    logger->logMsg(DEBUG, "init heartbeat timer success", true);
    return 0; // 意味着初始化心跳定时器成功
}

int Client::startHeartbeatTimer() {
    if (this->m_heartbeatFd == -1)  return -1; // 意味着未初始化 
    
    if (this->m_heartbeatTimerStartFlag == true) return -2; // 意味着已经开始了，不用开始
    struct itimerspec its;
    its.it_value.tv_sec = HEARTBEAT_INTERVAL;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = HEARTBEAT_INTERVAL;
    its.it_interval.tv_nsec = 0;
    int ret = timerfd_settime(this->m_heartbeatFd, 0, &its, NULL);
    if (ret < 0) {
        perror("\t\tset time failed");
        close(m_heartbeatFd);
        m_heartbeatFd = -1;
        return -3; // 意味着设置时间失败了
    }
    this->m_heartbeatTimerStartFlag = true;
    return 0; // 成功
}

int Client::stopHeartbeatTimer() {
    if (this->m_heartbeatFd == -1) return -1; // 意味着未初始化
    if (this->m_heartbeatTimerStartFlag == false) return -2; // 未开始
    struct itimerspec its = {0};
    int ret = timerfd_settime(this->m_heartbeatFd, 0, &its, NULL);
    if (ret < 0) {
        perror("\t\tset time failed");
        close(m_heartbeatFd);
        m_heartbeatFd = -1;
        return -3; // 意味着设置时间失败了
    }
    this->m_heartbeatTimerStartFlag = false;
    return 0; // 成功
}

void Client::requestRegisterToServer() {
    if (m_registered) return ;
    RegisterBag registerBag;
    registerBag.deviceUid   = m_metaMessage.deviceUid;
    registerBag.name        = m_metaMessage.deviceName;
    registerBag.group       = m_metaMessage.deviceGroup;
    registerBag.version     = m_metaMessage.deviceVersion;
    registerBag.seq         = ++m_seqToServer;
    char* jsonStr     = registerBag.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "failed to create register JSON", true);
        return;
    }
    if (!sendMessageToServer(jsonStr)) {
        logger->logMsg(ERROR, "failed to send register message to server", true);
    } else {
        logger->logMsg(DEBUG, "register message sent to server: " + std::string(jsonStr), true);
    }
    free(jsonStr);
}

void Client::sendHeartbeatBagToServer() {
    if (!m_registered || m_socketFd < 0) {
        return;
    }

    EmbeddedHeartbeatBag bag;
    bag.seq              = ++m_seqToServer;
    bag.cpuTemp          = getCpuTemp();
    bag.memUsage         = getMemUsage();
    bag.diskFreeMb       = getDiskFreeMb();

    char* jsonStr = bag.toJsonString();
    if (!jsonStr) {
        logger->logMsg(ERROR, "failed to create heartbeat JSON", true);
        return;
    }
    if (!sendMessageToServer(jsonStr)) {
        logger->logMsg(ERROR, "failed to send heartbeat message to server", true);
    } else {
        logger->logMsg(DEBUG,
            "heartbeat sent, cpu:" + bag.cpuTemp
            + " mem:" + std::to_string(bag.memUsage) + "%", true);
    }
    free(jsonStr);
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
    delete[] msgWithNewline;

    logger->logMsg(DEBUG, "sent message to server: " + std::string(message), true);
    return true;
}

std::string Client::getCpuTemp() {
    const char* path = m_metaMessage.cpuTempPath.empty()
        ? "/sys/class/thermal/thermal_zone0/temp"
        : m_metaMessage.cpuTempPath.c_str();

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return "-1";
    }

    char buf[256];
    memset(buf, 0, sizeof(buf));
    ssize_t ret = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (ret <= 0) {
        return "-1";
    }
    buf[ret] = '\0';
    if (buf[0] == '\0') {
        return "-1";
    }

    return std::to_string(atoi(buf) / 1000);
}

int Client::getMemUsage() {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char data[1024];
    memset(data, 0, sizeof(data));
    ssize_t ret = read(fd, data, sizeof(data) - 1);
    close(fd);
    if (ret < 0) {
        return -1;
    }
    data[ret] = '\0';

    long total = 0;
    long available = 0;
    char* line = data;
    while (line && *line) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %ld kB", &available);
        }
        char* nl = strchr(line, '\n');
        line = nl ? nl + 1 : nullptr;
    }

    if (total == 0) {
        return -1;
    }
    return static_cast<int>(100 - (available * 100 / total));
}

int Client::getDiskFreeMb() {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        return -1;
    }
    return static_cast<int>((stat.f_bavail * stat.f_frsize) / (1024 * 1024));
}