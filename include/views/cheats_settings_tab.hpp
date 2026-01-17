#pragma once

#include <borealis.hpp>

class CheatsSettingsTab : public brls::Box {
public:
    CheatsSettingsTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, contentBox, "content_box");

    // Helper methods
    void ensureConfigExists();
    bool getCurrentSetting(const std::string& settingName);
    bool updateConfigSetting(const std::string& settingName, bool value);
    void refreshUI();

    // Store references to items for updating
    brls::BooleanCell* autoEnableItem;
    brls::BooleanCell* rememberStateItem;
};
