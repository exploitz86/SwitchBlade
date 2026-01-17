#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

enum class ModOperationType {
    APPLY,
    REMOVE
};

struct ModOperationProgress {
    int currentFile = 0;
    int totalFiles = 0;
    int verifiedFiles = 0;
    int totalVerify = 0;
    std::string currentFileName;
    std::string status;  // Result status: "ACTIVE", "INACTIVE", "PARTIAL (X/Y)"
    double statusFraction = 0.0;  // 0.0 to 1.0
};

class ModOperationView : public brls::Box {
public:
    ModOperationView(ModOperationType opType,
                     const std::string& modName,
                     const std::string& modPath,
                     const std::string& installBase,
                     std::function<void(const std::string&, double)> onComplete);

    ~ModOperationView() {
        if(operationThread.joinable())
            operationThread.join();
        if(updateThread.joinable())
            updateThread.join();
    }

    static brls::View* create();

private:
    ModOperationType operationType;
    std::string modName;
    std::string modPath;
    std::string installBase;
    std::function<void(const std::string&, double)> onCompleteCallback;

    BRLS_BIND(brls::Label, operation_text, "operation_text");
    BRLS_BIND(brls::ProgressSpinner, operation_spinner, "operation_spinner");
    BRLS_BIND(brls::Label, operation_percent, "operation_percent");
    BRLS_BIND(brls::Slider, operation_progressBar, "operation_progressBar");

    BRLS_BIND(brls::Label, verify_text, "verify_text");
    BRLS_BIND(brls::ProgressSpinner, verify_spinner, "verify_spinner");
    BRLS_BIND(brls::Label, verify_percent, "verify_percent");
    BRLS_BIND(brls::Slider, verify_progressBar, "verify_progressBar");

    void performOperation();
    void updateProgress();
    void applyMod();
    void removeMod();
    void verifyModStatus();

    std::vector<std::string> listModFiles();
    bool compareFiles(const std::string& file1, const std::string& file2);

    std::thread operationThread;
    std::thread updateThread;
    std::mutex threadMutex;
    std::condition_variable threadCondition;

    bool operationFinished = false;
    bool verifyFinished = false;

    ModOperationProgress progress;
};
