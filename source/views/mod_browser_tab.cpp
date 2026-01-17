#include "views/mod_browser_tab.hpp"
#include "views/mod_operation_view.hpp"
#include "activity/mods_activity.hpp"
#include "utils/config.hpp"
#include "utils/mod_status.hpp"

#include <borealis.hpp>
#include <filesystem>
#include <algorithm>
#include <fstream>

using namespace brls::literals;

ModBrowserTab::ModBrowserTab(const std::string& gameName, const std::string& gamePath, const std::string& titleId)
    : gameName(gameName), gamePath(gamePath), titleId(titleId) {
    brls::Logger::debug("ModBrowserTab constructor START");
    brls::Logger::debug("Game: {}, Path: {}, TitleID: {}", this->gameName, this->gamePath, this->titleId);

    this->inflateFromXMLRes("xml/tabs/mod_browser_tab.xml");
    brls::Logger::debug("XML inflated");

    if (!modListBox) {
        brls::Logger::error("modListBox is nullptr!");
        return;
    }

    brls::Logger::debug("About to scan mods...");

    if (gamePath.empty()) {
        brls::Logger::error("gamePath is empty!");
        return;
    }

    if (!std::filesystem::exists(gamePath)) {
        auto* errorLabel = new brls::Label();
        errorLabel->setText(fmt::format("menu/mods/path_not_found"_i18n, gamePath));
        errorLabel->setFontSize(18);
        modListBox->addView(errorLabel);
        return;
    }

    // Scan mod folders
    std::vector<std::string> modFolders;
    for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
        if (entry.is_directory()) {
            modFolders.push_back(entry.path().filename().string());
        }
    }

    if (modFolders.empty()) {
        auto* noModsLabel = new brls::Label();
        noModsLabel->setText(fmt::format("menu/mods/no_mods_in"_i18n, gamePath));
        noModsLabel->setFontSize(18);
        modListBox->addView(noModsLabel);
        return;
    }

    std::sort(modFolders.begin(), modFolders.end());

    std::string installBase = "sdmc:/atmosphere";

    // Create cells for each mod
    for (const auto& modFolder : modFolders) {
        ModInfo modInfo;
        modInfo.modName = modFolder;
        modInfo.modPath = gamePath + "/" + modFolder;
        modInfo.status = ModStatus::CANONICAL_UNCHECKED;
        modInfo.statusFraction = 0.0;

        // Create DetailCell
        auto* cell = new brls::DetailCell();
        modInfo.cell = cell;

        // Set initial text
        cell->setText(modFolder);
        cell->setDetailText(ModStatus::toDisplayString(ModStatus::CANONICAL_UNCHECKED));

        // Store in vector before setting up callbacks (we'll reference it by index)
        mods.push_back(modInfo);
        size_t modIndex = mods.size() - 1;

        // A Button: Apply mod
        cell->registerAction("menu/mods/apply"_i18n, brls::ControllerButton::BUTTON_A, [this, modIndex, installBase](brls::View* view) {
            auto* dialog = new brls::Dialog(fmt::format("menu/mods/apply_confirm"_i18n, this->mods[modIndex].modName));

            // Copy values to avoid capturing this
            std::string modName = this->mods[modIndex].modName;
            std::string modPath = this->mods[modIndex].modPath;

            dialog->addButton("hints/ok"_i18n, [this, modIndex, modName, modPath, installBase]() {
                this->present(new ModOperationView(
                    ModOperationType::APPLY,
                    modName,
                    modPath,
                    installBase,
                    [this, modIndex](const std::string& status, double fraction) {
                        // Update the mod status after operation completes
                        this->mods[modIndex].status = status;
                        this->mods[modIndex].statusFraction = fraction;
                        this->updateCellStatus(modIndex);
                        this->saveModStatusCache();
                    }
                ));
            });
            dialog->addButton("hints/cancel"_i18n, []() {});
            dialog->setCancelable(true);
            dialog->open();
            return true;
        });

        // X Button: Disable mod
        cell->registerAction("menu/mods/disable"_i18n, brls::ControllerButton::BUTTON_X, [this, modIndex, installBase](brls::View* view) {
            auto* dialog = new brls::Dialog(fmt::format("menu/mods/disable_confirm"_i18n, this->mods[modIndex].modName));

            // Copy values to avoid capturing this
            std::string modName = this->mods[modIndex].modName;
            std::string modPath = this->mods[modIndex].modPath;

            dialog->addButton("hints/ok"_i18n, [this, modIndex, modName, modPath, installBase]() {
                this->present(new ModOperationView(
                    ModOperationType::REMOVE,
                    modName,
                    modPath,
                    installBase,
                    [this, modIndex](const std::string& status, double fraction) {
                        // Update the mod status after operation completes
                        this->mods[modIndex].status = status;
                        this->mods[modIndex].statusFraction = fraction;
                        this->updateCellStatus(modIndex);
                        this->saveModStatusCache();
                    }
                ));
            });
            dialog->addButton("hints/cancel"_i18n, []() {});
            dialog->setCancelable(true);
            dialog->open();
            return true;
        });

        modListBox->addView(cell);
    }

    // Load status from cache file
    loadModStatusCache();

    // Update cell colors based on cached status
    for (size_t i = 0; i < mods.size(); i++) {
        updateCellStatus(i);
    }

    brls::Logger::debug("ModBrowserTab constructor END");

    // Set the title in the constructor as well
    brls::sync([this]() {
        this->getAppletFrame()->setTitle(this->gameName);
    });

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

void ModBrowserTab::willAppear(bool resetState) {
    Box::willAppear(resetState);
    brls::sync([this]() {
        this->getAppletFrame()->setTitle(this->gameName);
    });
}

void ModBrowserTab::updateCellStatus(size_t modIndex) {
    if (modIndex >= mods.size()) return;

    auto& modInfo = mods[modIndex];

    // Update cell detail text with localized status
    modInfo.cell->setDetailText(ModStatus::toDisplayString(modInfo.status));

    // Set color based on status
    NVGcolor color;
    if (modInfo.statusFraction == 0.0) {
        // INACTIVE - Gray
        color = nvgRGB(80, 80, 80);
    } else if (modInfo.statusFraction == 1.0) {
        // ACTIVE - Teal/Green
        color = nvgRGB(88, 195, 169);
    } else {
        // PARTIAL - Orange
        color = nvgRGB(208, 168, 50);
    }

    // Apply color to the detail text only
    modInfo.cell->setDetailTextColor(color);
}

brls::View* ModBrowserTab::create() {
    // Not used
    return nullptr;
}

void ModBrowserTab::updateModStatus() {
    // Called to refresh all mod statuses
    // This would need to be implemented with a background thread
    // For now, it's a placeholder
}

void ModBrowserTab::loadModStatusCache() {
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";

    if (!std::filesystem::exists(cacheFilePath)) {
        brls::Logger::info("No cache file found at {}", cacheFilePath);
        // Set all to INACTIVE as default
        for (auto& mod : mods) {
            mod.status = ModStatus::CANONICAL_INACTIVE;
            mod.statusFraction = 0.0;
        }
        return;
    }

    std::ifstream cacheFile(cacheFilePath);
    if (!cacheFile.is_open()) {
        brls::Logger::error("Failed to open cache file: {}", cacheFilePath);
        return;
    }

    std::string line;
    while (std::getline(cacheFile, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        // Parse format: "preset: modName = status = fraction"
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        size_t firstEquals = line.find('=', colonPos);
        if (firstEquals == std::string::npos) continue;

        size_t secondEquals = line.find('=', firstEquals + 1);

        // Extract components
        std::string modName = line.substr(colonPos + 1, firstEquals - colonPos - 1);
        std::string status = line.substr(firstEquals + 1,
            secondEquals != std::string::npos ? secondEquals - firstEquals - 1 : std::string::npos);

        // Trim modName and status
        modName.erase(0, modName.find_first_not_of(" \t"));
        modName.erase(modName.find_last_not_of(" \t") + 1);
        status.erase(0, status.find_first_not_of(" \t"));
        status.erase(status.find_last_not_of(" \t") + 1);

        double fraction = 0.0;
        if (secondEquals != std::string::npos) {
            std::string fractionStr = line.substr(secondEquals + 1);
            fractionStr.erase(0, fractionStr.find_first_not_of(" \t"));
            try {
                fraction = std::stod(fractionStr);
            } catch (...) {
                fraction = 0.0;
            }
        }

        // Find and update the mod
        for (auto& mod : mods) {
            if (mod.modName == modName) {
                mod.status = status;
                mod.statusFraction = fraction;
                brls::Logger::info("Loaded cache for {}: {} ({})", modName, status, fraction);
                break;
            }
        }
    }

    cacheFile.close();
}

void ModBrowserTab::saveModStatusCache() {
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";

    std::ofstream cacheFile(cacheFilePath);
    if (!cacheFile.is_open()) {
        brls::Logger::error("Failed to create cache file: {}", cacheFilePath);
        return;
    }

    for (const auto& mod : mods) {
        cacheFile << "default: " << mod.modName
                  << " = " << mod.status
                  << " = " << mod.statusFraction << "\n";
    }

    cacheFile.close();
    brls::Logger::info("Saved mod status cache to {}", cacheFilePath);
}

// These member functions are no longer used, but kept for header compatibility
void ModBrowserTab::scanMods() {}
std::string ModBrowserTab::checkModStatus(const std::string& modPath, double& outFraction) { return ""; }
void ModBrowserTab::applyMod(const std::string& modName, const std::string& modPath) {}
void ModBrowserTab::removeMod(const std::string& modName, const std::string& modPath) {}
std::vector<std::string> ModBrowserTab::listModFiles(const std::string& modPath) { return {}; }
