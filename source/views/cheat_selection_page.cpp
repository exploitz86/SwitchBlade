#include "views/cheat_selection_page.hpp"
#include "views/installed_cheats_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include "api/net.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>

using namespace brls::literals;

CheatSelectionPage::CheatSelectionPage(u64 tid, const std::string& name, bool isGfxCheats)
    : titleId(tid), gameName(name), isGfxCheats(isGfxCheats) {

    // Create scrolling frame first
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setWidth(brls::View::AUTO);
    list->setHeight(brls::View::AUTO);
    list->setPadding(20, 20, 20, 20);

    // Get Build ID
    getBuildId();

    // Add info section
    std::string tidFormatted = utils::formatApplicationId(titleId);
    brls::Label* tidLabel = new brls::Label();
    tidLabel->setText("Title ID: " + tidFormatted);
    tidLabel->setFontSize(16);
    tidLabel->setMarginBottom(10);
    tidLabel->setFocusable(false);
    list->addView(tidLabel);

    if (!buildId.empty()) {
        brls::Label* bidLabel = new brls::Label();
        bidLabel->setText("Build ID: " + buildId);
        bidLabel->setFontSize(16);
        bidLabel->setMarginBottom(20);
        bidLabel->setFocusable(false);
        list->addView(bidLabel);

        populateCheats();
    } else {
        brls::Label* errorLabel = new brls::Label();
        errorLabel->setText("menu/cheats_menu/build_id_not_detected"_i18n);
        errorLabel->setFontSize(18);
        errorLabel->setFocusable(false);
        list->addView(errorLabel);
    }

    brls::Logger::debug("CheatSelectionPage: list has {} children", list->getChildren().size());

    scrollFrame->setContentView(list);
    scrollFrame->setFocusable(true);
    this->setContentView(scrollFrame);

    // Set title and icon AFTER content is set
    this->setTitle(name);
    // this->setIcon("romfs:/gui/app_icon.png");

    // Give focus to the scrolling frame explicitly
    brls::Application::giveFocus(scrollFrame);

    // Register custom back button handler to ensure proper cleanup
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);  // hidden=true to override default behavior

    // Register X button handler to view installed cheats (only if Build ID is available)
    if (!buildId.empty()) {
        // Capture by value to avoid 'this' pointer issues
        u64 tidCopy = titleId;
        std::string gameNameCopy = gameName;
        std::string buildIdCopy = buildId;

        this->registerAction("menu/cheats_menu/view_installed_cheats"_i18n, brls::ControllerButton::BUTTON_X,
            [tidCopy, gameNameCopy, buildIdCopy](brls::View* view) {
                // Construct cheat file path
                std::string cheatFilePath = AMS_CONTENTS + utils::formatApplicationId(tidCopy) + "/cheats/" + buildIdCopy + ".txt";

                // Check if cheat file exists
                if (!std::filesystem::exists(cheatFilePath)) {
                    brls::Dialog* dialog = new brls::Dialog("menu/cheats_menu/no_cheats_installed"_i18n + cheatFilePath);
                    dialog->addButton("hints/ok"_i18n, []() {});
                    dialog->open();
                    return true;
                }

                // Create and push the installed cheats view
                auto* installedView = new InstalledCheatsView(gameNameCopy, buildIdCopy, cheatFilePath, tidCopy);
                brls::Application::pushActivity(new brls::Activity(installedView));
                return true;
            });
    }
}

void CheatSelectionPage::getBuildId() {
    // Try to get from dmnt first (if game is running)
    getBuildIdFromDmnt();
    
    // If that failed, try versions database
    if (buildId.empty()) {
        getBuildIdFromVersions();
    }
}

void CheatSelectionPage::getBuildIdFromDmnt() {
    // This requires the game to be running
    // Uses Atmosphere's dmnt:cht service
    Service dmntcht = {};

    Result rc = smGetService(&dmntcht, "dmnt:cht");
    if (R_FAILED(rc)) {
        brls::Logger::debug("Failed to get dmnt:cht service: 0x{:X}", rc);
        return;
    }

    // Query/initialize the service (command 65003)
    rc = serviceDispatch(&dmntcht, 65003);
    if (R_FAILED(rc)) {
        brls::Logger::debug("Failed to query dmnt service: 0x{:X}", rc);
        serviceClose(&dmntcht);
        return;
    }

    // Get current process metadata (command 65002)
    struct DmntCheatProcessMetadata {
        u64 process_id;
        u64 title_id;
        u8 main_nso_build_id[0x20];
        u64 main_nso_extents_base;
        u64 main_nso_extents_size;
        u64 heap_extents_base;
        u64 heap_extents_size;
    } metadata = {};

    rc = serviceDispatchOut(&dmntcht, 65002, metadata);
    if (R_SUCCEEDED(rc)) {
        if (metadata.title_id == titleId) {
            // Extract first 8 bytes and convert to hex
            u64 bid_value = 0;
            memcpy(&bid_value, metadata.main_nso_build_id, sizeof(u64));
            buildId = fmt::format("{:016X}", __builtin_bswap64(bid_value));
            brls::Logger::debug("Got Build ID from dmnt: {}", buildId);
        } else {
            brls::Logger::debug("dmnt title_id ({:016X}) doesn't match requested ({:016X})", metadata.title_id, titleId);
        }
    } else {
        brls::Logger::debug("Failed to dispatch dmnt service: 0x{:X}", rc);
    }

    serviceClose(&dmntcht);
}

void CheatSelectionPage::getBuildIdFromVersions() {
    // Fetch versions JSON from GitHub
    std::string url = std::string(VERSIONS_DIRECTORY) + utils::formatApplicationId(titleId) + ".json";

    std::string response = utils::downloadFileToString(url);
    if (response.empty()) {
        brls::Logger::debug("Failed to download versions JSON for title");
        return;
    }

    try {
        nlohmann::json versionsJson = nlohmann::json::parse(response);

        // Get highest installed game version (handles base + updates)
        NsApplicationContentMetaStatus* metaStatuses = new NsApplicationContentMetaStatus[100];
        s32 count = 0;

        if (R_SUCCEEDED(nsListApplicationContentMetaStatus(titleId, 0, metaStatuses, 100, &count))) {
            u32 highestVersion = 0;

            // Find the highest version (most recent update)
            for (s32 i = 0; i < count; i++) {
                if (metaStatuses[i].version > highestVersion) {
                    highestVersion = metaStatuses[i].version;
                }
            }

            brls::Logger::debug("Found {} content entries, highest version: {}", count, highestVersion);

            std::string versionStr = std::to_string(highestVersion);
            if (versionsJson.contains(versionStr)) {
                buildId = versionsJson[versionStr].get<std::string>();
                brls::Logger::debug("Got Build ID from versions JSON: {}", buildId);
            } else {
                brls::Logger::debug("Version {} not found in versions JSON", versionStr);
            }
        } else {
            brls::Logger::debug("Failed to list application content meta status");
        }

        delete[] metaStatuses;
    } catch (const std::exception& e) {
        brls::Logger::error("Exception parsing versions JSON: {}", e.what());
    } catch (...) {
        brls::Logger::error("Unknown exception parsing versions JSON");
    }
}

void CheatSelectionPage::populateCheats() {
    // Fetch cheats JSON for this game - use GFX directory if graphics cheats
    const char* directory = isGfxCheats ? CHEATS_DIRECTORY_GFX : CHEATS_DIRECTORY_GBATEMP;
    std::string url = std::string(directory) + utils::formatApplicationId(titleId) + ".json";
    
    std::string response = utils::downloadFileToString(url);
    if (response.empty()) {
        brls::Label* errorLabel = new brls::Label();
        errorLabel->setText("menu/cheats_menu/no_cheats_available"_i18n);
        errorLabel->setFontSize(18);
        errorLabel->setFocusable(false);
        list->addView(errorLabel);
        return;
    }
    
    try {
        // Strip UTF-8 BOM if present
        if (response.size() >= 3 && (unsigned char)response[0] == 0xEF && (unsigned char)response[1] == 0xBB && (unsigned char)response[2] == 0xBF) {
            response = response.substr(3);
            brls::Logger::debug("Stripped UTF-8 BOM from response");
        }

        // Strip null bytes and other problematic characters
        response.erase(std::remove(response.begin(), response.end(), '\0'), response.end());
        
        // Log first 100 chars for debugging
        std::string preview = response.substr(0, std::min(response.size(), size_t(100)));
        brls::Logger::debug("CheatSelectionPage: Response preview ({}B): {}", response.size(), preview);
        
        // Check for obvious HTML error pages first (before trying to parse as JSON)
        if (response.find("<!DOCTYPE") != std::string::npos || 
            response.find("<html") != std::string::npos) {
            
            brls::Label* errorLabel = new brls::Label();
            errorLabel->setText("menu/cheats_menu/no_cheats_for_game"_i18n);
            errorLabel->setFontSize(18);
            errorLabel->setFocusable(false);
            list->addView(errorLabel);
            brls::Logger::warning("CheatSelectionPage: Got HTML error page for TID {}", utils::formatApplicationId(titleId));
            return;
        }
        
        // Check for short error messages like "404: Not Found"
        if (response.size() < 50 && response.find("404") != std::string::npos) {
            brls::Label* errorLabel = new brls::Label();
            errorLabel->setText("menu/cheats_menu/no_cheats_for_game"_i18n);
            errorLabel->setFontSize(18);
            errorLabel->setFocusable(false);
            list->addView(errorLabel);
            brls::Logger::warning("CheatSelectionPage: Got 404 error for TID {}: {}", 
                                utils::formatApplicationId(titleId), preview);
            return;
        }

        // Parse without throwing; handle errors explicitly
        nlohmann::json cheatsJson = nlohmann::json::parse(response, nullptr, false);
        if (cheatsJson.is_discarded()) {
            brls::Label* errorLabel = new brls::Label();
            std::string errorText = "menu/cheats_menu/cheat_parse_error"_i18n + 
                                   response.substr(0, std::min(response.size(), size_t(80))) + "...";
            errorLabel->setText(errorText);
            errorLabel->setFontSize(16);
            errorLabel->setFocusable(false);
            list->addView(errorLabel);
            brls::Logger::error("CheatSelectionPage: JSON parse failed for TID {} (BID {}). Response size {} bytes. Preview: {}", 
                              utils::formatApplicationId(titleId), buildId, response.size(), preview);
            return;
        }
        
        // Check if this Build ID has cheats
        if (!cheatsJson.contains(buildId)) {
            std::string available;
            for (auto& [key, val] : cheatsJson.items()) {
                if (key == "attribution") continue;
                if (!available.empty()) available += " ";
                available += key;
            }

            brls::Label* errorLabel = new brls::Label();
            if (!available.empty()) {
                errorLabel->setText("menu/cheats_menu/no_cheat_for_build_id2"_i18n + available);
            } else {
                errorLabel->setText("menu/cheats_menu/no_cheat_for_build_id"_i18n);
            }
            errorLabel->setFontSize(18);
            errorLabel->setFocusable(false);
            list->addView(errorLabel);
            return;
        }
        
        // Add each cheat as a button
        int cheatCount = 0;
        for (auto& [cheatName, cheatContent] : cheatsJson[buildId].items()) {
            if (cheatName == "attribution") continue; // Skip metadata

            std::string content = cheatContent.get<std::string>();

            std::string cheatNameCopy = cheatName;
            std::string contentCopy = content;

            auto* item = new brls::RadioCell();
            item->title->setText(cheatName);
            item->setSelected(false);

            // Capture needed data by value to avoid dangling 'this' pointer
            u64 tidCopy = titleId;
            std::string bidCopy = buildId;

            item->registerClickAction([tidCopy, bidCopy, contentCopy, cheatNameCopy](brls::View* view) {
                // Write cheat inline to avoid needing 'this'
                std::string basePath = AMS_CONTENTS;
                std::string cheatPath = basePath + utils::formatApplicationId(tidCopy) + "/cheats/";
                std::filesystem::create_directories(cheatPath);
                std::string cheatFile = cheatPath + bidCopy + ".txt";

                std::ofstream file(cheatFile, std::ios::app);
                if (file.is_open()) {
                    file << "\n\n" << contentCopy;
                    file.close();
                }

                brls::Dialog* dialog = new brls::Dialog(fmt::format("menu/cheats_menu/cheat_download_success"_i18n, cheatNameCopy));
                dialog->addButton("hints/ok"_i18n, []() {});
                dialog->open();
                return true;
            });
            list->addView(item);
            cheatCount++;
        }

        // If no cheats were added (all were attribution)
        if (cheatCount == 0) {
            brls::Logger::warning("No cheats found in JSON (all attribution)");
            brls::Label* errorLabel = new brls::Label();
            errorLabel->setText("menu/cheats_menu/no_cheat_for_build_id"_i18n);
            errorLabel->setFontSize(18);
            errorLabel->setFocusable(false);
            list->addView(errorLabel);
        } else {
            brls::Logger::info("Added {} cheats to list", cheatCount);
        }
    } catch (...) {
        brls::Label* errorLabel = new brls::Label();
        errorLabel->setText("menu/cheats_menu/cheat_parse_error2"_i18n);
        errorLabel->setFontSize(18);
        errorLabel->setFocusable(false);
        list->addView(errorLabel);
    }
}


