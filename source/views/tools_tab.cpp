#include "views/tools_tab.hpp"
#include "views/outdated_titles_page.hpp"
#include "views/joycon_color_page.hpp"
#include "views/procon_color_page.hpp"
#include "views/payload_page.hpp"
#include "views/net_page.hpp"
#include "utils/config.hpp"
#include "utils/fs.hpp"

#include <filesystem>

using namespace brls::literals;

ToolsTab::ToolsTab() {
    this->inflateFromXMLRes("xml/tabs/tools_tab.xml");
    cfg::Config config;

    // Add Missing updates menu item
    auto* outdatedTitlesItem = new brls::RadioCell();
    outdatedTitlesItem->title->setText("menu/tools_tab/outdated_titles"_i18n);
    outdatedTitlesItem->setSelected(false);
    outdatedTitlesItem->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new brls::Activity(new OutdatedTitlesPage()));
        return true;
    });
    tools_box->addView(outdatedTitlesItem);

    // Add Joy-Con color changer menu item
    auto* joyconColorItem = new brls::RadioCell();
    joyconColorItem->title->setText("menu/tools_tab/joycon_color"_i18n);
    joyconColorItem->setSelected(false);
    joyconColorItem->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new brls::Activity(new JoyConColorPage()));
        return true;
    });
    tools_box->addView(joyconColorItem);

    // Add Pro Controller color changer menu item
    auto* proconColorItem = new brls::RadioCell();
    proconColorItem->title->setText("menu/tools_tab/procon_color"_i18n);
    proconColorItem->setSelected(false);
    proconColorItem->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new brls::Activity(new ProConColorPage()));
        return true;
    });
    tools_box->addView(proconColorItem);

    // Add Payload injector menu item
    auto* payloadItem = new brls::RadioCell();
    payloadItem->title->setText("menu/tools_tab/inject_payload"_i18n);
    payloadItem->setSelected(false);
    payloadItem->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new brls::Activity(new PayloadPage()));
        return true;
    });
    tools_box->addView(payloadItem);

    // Add Edit internet settings menu item
    auto* netSettingsItem = new brls::RadioCell();
    netSettingsItem->title->setText("menu/tools_tab/net_settings"_i18n);
    netSettingsItem->setSelected(false);
    netSettingsItem->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new brls::Activity(new NetPage()));
        return true;
    });
    tools_box->addView(netSettingsItem);

    // Add Clean up Atmosphere reports menu item
    auto* cleanupItem = new brls::RadioCell();
    cleanupItem->title->setText("menu/tools_tab/cleanup_reports"_i18n);
    cleanupItem->setSelected(false);
    cleanupItem->registerClickAction([this](brls::View* view) {
        auto* dialog = new brls::Dialog("menu/tools_tab/cleanup_confirm"_i18n);

        dialog->addButton("hints/delete"_i18n, []() {
            std::vector<std::string> reportDirs = {
                "/atmosphere/crash_reports",
                "/atmosphere/fatal_reports",
                "/atmosphere/fatal_errors",
                "/atmosphere/erpt_reports"
            };

            int deletedCount = 0;

            for (const auto& dir : reportDirs) {
                if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                    if (fs::removeDir(dir)) {
                        deletedCount++;
                        brls::Logger::info("Deleted: {}", dir);
                    } else {
                        brls::Logger::error("Failed to delete: {}", dir);
                    }
                }
            }

            auto* resultDialog = new brls::Dialog(
                deletedCount > 0
                ?  std::to_string(deletedCount) + "menu/tools_tab/cleaned_up_numer"_i18n
                : "menu/tools_tab/no_reports_found"_i18n
            );
            resultDialog->addButton("hints/ok"_i18n, []() {});
            resultDialog->open();
        });

        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->open();

        return true;
    });
    tools_box->addView(cleanupItem);

    #ifndef NDEBUG
    auto debug_cell = new brls::BooleanCell();
    debug_cell->init("menu/tools_tab/debug"_i18n, brls::Application::isDebuggingViewEnabled(), [](bool value){
        brls::Application::enableDebuggingView(value);
        brls::sync([value](){
            brls::Logger::info("{} the debug layer", value ? "Open" : "Close");
        });
    });
    tools_box->addView(debug_cell);

    auto wireframe_cell = new brls::BooleanCell();
    wireframe_cell->init("menu/tools_tab/debug"_i18n, brls::Application::isDebuggingViewEnabled(), [](bool value){
        brls::sync([value](){
            brls::Logger::info("{} wireframe", value ? "Enable" : "Disable");
        });
        cfg::Config config;
        config.setWireframe(value);
    });
    tools_box->addView(wireframe_cell);
    #endif

    std::filesystem::path i18n_path = "romfs:/i18n";
    std::vector<std::string> languages = {"auto"};
    for (const auto& entry : std::filesystem::directory_iterator(i18n_path)) {
        if (entry.is_directory()) {
            languages.push_back(entry.path().filename().string());
        }
    }

    int selected = 0;
    std::string current_language = config.getAppLanguage();
    for(auto i = 0; i < languages.size(); ++i) {
        if(languages[i] == current_language) {
            selected = i;
            break;
        }
    }

    auto* language_selector_cell = new brls::SelectorCell();
    language_selector_cell->init("menu/tools_tab/language"_i18n, languages, selected, [](int selected){}, [languages = std::move(languages)](int selected) {
        cfg::Config config;
        config.setAppLanguage(languages[selected]);
    });
    tools_box->addView(language_selector_cell);
}

brls::View* ToolsTab::create()
{
    return new ToolsTab;
}