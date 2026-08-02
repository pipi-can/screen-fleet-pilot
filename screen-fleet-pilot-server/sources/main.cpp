#include "../includes/main.h"

int main(int argc, char const *argv[])
{
    static EpollManager* epollMgr = &EpollManager::getInstance();
    static SocketMgr* socketMgr = &SocketMgr::getInstance();

    LogMgr::getInstance().logInit(DEBUG);
    DatabaseMgr::getInstance().init();
    ScheduleMgr::getInstance().init();

    epollMgr->init();
    socketMgr->init();

    epollMgr->add(ScheduleMgr::getInstance().timerFd(), EPOLLIN);
    epollMgr->add(socketMgr->getSocketFd(), EPOLLIN);

    epollMgr->wait();
    return 0;
}
