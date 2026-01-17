#include "views/mod_presets_tab.hpp"
#include "views/preset_apply_view.hpp"
#include "views/preset_editor.hpp"
#include "utils/config.hpp"

#include <borealis.hpp>
#include <filesystem>

using namespace brls::literals;

ModPresetsTab::ModPresetsTab(const std::string& gameName,
                             const std::string& gamePath,
                             const std::string& titleId,
                             std::function<void()> onPresetApplied)
    : gameName(gameName), gamePath(gamePath), titleId(titleId),
      onPresetAppliedCallback(onPresetApplied) {

    this->inflateFromXMLRes("xml/tabs/mod_presets_tab.xml");

    // Initialize preset handler
    presetHandler.setGameFolder(gamePath);

    // Populate the preset list
    populatePresetList();

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif
}

void ModPresetsTab::populatePresetList() {
    presetList->clearViews();

    // Add existing presets
    const auto& presets = presetHandler.getPresetList();
    for (size_t i = 0; i < presets.size(); i++) {
        const auto& preset = presets[i];

        std::string subtitle = fmt::format("menu/mods/mods_in_this_set"_i18n, preset.modList.size());
        auto* item = new brls::DetailCell();
        item->setText(preset.name);
        item->setDetailText(subtitle);

        // A button: Apply preset
        item->registerAction("menu/mods/apply"_i18n, brls::ControllerButton::BUTTON_A, [this, i](brls::View* view) {
            applyPreset(i);
            return true;
        });

        // Y button: Edit preset
        item->registerAction("menu/mods/edit"_i18n, brls::ControllerButton::BUTTON_Y, [this, i](brls::View* view) {
            editPreset(i);
            return true;
        });

        // X button: Delete preset
        item->registerAction("hints/delete"_i18n, brls::ControllerButton::BUTTON_X, [this, i](brls::View* view) {
            deletePreset(i);
            return true;
        });

        presetList->addView(item);
    }

    // Add "Create New Preset" button at the end
    auto* createButton = new brls::DetailCell();
    createButton->setText("menu/mods/create_new_preset"_i18n);

    createButton->registerAction("menu/mods/create"_i18n, brls::ControllerButton::BUTTON_A, [this](brls::View* view) {
        createPreset();
        return true;
    });

    presetList->addView(createButton);
}

void ModPresetsTab::createPreset() {
    // Gather existing preset names
    std::vector<std::string> existingNames;
    const auto& presets = presetHandler.getPresetList();
    for (const auto& preset : presets) {
        existingNames.push_back(preset.name);
    }

    this->present(new PresetEditor(
        gamePath,
        "",
        {},
        [this](const std::string& name, const std::vector<std::string>& mods) {
            presetHandler.createNewPreset(name, mods);
            presetHandler.writeConfigFile();
            populatePresetList();
        },
        existingNames
    ));
}

void ModPresetsTab::editPreset(size_t index) {
    const auto& presets = presetHandler.getPresetList();
    if (index >= presets.size()) {
        return;
    }

    const auto& preset = presets[index];

    // Gather existing preset names
    std::vector<std::string> existingNames;
    for (const auto& p : presets) {
        existingNames.push_back(p.name);
    }

    this->present(new PresetEditor(
        gamePath,
        preset.name,
        preset.modList,
        [this, index](const std::string& name, const std::vector<std::string>& mods) {
            presetHandler.editPreset(index, name, mods);
            presetHandler.writeConfigFile();
            populatePresetList();
        },
        existingNames
    ));
}

void ModPresetsTab::applyPreset(size_t index) {
    const auto& presets = presetHandler.getPresetList();
    if (index >= presets.size()) {
        return;
    }

    const auto& preset = presets[index];

    if (preset.modList.empty()) {
        brls::Dialog* dialog = new brls::Dialog("menu/mods/cannot_apply_no_mods"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return;
    }

    // Show confirmation dialog
    std::string message = fmt::format(
        "menu/mods/apply_preset_confirm"_i18n,
        preset.name, preset.modList.size()
    );

    brls::Dialog* dialog = new brls::Dialog(message);
    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("menu/mods/apply"_i18n, [this, preset]() {
        // Schedule the preset apply view to be presented AFTER the dialog closes
        brls::sync([this, preset]() {
            // Get install base path
            std::string installBase = "sdmc:/atmosphere";

            // Present preset apply view
            this->present(new PresetApplyView(
                preset.name,
                preset.modList,
                this->gamePath,
                installBase,
                [this]() {
                    if (onPresetAppliedCallback) {
                        onPresetAppliedCallback();
                    }
                }
            ));
        });
    });
    dialog->open();
}

void ModPresetsTab::deletePreset(size_t index) {
    const auto& presets = presetHandler.getPresetList();
    if (index >= presets.size()) {
        return;
    }

    std::string presetName = presets[index].name;

    // Show confirmation dialog
    brls::Dialog* dialog = new brls::Dialog(fmt::format("menu/mods/delete_preset_confirm"_i18n, presetName));
    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("hints/delete"_i18n, [this, presetName, index]() {
        // Delete the preset
        presetHandler.deletePreset(presetName);
        presetHandler.writeConfigFile();

        // Show success message
        brls::Application::notify("menu/mods/preset_deleted"_i18n);

        // Schedule repopulation and focus management after dialog closes
        brls::sync([this, index]() {
            // Repopulate the list
            populatePresetList();

            // Safely restore focus to a valid item
            // If we deleted the last preset, focus on "Create New Preset" button
            // Otherwise, focus on the item at the same index (or previous if we deleted the last one)
            auto children = presetList->getChildren();
            if (!children.empty()) {
                size_t focusIndex = std::min(index, children.size() - 1);
                brls::Application::giveFocus(children[focusIndex]);
            }
        });
    });

    dialog->open();
}

void ModPresetsTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    brls::sync([this]() {
        this->getAppletFrame()->setTitle(this->gameName);
    });
}

brls::View* ModPresetsTab::create() {
    // This won't be used since we need to pass parameters
    return nullptr;
}
