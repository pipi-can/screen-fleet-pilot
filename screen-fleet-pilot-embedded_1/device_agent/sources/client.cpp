#include "../includes/client.h"
#include "logmgr.h"
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <string>
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

void Client::requestRegisterToServer() {
    if (m_registered) return ;
    RegisterBag registerBag;
    registerBag.deviceUid   = m_metaMessage.deviceUid;
    registerBag.name        = m_metaMessage.deviceName;
    registerBag.group       = m_metaMessage.deviceGroup;
    registerBag.version     = m_metaMessage.deviceVersion;
    registerBag.seq         = ++m_seqToServer;
    const char* jsonStr     = registerBag.toJsonString();
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