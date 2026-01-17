#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

enum class ModBatchOperationType {
    RECHECK_ALL,
    REMOVE_ALL
};

struct ModBatchProgress {
    int currentMod = 0;
    int totalMods = 0;
    int currentFile = 0;
    int totalFiles = 0;
    std::string currentModName;
    std::string currentFileName;
};

class ModBatchOperationView : public brls::Box {
public:
    ModBatchOperationView(ModBatchOperationType opType,
                          const std::string& gamePath,
                          const std::string& installBase,
                          std::function<void()> onComplete);

    ~ModBatchOperationView() {
        if(operationThread.joinable())
            operationThread.join();
        if(updateThread.joinable())
            updateThread.join();
    }

    static brls::View* create();

private:
    ModBatchOperationType operationType;
    std::string gamePath;
    std::string installBase;
    std::function<void()> onCompleteCallback;

    BRLS_BIND(brls::Label, operation_text, "operation_text");
    BRLS_BIND(brls::ProgressSpinner, operation_spinner, "operation_spinner");
    BRLS_BIND(brls::Label, progress_label, "progress_label");
    BRLS_BIND(brls::Slider, progressBar, "progressBar");

    void performOperation();
    void updateProgress();
    void recheckAllMods();
    void removeAllMods();

    std::vector<std::string> listModFiles(const std::string& modPath);
    bool compareFiles(const std::string& file1, const std::string& file2);
    void verifyMod(const std::string& modPath, std::string& outStatus, double& outFraction);

    std::thread operationThread;
    std::thread updateThread;
    std::mutex threadMutex;

    bool operationFinished = false;
    int totalModsProcessed = 0;
    int totalFilesProcessed = 0;

    ModBatchProgress progress;
};
