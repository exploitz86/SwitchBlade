#pragma once

#include <vector>
#include <string>
#include <ctime>
#include <switch.h>

namespace utils {
    std::vector<std::pair<std::string, std::string>> getInstalledGames();
    uint8_t* getIconFromTitleId(const std::string& titleId);
    std::string removeHtmlTags(const std::string& str);
    std::string getModInstallPath();
    std::string timestamp_to_date(time_t timestamp);
    std::string file_size_to_string(int file_size);

    // Cheat utilities
    std::string getCheatsVersion();
    std::string readFile(const std::string& path);
    std::string formatApplicationId(u64 applicationId);
    std::string downloadFileToString(const std::string& url);
    std::string getContentsPath();
    void saveToFile(const std::string& text, const std::string& path);

    // Version and DLC utilities
    u32 getInstalledVersion(uint64_t title_id);
    bool isDlcInstalled(uint64_t dlc_title_id);
    
    // App update utilities
    std::string getLatestTag(const std::string& url);
    bool isUpdateAvailable(const std::string& currentVersion);
}