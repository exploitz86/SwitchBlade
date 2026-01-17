#pragma once

#include <borealis.hpp>
#include <vector>
#include <string>
#include <functional>

struct ModInfo {
    std::string modName;
    std::string modPath;
    std::vector<int> selectedPositions;  // Positions where this mod is selected (#1, #2, etc.)
};

class PresetEditor : public brls::Box {
public:
    PresetEditor(const std::string& gamePath,
                 const std::string& presetName = "",
                 const std::vector<std::string>& existingMods = {},
                 std::function<void(const std::string&, const std::vector<std::string>&)> onSave = nullptr,
                 const std::vector<std::string>& existingPresetNames = {});

    static brls::View* create();

private:
    std::string gamePath;
    std::string currentPresetName;
    std::string originalPresetName;  // Track if we're editing an existing preset
    std::vector<std::string> selectedMods;  // Ordered list of selected mod names
    std::vector<ModInfo> availableMods;
    std::vector<brls::DetailCell*> modListItems;  // Keep references to DetailCell items
    std::vector<std::string> existingPresetNames;  // List of existing preset names
    std::function<void(const std::string&, const std::vector<std::string>&)> onSaveCallback;

    BRLS_BIND(brls::Box, modList, "mod_list");
    BRLS_BIND(brls::Label, presetNameLabel, "preset_name_label");
    BRLS_BIND(brls::Label, selectedCountLabel, "selected_count_label");

    void loadAvailableMods();
    void populateModList();
    void toggleModSelection(size_t modIndex);
    void removeModOccurrence(size_t modIndex);
    void editPresetName();
    void savePreset();
    void updateModTags();
    std::string getModTagText(const ModInfo& mod);
    std::string generateUniquePresetName(const std::string& baseName);
};
