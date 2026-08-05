#ifndef DATABASEMGR_H
#define DATABASEMGR_H

#include <optional>
#include <string>
#include <vector>

struct sqlite3;

struct DeviceRecord {
    std::string uid;
    std::string name;
    std::string group;
    std::string type;
};

class DatabaseMgr {
public:
    DatabaseMgr(const DatabaseMgr& other) = delete;
    void operator=(const DatabaseMgr& other) = delete;

    static DatabaseMgr& getInstance();

    void init(const std::string& dbPath = "screen_fleet.db");
    void close();

    bool deviceExists(const std::string& uid) const;
    bool insertDevice(const std::string& uid, const std::string& name,
                      const std::string& group, const std::string& type);
    bool updateDevice(const std::string& uid, const std::string& name,
                      const std::string& group);
    std::optional<DeviceRecord> queryDeviceByUid(const std::string& uid) const;
    std::vector<DeviceRecord> queryAllByType(const std::string& type) const;

private:
    DatabaseMgr();
    ~DatabaseMgr();

    void createTables();
    bool isOpen() const { return m_db != nullptr; }

    sqlite3* m_db;
};

#endif
