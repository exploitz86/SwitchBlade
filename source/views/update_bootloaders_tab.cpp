#include "views/update_bootloaders_tab.hpp"
#include "views/bootloader_download_view.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"
#include "api/net.hpp"
#include <borealis.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <fstream>
#include <filesystem>

using namespace brls::literals;

UpdateBootloadersTab::UpdateBootloadersTab() : brls::Box(brls::Axis::COLUMN)
{
    try {
        this->inflateFromXMLRes("xml/tabs/update_bootloaders_tab.xml");
        this->fetchBootloaderLinks();
        this->setDescription();
        this->fetchHekateIplLinks();
        this->fetchPayloadLinks();
    } catch (const std::exception& e) {
        brls::Logger::error("Exception in UpdateBootloadersTab constructor: {}", e.what());
        throw;
    } catch (...) {
        brls::Logger::error("Unknown exception in UpdateBootloadersTab constructor");
        throw;
    }
}

brls::View* UpdateBootloadersTab::create()
{
    return new UpdateBootloadersTab();
}

void UpdateBootloadersTab::setDescription()
{
    bootloaders_desc->setText("menu/bootloader_update/bootloader_desc"_i18n);

    bootloaders_title->setText("menu/bootloader_update/bootloader_title"_i18n);
    bootloaders_title->setTextColor(nvgRGB(150, 150, 150));
    hekate_ipl_title->setText("menu/bootloader_update/hekate_ipl_title"_i18n);
    hekate_ipl_title->setTextColor(nvgRGB(150, 150, 150));
    payloads_title->setText("menu/bootloader_update/payloads_title"_i18n);
    payloads_title->setTextColor(nvgRGB(150, 150, 150));
}

void UpdateBootloadersTab::fetchBootloaderLinks()
{
    try {
        nlohmann::json nxlinks;

        if (download::getRequest(NXLINKS_URL, nxlinks)) {
            brls::Logger::info("Successfully fetched nx-links JSON");

            if (nxlinks.contains("bootloaders") && nxlinks["bootloaders"].is_object()) {
                auto bootloaders = nxlinks["bootloaders"];
                auto links = download::getLinksFromJson(bootloaders);

                // Show newest first
                std::vector<std::pair<std::string, std::string>> reversed(links.rbegin(), links.rend());

                for (const auto& [name, url] : reversed) {
                    auto* button = new brls::Button();
                    button->setText(name);
                    button->setMarginBottom(5);
                    button->registerClickAction([name, url, this](brls::View* view) {
                        try {
                            brls::Logger::debug("Creating BootloaderDownloadView for: {}", name);
                            this->present(new BootloaderDownloadView(name, url));
                            brls::Logger::debug("View presented successfully");
                        } catch (const std::exception& e) {
                            brls::Logger::error("Exception while presenting BootloaderDownloadView: {}", e.what());
                        }
                        return true;
                    });
                    bootloader_list->addView(button);
                }
            } else {
                brls::Logger::error("Bootloaders section not found in nx-links JSON");
                auto* errorLabel = new brls::Label();
                errorLabel->setText("menu/bootloader_update/no_bootloader_data"_i18n);
                errorLabel->setTextColor(nvgRGB(255, 100, 100));
                bootloader_list->addView(errorLabel);
            }
        } else {
            brls::Logger::error("Failed to fetch nx-links JSON from: {}", NXLINKS_URL);
            auto* errorLabel = new brls::Label();
            errorLabel->setText("menu/bootloader_update/cannot_fetch_nx"_i18n);
            errorLabel->setTextColor(nvgRGB(255, 100, 100));
            bootloader_list->addView(errorLabel);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Exception in fetchBootloaderLinks: {}", e.what());
    }
}

static std::string basenameFromUrl(const std::string& url)
{
    std::string path = url;
    auto qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos) return path.substr(slash + 1);
    return path;
}

void UpdateBootloadersTab::fetchHekateIplLinks()
{
    try {
        nlohmann::json nxlinks;

        if (download::getRequest(NXLINKS_URL, nxlinks)) {
            if (nxlinks.contains("hekate_ipl") && nxlinks["hekate_ipl"].is_object()) {
                auto hekateIpl = nxlinks["hekate_ipl"];
                auto links = download::getLinksFromJson(hekateIpl);
                std::vector<std::pair<std::string, std::string>> reversed(links.rbegin(), links.rend());

                for (const auto& [name, url] : reversed) {
                    auto* button = new brls::Button();
                    button->setText(name);
                    button->setMarginBottom(5);
                    auto urlCopy = std::make_shared<std::string>(url);
                    button->registerClickAction([urlCopy](brls::View* view) {
                        // UI-thread confirmation dialog; synchronous download on Yes
                        brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/overwrite_ipl"_i18n);
                        dialog->addButton("hints/no"_i18n, []() {});
                        dialog->addButton("hints/yes"_i18n, [urlCopy]() {
                            try { std::filesystem::create_directories(BOOTLOADER_PATH); } catch (...) {}
                            bool ok = net::downloadFile(*urlCopy, HEKATE_IPL_PATH);
                            
                            // Process copy_files.txt if download succeeded
                            if (ok && std::filesystem::exists(COPY_FILES_TXT)) {
                                std::ifstream file(COPY_FILES_TXT);
                                std::string line;
                                while (std::getline(file, line)) {
                                    if (line.empty() || line[0] == '#') continue;
                                    size_t pos = line.find('|');
                                    if (pos != std::string::npos) {
                                        std::string src = line.substr(0, pos);
                                        std::string dst = line.substr(pos + 1);
                                        if (std::filesystem::exists(src)) {
                                            try {
                                                std::filesystem::create_directories(std::filesystem::path(dst).parent_path());
                                                
                                                std::ifstream sourceFile(src, std::ios::binary);
                                                if (sourceFile.is_open()) {
                                                    std::ofstream destFile(dst, std::ios::binary | std::ios::trunc);
                                                    if (destFile.is_open()) {
                                                        destFile << sourceFile.rdbuf();
                                                        destFile.close();
                                                    }
                                                    sourceFile.close();
                                                }
                                            } catch (...) {}
                                        }
                                    }
                                }
                            }
                            
                            brls::Dialog* resultDialog = new brls::Dialog(
                                ok ? "menu/bootloader_update/ipl_success"_i18n : "menu/bootloader_update/ipl_failed"_i18n);
                            resultDialog->addButton("hints/ok"_i18n, []() {});
                            resultDialog->setCancelable(false);
                            resultDialog->open();
                        });
                        dialog->open();
                        return true;
                    });
                    hekate_ipl_list->addView(button);
                }
            } else {
                auto* errorLabel = new brls::Label();
                errorLabel->setText("menu/bootloader_update/no_bootloader_data"_i18n);
                errorLabel->setTextColor(nvgRGB(255, 100, 100));
                hekate_ipl_list->addView(errorLabel);
            }
        } else {
            auto* errorLabel = new brls::Label();
            errorLabel->setText("menu/bootloader_update/cannot_fetch_nx"_i18n);
            errorLabel->setTextColor(nvgRGB(255, 100, 100));
            hekate_ipl_list->addView(errorLabel);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Exception in fetchHekateIplLinks: {}", e.what());
    }
}

void UpdateBootloadersTab::fetchPayloadLinks()
{
    try {
        nlohmann::json nxlinks;

        if (download::getRequest(NXLINKS_URL, nxlinks)) {
            if (nxlinks.contains("payloads") && nxlinks["payloads"].is_object()) {
                auto payloads = nxlinks["payloads"];
                auto links = download::getLinksFromJson(payloads);
                std::vector<std::pair<std::string, std::string>> reversed(links.rbegin(), links.rend());

                for (const auto& [name, url] : reversed) {
                    auto* button = new brls::Button();
                    button->setText(name);
                    button->setMarginBottom(5);
                    auto nameCopy = std::make_shared<std::string>(name);
                    auto urlCopy = std::make_shared<std::string>(url);
                    button->registerClickAction([nameCopy, urlCopy](brls::View* view) {
                        try {
                            // Ensure payloads dir exists
                            try { std::filesystem::create_directories(BOOTLOADER_PL_PATH); } catch (...) {}

                            std::string filename = basenameFromUrl(*urlCopy);
                            std::string target = std::string(BOOTLOADER_PL_PATH) + filename;
                            bool ok = net::downloadFile(*urlCopy, target);
                            auto targetCopy = std::make_shared<std::string>(target);
                            brls::sync([ok, targetCopy, nameCopy]() {
                                brls::Dialog* dialog = new brls::Dialog(
                                    ok ? ("menu/bootloader_update/payload_downloaded"_i18n + *nameCopy + "menu/bootloader_update/saved_to"_i18n + *targetCopy)
                                       : ("menu/bootloader_update/payload_failed"_i18n + *nameCopy));
                                dialog->addButton("hints/ok"_i18n, []() {});
                                dialog->setCancelable(false);
                                dialog->open();
                            });
                        } catch (const std::exception& e) {
                            brls::Logger::error("Exception in payload download: {}", e.what());
                        }
                        return true;
                    });
                    payloads_list->addView(button);
                }
            } else {
                auto* errorLabel = new brls::Label();
                errorLabel->setText("menu/bootloader_update/no_bootloader_data"_i18n);
                errorLabel->setTextColor(nvgRGB(255, 100, 100));
                payloads_list->addView(errorLabel);
            }
        } else {
            auto* errorLabel = new brls::Label();
            errorLabel->setText("menu/bootloader_update/cannot_fetch_nx"_i18n);
            errorLabel->setTextColor(nvgRGB(255, 100, 100));
            payloads_list->addView(errorLabel);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Exception in fetchPayloadLinks: {}", e.what());
    }
}
