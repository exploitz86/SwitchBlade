#include "views/downloaded_cheats_list_view.hpp"
#include "views/installed_cheats_view.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <regex>
#include <sstream>

using namespace brls::literals;

static bool isHex16(const std::string& s) {
    if (s.size() != 16) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

DownloadedCheatsListView::DownloadedCheatsListView() {
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    auto* list = new brls::Box();
    list->setAxis(brls::Axis::COLUMN);
    list->setWidth(brls::View::AUTO);
    list->setHeight(brls::View::AUTO);
    list->setPadding(20, 20, 20, 20);

    std::unordered_map<std::string, std::string> titleNameByTid;
    for (auto& pair : utils::getInstalledGames()) {
        titleNameByTid[pair.second] = pair.first;
    }

    struct CheatItem { std::string bid; std::string path; };
    struct Group { std::string tid; std::string name; std::vector<CheatItem> items; std::filesystem::path cheatsDir; };
    std::vector<Group> groups;

    std::filesystem::path contents(AMS_CONTENTS);
    if (std::filesystem::exists(contents) && std::filesystem::is_directory(contents)) {
        for (auto& entry : std::filesystem::directory_iterator(contents)) {
            if (!entry.is_directory()) continue;
            std::string tidDir = entry.path().filename().string();
            if (!isHex16(tidDir)) continue;

            std::filesystem::path cheatsDir = entry.path() / "cheats";
            if (!std::filesystem::exists(cheatsDir) || !std::filesystem::is_directory(cheatsDir)) continue;

            Group group;
            group.tid = tidDir;
            group.name = titleNameByTid.count(tidDir) ? titleNameByTid[tidDir] : ("TID " + tidDir);
            group.cheatsDir = cheatsDir;

            for (auto& cheatEntry : std::filesystem::directory_iterator(cheatsDir)) {
                if (!cheatEntry.is_regular_file()) continue;
                auto p = cheatEntry.path();
                if (p.extension() != ".txt") continue;
                std::string bid = p.stem().string();
                if (!isHex16(bid)) continue;

                group.items.push_back({bid, p.string()});
            }

            if (!group.items.empty()) {
                // sort BIDs for this group
                std::sort(group.items.begin(), group.items.end(), [](const CheatItem& a, const CheatItem& b){ return a.bid < b.bid; });
                groups.push_back(std::move(group));
            }
        }
    }

    // Sort groups by game name (case-sensitive alphabetical)
    std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b){ return a.name < b.name; });

    // Build UI
    if (groups.empty()) {
        brls::Label* empty = new brls::Label();
        empty->setText("menu/cheats_menu/no_downloaded_cheats"_i18n);
        empty->setFontSize(18);
        empty->setFocusable(false);
        list->addView(empty);
    } else {
        for (const auto& g : groups) {
            // Group header - larger, bold, more spacing
            auto* headerLabel = new brls::Label();
            headerLabel->setText(g.name);
            headerLabel->setFontSize(24);
            headerLabel->setMarginTop(20);
            headerLabel->setMarginBottom(5);
            headerLabel->setFocusable(false);
            list->addView(headerLabel);

            // Subtitle with TID - smaller gray text
            auto* tidLabel = new brls::Label();
            tidLabel->setText("Title ID: " + g.tid);
            tidLabel->setFontSize(14);
            tidLabel->setTextColor(nvgRGB(150, 150, 150));
            tidLabel->setMarginBottom(10);
            tidLabel->setFocusable(false);
            list->addView(tidLabel);

            // Delete-all cell for this game - styled as destructive action
            auto* deleteAllCell = new brls::RadioCell();
            deleteAllCell->title->setText("menu/cheats_menu/delete_all_cheats_for_game"_i18n);
            deleteAllCell->setSelected(false);

            std::string tidCopy = g.tid;
            std::filesystem::path cheatsDirCopy = g.cheatsDir;

            deleteAllCell->registerClickAction([tidCopy, cheatsDirCopy](brls::View* view) {
                auto* confirm = new brls::Dialog("menu/cheats_menu/delete_all_cheats_confirm"_i18n + "\n\nTID: " + tidCopy);
                confirm->addButton("hints/cancel"_i18n, []() {});
                confirm->addButton("hints/delete"_i18n, [tidCopy, cheatsDirCopy]() {
                    bool anyFailed = false;
                    try {
                        if (std::filesystem::exists(cheatsDirCopy) && std::filesystem::is_directory(cheatsDirCopy)) {
                            for (auto& e : std::filesystem::directory_iterator(cheatsDirCopy)) {
                                if (!e.is_regular_file()) continue;
                                if (e.path().extension() != ".txt") continue;
                                std::error_code ec;
                                std::filesystem::remove(e.path(), ec);
                                if (ec) anyFailed = true;
                            }
                        }
                    } catch (...) {
                        anyFailed = true;
                    }

                    auto* result = new brls::Dialog(anyFailed ? "menu/cheats_menu/failed_to_delete_some"_i18n : "menu/cheats_menu/cheat_files_deleted"_i18n);
                    result->addButton("hints/ok"_i18n, []() {
                        // refresh the list by recreating the view
                        brls::Application::popActivity();
                        brls::Application::pushActivity(new brls::Activity(new DownloadedCheatsListView()));
                    });
                    result->open();
                });
                confirm->open();
                return true;
            });
            list->addView(deleteAllCell);
            deleteAllCell->title->setTextColor(nvgRGB(255, 100, 100)); // Apply red color after adding to list

            // Items under the group - indented and smaller font
            for (const auto& item : g.items) {
                auto* cell = new brls::RadioCell();
                cell->title->setText("   Build ID: " + item.bid);
                cell->title->setFontSize(18);
                cell->setMarginLeft(20);
                cell->setSelected(false);

                std::string cheatFilePath = item.path;
                std::string gameNameCopy = g.name;
                std::string bidCopy = item.bid;

                u64 titleId = 0;
                {
                    std::istringstream buffer(g.tid);
                    buffer >> std::hex >> titleId;
                }

                cell->registerClickAction([gameNameCopy, bidCopy, cheatFilePath, titleId](brls::View* view) {
                    auto* installedView = new InstalledCheatsView(gameNameCopy, bidCopy, cheatFilePath, titleId);
                    brls::Application::pushActivity(new brls::Activity(installedView));
                    return true;
                });

                list->addView(cell);
            }
        }
    }

    scrollFrame->setContentView(list);
    scrollFrame->setFocusable(true);
    this->setContentView(scrollFrame);

    this->setTitle("menu/cheats_menu/downloaded_cheats"_i18n);
    brls::Application::giveFocus(scrollFrame);

    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);
}
