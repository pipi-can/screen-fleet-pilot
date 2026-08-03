#include "../includes/client.h"
#include "logmgr.h"
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>

static LogMgr* logger = &LogMgr::getInstance();

Client::~Client() {
    if (m_socketFd >= 0) {
        close(m_socketFd);
    }
}

void Client::connectToServer() {
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

    if (connect(m_socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        logger->logMsg(ERROR, "connect to server failed", true);
        perror("\t\tconnect error");
        close(m_socketFd);
        m_socketFd = -1;
        return;
    }

    logger->logMsg(DEBUG, "connect to server success", true);
}
