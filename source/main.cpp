#include <borealis.hpp>
#include <switch.h>
#include <curl/curl.h>
#include <filesystem>
#include <iostream>

#include "views/game_list_tab.hpp"
#include "views/tools_tab.hpp"
#include "views/cheats_mods_tab.hpp"
#include "views/mods_settings_tab.hpp"
#include "views/mod_install_tab.hpp"
#include "views/mod_browser_tab.hpp"
#include "views/mod_presets_tab.hpp"
#include "views/mod_options_tab.hpp"
#include "views/update_atmosphere_tab.hpp"
#include "views/cheat_menu_tab.hpp"
#include "views/cheats_tools_tab.hpp"
#include "views/cheats_settings_tab.hpp"
#include "views/download_firmware_tab.hpp"
#include "views/update_bootloaders_tab.hpp"
#include "views/custom_downloads_tab.hpp"
#include "views/app_update_view.hpp"
#include "activity/main_activity.hpp"
#include "utils/config.hpp"
#include "utils/utils.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include "utils/fs.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"

using namespace brls::literals;

void init();
void exit();

int main(int argc, char* argv[]) {     
    init();
    
    brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);

    std::filesystem::create_directories("sdmc:" + std::string(CONFIG_PATH));

    #ifdef NDEBUG //release
        // Using FILE* because brls::Logger::setLogOutput only takes FILE*, not std::ofstream
        std::string logPath = "sdmc:" + std::string(CONFIG_PATH) + "log.log";
        FILE* logFile = fopen(logPath.c_str(), "w");
        brls::Logger::setLogOutput(logFile);
    #endif

    {
        cfg::Config config;
        if(config.getAppLanguage() != "auto") {
            brls::Platform::APP_LOCALE_DEFAULT = config.getAppLanguage();
            brls::Logger::debug("Loaded translations for language {}", config.getAppLanguage());
        }
    }

    if(!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
    }

    brls::Application::setGlobalQuit(false);

    brls::Application::createWindow("SwitchBlade");
    brls::Application::setGlobalQuit(false);

    // Check if running in applet mode after Borealis init
    AppletType appletType = appletGetAppletType();
    if (appletType != AppletType_Application) {
        auto appletErrorView = new brls::Box(brls::Axis::COLUMN);
        appletErrorView->setMarginBottom(60);
        appletErrorView->setMarginTop(60);
        appletErrorView->setAlignItems(brls::AlignItems::CENTER);
        appletErrorView->setJustifyContent(brls::JustifyContent::CENTER);
        
        auto title = new brls::Label();
        title->setText("menu/main/applet_mode_detect"_i18n);
        title->setFontSize(32);
        title->setTextColor(nvgRGB(255, 30, 30));
        title->setMarginBottom(40);
        
        auto message = new brls::Label();
        message->setText("menu/main/applet_mode_desc"_i18n);
        message->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        message->setMarginBottom(40);
        
        auto exitButton = new brls::Button();
        exitButton->setText("hints/exit"_i18n);
        exitButton->setWidth(200);
        exitButton->registerClickAction([](brls::View* view) {
            brls::Application::quit();
            return true;
        });
        
        appletErrorView->addView(title);
        appletErrorView->addView(message);
        appletErrorView->addView(exitButton);
        
        brls::Application::pushActivity(new brls::Activity(appletErrorView));
        
        while(brls::Application::mainLoop()) ;
        
        exit();
        return 0;
    }

    //XML View
    brls::Application::registerXMLView("GameListTab", GameListTab::create);
    brls::Application::registerXMLView("ToolsTab", ToolsTab::create);
    brls::Application::registerXMLView("CheatsModsTab", CheatsModsTab::create);
    brls::Application::registerXMLView("ModsSettingsTab", ModsSettingsTab::create);
    brls::Application::registerXMLView("ModInstallTab", ModInstallTab::create);
    brls::Application::registerXMLView("ModBrowserTab", ModBrowserTab::create);
    brls::Application::registerXMLView("ModPresetsTab", ModPresetsTab::create);
    brls::Application::registerXMLView("ModOptionsTab", ModOptionsTab::create);
    brls::Application::registerXMLView("UpdateAtmosphereTab", UpdateAtmosphereTab::create);
    brls::Application::registerXMLView("CheatMenuTab", CheatMenuTab::create);
    brls::Application::registerXMLView("CheatsToolsTab", CheatsToolsTab::create);
    brls::Application::registerXMLView("CheatsSettingsTab", CheatsSettingsTab::create);
    brls::Application::registerXMLView("DownloadFirmwareTab", DownloadFirmwareTab::create);
    brls::Application::registerXMLView("UpdateBootloadersTab", UpdateBootloadersTab::create);
    brls::Application::registerXMLView("CustomDownloadsTab", CustomDownloadsTab::create);
    brls::Theme::getLightTheme().addColor("captioned_image/caption", nvgRGB(2, 176, 183));
    brls::Theme::getDarkTheme().addColor("captioned_image/caption", nvgRGB(51, 186, 227));

    // Add custom values to the style
    brls::getStyle().addMetric("about/padding_top_bottom", 50);
    brls::getStyle().addMetric("about/padding_sides", 75);
    brls::getStyle().addMetric("about/description_margin", 50);

    // Check for app updates
    std::string latestTag = utils::getLatestTag(TAGS_INFO);
    if (!latestTag.empty() && latestTag != APP_VERSION) {
        brls::Logger::info("Update available: {} -> {}", APP_VERSION, latestTag);
        try {
            brls::Application::pushActivity(new brls::Activity(new AppUpdateView(latestTag)));
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to create AppUpdateView: {}", e.what());
            brls::Application::pushActivity(new MainActivity());
        }
    } else {
        brls::Logger::info("App is up to date: {}", APP_VERSION);
        brls::Application::pushActivity(new MainActivity());
    }

    while(brls::Application::mainLoop()) ;

    exit();

    return -1;
}

void init() {
    setsysInitialize();
    socketInitializeDefault();
    nxlinkStdio();
    plInitialize(PlServiceType_User);
    nsInitialize();
    pmdmntInitialize();
    pminfoInitialize();
    splInitialize();
    fsInitialize();
    romfsInit();
    setInitialize();
    psmInitialize();
    nifmInitialize(NifmServiceType_User);
    lblInitialize();
    curl_global_init(CURL_GLOBAL_ALL);
}

void exit() {
    lblExit();
    nifmExit();
    psmExit();
    setExit();
    romfsExit();
    splExit();
    pminfoExit();
    pmdmntExit();
    nsExit();
    setsysExit();
    fsExit();
    plExit();
    socketExit();
    curl_global_cleanup();
}