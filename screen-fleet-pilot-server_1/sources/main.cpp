#include "../includes/main.h"
#include "../includes/databasemgr.h"
#include "../includes/schedulemgr.h"
#include "version.h"
#include <iostream>
#include <string>

int main()
{
    LogMgr::getInstance().logInit(DEBUG);

    const std::string versionMsg = std::string(PROJECT_NAME) + " v" + PROJECT_VERSION;
    LogMgr::getInstance().logMsg(DEBUG, versionMsg.c_str(), true);

    DatabaseMgr::getInstance().init();
    ScheduleMgr::getInstance().init();

    EpollMgr::getInstance().init();
    SocketMgr::getInstance().init();
    EpollMgr::getInstance().setNonBlock(SocketMgr::getInstance().getSocketFd());
    EpollMgr::getInstance().addFd(SocketMgr::getInstance().getSocketFd(), EPOLLET | EPOLLIN);

    int scheduleFd = ScheduleMgr::getInstance().timerFd();
    if (scheduleFd >= 0) {
        EpollMgr::getInstance().addFd(scheduleFd, EPOLLIN);
    }

    EpollMgr::getInstance().wait();

    return 0;
}
