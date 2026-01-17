#include "views/procon_color_page.hpp"
#include "utils/color_swapper.hpp"
#include "utils/constants.hpp"
#include <borealis.hpp>
#include <switch.h>

using namespace brls::literals;

ProConColorPage::ProConColorPage() {
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
    descLabel->setText("menu/joycon/pc_desc"_i18n + "menu/joycon/profiles_stored"_i18n + std::string(PC_COLOR_PATH));
    descLabel->setFontSize(16);
    descLabel->setMarginBottom(20);
    contentBox->addView(descLabel);

    // Add backup button
    auto* backupItem = new brls::RadioCell();
    backupItem->title->setText("menu/joycon/backup"_i18n);
    backupItem->registerClickAction([](brls::View* view) {
        bool success = PC::backupPCColor(PC_COLOR_PATH);
        if (success) {
            auto* dialog = new brls::Dialog("menu/joycon/pc_success"_i18n);
            dialog->addButton("menu/joycon/restart"_i18n, []() {
                envSetNextLoad(NRO_PATH, NRO_PATH);
                brls::Application::quit();
            });
            dialog->addButton("menu/joycon/later"_i18n, []() {});
            dialog->open();
        } else {
            auto* dialog = new brls::Dialog("menu/joycon/pc_bk_fail"_i18n);
            dialog->addButton("hints/ok"_i18n, []() {});
            dialog->open();
        }
        return true;
    });
    contentBox->addView(backupItem);

    // Add separator header
    auto* header = new brls::Header();
    header->setTitle("menu/joycon/color_presets"_i18n);
    contentBox->addView(header);

    // Get color profiles and add them
    auto profiles = PC::getProfiles(PC_COLOR_PATH);

    for (const auto& [name, colors] : profiles) {
        auto* item = new brls::RadioCell();
        item->title->setText(name);
        item->registerClickAction([colors](brls::View* view) {
            PC::changePCColor(colors);
            return true;
        });

        contentBox->addView(item);
    }

    brls::Logger::info("Loaded {} Pro Controller color profiles", profiles.size());

    scrollFrame->setContentView(contentBox);
    this->setContentView(scrollFrame);

    // Set title and register B button AFTER setting content
    this->setTitle("menu/joycon/pc_title"_i18n);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}

void ProConColorPage::loadColorProfiles() {
    // Not needed anymore
}
