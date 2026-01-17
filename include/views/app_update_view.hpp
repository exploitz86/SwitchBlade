#pragma once

#include <borealis.hpp>
#include <thread>

class AppUpdateView : public brls::Box {
public:
    AppUpdateView(const std::string& newVersion);
    ~AppUpdateView() {
        if(downloadThread.joinable())
            downloadThread.join();
        if(updateThread.joinable())
            updateThread.join();
    }

    void willAppear(bool resetState = false) override;

private:
    std::string version;

    brls::Label* download_text = nullptr;
    brls::Label* download_percent = nullptr;
    brls::Label* extract_text = nullptr;
    brls::Label* extract_percent = nullptr;

    void downloadAndUpdate();
    void updateProgress();

    std::thread updateThread;
    std::thread downloadThread;

    bool downloadFinished = false;
    bool extractFinished = false;
};
