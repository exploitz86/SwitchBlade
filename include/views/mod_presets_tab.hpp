#pragma once

#include <borealis.hpp>
#include "utils/preset_handler.hpp"
#include <functional>

class ModPresetsTab : public brls::Box {
public:
    ModPresetsTab(const std::string& gameName,
                  const std::string& gamePath,
                  const std::string& titleId,
                  std::function<void()> onPresetApplied = nullptr);

    static brls::View* create();
    void willAppear(bool resetState = false) override;

private:
    BRLS_BIND(brls::Box, presetList, "preset_list");

    std::string gameName;
    std::string gamePath;
    std::string titleId;
    PresetHandler presetHandler;
    std::function<void()> onPresetAppliedCallback;

    void populatePresetList();
    void createPreset();
    void editPreset(size_t index);
    void applyPreset(size_t index);
    void deletePreset(size_t index);
};
