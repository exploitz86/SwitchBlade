#include "views/cheat_slips_download_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"
#include <fstream>
#include <fmt/format.h>
#include <switch.h>
#include <algorithm>

using namespace brls::literals;

CheatSlipsDownloadView::CheatSlipsDownloadView(uint64_t tid, const std::string& gameName) 
    : titleId(tid), gameTitle(gameName) {
    
    this->setTitle(fmt::format("CheatSlips - {}", gameName));
    // Initial placeholder to avoid showing previous view content while loading
    auto* placeholder = new brls::Box();
    placeholder->setAxis(brls::Axis::COLUMN);
    placeholder->setPadding(20, 20, 20, 20);
    auto* loading = new brls::Label();
    loading->setText("menu/cheats_menu/loading_cheats"_i18n);
    loading->setFontSize(18);
    placeholder->addView(loading);
    this->setContentView(placeholder);
    
    // Start loading cheats in background
    loadThread = std::thread([this]() { loadCheats(); });
}

void CheatSlipsDownloadView::loadCheats() {
    // Try to find build ID using robust methods
    getBuildId();
    
    // If still no build ID found, show error and provide back action
    if (this->buildId.empty()) {
        brls::sync([this]() {
            auto* box = new brls::Box();
            box->setAxis(brls::Axis::COLUMN);
            box->setPadding(20, 20, 20, 20);

            auto* label = new brls::Label();
            label->setText("menu/cheats_menu/build_id_not_detected"_i18n);
            label->setFontSize(18);
            box->addView(label);

            auto* scroll = new brls::ScrollingFrame();
            scroll->setContentView(box);
            scroll->setFocusable(true);

            this->setContentView(scroll);
            this->setTitle(fmt::format("CheatSlips - {}", gameTitle));
            brls::Application::giveFocus(scroll);
            this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
                [](brls::View* view) {
                    brls::Application::popActivity();
                    return true;
                },
                false);
        });
        return;
    }
    
    // Fetch cheats from CheatSlips API WITHOUT token to avoid consuming quota
    try {
        nlohmann::json cheatsInfo;
        std::string url = std::string(CHEATSLIPS_CHEATS_URL) + utils::formatApplicationId(this->titleId) + "/" + this->buildId;
        download::getRequest(url, cheatsInfo);
        
        brls::sync([this, cheatsInfo]() {
            displayCheats(cheatsInfo);
        });
    } catch (const std::exception& ex) {
        brls::Logger::error("Failed to load cheats from CheatSlips: {}", ex.what());
        brls::sync([this]() {
            brls::Dialog* dialog = new brls::Dialog("menu/cheats_menu/failed_to_load_cheats"_i18n);
            dialog->addButton("OK"_i18n, []() {});
            dialog->open();
        });
    }
}

void CheatSlipsDownloadView::displayCheats(const json& cheatsInfo) {
    auto* box = new brls::Box();
    box->setAxis(brls::Axis::COLUMN);
    box->setWidth(brls::View::AUTO);
    box->setHeight(brls::View::AUTO);
    box->setPadding(20, 20, 20, 20);
    
    if (cheatsInfo.find("cheats") == cheatsInfo.end()) {
        auto* noCheatLabel = new brls::Label();
        noCheatLabel->setText("menu/cheats_menu/no_cheats_for_game"_i18n);
        noCheatLabel->setFontSize(18);
        box->addView(noCheatLabel);

        // Try to list available Build IDs for this title
        try {
            nlohmann::json buildsInfo;
            std::string url = std::string(CHEATSLIPS_CHEATS_URL) + utils::formatApplicationId(this->titleId);
            download::getRequest(url, buildsInfo);

            auto isHex16 = [](const std::string& s) {
                if (s.size() != 16) return false;
                for (char c : s) {
                    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
                }
                return true;
            };

            std::string available;
            if (buildsInfo.is_object()) {
                for (auto& [k, v] : buildsInfo.items()) {
                    if (k == "message") continue; // skip generic message payloads
                    if (!isHex16(k)) continue;
                    if (!available.empty()) available += " ";
                    available += k;
                }
            } else if (buildsInfo.is_array()) {
                for (auto& v : buildsInfo) {
                    if (!v.is_string()) continue;
                    std::string s = v.get<std::string>();
                    if (!isHex16(s)) continue;
                    if (!available.empty()) available += " ";
                    available += s;
                }
            }

            if (!available.empty()) {
                auto* buildsLabel = new brls::Label();
                buildsLabel->setText("menu/cheats_menu/available_buildid"_i18n + available);
                buildsLabel->setFontSize(16);
                box->addView(buildsLabel);
            }
        } catch (...) {}

        // Set content and provide back action
        auto* scroll = new brls::ScrollingFrame();
        scroll->setContentView(box);
        scroll->setFocusable(true);

        this->setContentView(scroll);
        this->setTitle(fmt::format("CheatSlips - {}", gameTitle));
        brls::Application::giveFocus(scroll);
        this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
            [](brls::View* view) {
                brls::Application::popActivity();
                return true;
            },
            false);
        return;
    }
    
    auto* infoLabel = new brls::Label();
    infoLabel->setText(fmt::format("TID: {}\nBuild ID: {}",
                   utils::formatApplicationId(titleId), buildId));
    infoLabel->setFontSize(16);
    infoLabel->setMarginBottom(20);
    box->addView(infoLabel);
    
    // Process each cheat into a toggleable cell with a short preview
    for (const auto& p : cheatsInfo.at("cheats").items()) {
        const json& cheat = p.value();

        // Build a readable title from the provided titles array (no content preview to avoid quota)
        std::string title;
        if (cheat.contains("titles") && cheat.at("titles").is_array()) {
            // Show up to first two titles inline
            int shown = 0;
            for (const auto& t : cheat.at("titles")) {
                if (shown >= 2) break;
                if (!title.empty()) title += " | ";
                title += t.get<std::string>();
                shown++;
            }
        }
        if (title.empty()) title = fmt::format("Cheat #{}", cheat.value("id", 0));

        // Create toggle cell (compact radio-style with checkbox)
        auto* cell = new brls::RadioCell();
        cell->title->setText(title);
        cell->setSelected(false);

        // Keep subtitle empty to reduce row height and avoid using content

        cell->setMarginTop(4);
        cell->setMarginBottom(4);
        box->addView(cell);

        int cheatId = cheat.at("id").get<int>();
        // Toggle selection on click for intuitive behavior
        cell->registerClickAction([cell](brls::View* view) {
            cell->setSelected(!cell->getSelected());
            return true;
        });

        this->toggles.emplace_back(cell, cheatId);
    }
    
    // Wrap in a ScrollingFrame to ensure proper focus/highlight behavior
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(box);
    scroll->setFocusable(true);
    this->setContentView(scroll);
    this->setTitle(fmt::format("CheatSlips - {}", gameTitle));
    brls::Application::giveFocus(scroll);

    // Register actions
    // B: Download selected
    this->registerAction("menu/cheats_menu/download_selected"_i18n, brls::ControllerButton::BUTTON_B,
        [this](brls::View* view) {
            downloadSelectedCheats();
            return true;
        },
        false);

    // X: Select all
    this->registerAction("menu/cheats_menu/select_all"_i18n, brls::ControllerButton::BUTTON_X,
        [this](brls::View* view) {
            for (auto& pair : this->toggles) pair.first->setSelected(true);
            return true;
        },
        false);

    // Y: Clear all
    this->registerAction("menu/cheats_menu/clear_all"_i18n, brls::ControllerButton::BUTTON_Y,
        [this](brls::View* view) {
            for (auto& pair : this->toggles) pair.first->setSelected(false);
            return true;
        },
        false);
}

void CheatSlipsDownloadView::downloadSelectedCheats() {
    std::vector<int> selectedIds;
    // Gather only selected cheats
    for (const auto& pair : this->toggles) {
        if (pair.first->getSelected()) selectedIds.push_back(pair.second);
    }

    // If none selected, act as Back
    if (selectedIds.empty()) {
        brls::Application::popActivity();
        return;
    }
    
    // Read token from file
    json tokenData;
    std::string tokenStr;
    
    if (std::filesystem::exists(TOKEN_PATH)) {
        std::ifstream tokenFile(TOKEN_PATH);
        tokenFile >> tokenData;
        tokenFile.close();
        
        if (tokenData.find("token") != tokenData.end()) {
            tokenStr = tokenData.at("token").get<std::string>();
        }
    }
    
    try {
        nlohmann::json cheatsInfo;
        std::string url = std::string(CHEATSLIPS_CHEATS_URL) + utils::formatApplicationId(this->titleId) + "/" + this->buildId;

        std::vector<std::string> headers = {"accept: application/json"};
        // Load token if present
        try {
            if (std::filesystem::exists(TOKEN_PATH)) {
                nlohmann::json tokenJson;
                std::ifstream tf(TOKEN_PATH);
                tf >> tokenJson;
                tf.close();
                if (tokenJson.contains("token")) {
                    headers.push_back(std::string("X-API-TOKEN: ") + tokenJson["token"].get<std::string>());
                }
            }
        } catch (...) {}

        download::getRequest(url, cheatsInfo, headers);
        
        int successCount = 0;
        int errorCount = 0;
        std::string errorMsg;
        
        if (cheatsInfo.find("cheats") != cheatsInfo.end()) {
            for (const auto& p : cheatsInfo.at("cheats").items()) {
                const json& cheat = p.value();
                int cheatId = cheat.at("id").get<int>();
                
                if (std::find(selectedIds.begin(), selectedIds.end(), cheatId) != selectedIds.end()) {
                    std::string content = cheat.at("content").get<std::string>();
                    
                    if (content == "Quota exceeded for today !") {
                        errorCount++;
                        errorMsg = "menu/cheats_menu/quota_exceeded"_i18n;
                    } else {
                        // Write cheat to file
                        std::string cheatsPath = fmt::format("/atmosphere/contents/{}/cheats/", 
                                                            utils::formatApplicationId(this->titleId));
                        std::filesystem::create_directories(cheatsPath);
                        
                        std::string filename = fmt::format("{}/{}.txt", cheatsPath, this->buildId);
                        std::ofstream cheatsFile(filename, std::ios::app);
                        cheatsFile << content << "\n";
                        cheatsFile.close();
                        
                        successCount++;
                    }
                }
            }
        }
        
        // Show result dialog
        std::string resultMsg = fmt::format("menu/cheats_menu/downloaded_cheat_count"_i18n, successCount);
        if (errorCount > 0) {
            resultMsg += fmt::format("\nErrors: {}", errorMsg);
        }
        
        brls::Dialog* resultDialog = new brls::Dialog(resultMsg);
        resultDialog->addButton("OK"_i18n, [this]() {
            brls::Application::popActivity();
        });
        resultDialog->open();
        
    } catch (const std::exception& ex) {
        brls::Logger::error("Failed to download cheats: {}", ex.what());
        brls::Dialog* errorDialog = new brls::Dialog("menu/cheats_menu/download_failed"_i18n);
        errorDialog->addButton("OK"_i18n, []() {});
        errorDialog->open();
    }
}

void CheatSlipsDownloadView::showCheatsContent(const json& titles) {
    // For now, just log the titles
    std::string titleList;
    for (const auto& title : titles) {
        titleList += title.get<std::string>() + "\n";
    }
    
    brls::Dialog* dialog = new brls::Dialog(titleList);
    dialog->addButton("OK"_i18n, []() {});
    dialog->open();
}

void CheatSlipsDownloadView::getBuildId() {
    // Try to get from dmnt first (if game is running)
    getBuildIdFromDmnt();
    
    // If that failed, try versions database
    if (buildId.empty()) {
        getBuildIdFromVersions();
    }
}

void CheatSlipsDownloadView::getBuildIdFromDmnt() {
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

void CheatSlipsDownloadView::getBuildIdFromVersions() {
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

brls::View* CheatSlipsDownloadView::create() {
    return new CheatSlipsDownloadView(0, "");
}
