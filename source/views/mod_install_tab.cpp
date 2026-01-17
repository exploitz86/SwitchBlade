#include "views/mod_install_tab.hpp"
#include "views/mod_browser_tab.hpp"
#include "views/mod_presets_tab.hpp"
#include "views/mod_options_tab.hpp"
#include "utils/config.hpp"
#include "utils/utils.hpp"

#include <borealis.hpp>
#include <filesystem>
#include <algorithm>
#include <cctype>

using namespace brls::literals;

// Helper function to check if a string looks like a Title ID (16 hex chars)
static bool isTitleIdLike(const std::string& str) {
    if (str.length() != 16) return false;
    for (char c : str) {
        if (!std::isxdigit(c)) return false;
    }
    return true;
}

// Recursively search for a Title ID in subfolders
static std::string lookForTitleIdInSubFolders(const std::string& folderPath, int maxDepth = 5) {
    if (maxDepth <= 0) return "";

    if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
        return "";
    }

    // Check immediate subdirectories
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_directory()) {
            std::string folderName = entry.path().filename().string();
            if (isTitleIdLike(folderName)) {
                return folderName;
            }
        }
    }

    // Recursively search deeper if not found
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_directory()) {
            std::string titleId = lookForTitleIdInSubFolders(entry.path().string(), maxDepth - 1);
            if (!titleId.empty()) {
                return titleId;
            }
        }
    }

    return "";
}

ModGameCell::ModGameCell() {
    this->inflateFromXMLRes("xml/cells/cell.xml");
}

ModGameCell* ModGameCell::create() {
    return new ModGameCell();
}

ModGameData::ModGameData() {
    // Scan /mods/ directory for game folders
    std::string modsPath = "sdmc:/mods";

    if (std::filesystem::exists(modsPath) && std::filesystem::is_directory(modsPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(modsPath)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                std::string folderPath = entry.path().string();

                brls::Logger::debug("Found mod folder: {}", folderName);

                std::string titleId = lookForTitleIdInSubFolders(folderPath);

                if (!titleId.empty()) {
                    brls::Logger::debug("  Found titleId in subfolders: {}", titleId);
                } else {
                    brls::Logger::debug("  No titleId found for this folder");
                }

                gameFolders.push_back({folderName, titleId});
            }
        }
    }

    // Sort alphabetically by game name
    std::sort(gameFolders.begin(), gameFolders.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    brls::Logger::debug("{} game folders found in /mods/", gameFolders.size());
}

int ModGameData::numberOfSections(brls::RecyclerFrame* recycler) {
    return 1;
}

int ModGameData::numberOfRows(brls::RecyclerFrame* recycler, int section) {
    return gameFolders.size();
}

std::string ModGameData::titleForHeader(brls::RecyclerFrame* recycler, int section) {
    return "";
}

brls::RecyclerCell* ModGameData::cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    brls::Logger::debug("cellForRow START for row {}", indexPath.row);
    auto cell = (ModGameCell*)recycler->dequeueReusableCell("Cell");
    brls::Logger::debug("Cell dequeued");
    cell->label->setText(gameFolders[indexPath.row].first);
    brls::Logger::debug("Label set to: {}", gameFolders[indexPath.row].first);

    // Count mods in this game folder
    std::string gamePath = "sdmc:/mods/" + gameFolders[indexPath.row].first;
    int modCount = 0;

    if (std::filesystem::exists(gamePath) && std::filesystem::is_directory(gamePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
            if (entry.is_directory()) {
                modCount++;
            }
        }
    }

    cell->subtitle->setText(fmt::format("menu/mods/mod_count"_i18n, modCount));

    // Set game icon if we have a title ID
    if (!gameFolders[indexPath.row].second.empty()) {
        uint8_t* icon = utils::getIconFromTitleId(gameFolders[indexPath.row].second);
        if (icon != nullptr) {
            cell->image->setImageFromMem(icon, 0x20000);
            cell->image->setVisibility(brls::Visibility::VISIBLE);
        } else {
            cell->image->setVisibility(brls::Visibility::GONE);
        }
    } else {
        cell->image->setVisibility(brls::Visibility::GONE);
    }

    brls::Logger::debug("cellForRow END for row {}", indexPath.row);
    return cell;
}

void ModGameData::didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) {
    std::string gameName = gameFolders[indexPath.row].first;
    std::string titleId = gameFolders[indexPath.row].second;
    std::string gamePath = "sdmc:/mods/" + gameName;


    // Create TabFrame programmatically
    auto* tabFrame = new brls::TabFrame();
    brls::Logger::debug("TabFrame created");

    // Add Browser tab - using lambda to capture game data
    tabFrame->addTab("menu/mods/browser_tab"_i18n, [gameName, gamePath, titleId]() -> brls::View* {
        return new ModBrowserTab(gameName, gamePath, titleId);
    });

    // Add Presets tab - passing game data
    tabFrame->addTab("menu/mods/presets_tab"_i18n, [gameName, gamePath, titleId]() -> brls::View* {
        return new ModPresetsTab(gameName, gamePath, titleId);
    });

    // Add separator
    tabFrame->addSeparator();

    // Add Options tab - passing game data
    tabFrame->addTab("menu/mods/options_tab"_i18n, [gameName, gamePath, titleId]() -> brls::View* {
        return new ModOptionsTab(gameName, gamePath, titleId);
    });

    brls::Logger::debug("About to present tabFrame...");
    recycler->present(tabFrame);
    brls::Logger::debug("TabFrame presented successfully");
}

ModInstallTab::ModInstallTab() {
    this->inflateFromXMLRes("xml/tabs/mod_install_tab.xml");

    gameData = new ModGameData();

    recycler->estimatedRowHeight = 100;
    recycler->registerCell("Cell", []() { return ModGameCell::create(); });
    recycler->setDataSource(gameData, false);

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif
}

brls::View* ModInstallTab::create() {
    return new ModInstallTab();
}
