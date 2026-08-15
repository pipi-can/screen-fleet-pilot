#ifndef FIRMWAREMGR_H
#define FIRMWAREMGR_H

#include <string>
#include <vector>
#include "json_bags_ext.h"

class FirmwareMgr {
public:
    FirmwareMgr(const FirmwareMgr&) = delete;
    void operator=(const FirmwareMgr&) = delete;

    static FirmwareMgr& getInstance() {
        static FirmwareMgr instance;
        return instance;
    }

    void loadFirmwareList(std::vector<FirmwareEntry>& firmwares);
    bool checkFirmwareMd5(const std::string& relativePath, const std::string& expectedMd5);
    bool resolveFirmwarePath(const std::string& relativePath, std::string& fullPath);
    std::string computeFileMd5(const std::string& filePath);

private:
    FirmwareMgr() = default;
};

#endif
