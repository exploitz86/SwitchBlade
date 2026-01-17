#include "views/update_atmosphere_tab.hpp"
#include "views/atmosphere_download_view.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"
#include "utils/current_cfw.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <switch.h>
#include <borealis.hpp>

using namespace brls::literals;

std::string readAtmosphereVersion() {
    return CurrentCfw::getAmsInfo();
}

std::string readAscentVersion() {
    std::string ascentFile = "/atmosphere/ASCENT";
    if (std::filesystem::exists(ascentFile)) {
        std::ifstream file(ascentFile);
        std::string version;
        if (std::getline(file, version)) {
            return version;
        }
        return "Installed (version unknown)";
    }
    return "Not installed";
}

std::string getDeviceRevision() {
    // Detect hardware revision using setsys
    SetSysProductModel model;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        if (model == SetSysProductModel_Nx || model == SetSysProductModel_Copper) {
            return "Erista";
        } else {
            return "Mariko";
        }
    }
    return "Unknown";
}

UpdateAtmosphereTab::UpdateAtmosphereTab()
{
    this->inflateFromXMLRes("xml/tabs/update_atmosphere_tab.xml");
    this->createAtmosphereUpdateUI();
}

void UpdateAtmosphereTab::createAtmosphereUpdateUI()
{
    // Update labels with actual values
    atmosphereDescriptionLabel->setText("menu/ams_update/atmosphere_desc"_i18n);
    atmosphereDescriptionLabel->setTextColor(nvgRGB(150, 150, 150));

    currentAtmosphereLabel->setText("menu/ams_update/current_ams"_i18n + readAtmosphereVersion());
    currentAtmosphereLabel->setTextColor(nvgRGB(0, 255, 200));

    revisionLabel->setText("menu/ams_update/revision"_i18n + getDeviceRevision());
    revisionLabel->setTextColor(nvgRGB(150, 150, 150));

    ascentDescriptionLabel->setText("menu/ams_update/ascent_desc"_i18n);
    ascentDescriptionLabel->setTextColor(nvgRGB(150, 150, 150));

    currentAscentLabel->setText("menu/ams_update/current_ascent"_i18n + readAscentVersion());
    currentAscentLabel->setTextColor(nvgRGB(0, 255, 200));

    // Latest HOS Label to be implemented
    // latestHOSLabel->setText("Latest supported HOS: Checking...");
    // atestHOSLabel->setTextColor(nvgRGB(150, 150, 150));

    // Fetch and populate CFW links
    this->fetchCFWLinks();
}

void UpdateAtmosphereTab::fetchCFWLinks()
{
    nlohmann::json nxlinks;

    if (download::getRequest(NXLINKS_URL, nxlinks)) {
        brls::Logger::info("Successfully fetched nx-links JSON");

        // Extract Hekate URL from the hekate section
        std::string hekateUrl = "";
        if (nxlinks.contains("hekate") && nxlinks["hekate"].is_object()) {
            auto hekateLinks = download::getLinksFromJson(nxlinks["hekate"]);
            if (!hekateLinks.empty()) {
                hekateUrl = hekateLinks.begin()->second;  // Get the first Hekate URL
                brls::Logger::info("Found Hekate URL: {}", hekateUrl);
            }
        }

        // Extract CFW section
        if (nxlinks.contains("cfws") && nxlinks["cfws"].is_object()) {
            auto cfws = nxlinks["cfws"];

            // Add Atmosphere versions (offer Hekate download)
            if (cfws.contains("Atmosphere") && cfws["Atmosphere"].is_object()) {
                auto atmosphereLinks = download::getLinksFromJson(cfws["Atmosphere"]);

                for (const auto& [name, url] : atmosphereLinks) {
                    auto* button = new brls::Button();
                    button->setText(name);
                    button->setMarginBottom(5);
                    button->registerClickAction([name, url, hekateUrl, this](brls::View* view) {
                        try {
                            brls::Logger::debug("Creating AtmosphereDownloadView for: {} (with Hekate offer)", name);
                            this->present(new AtmosphereDownloadView(name, url, true, hekateUrl));
                            brls::Logger::debug("View presented successfully");
                        } catch (const std::exception& e) {
                            brls::Logger::error("Exception while presenting AtmosphereDownloadView: {}", e.what());
                        }
                        return true;
                    });
                    atmosphereVersionsBox->addView(button);
                }
            }

            // Add Ascent versions (no Hekate offer - already bundled)
            if (cfws.contains("Ascent") && cfws["Ascent"].is_object()) {
                auto ascentLinks = download::getLinksFromJson(cfws["Ascent"]);

                for (const auto& [name, url] : ascentLinks) {
                    auto* button = new brls::Button();
                    button->setText(name);
                    button->setMarginBottom(5);
                    button->registerClickAction([name, url, this](brls::View* view) {
                        try {
                            brls::Logger::debug("Creating AtmosphereDownloadView for: {} (Hekate bundled)", name);
                            this->present(new AtmosphereDownloadView(name, url, false, ""));
                            brls::Logger::debug("View presented successfully");
                        } catch (const std::exception& e) {
                            brls::Logger::error("Exception while presenting AtmosphereDownloadView: {}", e.what());
                        }
                        return true;
                    });
                    ascentVersionsBox->addView(button);
                }
            }
        } else {
            brls::Logger::error("CFWs section not found in nx-links JSON");
            auto* errorLabel = new brls::Label();
            errorLabel->setText("menu/ams_update/no_cfw_data"_i18n);
            errorLabel->setTextColor(nvgRGB(255, 100, 100));
            atmosphereVersionsBox->addView(errorLabel);
        }
    } else {
        brls::Logger::error("Failed to fetch nx-links JSON from: {}", NXLINKS_URL);
        auto* errorLabel = new brls::Label();
        errorLabel->setText("menu/ams_update/cannot_fetch_nx"_i18n);
        errorLabel->setTextColor(nvgRGB(255, 100, 100));
        atmosphereVersionsBox->addView(errorLabel);
    }
}

brls::View* UpdateAtmosphereTab::create()
{
    return new UpdateAtmosphereTab();
}
