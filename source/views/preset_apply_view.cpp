#include "views/preset_apply_view.hpp"
#include "views/mod_batch_operation_view.hpp"
#include "utils/mod_status.hpp"
#include <borealis.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace brls::literals;

PresetApplyView::PresetApplyView(const std::string& presetName,
                                 const std::vector<std::string>& modsToApply,
                                 const std::string& gamePath,
                                 const std::string& installBase,
                                 std::function<void()> onComplete)
    : presetName(presetName), modsToApply(modsToApply),
      gamePath(gamePath), installBase(installBase),
      onCompleteCallback(onComplete) {

    this->inflateFromXMLRes("xml/views/preset_apply_view.xml");

    // Initialize progress bars
    phase_progressBar->hidePointer();
    phase_progressBar->setProgress(0);
    mod_progressBar->hidePointer();
    mod_progressBar->setProgress(0);
    phase_percent->setText("0%");
    mod_percent->setText("0%");

    // Set initial text
    phase_text->setText("menu/mods/preparing_preset"_i18n);
    mod_text->setText("");

    this->setActionAvailable(brls::ControllerButton::BUTTON_B, false);

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    // Start async operation and update thread
    operationFuture = std::async(std::launch::async, &PresetApplyView::performOperation, this);
    updateThread = std::thread(&PresetApplyView::updateProgress, this);

    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        getAppletFrame()->setTitle(fmt::format("menu/mods/applying_preset"_i18n, this->presetName));
    });
}

std::vector<std::string> PresetApplyView::listActiveMods() {
    std::vector<std::string> activeMods;

    // Read from cache file to find active mods
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";
    if (!std::filesystem::exists(cacheFilePath)) {
        return activeMods;
    }

    std::ifstream cacheFile(cacheFilePath);
    std::string line;
    while (std::getline(cacheFile, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') continue;

        size_t colonPos = line.find(':');
        size_t firstEquals = line.find('=', colonPos);
        size_t secondEquals = line.find('=', firstEquals + 1);

        std::string modName = line.substr(colonPos + 1, firstEquals - colonPos - 1);
        std::string status = line.substr(firstEquals + 1,
            secondEquals != std::string::npos ? secondEquals - firstEquals - 1 : std::string::npos);

        modName.erase(0, modName.find_first_not_of(" \t"));
        modName.erase(modName.find_last_not_of(" \t") + 1);
        status.erase(0, status.find_first_not_of(" \t"));
        status.erase(status.find_last_not_of(" \t") + 1);

        if (ModStatus::isActive(status)) {
            activeMods.push_back(modName);
        }
    }
    cacheFile.close();

    return activeMods;
}

std::vector<std::string> PresetApplyView::listModFiles(const std::string& modPath) {
    std::vector<std::string> files;

    if (!std::filesystem::exists(modPath)) {
        return files;
    }

    size_t baselen = modPath.length();
    if (!modPath.empty() && modPath.back() != '/') baselen++;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(modPath)) {
        if (entry.is_regular_file()) {
            std::string fullPath = entry.path().string();
            std::string relativePath = fullPath.substr(baselen);
            files.push_back(relativePath);
        }
    }

    return files;
}

void PresetApplyView::removeAllActiveMods() {
    std::vector<std::string> activeMods = listActiveMods();

    progress.phase = "REMOVING";
    progress.totalMods = activeMods.size();

    brls::Logger::info("Found {} active mods to remove", activeMods.size());

    for (size_t i = 0; i < activeMods.size(); i++) {
        progress.currentModIndex = i + 1;
        progress.currentModName = activeMods[i];

        std::string modPath = gamePath + "/" + activeMods[i];
        std::vector<std::string> modFiles = listModFiles(modPath);
        progress.totalFiles = modFiles.size();

        for (size_t j = 0; j < modFiles.size(); j++) {
            progress.currentFile = j + 1;
            progress.currentFileName = std::filesystem::path(modFiles[j]).filename().string();

            std::string installedFile = installBase + "/" + modFiles[j];
            if (std::filesystem::exists(installedFile)) {
                try {
                    std::filesystem::remove(installedFile);

                    // Clean up empty directories
                    std::filesystem::path parentPath = std::filesystem::path(installedFile).parent_path();
                    while (parentPath != installBase && std::filesystem::is_empty(parentPath)) {
                        std::filesystem::remove(parentPath);
                        parentPath = parentPath.parent_path();
                    }

                    brls::Logger::debug("Removed: {}", modFiles[j]);
                } catch (const std::exception& e) {
                    brls::Logger::error("Failed to remove {}: {}", modFiles[j], e.what());
                }
            }
        }
    }

    brls::Logger::info("All active mods removed");
}

void PresetApplyView::applyMod(const std::string& modName) {
    brls::Logger::info("Applying mod: {}", modName);
    std::string modPath = gamePath + "/" + modName;

    if (!std::filesystem::exists(modPath)) {
        brls::Logger::error("Mod path does not exist: {}", modPath);
        return;
    }

    std::vector<std::string> modFiles = listModFiles(modPath);
    progress.totalFiles = modFiles.size();
    brls::Logger::info("Found {} files in mod {}", modFiles.size(), modName);

    for (size_t i = 0; i < modFiles.size(); i++) {
        std::string srcPath = modPath + "/" + modFiles[i];
        std::string dstPath = installBase + "/" + modFiles[i];

        progress.currentFile = i + 1;
        progress.currentFileName = std::filesystem::path(modFiles[i]).filename().string();

        // Create destination directory
        std::filesystem::path dstFilePath(dstPath);
        std::filesystem::create_directories(dstFilePath.parent_path());

        try {
            if (!std::filesystem::exists(srcPath)) {
                brls::Logger::error("Source file does not exist: {}", srcPath);
                continue;
            }

            std::ifstream src(srcPath, std::ios::binary);
            if (!src.is_open()) {
                brls::Logger::error("Failed to open source file: {}", srcPath);
                continue;
            }

            std::ofstream dst(dstPath, std::ios::binary);
            if (!dst.is_open()) {
                brls::Logger::error("Failed to open destination file: {}", dstPath);
                src.close();
                continue;
            }

            const size_t bufferSize = 2 * 1024 * 1024;
            char* buffer = new char[bufferSize];

            while (src.read(buffer, bufferSize) || src.gcount() > 0) {
                dst.write(buffer, src.gcount());
            }

            delete[] buffer;
            dst.flush();
            src.close();
            dst.close();

            brls::Logger::debug("Copied: {}", modFiles[i]);
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to copy {}: {}", modFiles[i], e.what());
        }
    }
}

void PresetApplyView::verifyAllMods() {
    progress.phase = "VERIFYING";
    progress.totalMods = modsToApply.size();

    // Update cache file to reflect new mod statuses
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";
    std::ofstream cacheFile(cacheFilePath);

    // Get all available mods in the game folder
    std::vector<std::string> allMods;
    if (std::filesystem::exists(gamePath) && std::filesystem::is_directory(gamePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
            if (entry.is_directory()) {
                std::string modName = entry.path().filename().string();
                // Skip special directories
                if (modName != "." && modName != ".." && modName != "exefs" && modName != "romfs") {
                    allMods.push_back(modName);
                }
            }
        }
    }

    // Write status for all mods
    for (const auto& modName : allMods) {
        bool isInPreset = std::find(modsToApply.begin(), modsToApply.end(), modName) != modsToApply.end();

        if (isInPreset) {
            // This mod is in the preset - mark as ACTIVE
            cacheFile << "default: " << modName << " = ACTIVE = 1.0\n";
        } else {
            // This mod is not in the preset - mark as INACTIVE
            cacheFile << "default: " << modName << " = INACTIVE = 0.0\n";
        }

        progress.currentModIndex++;
        if (progress.currentModIndex <= progress.totalMods) {
            progress.currentModName = modName;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    cacheFile.close();
    brls::Logger::info("Cache file updated with preset status");
}

void PresetApplyView::performOperation() {
    brls::Logger::debug("performOperation async START");

    // Phase 1: Remove all active mods
    brls::Logger::debug("Phase 1: Removing active mods");
    removeAllActiveMods();
    brls::Logger::debug("Phase 1 complete");

    // Phase 2: Apply preset mods in order
    brls::Logger::debug("Phase 2: Applying preset mods");
    progress.phase = "APPLYING";
    progress.totalMods = modsToApply.size();
    progress.currentModIndex = 0;

    for (size_t i = 0; i < modsToApply.size(); i++) {
        progress.currentModIndex = i + 1;
        progress.currentModName = modsToApply[i];

        brls::Logger::info("Applying mod {}/{}: {}", i + 1, modsToApply.size(), modsToApply[i]);
        applyMod(modsToApply[i]);
    }
    brls::Logger::debug("Phase 2 complete");

    // Give filesystem time to sync
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Phase 3: Verify
    brls::Logger::debug("Phase 3: Verifying");
    verifyAllMods();
    brls::Logger::debug("Phase 3 complete");

    operationFinished = true;
    brls::Logger::info("Preset application complete");
    brls::Logger::debug("performOperation async END");
}

void PresetApplyView::updateProgress() {
    brls::Logger::debug("updateProgress thread START");
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    brls::Logger::debug("updateProgress mutex unlocked");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    brls::Logger::debug("Starting progress updates");

    {
        while (!operationFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN](){
                ASYNC_RELEASE
                // Update phase progress and text
                if (this->progress.totalMods > 0) {
                    float progressValue = (float)this->progress.currentModIndex / this->progress.totalMods;
                    this->phase_progressBar->setProgress(progressValue);
                    this->phase_percent->setText(fmt::format("{}%", (int)(progressValue * 100)));

                    // Update phase text based on current phase
                    if (this->progress.phase == "REMOVING") {
                        this->phase_text->setText(fmt::format("Removing active mods ({}/{})",
                            this->progress.currentModIndex, this->progress.totalMods));
                    } else if (this->progress.phase == "APPLYING") {
                        this->phase_text->setText(fmt::format("Applying mods ({}/{})",
                            this->progress.currentModIndex, this->progress.totalMods));
                    } else if (this->progress.phase == "VERIFYING") {
                        this->phase_text->setText(fmt::format("Verifying ({}/{})",
                            this->progress.currentModIndex, this->progress.totalMods));
                    }
                }

                // Update file progress
                if (this->progress.totalFiles > 0) {
                    float fileProgress = (float)this->progress.currentFile / this->progress.totalFiles;
                    this->mod_progressBar->setProgress(fileProgress);
                    this->mod_percent->setText(fmt::format("{}%", (int)(fileProgress * 100)));
                    this->mod_text->setText(fmt::format("File {}/{}", this->progress.currentFile, this->progress.totalFiles));
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->phase_progressBar->setProgress(1.0f);
            this->mod_progressBar->setProgress(1.0f);
            this->phase_percent->setText("100%");
            this->mod_percent->setText("100%");
            this->phase_spinner->animate(false);
            this->mod_spinner->animate(false);
        });
    }

    // Call completion callback
    if (onCompleteCallback) {
        brls::Logger::debug("Calling completion callback");
        onCompleteCallback();
    }

    // Enable back button and dismiss
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        this->dismiss();
    });

    brls::Logger::debug("Preset application complete, threads finishing");
    brls::Logger::debug("updateProgress thread END");
}

brls::View* PresetApplyView::create() {
    return nullptr;
}
