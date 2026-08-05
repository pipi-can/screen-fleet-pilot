#include "../includes/databasemgr.h"
#include "logmgr.h"
#include <sqlite3.h>

static LogMgr* logger = &LogMgr::getInstance();

DatabaseMgr::DatabaseMgr() : m_db(nullptr) {}

DatabaseMgr::~DatabaseMgr() {
    close();
}

DatabaseMgr& DatabaseMgr::getInstance() {
    static DatabaseMgr instance;
    return instance;
}

void DatabaseMgr::init(const std::string& dbPath) {
    if (m_db) {
        return;
    }

    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "database open failed: " + dbPath, true);
        m_db = nullptr;
        return;
    }

    createTables();
    logger->logMsg(DEBUG, "database init success", true);
}

void DatabaseMgr::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

void DatabaseMgr::createTables() {
    if (!isOpen()) {
        return;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS device ("
        "uid    TEXT PRIMARY KEY NOT NULL,"
        "name   TEXT NOT NULL DEFAULT '',"
        "group_name TEXT NOT NULL DEFAULT '',"
        "type   TEXT NOT NULL"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, errMsg ? errMsg : "create device table failed", true);
        sqlite3_free(errMsg);
        return;
    }

    logger->logMsg(DEBUG, "device table ready", true);
}

/*
 * @brief: 查看一个设备是否存在
 */
bool DatabaseMgr::deviceExists(const std::string& uid) const {
    if (!isOpen() || uid.empty()) {
        return false;
    }

    const char* sql = "SELECT 1 FROM device WHERE uid = ? LIMIT 1;";
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

bool DatabaseMgr::insertDevice(const std::string& uid, const std::string& name,
                               const std::string& group, const std::string& type) {
    if (!isOpen() || uid.empty() || type.empty()) {
        return false;
    }

    const char* sql =
        "INSERT INTO device (uid, name, group_name, type) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "insertDevice prepare failed", true);
        return false;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, group.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "insertDevice failed, uid=" + uid, true);
        return false;
    }

    logger->logMsg(DEBUG, "insertDevice success, uid=" + uid, true);
    return true;
}

bool DatabaseMgr::updateDevice(const std::string& uid, const std::string& name,
                               const std::string& group) {
    if (!isOpen() || uid.empty()) {
        return false;
    }

    const char* sql =
        "UPDATE device SET name = ?, group_name = ? WHERE uid = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "updateDevice prepare failed", true);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, group.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, uid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        logger->logMsg(ERROR, "updateDevice failed, uid=" + uid, true);
        return false;
    }

    return sqlite3_changes(m_db) > 0;
}

std::optional<DeviceRecord> DatabaseMgr::queryDeviceByUid(const std::string& uid) const {
    if (!isOpen() || uid.empty()) {
        return std::nullopt;
    }

    const char* sql =
        "SELECT uid, name, group_name, type FROM device WHERE uid = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "queryDeviceByUid prepare failed", true);
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, uid.c_str(), -1, SQLITE_STATIC);

    std::optional<DeviceRecord> record;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceRecord rec;
        rec.uid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.group = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record = rec;
    }

    sqlite3_finalize(stmt);
    return record;
}

std::vector<DeviceRecord> DatabaseMgr::queryAllByType(const std::string& type) const {
    std::vector<DeviceRecord> results;
    if (!isOpen() || type.empty()) {
        return results;
    }

    const char* sql =
        "SELECT uid, name, group_name, type FROM device WHERE type = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger->logMsg(ERROR, "queryAllByType prepare failed", true);
        return results;
    }

    sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DeviceRecord rec;
        rec.uid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.group = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        results.push_back(rec);
    }

    sqlite3_finalize(stmt);
    return results;
}
