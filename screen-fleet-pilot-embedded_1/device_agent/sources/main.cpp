#include "../includes/main.h"
#include "../includes/contentmgr.h"
#include "version.h"
#include <string>

int main()
{
    LogMgr::getInstance().logInit(DEBUG);

    const std::string versionMsg = std::string(PROJECT_NAME) + " v" + PROJECT_VERSION;
    LogMgr::getInstance().logMsg(DEBUG, versionMsg.c_str(), true);

    ContentMgr::getInstance().connectToQt();

    Client::getInstance().connectToServer();
    if (Client::getInstance().getSocketFd() < 0) {
        return 1;
    }

    EpollMgr::getInstance().init();
    EpollMgr::getInstance().addFd(Client::getInstance().getSocketFd(), EPOLLIN);
    if (ContentMgr::getInstance().qtFd() >= 0) {
        EpollMgr::getInstance().addFd(ContentMgr::getInstance().qtFd(), EPOLLIN);
    }
    Client::getInstance().requestRegisterToServer();
    EpollMgr::getInstance().wait();

    return 0;
}
