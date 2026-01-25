#include "views/about_tab.hpp"

using namespace brls::literals;

AboutTab::AboutTab() {
    this->inflateFromXMLRes("xml/tabs/about_tab.xml");

    appTitleLabel->setText("menu/main/app_name"_i18n + " - v" + APP_VERSION);
}

brls::View* AboutTab::create() {
    return new AboutTab();
}