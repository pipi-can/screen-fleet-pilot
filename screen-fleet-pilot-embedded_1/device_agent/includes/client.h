#ifndef CLIENT_H
#define CLIENT_H

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
}
#include "global_def.h"
#include "jsonpacker.h"
#include "logmgr.h"

extern struct ClientMetaMessage;

class Client {
public:
    Client(const Client& other) = delete;
    void operator=(const Client& other) = delete;

    static Client& getInstance() {
        static Client instance;
        return instance;
    }

    void connectToServer();

    void requestRegisterToServer();

    bool sendMessageToServer(const char* message);

    void setName(const std::string& name) { 
        m_metaMessage.name = name;
        m_metaMessage.writeMetaMessageToFile();
    }
    void setGroup(const std::string& group) { 
        m_metaMessage.group = group;
        m_metaMessage.writeMetaMessageToFile();
    }
    void setVersion(const std::string& version) { 
        m_metaMessage.version = version;
        m_metaMessage.writeMetaMessageToFile();
    }

    int getSocketFd() const { return m_socketFd; }
    std::string getName() const { return m_metaMessage.deviceName; }
    std::string getGroup() const { return m_metaMessage.deviceGroup; }
    std::string getVersion() const { return m_metaMessage.deviceVersion; }
    std::string getDeviceUid() const { return m_metaMessage.deviceUid; }

private:
    Client() {
        m_metaMessage.loadMetaMessage();
    }
    ~Client();

    int             m_socketFd = -1;
    ClientMetaMessage m_metaMessage;
};

#endif
