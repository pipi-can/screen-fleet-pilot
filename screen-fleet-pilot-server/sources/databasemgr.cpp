#include "../includes/databasemgr.h"
#include "../includes/logmgr.h"

extern "C" {
#include <json-c/json.h>
}

static LogMgr* logger = &LogMgr::getInstance();

DatabaseMgr::DatabaseMgr() : m_db(nullptr) {

}

DatabaseMgr::~DatabaseMgr() {
    close();
}

DatabaseMgr& DatabaseMgr::getInstance() {
    static DatabaseMgr instance;
    return instance;
}

void DatabaseMgr::init(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "database open failed", true);
        m_db = nullptr;
        return;
    }
    logger->logMsg(DEBUG, "database init success", true);
    createTable();
}

void DatabaseMgr::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

void DatabaseMgr::createTable() {
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS device_info ("
        "uid          TEXT UNIQUE NOT NULL PRIMARY KEY,"
        "device_name  TEXT,"
        "device_group TEXT,"
        "type         TEXT"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, errMsg, true);
        sqlite3_free(errMsg);
        return;
    }
    logger->logMsg(DEBUG, "device_info table ready", true);

    const char* maskSql =
        "CREATE TABLE IF NOT EXISTS device_mask ("
        "client_uid  TEXT NOT NULL,"
        "device_uid  TEXT NOT NULL,"
        "PRIMARY KEY (client_uid, device_uid)"
        ");";

    rc = sqlite3_exec(m_db, maskSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, errMsg, true);
        sqlite3_free(errMsg);
        return;
    }
    logger->logMsg(DEBUG, "device_mask table ready", true);

    const char* scheduleSql =
        "CREATE TABLE IF NOT EXISTS schedule_task ("
        "id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "device_uid    TEXT NOT NULL,"
        "schedule_date TEXT NOT NULL,"
        "schedule_time TEXT NOT NULL,"
        "duration_sec  INTEGER NOT NULL,"
        "trigger_at    INTEGER NOT NULL,"
        "paths         TEXT NOT NULL,"
        "enabled       INTEGER NOT NULL DEFAULT 1"
        ");";

    rc = sqlite3_exec(m_db, scheduleSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, errMsg, true);
        sqlite3_free(errMsg);
        return;
    }
    logger->logMsg(DEBUG, "schedule_task table ready", true);
}

bool DatabaseMgr::deviceExists(const std::string& uid) {
    const char* sql = "SELECT 1 FROM device_info WHERE uid = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "deviceExists prepare failed", true);
        return false;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_STATIC);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

void DatabaseMgr::insertDevice(const std::string& uid, const std::string& name,
                               const std::string& group, const std::string& type) {
    const char* sql = "INSERT INTO device_info (uid, device_name, device_group, type) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "insertDevice prepare failed", true);
        return;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, group.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "insertDevice exec failed", true);
    } else {
        logger->logMsg(DEBUG, "insertDevice success", true);
    }
    sqlite3_finalize(stmt);
}

void DatabaseMgr::updateDevice(const std::string& uid, const std::string& name,
                               const std::string& group) {
    const char* sql = "UPDATE device_info SET device_name = ?, device_group = ? WHERE uid = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "updateDevice prepare failed", true);
        return;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, group.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, uid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "updateDevice exec failed", true);
    }
    sqlite3_finalize(stmt);
}

std::vector<DeviceRecord> DatabaseMgr::queryAllByType(const std::string& type) {
    std::vector<DeviceRecord> results;
    const char* sql = "SELECT uid, device_name, device_group, type FROM device_info WHERE type = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "queryAllByType prepare failed", true);
        return results;
    }

    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceRecord rec;
        rec.uid   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.group = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.type  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        results.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return results;
}

int DatabaseMgr::countByType(const std::string& type) {
    const char* sql = "SELECT COUNT(*) FROM device_info WHERE type = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "countByType prepare failed", true);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

void DatabaseMgr::insertMask(const std::string& clientUid, const std::string& deviceUid) {
    const char* sql = "INSERT OR IGNORE INTO device_mask (client_uid, device_uid) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "insertMask prepare failed", true);
        return;
    }

    sqlite3_bind_text(stmt, 1, clientUid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, deviceUid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "insertMask exec failed", true);
    }
    sqlite3_finalize(stmt);
}

std::vector<std::string> DatabaseMgr::queryMasksByClient(const std::string& clientUid) {
    std::vector<std::string> results;
    const char* sql = "SELECT device_uid FROM device_mask WHERE client_uid = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "queryMasksByClient prepare failed", true);
        return results;
    }

    sqlite3_bind_text(stmt, 1, clientUid.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<ScheduleTask> DatabaseMgr::queryPendingScheduleTasks(time_t now) {
    std::vector<ScheduleTask> results;
    if (!m_db) {
        return results;
    }
    const char* sql =
        "SELECT id, device_uid, schedule_date, schedule_time, "
        "duration_sec, trigger_at, paths, enabled "
        "FROM schedule_task "
        "WHERE enabled = 1 AND trigger_at > ? "
        "ORDER BY trigger_at ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "queryPendingScheduleTasks prepare failed", true);
        return results;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(now));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ScheduleTask task;
        task.id           = sqlite3_column_int(stmt, 0);
        task.deviceUid    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        task.scheduleDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        task.scheduleTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        task.durationSec  = sqlite3_column_int(stmt, 4);
        task.triggerAt    = static_cast<time_t>(sqlite3_column_int64(stmt, 5));
        const char* pathsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (pathsJson && pathsJson[0] != '\0') {
            task.paths = ScheduleMgr::parsePaths(pathsJson);
        }
        task.enabled = (sqlite3_column_int(stmt, 7) != 0);
        
        results.push_back(task);
    }
    sqlite3_finalize(stmt);
    return results;
}

int DatabaseMgr::insertScheduleTask(const ScheduleTask& task) {
    if (!m_db) {
        return -1;
    }

    const char* sql =
        "INSERT INTO schedule_task "
        "(device_uid, schedule_date, schedule_time, duration_sec, trigger_at, paths, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "insertScheduleTask prepare failed", true);
        return -1;
    }

    std::string pathsJson = ScheduleMgr::encodePaths(task.paths);
    sqlite3_bind_text(stmt, 1, task.deviceUid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, task.scheduleDate.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, task.scheduleTime.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, task.durationSec);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(task.triggerAt));
    sqlite3_bind_text(stmt, 6, pathsJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, task.enabled ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "insertScheduleTask exec failed", true);
        return -1;
    }

    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

void DatabaseMgr::disableScheduleTask(int id) {
    if (!m_db) {
        return;
    }

    const char* sql = "UPDATE schedule_task SET enabled = 0 WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "disableScheduleTask prepare failed", true);
        return;
    }

    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "disableScheduleTask exec failed", true);
    }
    sqlite3_finalize(stmt);
}