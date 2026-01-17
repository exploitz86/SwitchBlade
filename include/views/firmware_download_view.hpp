#pragma once

#include <borealis.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fmt/format.h>

class FirmwareDownloadView : public brls::Box {
public:
    FirmwareDownloadView(const std::string& name, const std::string& url);
    ~FirmwareDownloadView() {
        if(downloadThread.joinable())
            downloadThread.join();
        if(updateThread.joinable())
            updateThread.join();
    }

    void willAppear(bool resetState = false) override;

private:
    std::string firmwareName;
    std::string firmwareUrl;

    BRLS_BIND(brls::Label, download_text, "download_text");
    BRLS_BIND(brls::ProgressSpinner, download_spinner, "download_spinner");
    BRLS_BIND(brls::Label, download_percent, "download_percent");
    BRLS_BIND(brls::Label, download_speed, "download_speed");
    BRLS_BIND(brls::Label, download_eta, "download_eta");
    BRLS_BIND(brls::Slider, download_progressBar, "download_progressBar");

    BRLS_BIND(brls::Label, extract_text, "extract_text");
    BRLS_BIND(brls::ProgressSpinner, extract_spinner, "extract_spinner");
    BRLS_BIND(brls::Label, extract_percent, "extract_percent");
    BRLS_BIND(brls::Slider, extract_progressBar, "extract_progressBar");

    void downloadFile();
    void updateProgress();

    std::thread updateThread;
    std::thread downloadThread;
    std::mutex threadMutex;
    std::condition_variable threadCondition;

    bool downloadFinished = false;
    bool extractFinished = false;
    bool wasCancelled = false;
    bool shouldStopThreads = false;
};
