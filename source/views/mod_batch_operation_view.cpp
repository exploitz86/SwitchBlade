#include "views/mod_batch_operation_view.hpp"
#include "utils/config.hpp"
#include "utils/mod_status.hpp"

#include <filesystem>
#include <fstream>

using namespace brls::literals;

ModBatchOperationView::ModBatchOperationView(ModBatchOperationType opType,
                                             const std::string& gamePath,
                                             const std::string& installBase,
                                             std::function<void()> onComplete)
    : operationType(opType), gamePath(gamePath), installBase(installBase),
      onCompleteCallback(onComplete) {

    this->inflateFromXMLRes("xml/views/mod_batch_operation_view.xml");

    // Initialize progress bar
    progressBar->hidePointer();
    progressBar->getPointer()->setFocusable(false);
    progressBar->setProgress(0);

    // Set operation text
    if (operationType == ModBatchOperationType::RECHECK_ALL) {
        operation_text->setText("menu/mods/recheck_all"_i18n);
    } else {
        operation_text->setText("menu/mods/removing_all"_i18n);
    }

    progress_label->setText("menu/mods/starting"_i18n);

    this->setActionAvailable(brls::ControllerButton::BUTTON_B, false);

    #ifndef NDEBUG
    cfg::Config config;
    if (config.getWireframe()) {
        this->setWireframeEnabled(true);
        for(auto& view : this->getChildren()) {
            view->setWireframeEnabled(true);
        }
    }
    #endif

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    brls::Logger::debug("Starting threads...");
    operationThread = std::thread(&ModBatchOperationView::performOperation, this);
    updateThread = std::thread(&ModBatchOperationView::updateProgress, this);
    brls::Logger::debug("Threads started");

    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        if (this->operationType == ModBatchOperationType::RECHECK_ALL) {
            getAppletFrame()->setTitle("menu/mods/recheck_all"_i18n);
        } else {
            getAppletFrame()->setTitle("menu/mods/removing_all"_i18n);
        }
    });

    brls::Logger::debug("ModBatchOperationView constructor END");
}

brls::View* ModBatchOperationView::create() {
    return nullptr;
}

std::vector<std::string> ModBatchOperationView::listModFiles(const std::string& modPath) {
    std::vector<std::string> modFiles;

    if (!std::filesystem::exists(modPath)) {
        return modFiles;
    }

    size_t baselen = modPath.length();
    if (!modPath.empty() && modPath.back() != '/') baselen++;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(modPath)) {
        if (entry.is_regular_file()) {
            std::string fullPath = entry.path().string();
            std::string relativePath = fullPath.substr(baselen);
            modFiles.push_back(relativePath);
        }
    }

    return modFiles;
}

bool ModBatchOperationView::compareFiles(const std::string& file1, const std::string& file2) {
    if (!std::filesystem::exists(file1) || !std::filesystem::exists(file2)) {
        return false;
    }

    auto size1 = std::filesystem::file_size(file1);
    auto size2 = std::filesystem::file_size(file2);

    if (size1 != size2) {
        return false;
    }

    if (size1 < 1024 * 1024) {
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);

        if (!f1.is_open() || !f2.is_open()) {
            return false;
        }

        const size_t bufferSize = 4096;
        char buffer1[bufferSize];
        char buffer2[bufferSize];

        while (true) {
            f1.read(buffer1, bufferSize);
            f2.read(buffer2, bufferSize);

            std::streamsize count1 = f1.gcount();
            std::streamsize count2 = f2.gcount();

            if (count1 != count2) {
                return false;
            }

            if (count1 == 0) {
                break;
            }

            if (std::memcmp(buffer1, buffer2, count1) != 0) {
                return false;
            }
        }

        return true;
    }

    return true;
}

void ModBatchOperationView::verifyMod(const std::string& modPath, std::string& outStatus, double& outFraction) {
    std::vector<std::string> modFiles = listModFiles(modPath);

    if (modFiles.empty()) {
        outStatus = "menu/mods/no_file"_i18n;
        outFraction = 0.0;
        return;
    }

    int matchingFiles = 0;

    for (size_t i = 0; i < modFiles.size(); i++) {
        std::string srcPath = modPath + "/" + modFiles[i];
        std::string dstPath = installBase + "/" + modFiles[i];

        progress.currentFile = i + 1;
        progress.totalFiles = modFiles.size();
        progress.currentFileName = std::filesystem::path(modFiles[i]).filename().string();

        if (compareFiles(srcPath, dstPath)) {
            matchingFiles++;
        }
    }

    outFraction = (double)matchingFiles / (double)modFiles.size();

    if (matchingFiles == 0) {
        outStatus = ModStatus::CANONICAL_INACTIVE;
    } else if (matchingFiles == modFiles.size()) {
        outStatus = ModStatus::CANONICAL_ACTIVE;
    } else {
        outStatus = ModStatus::createPartial(matchingFiles, modFiles.size());
    }
}

void ModBatchOperationView::recheckAllMods() {
    // Scan for all mod folders
    std::vector<std::string> modFolders;

    if (std::filesystem::exists(gamePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
            if (entry.is_directory()) {
                modFolders.push_back(entry.path().string());
            }
        }
    }

    progress.totalMods = modFolders.size();

    // Create new cache file
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";
    std::ofstream cacheFile(cacheFilePath);

    if (!cacheFile.is_open()) {
        brls::Logger::error("Failed to create cache file: {}", cacheFilePath);
        operationFinished = true;
        return;
    }

    // Verify each mod and write to cache
    for (size_t i = 0; i < modFolders.size(); i++) {
        std::string modPath = modFolders[i];
        std::string modName = std::filesystem::path(modPath).filename().string();

        progress.currentMod = i + 1;
        progress.currentModName = modName;

        std::string status;
        double fraction;
        verifyMod(modPath, status, fraction);

        cacheFile << "default: " << modName
                  << " = " << status
                  << " = " << fraction << "\n";

        totalFilesProcessed += progress.totalFiles;
        brls::Logger::info("Verified mod {}: {} ({})", modName, status, fraction);
    }

    cacheFile.close();
    totalModsProcessed = modFolders.size();
    brls::Logger::info("Recheck complete: {} mods verified", totalModsProcessed);

    operationFinished = true;
}

void ModBatchOperationView::removeAllMods() {
    // Scan for all mod folders
    std::vector<std::string> modFolders;

    if (std::filesystem::exists(gamePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(gamePath)) {
            if (entry.is_directory()) {
                modFolders.push_back(entry.path().string());
            }
        }
    }

    progress.totalMods = modFolders.size();

    // Remove files from each mod
    for (size_t i = 0; i < modFolders.size(); i++) {
        std::string modPath = modFolders[i];
        std::string modName = std::filesystem::path(modPath).filename().string();

        progress.currentMod = i + 1;
        progress.currentModName = modName;

        std::vector<std::string> modFiles = listModFiles(modPath);
        progress.totalFiles = modFiles.size();

        for (size_t j = 0; j < modFiles.size(); j++) {
            progress.currentFile = j + 1;
            progress.currentFileName = std::filesystem::path(modFiles[j]).filename().string();

            std::string srcPath = modPath + "/" + modFiles[j];
            std::string dstPath = installBase + "/" + modFiles[j];

            if (!std::filesystem::exists(dstPath)) {
                continue;
            }

            try {
                auto srcSize = std::filesystem::file_size(srcPath);
                auto dstSize = std::filesystem::file_size(dstPath);

                if (srcSize == dstSize) {
                    std::filesystem::remove(dstPath);
                    totalFilesProcessed++;

                    // Clean up empty directories
                    std::filesystem::path parentPath = std::filesystem::path(dstPath).parent_path();
                    while (parentPath != installBase && std::filesystem::is_empty(parentPath)) {
                        std::filesystem::remove(parentPath);
                        parentPath = parentPath.parent_path();
                    }
                }
            } catch (const std::exception& e) {
                brls::Logger::error("Failed to remove {}: {}", modFiles[j], e.what());
            }
        }
    }

    // Delete cache file
    std::string cacheFilePath = gamePath + "/mods_status_cache.txt";
    if (std::filesystem::exists(cacheFilePath)) {
        std::filesystem::remove(cacheFilePath);
    }

    totalModsProcessed = modFolders.size();
    brls::Logger::info("Remove complete: {} files removed from {} mods", totalFilesProcessed, totalModsProcessed);

    operationFinished = true;
}

void ModBatchOperationView::performOperation() {
    brls::Logger::debug("performOperation thread START");
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    brls::Logger::debug("performOperation mutex unlocked");

    if (operationType == ModBatchOperationType::RECHECK_ALL) {
        brls::Logger::debug("Calling recheckAllMods()");
        recheckAllMods();
    } else {
        brls::Logger::debug("Calling removeAllMods()");
        removeAllMods();
    }

    brls::Logger::debug("performOperation thread END");
}

void ModBatchOperationView::updateProgress() {
    brls::Logger::debug("updateProgress thread START");
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    brls::Logger::debug("updateProgress mutex unlocked");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (!operationFinished) {
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            if (this->progress.totalMods > 0) {
                float progressValue = (float)this->progress.currentMod / this->progress.totalMods;
                this->progressBar->setProgress(progressValue);

                this->progress_label->setText(fmt::format(
                    "menu/mods/mod_file"_i18n,
                    this->progress.currentMod, this->progress.totalMods, this->progress.currentModName,
                    this->progress.currentFile, this->progress.totalFiles, this->progress.currentFileName
                ));
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    {
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->progressBar->setProgress(1);
            this->operation_spinner->animate(false);

            if (this->operationType == ModBatchOperationType::RECHECK_ALL) {
                this->progress_label->setText(fmt::format("menu/mods/verification_complete"_i18n, this->totalModsProcessed));
                brls::Application::notify(fmt::format("menu/mods/verification_complete"_i18n, this->totalModsProcessed));
            } else {
                this->progress_label->setText(fmt::format("menu/mods/removal_complete"_i18n,
                    this->totalFilesProcessed, this->totalModsProcessed));
                brls::Application::notify(fmt::format("menu/mods/removal_complete"_i18n, this->totalFilesProcessed, this->totalModsProcessed));
            }
        });
    }

    // Call completion callback
    if (onCompleteCallback) {
        brls::Logger::debug("Calling completion callback");
        onCompleteCallback();
    }

    // Enable back button and dismiss
    {
        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
            this->dismiss();
        });
    }

    brls::Logger::debug("updateProgress thread END");
}
