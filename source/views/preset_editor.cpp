#include "views/preset_editor.hpp"
#include <borealis.hpp>
#include <filesystem>
#include <algorithm>

using namespace brls::literals;

PresetEditor::PresetEditor(const std::string& gamePath,
                           const std::string& presetName,
                           const std::vector<std::string>& existingMods,
                           std::function<void(const std::string&, const std::vector<std::string>&)> onSave,
                           const std::vector<std::string>& existingPresetNames)
    : gamePath(gamePath), originalPresetName(presetName), currentPresetName(presetName),
      selectedMods(existingMods), onSaveCallback(onSave), existingPresetNames(existingPresetNames) {

    brls::Logger::debug("PresetEditor constructor START");
    this->inflateFromXMLRes("xml/views/preset_editor.xml");
    brls::Logger::debug("PresetEditor XML inflated");

    // Set initial preset name
    if (currentPresetName.empty()) {
        currentPresetName = generateUniquePresetName("menu/mods/new_preset"_i18n);
    }
    presetNameLabel->setText(currentPresetName);
    brls::Logger::debug("Preset name set to: {}", currentPresetName);

    // Load available mods
    loadAvailableMods();
    brls::Logger::debug("Available mods loaded");

    // Populate the mod list
    populateModList();
    brls::Logger::debug("Mod list populated");

    // Update selected count
    selectedCountLabel->setText(fmt::format("menu/mods/mods_selected"_i18n, selectedMods.size()));

    // Register Plus button to save
    this->registerAction("hints/save"_i18n, brls::ControllerButton::BUTTON_START, [this](brls::View* view) {
        savePreset();
        return true;
    });

    // Register Y button to edit name
    this->registerAction("menu/mods/edit_name"_i18n, brls::ControllerButton::BUTTON_Y, [this](brls::View* view) {
        editPresetName();
        return true;
    });

    brls::Logger::debug("PresetEditor constructor END");
}

void PresetEditor::loadAvailableMods() {
    availableMods.clear();

    if (!std::filesystem::exists(gamePath) || !std::filesystem::is_directory(gamePath)) {
        brls::Logger::error("Game path doesn't exist: {}", gamePath);
        return;
    }

    // Scan all mod folders
    for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
        if (entry.is_directory()) {
            std::string modName = entry.path().filename().string();

            // Skip special directories
            if (modName == "." || modName == ".." || modName == "exefs" || modName == "romfs") {
                continue;
            }

            ModInfo info;
            info.modName = modName;
            info.modPath = entry.path().string();

            // Find all positions where this mod is selected
            for (size_t i = 0; i < selectedMods.size(); i++) {
                if (selectedMods[i] == modName) {
                    info.selectedPositions.push_back(i + 1);  // 1-indexed for display
                }
            }

            availableMods.push_back(info);
        }
    }

    // Sort alphabetically
    std::sort(availableMods.begin(), availableMods.end(),
        [](const ModInfo& a, const ModInfo& b) { return a.modName < b.modName; });

    brls::Logger::info("Loaded {} available mods", availableMods.size());
}

void PresetEditor::populateModList() {
    modList->clearViews();
    modListItems.clear();

    for (size_t i = 0; i < availableMods.size(); i++) {
        const auto& mod = availableMods[i];

        std::string tagText = getModTagText(mod);
        auto* item = new brls::DetailCell();
        item->setText(mod.modName);
        item->setDetailText(tagText);

        // Set maximum width to prevent stretching
        item->setMaxWidth(650);

        // A button: Add mod to selection
        item->registerAction("menu/mods/add"_i18n, brls::ControllerButton::BUTTON_A, [this, i](brls::View* view) {
            toggleModSelection(i);
            return true;
        });

        // X button: Remove last occurrence
        item->registerAction("menu/mods/remove"_i18n, brls::ControllerButton::BUTTON_X, [this, i](brls::View* view) {
            removeModOccurrence(i);
            return true;
        });

        modList->addView(item);
        modListItems.push_back(item);  // Store reference
    }

    if (availableMods.empty()) {
        auto* emptyLabel = new brls::Label();
        emptyLabel->setText("menu/mods/no_mods_in_folder"_i18n);
        emptyLabel->setFontSize(20);
        emptyLabel->setMargins(20, 40, 20, 20);
        modList->addView(emptyLabel);
    }
}

std::string PresetEditor::getModTagText(const ModInfo& mod) {
    if (mod.selectedPositions.empty()) {
        return "";
    }

    std::string tag;
    for (size_t i = 0; i < mod.selectedPositions.size(); i++) {
        if (i > 0) tag += " & ";
        tag += "#" + std::to_string(mod.selectedPositions[i]);
    }
    return tag;
}

void PresetEditor::toggleModSelection(size_t modIndex) {
    if (modIndex >= availableMods.size()) {
        return;
    }

    const std::string modName = availableMods[modIndex].modName;

    // Add this mod to the end of the selection
    selectedMods.push_back(modName);

    // Update the mod info
    availableMods[modIndex].selectedPositions.push_back(selectedMods.size());

    brls::Logger::info("Added mod '{}' at position {}", modName, selectedMods.size());

    // Update UI - no longer destroys views, safe to call directly
    updateModTags();
    selectedCountLabel->setText(fmt::format("menu/mods/mods_selected"_i18n, selectedMods.size()));
}

void PresetEditor::removeModOccurrence(size_t modIndex) {
    if (modIndex >= availableMods.size()) {
        return;
    }

    const std::string modName = availableMods[modIndex].modName;

    // Find last occurrence of this mod
    auto it = std::find(selectedMods.rbegin(), selectedMods.rend(), modName);
    if (it != selectedMods.rend()) {
        // Convert reverse iterator to forward iterator and erase
        selectedMods.erase(std::next(it).base());

        // Rebuild all mod positions
        for (auto& mod : availableMods) {
            mod.selectedPositions.clear();
        }

        for (size_t i = 0; i < selectedMods.size(); i++) {
            for (auto& mod : availableMods) {
                if (mod.modName == selectedMods[i]) {
                    mod.selectedPositions.push_back(i + 1);
                    break;
                }
            }
        }

        brls::Logger::info("Removed last occurrence of mod '{}'", modName);

        // Update UI - no longer destroys views, safe to call directly
        updateModTags();
        selectedCountLabel->setText(fmt::format("menu/mods/mods_selected"_i18n, selectedMods.size()));
    }
}

void PresetEditor::updateModTags() {
    // Just update the detail text of existing items instead of recreating
    for (size_t i = 0; i < availableMods.size() && i < modListItems.size(); i++) {
        std::string tagText = getModTagText(availableMods[i]);
        modListItems[i]->setDetailText(tagText);
    }
}

void PresetEditor::editPresetName() {
    brls::Application::getImeManager()->openForText([this](std::string text) {
        if (!text.empty()) {
            currentPresetName = text;
            presetNameLabel->setText(currentPresetName);
            brls::Logger::info("Preset name changed to: {}", currentPresetName);
        }
    }, "Enter preset name", "", 64, currentPresetName);
}

void PresetEditor::savePreset() {
    if (selectedMods.empty()) {
        brls::Dialog* dialog = new brls::Dialog("menu/mods/cannot_save_no_mods"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }

    if (onSaveCallback) {
        onSaveCallback(currentPresetName, selectedMods);
    }

    brls::Logger::info("Preset '{}' saved with {} mods", currentPresetName, selectedMods.size());

    // Close the editor
    this->dismiss();
}

std::string PresetEditor::generateUniquePresetName(const std::string& baseName) {
    // Check if the base name itself is unique
    bool nameExists = false;
    for (const auto& name : existingPresetNames) {
        if (name == baseName && name != originalPresetName) {
            nameExists = true;
            break;
        }
    }

    if (!nameExists) {
        return baseName;
    }

    // Try appending (1), (2), etc.
    int counter = 1;
    while (true) {
        std::string candidateName = baseName + " (" + std::to_string(counter) + ")";
        nameExists = false;

        for (const auto& name : existingPresetNames) {
            if (name == candidateName && candidateName != originalPresetName) {
                nameExists = true;
                break;
            }
        }

        if (!nameExists) {
            return candidateName;
        }

        counter++;
    }
}

brls::View* PresetEditor::create() {
    // This won't be used since we need to pass parameters
    return nullptr;
}
