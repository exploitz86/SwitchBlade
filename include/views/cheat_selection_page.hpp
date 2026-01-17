#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <string>

class CheatSelectionPage : public brls::AppletFrame {
public:
    CheatSelectionPage(u64 tid, const std::string& name, bool isGfxCheats = false);

private:
    brls::Box* list;
    u64 titleId;
    std::string gameName;
    std::string buildId;
    bool isGfxCheats;

    void getBuildId();
    void getBuildIdFromDmnt();
    void getBuildIdFromVersions();
    void populateCheats();
};
