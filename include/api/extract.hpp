#pragma once

#include <string>
#include <vector>

// Forward declare CFW enum from constants.hpp
enum class CFW;

namespace extract {
    bool extractEntry(const std::string& path, const std::string& outputDir, const std::string& tid);
    bool extractCFW(const std::string& archiveFile, const std::string& outputDir = "/", bool preserveInis = false, std::string* hekatePayloadOut = nullptr);

    // Cheat-specific extraction functions
    bool extractCheats(const std::string& archivePath, const std::vector<std::string>& titles, CFW cfw, const std::string& version, bool extractAll = false);
    bool extractAllCheats(const std::string& archivePath, CFW cfw, const std::string& version);

    // Helper functions for title management
    std::vector<std::string> getInstalledTitles();
    std::vector<std::string> excludeTitles(const std::string& path, const std::vector<std::string>& listedTitles);
}