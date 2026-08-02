#ifndef __DATABASEMGR_H__
#define __DATABASEMGR_H__

#include <string>
#include <vector>
#include <sqlite3.h>
#include "schedulemgr.h"

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

    void createTable();

    bool deviceExists(const std::string& uid);

    void insertDevice(const std::string& uid, const std::string& name,
                      const std::string& group, const std::string& type);

    void updateDevice(const std::string& uid, const std::string& name,
                      const std::string& group);

    std::vector<DeviceRecord> queryAllByType(const std::string& type);

    int countByType(const std::string& type);

    void insertMask(const std::string& clientUid, const std::string& deviceUid);

    std::vector<std::string> queryMasksByClient(const std::string& clientUid);

    std::vector<ScheduleTask> queryPendingScheduleTasks(time_t now);

    int insertScheduleTask(const ScheduleTask& task);

    void disableScheduleTask(int id);
private: 
    DatabaseMgr();
    ~DatabaseMgr();

    sqlite3* m_db;
};

#endif
