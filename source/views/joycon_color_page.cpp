#include "views/joycon_color_page.hpp"
#include "utils/color_swapper.hpp"
#include "utils/constants.hpp"
#include <borealis.hpp>
#include <switch.h>

using namespace brls::literals;

JoyConColorPage::JoyConColorPage() {
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::COLUMN);
    contentBox->setWidth(brls::View::AUTO);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(20, 20, 20, 20);

    // Add description label
    auto* descLabel = new brls::Label();
    descLabel->setText("menu/joycon/jc_desc"_i18n + std::string(COLOR_PICKER_URL) + "menu/joycon/profiles_stored"_i18n + std::string(JC_COLOR_PATH));
    descLabel->setFontSize(16);
    descLabel->setMarginBottom(20);
    contentBox->addView(descLabel);

    // Add backup button
    auto* backupItem = new brls::RadioCell();
    backupItem->title->setText("menu/joycon/backup"_i18n);
    backupItem->registerClickAction([](brls::View* view) {
        bool success = JC::backupJCColor(JC_COLOR_PATH);
        if (success) {
            auto* dialog = new brls::Dialog("menu/joycon/jc_success"_i18n);
            dialog->addButton("menu/joycon/restart"_i18n, []() {
                envSetNextLoad(NRO_PATH, NRO_PATH);
                brls::Application::quit();
            });
            dialog->addButton("menu/joycon/later"_i18n, []() {});
            dialog->open();
        } else {
            brls::Application::notify("menu/joycon/jc_bk_fail"_i18n);
        }
        return true;
    });
    contentBox->addView(backupItem);

    // Add separator header
    auto* header = new brls::Header();
    header->setTitle("menu/joycon/color_presets"_i18n);
    contentBox->addView(header);

    // Get color profiles and add them
    auto profiles = JC::getProfiles(JC_COLOR_PATH);

    if (profiles.empty()) {
        auto* noProfilesLabel = new brls::Label();
        noProfilesLabel->setText("menu/joycon/no_profiles"_i18n);
        contentBox->addView(noProfilesLabel);
    } else {
        // Create a menu item for each profile
        for (const auto& [name, colors] : profiles) {
            auto* item = new brls::RadioCell();
            item->title->setText(name);

            item->registerClickAction([colors](brls::View* view) {
                JC::changeJCColor(colors);
                return true;
            });

            contentBox->addView(item);
        }

        brls::Logger::info("Loaded {} Joy-Con color profiles", profiles.size());
    }

    scrollFrame->setContentView(contentBox);
    this->setContentView(scrollFrame);

    // Set title and register B button AFTER setting content
    this->setTitle("menu/joycon/jc_title"_i18n);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}

void JoyConColorPage::loadColorProfiles() {
    // Not needed anymore
}
