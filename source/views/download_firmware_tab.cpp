#include "views/download_firmware_tab.hpp"
#include "views/firmware_download_view.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include <switch.h>
#include <filesystem>
#include <fstream>
#include <fmt/format.h>

using namespace brls::literals;

DownloadFirmwareTab::DownloadFirmwareTab() : brls::Box(brls::Axis::COLUMN)
{
    this->inflateFromXMLRes("xml/tabs/download_firmware_tab.xml");
    this->setDescription();
    this->fetchFirmwareLinks();
}

brls::View* DownloadFirmwareTab::create()
{
    return new DownloadFirmwareTab();
}

void DownloadFirmwareTab::setDescription()
{
    firmwareDescLabel->setText("menu/firmware_download/description"_i18n);

    SetSysFirmwareVersion ver;
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&ver))) {
        firmwareVersionLabel->setText(fmt::format("menu/firmware_download/currentFW"_i18n + " {}", ver.display_version));
    } else {
        firmwareVersionLabel->setText("menu/firmware_download/currentFW"_i18n + " Unknown");
    }
    
    firmwareVersionLabel->setTextColor(nvgRGB(0, 255, 200));

    boxTitleLabel->setText("menu/firmware_download/available_firmwares"_i18n);
}

void DownloadFirmwareTab::fetchFirmwareLinks()
{
    nlohmann::json nxlinks;

    if (download::getRequest(NXLINKS_URL, nxlinks)) {
        brls::Logger::info("Successfully fetched nx-links JSON");

        if (nxlinks.contains("firmwares") && nxlinks["firmwares"].is_object()) {
            auto firmwares = nxlinks["firmwares"];
            auto firmwareLinks = download::getLinksFromJson(firmwares);

            // Reverse the order so newest firmware is at the top
            std::vector<std::pair<std::string, std::string>> reversedLinks(firmwareLinks.rbegin(), firmwareLinks.rend());

            for (const auto& [name, url] : reversedLinks) {
                auto* button = new brls::Button();
                button->setText(name);
                button->setMarginBottom(5);
                button->registerClickAction([name, url, this](brls::View* view) {
                    try {
                        brls::Logger::debug("Creating FirmwareDownloadView for: {}", name);
                        this->present(new FirmwareDownloadView(name, url));
                        brls::Logger::debug("View presented successfully");
                    } catch (const std::exception& e) {
                        brls::Logger::error("Exception while presenting FirmwareDownloadView: {}", e.what());
                    }
                    return true;
                });
                firmwareListBox->addView(button);
            }
        } else {
            brls::Logger::error("Firmwares section not found in nx-links JSON");
            auto* errorLabel = new brls::Label();
            errorLabel->setText("Error: Could not find firmware data in nx-links");
            errorLabel->setTextColor(nvgRGB(255, 100, 100));
            firmwareListBox->addView(errorLabel);
        }
    } else {
        brls::Logger::error("Failed to fetch nx-links JSON from: {}", NXLINKS_URL);
        auto* errorLabel = new brls::Label();
        errorLabel->setText("Error: Could not fetch data from nx-links. Check internet connection.");
        errorLabel->setTextColor(nvgRGB(255, 100, 100));
        firmwareListBox->addView(errorLabel);
    }
}
