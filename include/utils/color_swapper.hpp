#pragma once

#include <string>
#include <vector>
#include <map>

namespace JC {
    int setColor(const std::vector<int>& colors);
    void changeJCColor(const std::vector<int>& values);
    bool backupJCColor(const std::string& path);
    std::vector<std::pair<std::string, std::vector<int>>> getProfiles(const std::string& path);
}

namespace PC {
    int setColor(const std::vector<int>& colors);
    void changePCColor(const std::vector<int>& values);
    bool backupPCColor(const std::string& path);
    std::vector<std::pair<std::string, std::vector<int>>> getProfiles(const std::string& path);
}

namespace ColorSwapper {
    int hexToBGR(const std::string& hex);
    std::string BGRToHex(int v);
}
