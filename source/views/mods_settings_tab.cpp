#include "views/mods_settings_tab.hpp"
#include "utils/config.hpp"

using namespace brls::literals;

ModsSettingsTab::ModsSettingsTab() {
    this->inflateFromXMLRes("xml/tabs/mods_settings_tab.xml");
    cfg::Config config;

    this->strict_cell->init("menu/mods/strict"_i18n, config.getStrictSearch(), [](bool value){
        cfg::Config config;
        config.setStringSearch(value);
    });
}

brls::View* ModsSettingsTab::create() {
    return new ModsSettingsTab();
}
