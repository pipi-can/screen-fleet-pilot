#include "../includes/main.h"
#include "version.h"
#include <iostream>
#include <string>

int main()
{
    /*
     * @brief: 初始化日志模块，并打印 CMake project() 注入的版本号
     */
    LogMgr::getInstance().logInit(DEBUG);

    const std::string versionMsg = std::string(PROJECT_NAME) + " v" + PROJECT_VERSION;
    LogMgr::getInstance().logMsg(DEBUG, versionMsg.c_str(), true);

    EpollMgr::getInstance().init();
    SocketMgr::getInstance().init();
    EpollMgr::getInstance().setNonBlock(SocketMgr::getInstance().getSocketFd());
    EpollMgr::getInstance().addFd(SocketMgr::getInstance().getSocketFd(), EPOLLET | EPOLLIN);

    EpollMgr::getInstance().wait();
    
    return 0;
}
