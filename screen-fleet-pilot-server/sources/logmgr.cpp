#include "../includes/logmgr.h"

LogMgr::LogMgr() {

}

LogMgr::~LogMgr() {

}

LogMgr& LogMgr::getInstance() {
    static LogMgr instance;
    return instance;
}

void LogMgr::logInit(LEVEL beginLevel) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_level = beginLevel;
    char level[10] = {0};
    if (m_level == DEBUG) {
        strcpy(level, "DEBUG");
    } else if (m_level == WARNING) {
        strcpy(level, "WARNING");
    } else if (m_level == ERROR) {
        strcpy(level, "ERROR");
    }
    printf("Log init success, level: %s\n", level);
}

void LogMgr::logMsg(LEVEL level, const char* message, bool isEndline) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (level < m_level) {
        return ;
    }
    char* head = (char*)malloc(sizeof(char) * 20);
    if (level == DEBUG) {
        strcpy(head, "[DEBUG]:\t\t");
    } else if (level == WARNING) {
        strcpy(head, "[WARNING]:\t");
    } else if (level == ERROR) {
        strcpy(head, "[ERROR]:\t\t");
    }
    write(STDOUT_FILENO, head, strlen(head));
    free(head);
    head = NULL;
    write(STDOUT_FILENO, message, strlen(message));
    if (isEndline) {
        write(STDOUT_FILENO, "\n", 1);
    }
}

void LogMgr::logMsg(LEVEL level, const std::string& message, bool isEndline) {
std::lock_guard<std::mutex> lock(m_logMutex);
    if (level < m_level) {
        return ;
    }
    char* head = (char*)malloc(sizeof(char) * 20);
    if (level == DEBUG) {
        strcpy(head, "[DEBUG]:\t\t");
    } else if (level == WARNING) {
        strcpy(head, "[WARNING]:\t");
    } else if (level == ERROR) {
        strcpy(head, "[ERROR]:\t\t");
    }
    write(STDOUT_FILENO, head, strlen(head));
    free(head);
    head = NULL;
    write(STDOUT_FILENO, message.c_str(), message.length());
    if (isEndline) {
        write(STDOUT_FILENO, "\n", 1);
    }
}