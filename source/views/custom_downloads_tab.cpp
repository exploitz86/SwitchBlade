#include "views/custom_downloads_tab.hpp"
#include "views/atmosphere_download_view.hpp"
#include "views/simple_download_view.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"
#include <filesystem>
#include <fstream>
#include <switch.h>
#include <borealis.hpp>

using namespace brls::literals;

CustomDownloadsTab::CustomDownloadsTab()
{
    this->inflateFromXMLRes("xml/tabs/custom_downloads_tab.xml");

    descriptionLabel->setText("menu/custom_downloads/desc"_i18n);
    descriptionLabel->setTextColor(nvgRGB(150, 150, 150));
    warningLabel->setText("menu/custom_downloads/warning"_i18n);
    warningLabel->setTextColor(nvgRGB(250, 50, 50));

    this->loadCustomDownloads();
}

void CustomDownloadsTab::loadCustomDownloads()
{
    // Load custom_packs.json
    if (std::filesystem::exists(CUSTOM_PACKS_PATH)) {
        try {
            std::ifstream file(CUSTOM_PACKS_PATH);
            file >> customPacks;
            file.close();
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to parse custom_packs.json: {}", e.what());
            customPacks = nlohmann::json::object();
        }
    } else {
        customPacks = nlohmann::json::object();
    }

    this->refreshDownloadsList();
}

void CustomDownloadsTab::refreshDownloadsList()
{

    // Clear existing views
    customDownloadsBox->clearViews();

    // Ensure proper JSON structure
    if (!customPacks.is_object()) {
        customPacks = nlohmann::json::object();
    }
    if (!customPacks.contains("ams") || !customPacks["ams"].is_object()) {
        customPacks["ams"] = nlohmann::json::object();
    }
    if (!customPacks.contains("misc") || !customPacks["misc"].is_object()) {
        customPacks["misc"] = nlohmann::json::object();
    }

    // Helper lambda to add downloads from a category
    auto addCategoryDownloads = [this](const std::string& category, const std::string& categoryTitle, const std::string& description) {
        // Add section header
        auto* header = new brls::Header();
        header->setTitle(categoryTitle);
        header->setMarginBottom(10);
        customDownloadsBox->addView(header);

        // Add description label
        auto* descLabel = new brls::Label();
        descLabel->setText(description);
        descLabel->setTextColor(nvgRGB(150, 150, 150));
        descLabel->setFontSize(15);
        descLabel->setMarginBottom(10);
        customDownloadsBox->addView(descLabel);

        // Add button to add new download for this category
        auto* addButton = new brls::RadioCell();
        addButton->title->setText("menu/custom_downloads/add_custom_download"_i18n);
        addButton->setSelected(false);
        addButton->title->setTextColor(nvgRGB(0, 255, 200));
        addButton->registerClickAction([this, category](brls::View* view) {
            this->addCustomDownload(category);
            return true;
        });
        customDownloadsBox->addView(addButton);

        // Add existing downloads for this category
        if (customPacks[category].is_object() && !customPacks[category].empty()) {
            for (auto& [name, url] : customPacks[category].items()) {
                if (!url.is_string()) continue;

                auto* button = new brls::RadioCell();
                button->title->setText(name);
                button->setSelected(false);
                button->title->setTextColor(nvgRGB(0, 200, 255));

                // Download on click
                button->registerClickAction([this, category, name, url = url.get<std::string>()](brls::View* view) {
                    this->downloadCustomPack(category, name, url);
                    return true;
                });

                // Delete on X button
                button->registerAction("hints/delete"_i18n, brls::ControllerButton::BUTTON_X,
                    [this, category, name](brls::View* view) {
                        auto* dialog = new brls::Dialog(fmt::format("menu/custom_downloads/delete_custom_download"_i18n, name));
                        dialog->addButton("hints/yes"_i18n, [this, category, name]() {
                            this->deleteCustomDownload(category, name);
                        });
                        dialog->addButton("hints/no"_i18n, []() {});
                        dialog->open();
                        return true;
                    });

                customDownloadsBox->addView(button);
            }
        }

        // Add spacing after category
        auto* spacer = new brls::Rectangle();
        spacer->setHeight(20);
        spacer->setColor(nvgRGBA(0, 0, 0, 0));
        customDownloadsBox->addView(spacer);
    };

    // Add Atmosphere CFW section
    addCategoryDownloads("ams", "menu/custom_downloads/atmosphere_packages"_i18n,
        "menu/custom_downloads/atmosphere_packages_desc"_i18n);

    // Add Homebrew/Tools section
    addCategoryDownloads("misc", "menu/custom_downloads/homebrew_tools"_i18n,
        "menu/custom_downloads/homebrew_tools_desc"_i18n);
}

void CustomDownloadsTab::addCustomDownload(const std::string& category)
{
    std::string title;
    std::string url;

    // Prompt for title
    bool titleOk = brls::Application::getImeManager()->openForText(
        [&title](std::string text) { title = text; },
        "Custom Download Name",
        "Enter a name for this download",
        256,
        "My Custom Pack");

    if (!titleOk || title.empty()) {
        return;
    }

    // Prompt for URL
    bool urlOk = brls::Application::getImeManager()->openForText(
        [&url](std::string text) { url = text; },
        "Download URL",
        "Enter the direct download link (ZIP, 7Z, RAR, TAR, etc.)",
        512,
        "https://example.com/download.zip");

    if (!urlOk || url.empty()) {
        return;
    }

    // Add to JSON in the appropriate category
    customPacks[category][title] = url;
    this->saveCustomDownloads();

    // Show restart dialog
    auto* dialog = new brls::Dialog("menu/custom_downloads/custom_added"_i18n);
    dialog->addButton("menu/custom_downloads/restart_now"_i18n, []() {
        envSetNextLoad(NRO_PATH, NRO_PATH);
        brls::Application::quit();
    });
    dialog->addButton("menu/custom_downloads/later"_i18n, []() {});
    dialog->open();
}

void CustomDownloadsTab::deleteCustomDownload(const std::string& category, const std::string& name)
{
    if (customPacks.contains(category) && customPacks[category].contains(name)) {
        customPacks[category].erase(name);
        this->saveCustomDownloads();

        // Show restart dialog
        auto* dialog = new brls::Dialog("menu/custom_downloads/custom_deleted"_i18n);
        dialog->addButton("menu/custom_downloads/restart_now"_i18n, []() {
            envSetNextLoad(NRO_PATH, NRO_PATH);
            brls::Application::quit();
        });
        dialog->addButton("menu/custom_downloads/later"_i18n, []() {});
        dialog->open();
    }
}

void CustomDownloadsTab::downloadCustomPack(const std::string& category, const std::string& name, const std::string& url)
{
    brls::Logger::debug("Downloading custom pack: {} from {} (category: {})", name, url, category);

    try {
        if (category == "ams") {
            // For Atmosphere CFW downloads, use AtmosphereDownloadView
            // This asks about INI preservation and handles sysmodule flags
            this->present(new AtmosphereDownloadView(name, url, false, ""));
        } else {
            // For misc/homebrew/tools, use SimpleDownloadView
            // This extracts directly to root without any dialogs
            this->present(new SimpleDownloadView(name, url));
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Exception while creating download view: {}", e.what());
        auto* errorDialog = new brls::Dialog(fmt::format("Error: {}", e.what()));
        errorDialog->addButton("hints/ok"_i18n, []() {});
        errorDialog->open();
    }
}

void CustomDownloadsTab::saveCustomDownloads()
{
    try {
        std::filesystem::create_directories(CONFIG_PATH);
        std::ofstream file(CUSTOM_PACKS_PATH);
        file << customPacks.dump(4);  // Pretty print with 4 space indentation
        file.close();
        brls::Logger::info("Saved custom downloads to {}", CUSTOM_PACKS_PATH);
    } catch (const std::exception& e) {
        brls::Logger::error("Failed to save custom_packs.json: {}", e.what());
        auto* errorDialog = new brls::Dialog(fmt::format("Failed to save: {}", e.what()));
        errorDialog->addButton("hints/ok"_i18n, []() {});
        errorDialog->open();
    }
}

brls::View* CustomDownloadsTab::create()
{
    return new CustomDownloadsTab();
}
