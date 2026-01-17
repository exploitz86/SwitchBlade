#include "views/mod_operation_view.hpp"
#include "utils/config.hpp"
#include "utils/mod_status.hpp"

#include <filesystem>
#include <fstream>

using namespace brls::literals;

ModOperationView::ModOperationView(ModOperationType opType,
                                   const std::string& modName,
                                   const std::string& modPath,
                                   const std::string& installBase,
                                   std::function<void(const std::string&, double)> onComplete)
    : operationType(opType), modName(modName), modPath(modPath),
      installBase(installBase), onCompleteCallback(onComplete) {

    brls::Logger::debug("ModOperationView constructor START");
    this->inflateFromXMLRes("xml/tabs/mod_operation_view.xml");
    brls::Logger::debug("XML inflated");

    // Initialize progress bars
    brls::Logger::debug("Initializing progress bars...");
    operation_progressBar->hidePointer();
    operation_progressBar->getPointer()->setFocusable(false);
    operation_progressBar->setProgress(0);
    verify_progressBar->hidePointer();
    verify_progressBar->getPointer()->setFocusable(false);
    verify_progressBar->setProgress(0);
    operation_percent->setText("0%");
    verify_percent->setText("0%");

    // Set operation text
    if (operationType == ModOperationType::APPLY) {
        operation_text->setText(fmt::format("menu/mods/applying"_i18n, modName));
    } else {
        operation_text->setText(fmt::format("menu/mods/removing"_i18n, modName));
    }

    verify_text->setText(fmt::format("menu/mods/verifying"_i18n, modName));

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
    operationThread = std::thread(&ModOperationView::performOperation, this);
    updateThread = std::thread(&ModOperationView::updateProgress, this);
    brls::Logger::debug("Threads started");

    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        if (this->operationType == ModOperationType::APPLY) {
            getAppletFrame()->setTitle("menu/mods/applying_mod"_i18n);
        } else {
            getAppletFrame()->setTitle("menu/mods/removing_mod"_i18n);
        }
    });

    brls::Logger::debug("ModOperationView constructor END");
}

brls::View* ModOperationView::create() {
    // This is not used since we pass parameters to constructor
    return nullptr;
}

std::vector<std::string> ModOperationView::listModFiles() {
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

bool ModOperationView::compareFiles(const std::string& file1, const std::string& file2) {
    if (!std::filesystem::exists(file1) || !std::filesystem::exists(file2)) {
        return false;
    }

    // First check file sizes
    auto size1 = std::filesystem::file_size(file1);
    auto size2 = std::filesystem::file_size(file2);

    if (size1 != size2) {
        return false;
    }

    // For small files, do byte-by-byte comparison
    // For large files, just trust the size check
    if (size1 < 1024 * 1024) {  // 1MB threshold
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

            // If read counts differ, files don't match
            if (count1 != count2) {
                return false;
            }

            // If nothing was read, we've reached EOF on both files
            if (count1 == 0) {
                break;
            }

            // Compare the bytes that were read
            if (std::memcmp(buffer1, buffer2, count1) != 0) {
                return false;
            }
        }

        return true;
    }

    return true;  // Sizes match for large files
}

void ModOperationView::applyMod() {
    std::vector<std::string> modFiles = listModFiles();
    progress.totalFiles = modFiles.size();

    for (size_t i = 0; i < modFiles.size(); i++) {
        std::string srcPath = modPath + "/" + modFiles[i];
        std::string dstPath = installBase + "/" + modFiles[i];

        progress.currentFile = i + 1;
        progress.currentFileName = std::filesystem::path(modFiles[i]).filename().string();

        // Create destination directory
        std::filesystem::path dstFilePath(dstPath);
        std::filesystem::create_directories(dstFilePath.parent_path());

        try {
            // Manual file copy
            std::ifstream src(srcPath, std::ios::binary);
            if (!src.is_open()) {
                brls::Logger::error("Cannot open source: {}", srcPath);
                continue;
            }

            std::ofstream dst(dstPath, std::ios::binary);
            if (!dst.is_open()) {
                brls::Logger::error("Cannot open destination: {}", dstPath);
                src.close();
                continue;
            }

            // Copy in 2MB chunks
            const size_t bufferSize = 2 * 1024 * 1024;
            char* buffer = new char[bufferSize];

            while (src.read(buffer, bufferSize) || src.gcount() > 0) {
                dst.write(buffer, src.gcount());
            }

            delete[] buffer;
            dst.flush();  // Ensure data is written to disk
            src.close();
            dst.close();

            brls::Logger::info("Copied: {}", modFiles[i]);
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to copy {}: {}", modFiles[i], e.what());
        }
    }

    operationFinished = true;

    // Give filesystem time to sync all writes before verification
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void ModOperationView::removeMod() {
    std::vector<std::string> modFiles = listModFiles();
    progress.totalFiles = modFiles.size();

    for (size_t i = 0; i < modFiles.size(); i++) {
        std::string srcPath = modPath + "/" + modFiles[i];
        std::string dstPath = installBase + "/" + modFiles[i];

        progress.currentFile = i + 1;
        progress.currentFileName = std::filesystem::path(modFiles[i]).filename().string();

        if (!std::filesystem::exists(dstPath)) {
            continue;
        }

        try {
            auto srcSize = std::filesystem::file_size(srcPath);
            auto dstSize = std::filesystem::file_size(dstPath);

            // Only remove if files match in size
            if (srcSize == dstSize) {
                std::filesystem::remove(dstPath);

                // Clean up empty directories
                std::filesystem::path parentPath = std::filesystem::path(dstPath).parent_path();
                while (parentPath != installBase && std::filesystem::is_empty(parentPath)) {
                    std::filesystem::remove(parentPath);
                    parentPath = parentPath.parent_path();
                }

                brls::Logger::info("Removed: {}", modFiles[i]);
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to remove {}: {}", modFiles[i], e.what());
        }
    }

    operationFinished = true;
}

void ModOperationView::verifyModStatus() {
    std::vector<std::string> modFiles = listModFiles();
    progress.totalVerify = modFiles.size();

    if (modFiles.empty()) {
        progress.status = "menu/mods/no_file"_i18n;
        progress.statusFraction = 0.0;
        verifyFinished = true;
        return;
    }

    int matchingFiles = 0;

    for (size_t i = 0; i < modFiles.size(); i++) {
        std::string srcPath = modPath + "/" + modFiles[i];
        std::string dstPath = installBase + "/" + modFiles[i];

        progress.verifiedFiles = i + 1;
        progress.currentFileName = std::filesystem::path(modFiles[i]).filename().string();

        if (compareFiles(srcPath, dstPath)) {
            matchingFiles++;
        } else {
            brls::Logger::debug("File mismatch: {}", modFiles[i]);
            if (!std::filesystem::exists(dstPath)) {
                brls::Logger::debug("  Destination file doesn't exist");
            } else {
                auto srcSize = std::filesystem::file_size(srcPath);
                auto dstSize = std::filesystem::file_size(dstPath);
                brls::Logger::debug("  Size: src={}, dst={}", srcSize, dstSize);
            }
        }
    }

    progress.statusFraction = (double)matchingFiles / (double)modFiles.size();

    if (matchingFiles == 0) {
        progress.status = ModStatus::CANONICAL_INACTIVE;
    } else if (matchingFiles == modFiles.size()) {
        progress.status = ModStatus::CANONICAL_ACTIVE;
    } else {
        progress.status = ModStatus::createPartial(matchingFiles, modFiles.size());
    }

    brls::Logger::info("Mod status: {} ({})", progress.status, progress.statusFraction);

    verifyFinished = true;
}

void ModOperationView::performOperation() {
    brls::Logger::debug("performOperation thread START");
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    brls::Logger::debug("performOperation mutex unlocked");

    // Phase 1: Apply or Remove
    if (operationType == ModOperationType::APPLY) {
        brls::Logger::debug("Calling applyMod()");
        applyMod();
    } else {
        brls::Logger::debug("Calling removeMod()");
        removeMod();
    }

    // Phase 2: Verify installation status
    brls::Logger::debug("Calling verifyModStatus()");
    verifyModStatus();
    brls::Logger::debug("performOperation thread END");
}

void ModOperationView::updateProgress() {
    brls::Logger::debug("updateProgress thread START");
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }
    brls::Logger::debug("updateProgress mutex unlocked");

    // Give the UI a moment to fully initialize before starting updates
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    brls::Logger::debug("Starting progress updates");

    // Phase 1: Operation (Apply/Remove)
    {
        while (!operationFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN](){
                ASYNC_RELEASE
                if (this->progress.totalFiles > 0) {
                    float progressValue = (float)this->progress.currentFile / this->progress.totalFiles;
                    int progressPercent = (int)(progressValue * 100);
                    this->operation_percent->setText(fmt::format("{}%", progressPercent));
                    this->operation_progressBar->setProgress(progressValue);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->operation_percent->setText("100%");
            this->operation_progressBar->setProgress(1);
            this->operation_spinner->animate(false);
        });
    }

    // Phase 2: Verification
    {
        while (!verifyFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN](){
                ASYNC_RELEASE
                if (this->progress.totalVerify > 0) {
                    float progressValue = (float)this->progress.verifiedFiles / this->progress.totalVerify;
                    int progressPercent = (int)(progressValue * 100);
                    this->verify_percent->setText(fmt::format("{}%", progressPercent));
                    this->verify_progressBar->setProgress(progressValue);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN](){
            ASYNC_RELEASE
            this->verify_percent->setText("100%");
            this->verify_progressBar->setProgress(1);
            this->verify_spinner->animate(false);
        });
    }

    // Call completion callback with status
    if (onCompleteCallback) {
        brls::Logger::debug("Calling completion callback");
        onCompleteCallback(progress.status, progress.statusFraction);
    }

    // Enable back button and dismiss
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        this->dismiss();
    });

    brls::Logger::debug("Operation complete, threads finishing");
    brls::Logger::debug("updateProgress thread END - thread will exit now");
}
