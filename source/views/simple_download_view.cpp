#include "views/simple_download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>

using namespace brls::literals;

SimpleDownloadView::SimpleDownloadView(const std::string& name, const std::string& url)
    : packName(name), packUrl(url)
{
    this->inflateFromXMLRes("xml/tabs/simple_download_view.xml");

    download_text->setText(fmt::format("menu/custom_downloads/downloading"_i18n, packName));
    extract_text->setText("menu/custom_downloads/extracting"_i18n);

    download_progressBar->hidePointer();
    download_progressBar->getPointer()->setFocusable(false);
    download_progressBar->setProgress(0);
    download_percent->setText("0%");
    download_speed->setText("");
    download_eta->setText("");

    extract_progressBar->hidePointer();
    extract_progressBar->getPointer()->setFocusable(false);
    extract_progressBar->setProgress(0);
    extract_percent->setText("0%");

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);
}

void SimpleDownloadView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);

    this->setActionAvailable(brls::ControllerButton::BUTTON_B, false);

    ProgressEvent::instance().reset();

    downloadThread = std::thread(&SimpleDownloadView::downloadFile, this);
    updateThread = std::thread(&SimpleDownloadView::updateProgress, this);

    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        getAppletFrame()->setTitle("menu/custom_downloads/download_extract_title"_i18n);
    });
}

void SimpleDownloadView::downloadFile()
{
    std::unique_lock<std::mutex> lock(threadMutex);

    // Download file
    std::string archivePath = CUSTOM_FILENAME;

    try {
        brls::Logger::info("Starting download: {} to {}", packUrl, archivePath);
        net::downloadFile(packUrl, archivePath);
        brls::Logger::info("Download completed successfully");
    } catch (const std::exception& e) {
        brls::Logger::error("Download failed: {}", e.what());
        brls::sync([this, e]() {
            auto* errorDialog = new brls::Dialog(fmt::format("menu/custom_downloads/download_failed"_i18n, e.what()));
            errorDialog->addButton("OK", [this]() {
                this->dismiss();
            });
            errorDialog->open();
        });
        downloadFinished = true;
        extractFinished = true;
        return;
    }

    downloadFinished = true;
    ProgressEvent::instance().reset();

    // Extract to root without asking about INI files
    brls::Logger::info("Starting extraction to root");

    try {
        // Use extractCFW with preserveInis=false (no dialog, don't preserve)
        std::string dummyPayload;
        bool extractSuccess = extract::extractCFW(archivePath, "/", false, &dummyPayload);

        if (!extractSuccess) {
            brls::Logger::error("Extraction failed");
            brls::sync([this]() {
                auto* errorDialog = new brls::Dialog("menu/custom_downloads/extract_failed"_i18n);
                errorDialog->addButton("hints/ok"_i18n, [this]() {
                    this->dismiss();
                });
                errorDialog->open();
            });
        } else {
            brls::Logger::info("Extraction completed successfully");

            // Show success message
            brls::sync([this]() {
                auto* successDialog = new brls::Dialog(fmt::format("menu/custom_downloads/install_success"_i18n, packName));
                successDialog->addButton("hints/ok"_i18n, [this]() {
                    this->dismiss();
                });
                successDialog->open();
            });
        }
    } catch (const std::exception& e) {
        brls::Logger::error("Extraction exception: {}", e.what());
        brls::sync([this, e]() {
            auto* errorDialog = new brls::Dialog(fmt::format("menu/custom_downloads/extract_failed2"_i18n, e.what()));
            errorDialog->addButton("hints/ok"_i18n, [this]() {
                this->dismiss();
            });
            errorDialog->open();
        });
    }

    extractFinished = true;

    // Clean up downloaded archive
    try {
        if (std::filesystem::exists(archivePath)) {
            std::filesystem::remove(archivePath);
        }
    } catch (...) {
        // Ignore cleanup errors
    }
}

void SimpleDownloadView::updateProgress()
{
    std::unique_lock<std::mutex> lock(threadMutex);

    // DOWNLOAD PHASE
    {
        // Wait for download to start
        while(ProgressEvent::instance().getTotal() == 0) {
            if(downloadFinished)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Track download progress
        auto startTime = std::chrono::steady_clock::now();
        size_t lastNow = 0;

        while(ProgressEvent::instance().getNow() < ProgressEvent::instance().getTotal() && !downloadFinished) {
            size_t now = ProgressEvent::instance().getNow();
            size_t total = ProgressEvent::instance().getTotal();
            int percent = total > 0 ? (int)(now * 100 / total) : 0;

            // Calculate speed and ETA
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

            if (elapsedSeconds > 0) {
                double speed = (double)now / elapsedSeconds;  // bytes per second
                double speedMB = speed / (1024 * 1024);  // MB/s

                size_t remaining = total - now;
                int etaSeconds = speed > 0 ? (int)(remaining / speed) : 0;

                ASYNC_RETAIN
                brls::sync([ASYNC_TOKEN, percent, speedMB, etaSeconds]() {
                    ASYNC_RELEASE
                    this->download_percent->setText(fmt::format("{}%", percent));
                    this->download_progressBar->setProgress((float)percent / 100.0f);
                    this->download_speed->setText(fmt::format("{:.2f} MB/s", speedMB));

                    if (etaSeconds < 60) {
                        this->download_eta->setText(fmt::format("ETA: {}s", etaSeconds));
                    } else {
                        int etaMinutes = etaSeconds / 60;
                        this->download_eta->setText(fmt::format("ETA: {}m {}s", etaMinutes, etaSeconds % 60));
                    }
                });
            }

            lastNow = now;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            this->download_percent->setText("100%");
            this->download_progressBar->setProgress(1.0f);
            this->download_speed->setText("");
            this->download_eta->setText("");
        });
    }

    // EXTRACTION PHASE
    {
        // Wait for extraction to start
        while(ProgressEvent::instance().getMax() == 0 && !extractFinished) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Track extraction progress
        while(ProgressEvent::instance().getStep() < ProgressEvent::instance().getMax() && !extractFinished) {
            int current = ProgressEvent::instance().getStep();
            int max = ProgressEvent::instance().getMax();
            int percent = max > 0 ? (current * 100 / max) : 0;

            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN, percent]() {
                ASYNC_RELEASE
                this->extract_percent->setText(fmt::format("{}%", percent));
                this->extract_progressBar->setProgress((float)percent / 100.0f);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            this->extract_percent->setText("100%");
            this->extract_progressBar->setProgress(1.0f);
        });
    }

    // Allow user to dismiss
    brls::sync([this]() {
        this->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
    });
}
