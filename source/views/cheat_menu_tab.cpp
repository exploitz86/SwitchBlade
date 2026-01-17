#include "views/cheat_menu_tab.hpp"
#include "views/cheat_download_view.hpp"
#include "views/cheat_app_page.hpp"
#include "views/cheat_slips_page.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include "utils/current_cfw.hpp"
#include <borealis/core/application.hpp>
#include "utils/download.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>

using namespace brls::literals;

CheatMenuTab::CheatMenuTab() {
    this->inflateFromXMLRes("xml/tabs/cheat_menu_tab.xml");

    // Description: "This will download... Current cheats version: "
    std::string currentVersion = utils::readFile(CHEATS_VERSION);
    if (currentVersion.empty()) {
        currentVersion = "Not installed";
    }

    brls::Label* description = new brls::Label();
    description->setText("menu/cheats_menu/gba_download_desc"_i18n + currentVersion);
    description->setFontSize(16);
    description->setTextColor(nvgRGB(150, 150, 150));
    description->setMarginBottom(10);
    description->setFocusable(false);
    contentBox->addView(description);

    // Download GBAtemp.net cheat archive (ver xxx)
    auto* downloadGbatempArchive = new brls::RadioCell();

    // Fetch online version
    std::string onlineVersion = utils::getCheatsVersion();
    brls::Logger::debug("Fetched online cheat version: '{}'", onlineVersion);
    std::string buttonText = "menu/cheats_menu/gbatemp_download_button"_i18n;
    if (!onlineVersion.empty()) {
        buttonText += " (" + onlineVersion + ")";
    }
    downloadGbatempArchive->title->setText(buttonText);
    downloadGbatempArchive->setSelected(false);

    downloadGbatempArchive->registerClickAction([this](brls::View* view) {
        // Fetch version when button is clicked (not during construction)
        std::string newCheatsVer = utils::getCheatsVersion();

        // Update button text with version if available
        if (!newCheatsVer.empty()) {
            brls::Logger::info("Cheat version: {}", newCheatsVer);
        }

        // Determine which URL to use based on CFW
        CFW cfw = CurrentCfw::getCFW();
        std::string url = (cfw == CFW::sxos) ? CHEATS_URL_TITLES : CHEATS_URL_CONTENTS;

        // Create and present the download view
        try {
            brls::Logger::debug("Creating CheatDownloadView");
            this->present(new CheatDownloadView(url, newCheatsVer));
            brls::Logger::debug("View presented successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("Exception while presenting CheatDownloadView: {}", e.what());
        }
        return true;
    });
    contentBox->addView(downloadGbatempArchive);

    // Download graphics enhancing cheats
    auto* downloadGfxCheats = new brls::RadioCell();
    downloadGfxCheats->title->setText("menu/cheats_menu/gfx_download_button"_i18n);
    downloadGfxCheats->setSelected(false);
    downloadGfxCheats->registerClickAction([this](brls::View* view) {
        // Fetch version when button is clicked
        std::string newCheatsVer = utils::getCheatsVersion();

        // Determine which URL to use based on CFW
        CFW cfw = CurrentCfw::getCFW();
        std::string url = (cfw == CFW::sxos) ? GFX_CHEATS_URL_TITLES : GFX_CHEATS_URL_CONTENTS;

        // Create and present the download view
        try {
            brls::Logger::debug("Creating CheatDownloadView for graphics cheats");
            this->present(new CheatDownloadView(url, newCheatsVer, true));
            brls::Logger::debug("View presented successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("Exception while presenting CheatDownloadView: {}", e.what());
        }
        return true;
    });
    contentBox->addView(downloadGfxCheats);

    // Label: Download individual cheat codes...
    brls::Label* individualLabel = new brls::Label();
    individualLabel->setText("menu/cheats_menu/individual_download_desc"_i18n);
    individualLabel->setFontSize(16);
    individualLabel->setTextColor(nvgRGB(150, 150, 150));
    individualLabel->setMarginTop(20);
    individualLabel->setMarginBottom(10);
    individualLabel->setFocusable(false);
    contentBox->addView(individualLabel);

    // Download individual cheat codes
    auto* individualCheats = new brls::RadioCell();
    individualCheats->title->setText("menu/cheats_menu/download_individual_button"_i18n);
    individualCheats->setSelected(false);
    individualCheats->registerClickAction([](brls::View* view) {
        try {
            brls::Logger::debug("Creating CheatAppPage");
            brls::Application::pushActivity(new brls::Activity(new CheatAppPage()));
            brls::Logger::debug("CheatAppPage pushed successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("Exception while creating CheatAppPage: {}", e.what());
        }
        return true;
    });
    contentBox->addView(individualCheats);

    // Download individual graphics enhancing cheat codes
    auto* individualGfxCheats = new brls::RadioCell();
    individualGfxCheats->title->setText("menu/cheats_menu/download_individual_gfx_button"_i18n);
    individualGfxCheats->setSelected(false);
    individualGfxCheats->registerClickAction([](brls::View* view) {
        try {
            brls::Logger::debug("Creating CheatAppPage for graphics cheats");
            brls::Application::pushActivity(new brls::Activity(new CheatAppPage(true)));
            brls::Logger::debug("CheatAppPage (GFX) pushed successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("Exception while creating CheatAppPage (GFX): {}", e.what());
        }
        return true;
    });
    contentBox->addView(individualGfxCheats);

    // Download Cheat Slips cheat sheets
    auto* cheatSlips = new brls::RadioCell();
    cheatSlips->title->setText("menu/cheats_menu/download_cheatslips_button"_i18n);
    cheatSlips->setSelected(false);
    cheatSlips->registerClickAction([](brls::View* view) {
        // If token already exists, go straight to game list
        if (std::filesystem::exists(TOKEN_PATH)) {
            brls::Application::pushActivity(new brls::Activity(new CheatSlipsPage()));
            return true;
        }

        // Prompt for credentials to fetch a fresh API token
        // Prompt email
        std::string email;
        bool emailOk = brls::Application::getImeManager()->openForText(
            [&email](std::string text) { email = text; },
            "CheatSlips email",
            "Enter your cheatslips.com email",
            128,
            "email@example.com");
        if (!emailOk || email.empty()) return true;

        // Prompt password
        std::string password;
        bool passOk = brls::Application::getImeManager()->openForText(
            [&password](std::string text) { password = text; },
            "CheatSlips password",
            "Enter your cheatslips.com password (visible)",
            128,
            "password");
        if (!passOk || password.empty()) return true;

        // Build request
        nlohmann::json bodyJson;
        bodyJson["email"] = email;
        bodyJson["password"] = password;
        std::vector<std::string> headers = {
            "Accept: application/json",
            "Content-Type: application/json",
            "charset: utf-8"
        };

        nlohmann::json tokenResponse;
        bool ok = download::postRequest(CHEATSLIPS_TOKEN_URL, headers, bodyJson.dump(), tokenResponse);

        if (!ok || !tokenResponse.contains("token")) {
            auto* err = new brls::Dialog("menu/cheats_menu/invalid_user_login"_i18n);
            err->addButton("hints/ok"_i18n, []() {});
            err->open();
            return true;
        }

        try {
            std::filesystem::create_directories(std::string(CONFIG_PATH));
            std::ofstream ofs(TOKEN_PATH);
            ofs << tokenResponse.dump();
            ofs.close();

            brls::Application::pushActivity(new brls::Activity(new CheatSlipsPage()));
        } catch (const std::exception& ex) {
            auto* err = new brls::Dialog(fmt::format("menu/cheats_menu/failed_to_save_token"_i18n, ex.what()));
            err->addButton("hints/ok"_i18n, []() {});
            err->open();
        }

        return true;
    });
    contentBox->addView(cheatSlips);
}

brls::View* CheatMenuTab::create() {
    return new CheatMenuTab();
}
