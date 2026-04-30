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
#include <regex>
#include <sstream>
#include <vector>

using namespace brls::literals;

namespace {

std::string extractVersionString(const std::string& text)
{
    static const std::regex versionRegex(R"((\d+(?:\.\d+)+))");
    std::smatch match;

    if (std::regex_search(text, match, versionRegex) && match.size() > 1)
        return match[1].str();

    return "";
}

std::vector<int> parseVersionParts(const std::string& version)
{
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string token;

    while (std::getline(ss, token, '.')) {
        try {
            parts.push_back(std::stoi(token));
        } catch (...) {
            return {};
        }
    }

    return parts;
}

int compareVersionStrings(const std::string& a, const std::string& b)
{
    auto aParts = parseVersionParts(a);
    auto bParts = parseVersionParts(b);

    if (aParts.empty() || bParts.empty())
        return 0;

    const size_t maxSize = std::max(aParts.size(), bParts.size());
    aParts.resize(maxSize, 0);
    bParts.resize(maxSize, 0);

    for (size_t i = 0; i < maxSize; ++i) {
        if (aParts[i] < bParts[i])
            return -1;
        if (aParts[i] > bParts[i])
            return 1;
    }

    return 0;
}

} // namespace

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

    firmwareWarningLabel->setText("menu/firmware_download/warning"_i18n);

    SetSysFirmwareVersion ver;
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&ver))) {
        currentFirmwareVersion = ver.display_version;
        hasCurrentFirmwareVersion = true;
        firmwareVersionLabel->setText(fmt::format("menu/firmware_download/currentFW"_i18n + " {}", ver.display_version));
    } else {
        hasCurrentFirmwareVersion = false;
        firmwareVersionLabel->setText("menu/firmware_download/currentFW"_i18n + " Unknown");
    }
    
    firmwareWarningLabel->setTextColor(nvgRGB(250, 50, 50));

    firmwareVersionLabel->setTextColor(nvgRGB(0, 255, 200));

    boxTitleLabel->setText("menu/firmware_download/available_firmwares"_i18n);
}

bool DownloadFirmwareTab::isFirmwareOlderThanCurrent(const std::string& selectedFirmwareName) const
{
    if (!hasCurrentFirmwareVersion)
        return false;

    const std::string selectedVersion = extractVersionString(selectedFirmwareName);
    const std::string currentVersion = extractVersionString(currentFirmwareVersion);

    if (selectedVersion.empty() || currentVersion.empty())
        return false;

    return compareVersionStrings(selectedVersion, currentVersion) < 0;
}

void DownloadFirmwareTab::openFirmwareDownloadView(const std::string& name, const std::string& url)
{
    try {
        brls::Logger::debug("Creating FirmwareDownloadView for: {}", name);
        this->present(new FirmwareDownloadView(name, url));
        brls::Logger::debug("View presented successfully");
    } catch (const std::exception& e) {
        brls::Logger::error("Exception while presenting FirmwareDownloadView: {}", e.what());
    }
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
                    if (this->isFirmwareOlderThanCurrent(name)) {
                        const std::string selectedVersion = extractVersionString(name);
                        const std::string currentVersion = extractVersionString(this->currentFirmwareVersion);

                        auto* warningDialog = new brls::Dialog(
                            fmt::format("menu/firmware_download/downgrade_warning"_i18n, currentVersion, selectedVersion));
                        warningDialog->addButton("hints/cancel"_i18n, []() {});
                        warningDialog->addButton("hints/proceed"_i18n, [this, name, url]() {
                            this->openFirmwareDownloadView(name, url);
                        });
                        warningDialog->setCancelable(false);
                        warningDialog->open();
                    } else {
                        this->openFirmwareDownloadView(name, url);
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
