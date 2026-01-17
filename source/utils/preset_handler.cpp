#include "utils/preset_handler.hpp"
#include <borealis.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

PresetHandler::PresetHandler() {}

void PresetHandler::setGameFolder(const std::string& gamePath) {
    this->gameFolder = gamePath;
    this->configFilePath = gamePath + "/mod_presets.conf";
    readConfigFile();
}

const std::vector<PresetData>& PresetHandler::getPresetList() const {
    return presetList;
}

void PresetHandler::createNewPreset(const std::string& name, const std::vector<std::string>& modList) {
    PresetData preset;
    preset.name = name.empty() ? generatePresetName() : name;
    preset.modList = modList;
    presetList.push_back(preset);
    writeConfigFile();
}

void PresetHandler::editPreset(size_t index, const std::string& name, const std::vector<std::string>& modList) {
    if (index >= presetList.size()) {
        brls::Logger::error("Invalid preset index: {}", index);
        return;
    }

    presetList[index].name = name;
    presetList[index].modList = modList;
    writeConfigFile();
}

void PresetHandler::deletePreset(size_t index) {
    if (index >= presetList.size()) {
        brls::Logger::error("Invalid preset index: {}", index);
        return;
    }

    presetList.erase(presetList.begin() + index);
    writeConfigFile();
}

void PresetHandler::deletePreset(const std::string& name) {
    auto it = std::find_if(presetList.begin(), presetList.end(),
        [&name](const PresetData& preset) { return preset.name == name; });

    if (it != presetList.end()) {
        presetList.erase(it);
        writeConfigFile();
    }
}

std::string PresetHandler::generatePresetName() {
    int counter = 1;
    std::string name;
    bool nameExists = true;

    while (nameExists) {
        name = "preset-" + std::to_string(counter);
        nameExists = false;

        for (const auto& preset : presetList) {
            if (preset.name == name) {
                nameExists = true;
                counter++;
                break;
            }
        }
    }

    return name;
}

void PresetHandler::writeConfigFile() {
    if (configFilePath.empty()) {
        brls::Logger::error("Config file path not set");
        return;
    }

    // Ensure parent directory exists
    std::filesystem::path filePath(configFilePath);
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        brls::Logger::error("Failed to open config file for writing: {}", configFilePath);
        return;
    }

    file << "# This is a config file\n\n\n";

    for (const auto& preset : presetList) {
        file << "########################################\n";
        file << "# mods preset name\n";
        file << "preset = " << preset.name << "\n\n";
        file << "# mods list\n";

        for (size_t i = 0; i < preset.modList.size(); i++) {
            file << "mod" << i << " = " << preset.modList[i] << "\n";
        }

        file << "########################################\n\n";
    }

    file.close();
    brls::Logger::info("Preset config file written: {}", configFilePath);
}

void PresetHandler::readConfigFile() {
    presetList.clear();

    if (configFilePath.empty()) {
        brls::Logger::error("Config file path not set");
        return;
    }

    if (!std::filesystem::exists(configFilePath)) {
        brls::Logger::debug("Preset config file doesn't exist yet: {}", configFilePath);
        return;
    }

    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        brls::Logger::error("Failed to open config file for reading: {}", configFilePath);
        return;
    }

    PresetData currentPreset;
    bool inPreset = false;
    std::string line;

    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse key=value
        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);

        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "preset") {
            // Save previous preset if exists
            if (inPreset && !currentPreset.name.empty()) {
                presetList.push_back(currentPreset);
            }

            // Start new preset
            currentPreset = PresetData();
            currentPreset.name = value;
            inPreset = true;
        } else if (key.find("mod") == 0 && inPreset) {
            // Add mod to current preset
            currentPreset.modList.push_back(value);
        }
    }

    // Save last preset
    if (inPreset && !currentPreset.name.empty()) {
        presetList.push_back(currentPreset);
    }

    file.close();
    brls::Logger::info("Loaded {} presets from config file", presetList.size());
}
