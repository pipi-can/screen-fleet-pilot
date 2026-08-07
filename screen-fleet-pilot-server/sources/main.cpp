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

    SocketMgr::setNonBlock(socketMgr->getSocketFd());
    epollMgr->add(ScheduleMgr::getInstance().timerFd(), EPOLLIN | EPOLLET);
    epollMgr->add(socketMgr->getSocketFd(), EPOLLIN | EPOLLET);

    epollMgr->wait();
    return 0;
}
