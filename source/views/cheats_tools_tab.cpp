#include "views/cheats_tools_tab.hpp"
#include "views/downloaded_cheats_list_view.hpp"
#include "views/exclude_titles_view.hpp"
#include "views/cheat_download_view.hpp"
#include "utils/current_cfw.hpp"
#include "utils/constants.hpp"
#include "utils/utils.hpp"

#include <filesystem>
#include <fmt/format.h>
#include <set>

using namespace brls::literals;

CheatsToolsTab::CheatsToolsTab() {
    this->inflateFromXMLRes("xml/tabs/cheats_tools_tab.xml");

    // View downloaded cheats
    auto* viewButton = new brls::RadioCell();
    viewButton->title->setText("menu/cheats_menu/view_cheats"_i18n);
    viewButton->setSelected(false);
    viewButton->registerClickAction([](brls::View* view) {
        auto* listView = new DownloadedCheatsListView();
        brls::Application::pushActivity(new brls::Activity(listView));
        return true;
    });
    contentBox->addView(viewButton);

    // Exclude titles from cheat downloads
    auto* excludeButton = new brls::RadioCell();
    excludeButton->title->setText("menu/cheats_menu/exclude_titles"_i18n);
    excludeButton->setSelected(false);
    excludeButton->registerClickAction([](brls::View* view) {
        auto* excludeView = new ExcludeTitlesView();
        brls::Application::pushActivity(new brls::Activity(excludeView));
        return true;
    });
    contentBox->addView(excludeButton);

    // Delete existing cheats
    auto* deleteExistingButton = new brls::RadioCell();
    deleteExistingButton->title->setText("menu/cheats_menu/delete_existing"_i18n);
    deleteExistingButton->setSelected(false);
    deleteExistingButton->registerClickAction([](brls::View* view) {
        auto confirm = new brls::Dialog("menu/cheats_menu/delete_all_cheats"_i18n);
        confirm->addButton("hints/cancel"_i18n, []() {});
        confirm->addButton("hints/delete"_i18n, []() {
            size_t titlesTouched = 0;
            size_t filesDeleted = 0;

            std::filesystem::path root(AMS_CONTENTS);
            if (std::filesystem::exists(root) && std::filesystem::is_directory(root)) {
                for (const auto& entry : std::filesystem::directory_iterator(root)) {
                    if (!entry.is_directory()) continue;
                    auto cheatsDir = entry.path() / "cheats";
                    if (std::filesystem::exists(cheatsDir) && std::filesystem::is_directory(cheatsDir)) {
                        std::error_code ec;
                        for (const auto& cheatFile : std::filesystem::directory_iterator(cheatsDir)) {
                            if (cheatFile.is_regular_file()) {
                                if (std::filesystem::remove(cheatFile.path(), ec)) {
                                    filesDeleted++;
                                }
                            }
                        }
                        // Remove empty cheats directory after deletions
                        if (std::filesystem::is_empty(cheatsDir, ec)) {
                            std::filesystem::remove(cheatsDir, ec);
                        }
                        // If the title directory became empty, remove it as well
                        std::error_code dirEc;
                        if (std::filesystem::is_empty(entry.path(), dirEc)) {
                            std::filesystem::remove(entry.path(), dirEc);
                        }
                        titlesTouched++;
                    }
                }
            }

            auto done = new brls::Dialog(fmt::format("menu/cheats_menu/deleted_confirmation"_i18n, filesDeleted, titlesTouched));
            done->addButton("hints/ok"_i18n, []() {});
            done->open();
        });
        confirm->open();
        return true;
    });
    contentBox->addView(deleteExistingButton);

    // Delete orphaned cheats
    auto* deleteOrphanedButton = new brls::RadioCell();
    deleteOrphanedButton->title->setText("menu/cheats_menu/delete_orphaned"_i18n);
    deleteOrphanedButton->setSelected(false);
    deleteOrphanedButton->registerClickAction([](brls::View* view) {
        auto confirm = new brls::Dialog("menu/cheats_menu/delete_orphaned_dialog"_i18n);
        confirm->addButton("hints/cancel"_i18n, []() {});
        confirm->addButton("hints/delete"_i18n, []() {
            // Build set of installed Title IDs
            std::set<std::string> installed;
            for (auto& pair : utils::getInstalledGames()) {
                installed.insert(pair.second);
            }

            size_t titlesTouched = 0;
            size_t filesDeleted = 0;

            std::filesystem::path root(AMS_CONTENTS);
            if (std::filesystem::exists(root) && std::filesystem::is_directory(root)) {
                for (const auto& entry : std::filesystem::directory_iterator(root)) {
                    if (!entry.is_directory()) continue;
                    std::string tid = entry.path().filename().string();
                    if (installed.find(tid) != installed.end()) continue; // skip installed

                    auto cheatsDir = entry.path() / "cheats";
                    if (std::filesystem::exists(cheatsDir) && std::filesystem::is_directory(cheatsDir)) {
                        std::error_code ec;
                        for (const auto& cheatFile : std::filesystem::directory_iterator(cheatsDir)) {
                            if (cheatFile.is_regular_file()) {
                                if (std::filesystem::remove(cheatFile.path(), ec)) {
                                    filesDeleted++;
                                }
                            }
                        }
                        // Remove empty cheats directory after deletions
                        if (std::filesystem::is_empty(cheatsDir, ec)) {
                            std::filesystem::remove(cheatsDir, ec);
                        }
                        // If the title directory became empty, remove it as well
                        std::error_code dirEc;
                        if (std::filesystem::is_empty(entry.path(), dirEc)) {
                            std::filesystem::remove(entry.path(), dirEc);
                        }
                        titlesTouched++;
                    }
                }
            }

            auto done = new brls::Dialog(fmt::format("menu/cheats_menu/deleted_orphaned_confirmation"_i18n, filesDeleted, titlesTouched));
            done->addButton("hints/ok"_i18n, []() {});
            done->open();
        });
        confirm->open();
        return true;
    });
    contentBox->addView(deleteOrphanedButton);

    // Download all cheats (extract for all titles)
    auto* downloadAllButton = new brls::RadioCell();
    downloadAllButton->title->setText("menu/cheats_menu/download_all"_i18n);
    downloadAllButton->setSelected(false);
    downloadAllButton->registerClickAction([this](brls::View* view) {
        // Fetch current online version
        std::string cheatsVer = utils::getCheatsVersion();

        // Determine URL based on CFW
        CFW cfw = CurrentCfw::getCFW();
        std::string url = (cfw == CFW::sxos) ? CHEATS_URL_TITLES : CHEATS_URL_CONTENTS;

        // Confirm large operation
        auto confirm = new brls::Dialog(fmt::format(
            "menu/cheats_menu/all_download_warning"_i18n,
            cheatsVer.empty() ? "Unknown" : cheatsVer)
            );
        confirm->addButton("hints/cancel"_i18n, []() {});
        confirm->addButton("hints/proceed"_i18n, [this, url, cheatsVer]() {
            try {
                this->present(new CheatDownloadView(url, cheatsVer, false, true));
            } catch (const std::exception& e) {
                auto err = new brls::Dialog(fmt::format("menu/cheats_menu/failed_to_start_download"_i18n, e.what()));
                err->addButton("hints/ok"_i18n, []() {});
                err->open();
            }
        });
        confirm->open();
        return true;
    });
    contentBox->addView(downloadAllButton);
}

brls::View* CheatsToolsTab::create() {
    return new CheatsToolsTab();
}
