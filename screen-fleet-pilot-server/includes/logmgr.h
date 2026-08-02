#ifndef __LOGMGR_H__
#define __LOGMGR_H__

#include <iostream>
#include <mutex>
extern "C" {
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>    
}

typedef enum {
    DEBUG, 
    WARNING, 
    ERROR
} LEVEL;

class LogMgr {
public: 

    LogMgr(const LogMgr& other) = delete;
    void operator=(const LogMgr& other) = delete;

    void logInit(LEVEL beginLevel);

    void logMsg(LEVEL level, const char* message, bool isEndline);

    void logMsg(LEVEL level, const std::string& message, bool isEndline);

    static LogMgr& getInstance();
private: 
    LogMgr();
    ~LogMgr();

    std::mutex m_logMutex;

    LEVEL m_level;

};

#endif