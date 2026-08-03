#ifndef CLIENT_H
#define CLIENT_H

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
}
#include "global_def.h"

class Client {
public:
    Client(const Client& other) = delete;
    void operator=(const Client& other) = delete;

    static Client& getInstance() {
        static Client instance;
        return instance;
    }

    void connectToServer();
    int getSocketFd() const { return m_socketFd; }

private:
    Client() = default;
    ~Client();

    int m_socketFd = -1;
};

#endif
