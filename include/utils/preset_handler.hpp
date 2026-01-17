#pragma once

#include <string>
#include <vector>

struct PresetData {
    std::string name;
    std::vector<std::string> modList;  // Ordered list of mod folder names
};

class PresetHandler {
public:
    PresetHandler();

    void setGameFolder(const std::string& gamePath);
    const std::vector<PresetData>& getPresetList() const;

    void createNewPreset(const std::string& name, const std::vector<std::string>& modList);
    void editPreset(size_t index, const std::string& name, const std::vector<std::string>& modList);
    void deletePreset(size_t index);
    void deletePreset(const std::string& name);

    void writeConfigFile();
    void readConfigFile();

private:
    std::string gameFolder;
    std::string configFilePath;
    std::vector<PresetData> presetList;

    std::string generatePresetName();
};
