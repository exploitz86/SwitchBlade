#include "views/cheats_mods_tab.hpp"
#include "activity/mods_menu_activity.hpp"
#include "activity/cheats_menu_activity.hpp"

using namespace brls::literals;

CheatsModsTab::CheatsModsTab() {
    this->inflateFromXMLRes("xml/tabs/cheats_mods_tab.xml");

    cheatsMenuButton->setSelected(false);
    cheatsMenuButton->title->setText("menu/cheats_mods/cheats_menu"_i18n);
    cheatsMenuButton->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new CheatsMenuActivity());
        return true;
    });

    cheatmenuDescriptionLabel->setTextColor(nvgRGB(150, 150, 150));

    modsMenuButton->setSelected(false);
    modsMenuButton->title->setText("menu/cheats_mods/mods_menu"_i18n);
    modsMenuButton->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new ModsMenuActivity());
        return true;
    });

    modmenuDescriptionLabel->setTextColor(nvgRGB(150, 150, 150));
}

brls::View* CheatsModsTab::create() {
    return new CheatsModsTab();
}
