#include "../includes/main.h"
#include "version.h"
#include <string>

int main()
{
    LogMgr::getInstance().logInit(DEBUG);

    const std::string versionMsg = std::string(PROJECT_NAME) + " v" + PROJECT_VERSION;
    LogMgr::getInstance().logMsg(DEBUG, versionMsg.c_str(), true);

    Client::getInstance().connectToServer();
    if (Client::getInstance().getSocketFd() < 0) {
        return 1;
    }

    EpollMgr::getInstance().init();
    // LT 模式：仅 EPOLLIN，不加 EPOLLET
    EpollMgr::getInstance().addFd(Client::getInstance().getSocketFd(), EPOLLIN);
    Client::getInstance().requestRegisterToServer();
    EpollMgr::getInstance().wait();

    return 0;
}
