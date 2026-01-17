#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>

struct PresetApplyProgress {
    int currentModIndex = 0;
    int totalMods = 0;
    int currentFile = 0;
    int totalFiles = 0;
    std::string currentModName;
    std::string currentFileName;
    std::string phase;  // "REMOVING", "APPLYING", "VERIFYING"
};

class PresetApplyView : public brls::Box {
public:
    PresetApplyView(const std::string& presetName,
                    const std::vector<std::string>& modsToApply,
                    const std::string& gamePath,
                    const std::string& installBase,
                    std::function<void()> onComplete);

    ~PresetApplyView() {
        if(operationFuture.valid())
            operationFuture.wait();
        if(updateThread.joinable())
            updateThread.join();
    }

    static brls::View* create();

private:
    std::string presetName;
    std::vector<std::string> modsToApply;
    std::string gamePath;
    std::string installBase;
    std::function<void()> onCompleteCallback;

    BRLS_BIND(brls::Label, phase_text, "phase_text");
    BRLS_BIND(brls::ProgressSpinner, phase_spinner, "phase_spinner");
    BRLS_BIND(brls::Label, phase_percent, "phase_percent");
    BRLS_BIND(brls::Slider, phase_progressBar, "phase_progressBar");

    BRLS_BIND(brls::Label, mod_text, "mod_text");
    BRLS_BIND(brls::ProgressSpinner, mod_spinner, "mod_spinner");
    BRLS_BIND(brls::Label, mod_percent, "mod_percent");
    BRLS_BIND(brls::Slider, mod_progressBar, "mod_progressBar");

    void performOperation();
    void updateProgress();
    void removeAllActiveMods();
    void applyMod(const std::string& modName);
    void verifyAllMods();

    std::vector<std::string> listModFiles(const std::string& modPath);
    std::vector<std::string> listActiveMods();
    bool compareFiles(const std::string& file1, const std::string& file2);

    std::future<void> operationFuture;
    std::thread updateThread;
    std::mutex threadMutex;
    std::condition_variable threadCondition;

    bool operationFinished = false;

    PresetApplyProgress progress;
};
