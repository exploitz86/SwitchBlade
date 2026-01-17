#include "views/bootloader_download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <switch.h>

using namespace brls::literals;

BootloaderDownloadView::BootloaderDownloadView(const std::string& name, const std::string& url)
    : brls::Box(brls::Axis::COLUMN), bootloaderName(name), bootloaderUrl(url)
{
    this->inflateFromXMLRes("xml/views/bootloader_download_view.xml");

    ProgressEvent::instance().reset();

    extract_progressBar->hidePointer();
    extract_progressBar->getPointer()->setFocusable(false);
    extract_progressBar->setProgress(0);
    download_progressBar->hidePointer();
    download_progressBar->getPointer()->setFocusable(false);
    download_progressBar->setProgress(0);
    download_percent->setText(fmt::format("{}%", 0));
    download_speed->setText("");
    download_eta->setText("");
    extract_percent->setText(fmt::format("{}%", 0));

    download_text->setText(fmt::format("menu/bootloader_update/downloading"_i18n, bootloaderName));
    extract_text->setText("menu/bootloader_update/extracting"_i18n);

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    // Cancellation disabled for bootloader updates to avoid partial state
}

void BootloaderDownloadView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);

    brls::sync([this]() {
        if (this->getAppletFrame()) {
            this->getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
            this->getAppletFrame()->setTitle("menu/bootloader_update/title"_i18n);
        }
    });

    downloadThread = std::thread(&BootloaderDownloadView::downloadFile, this);
    updateThread = std::thread(&BootloaderDownloadView::updateProgress, this);
}

void BootloaderDownloadView::downloadFile() {
    std::string archivePath = std::string(BOOTLOADER_FILENAME);
    
    try {
        brls::sync([this]() { download_spinner->animate(true); });

        bool downloadSuccess = net::downloadFile(bootloaderUrl, archivePath);

        if (ProgressEvent::instance().getInterupt()) {
            this->extractFinished = true;
            ProgressEvent::instance().setInterupt(false);
            return;
        }

        this->downloadFinished = true;

        if (!downloadSuccess) {
            brls::sync([this]() {
                brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/download_failed"_i18n);
                dialog->addButton("hints/ok"_i18n, [this]() { this->dismiss(); });
                dialog->setCancelable(false);
                dialog->open();
            });
            this->extractFinished = true;
            return;
        }

        ProgressEvent::instance().reset();

        // UI update
        brls::sync([this]() {
            this->download_spinner->animate(false);
            this->download_progressBar->setProgress(1.0f);
            this->download_percent->setText("100%");
            this->extract_spinner->animate(true);
            this->extract_progressBar->setProgress(0);
            this->extract_percent->setText("0%");
        });

        // Ask about preserving Hekate .ini config files
        bool preserveInis = false;
        {
            std::atomic<int> dialogResult{-1};

            brls::sync([&dialogResult]() {
                brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/hekate_ini_overwrite"_i18n);
                dialog->addButton("hints/yes"_i18n, [&dialogResult]() { dialogResult.store(0); });
                dialog->addButton("hints/no"_i18n, [&dialogResult]() { dialogResult.store(1); });
                dialog->open();
            });

            while(dialogResult.load() == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            preserveInis = (dialogResult.load() == 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }

        // Extract to root, payload will be detected
        std::string hekatePayloadPath;
        bool extractSuccess = extract::extractCFW(archivePath, ROOT_PATH, preserveInis, &hekatePayloadPath);

        if (ProgressEvent::instance().getInterupt()) {
            this->extractFinished = true;
            ProgressEvent::instance().setInterupt(false);
            return;
        }

        if (!extractSuccess) {
            brls::sync([this]() {
                brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/extract_failed"_i18n);
                dialog->addButton("hints/ok"_i18n, [this]() { this->dismiss(); });
                dialog->setCancelable(false);
                dialog->open();
            });
            this->extractFinished = true;
            return;
        }

        // If payload found, copy to update.bin and optionally to reboot_payload.bin
        if (!hekatePayloadPath.empty()) {
            // Manual file copy using streams (more reliable across filesystems)
            try {
                std::ifstream source(hekatePayloadPath, std::ios::binary);
                if (!source.is_open()) {
                    brls::Logger::error("Failed to open source file: {}", hekatePayloadPath);
                } else {
                    std::ofstream dest(UPDATE_BIN_PATH, std::ios::binary | std::ios::trunc);
                    if (!dest.is_open()) {
                        brls::Logger::error("Failed to open destination file: {}", UPDATE_BIN_PATH);
                    } else {
                        dest << source.rdbuf();
                        dest.close();
                        source.close();
                    }
                }
            } catch (const std::exception& e) {
                brls::Logger::error("Failed to copy to {}: {}", UPDATE_BIN_PATH, e.what());
            }

            std::atomic<int> copyResult{-1};
            brls::sync([&copyResult]() {
                brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/hekate_copy_reboot_payload"_i18n);
                dialog->addButton("hints/yes"_i18n, [&copyResult]() { copyResult.store(0); });
                dialog->addButton("hints/no"_i18n, [&copyResult]() { copyResult.store(1); });
                dialog->open();
            });
            while(copyResult.load() == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (copyResult.load() == 0) {
                try {
                    std::ifstream source(UPDATE_BIN_PATH, std::ios::binary);
                    if (!source.is_open()) {
                        brls::Logger::error("Failed to open source file: {}", UPDATE_BIN_PATH);
                    } else {
                        std::ofstream dest(REBOOT_PAYLOAD_PATH, std::ios::binary | std::ios::trunc);
                        if (!dest.is_open()) {
                            brls::Logger::error("Failed to open destination file: {}", REBOOT_PAYLOAD_PATH);
                        } else {
                            dest << source.rdbuf();
                            dest.close();
                            source.close();
                        }
                    }
                } catch (const std::exception& e) {
                    brls::Logger::error("Failed to copy to {}: {}", REBOOT_PAYLOAD_PATH, e.what());
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }

        // Done
        this->extractFinished = true;
        brls::sync([this]() {
            brls::Dialog* dialog = new brls::Dialog("menu/bootloader_update/update_complete"_i18n);
            dialog->addButton("hints/ok"_i18n, [this]() { this->dismiss(); });
            dialog->setCancelable(false);
            dialog->open();
            if (this->getAppletFrame())
                this->getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        });

    } catch (const std::exception& e) {
        brls::sync([this, e]() {
            brls::Dialog* dialog = new brls::Dialog(std::string("Error: ") + e.what());
            dialog->addButton("hints/ok"_i18n, [this]() { this->dismiss(); });
            dialog->setCancelable(false);
            dialog->open();
        });
        this->extractFinished = true;
    }
}

void BootloaderDownloadView::updateProgress() {
    // Set AppletFrame title and disable B button
    brls::sync([this]() {
        if (this->getAppletFrame()) {
            this->getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
            this->getAppletFrame()->setTitle("menu/bootloader_update/title"_i18n);
        }
    });

    // Download progress
    while(!downloadFinished && !shouldStopThreads) {
        double now = ProgressEvent::instance().getNow();
        double total = ProgressEvent::instance().getTotal();
        double speed = ProgressEvent::instance().getSpeed();

        if (total > 0 && !shouldStopThreads) {
            brls::sync([this, now, total, speed]() {
                this->download_progressBar->setProgress((float)(now / total));
                this->download_percent->setText(fmt::format("{}%", (int)(now / total * 100)));
                if (speed > 1024 * 1024)
                    this->download_speed->setText(fmt::format("{:.2f} MB/s", speed / (1024 * 1024)));
                else if (speed > 1024)
                    this->download_speed->setText(fmt::format("{:.2f} KB/s", speed / 1024));
                else
                    this->download_speed->setText(fmt::format("{:.0f} B/s", speed));
                if (speed > 0) {
                    double remaining = total - now;
                    int etaSeconds = (int)(remaining / speed);
                    int minutes = etaSeconds / 60;
                    int seconds = etaSeconds % 60;
                    this->download_eta->setText(fmt::format("ETA: {}:{:02d}", minutes, seconds));
                } else {
                    this->download_eta->setText("ETA: Calculating...");
                }
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Extract progress
    while(!extractFinished && !shouldStopThreads) {
        int current = ProgressEvent::instance().getStep();
        int max = ProgressEvent::instance().getMax();
        if (max > 0 && !shouldStopThreads) {
            brls::sync([this, current, max]() {
                this->extract_progressBar->setProgress((float)current / max);
                this->extract_percent->setText(fmt::format("{}%", (int)(current / (float)max * 100)));
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Re-enable B after completion
    brls::sync([this]() {
        if (this->getAppletFrame()) {
            this->getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
        }
    });
}
