#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <string>

class InstalledCheatsView : public brls::AppletFrame {
public:
    InstalledCheatsView(const std::string& gameName, const std::string& buildId,
                        const std::string& cheatFilePath, u64 titleId);
};
