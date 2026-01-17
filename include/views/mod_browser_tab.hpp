#pragma once

#include <borealis.hpp>
#include <vector>
#include <string>

class ModBrowserTab : public brls::Box {
public:
    ModBrowserTab(const std::string& gameName, const std::string& gamePath, const std::string& titleId);

    static brls::View* create();

    void updateModStatus();
    void updateCellStatus(size_t modIndex);
    void willAppear(bool resetState = false) override;

private:
    std::string gameName;
    std::string gamePath;
    std::string titleId;

    BRLS_BIND(brls::Box, modListBox, "mod_list_box");

    struct ModInfo {
        std::string modName;
        std::string modPath;
        brls::DetailCell* cell;
        std::string status;      // "ACTIVE", "INACTIVE", "PARTIAL", "NO FILE"
        double statusFraction;   // 0.0 to 1.0
    };

    std::vector<ModInfo> mods;

    void scanMods();
    std::string checkModStatus(const std::string& modPath, double& outFraction);
    void applyMod(const std::string& modName, const std::string& modPath);
    void removeMod(const std::string& modName, const std::string& modPath);
    std::vector<std::string> listModFiles(const std::string& modPath);

    void loadModStatusCache();
    void saveModStatusCache();
};
