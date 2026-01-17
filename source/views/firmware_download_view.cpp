#include "views/firmware_download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <switch.h>

using namespace brls::literals;

FirmwareDownloadView::FirmwareDownloadView(const std::string& name, const std::string& url)
    : brls::Box(brls::Axis::COLUMN), firmwareName(name), firmwareUrl(url)
{
    this->inflateFromXMLRes("xml/views/firmware_download_view.xml");

    ProgressEvent::instance().reset();
    brls::Logger::debug("ProgressEvent reset");

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

    download_text->setText(fmt::format("menu/firmware_download/downloading"_i18n, firmwareName));
    extract_text->setText("menu/firmware_download/extracting"_i18n);

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    // Register B button in constructor
    this->registerAction("back", brls::ControllerButton::BUTTON_B, [this](brls::View* view) {
        brls::Logger::error("!!! B BUTTON PRESSED - CANCELLING !!!");
        this->shouldStopThreads = true;  // Signal threads to stop immediately
        ProgressEvent::instance().setInterupt(true);
        this->wasCancelled = true;
        // Dismiss immediately
        this->dismiss();
        return true;
    }, true);

    brls::Logger::debug("FirmwareDownloadView constructor END");
}

void FirmwareDownloadView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);

    brls::Logger::debug("willAppear called");

    brls::Logger::debug("Starting threads...");
    downloadThread = std::thread(&FirmwareDownloadView::downloadFile, this);
    updateThread = std::thread(&FirmwareDownloadView::updateProgress, this);
    brls::Logger::debug("Threads started");

    brls::Logger::debug("willAppear END");
}

void FirmwareDownloadView::downloadFile() {
    brls::Logger::info("Download thread started for: {}", firmwareName);
    std::string firmwarePath = std::string(CONFIG_PATH) + "firmware.zip";
    
    try {
        // Start spinner
        brls::sync([this]() {
            download_spinner->animate(true);
        });
        
        // Download firmware
        brls::Logger::info("Starting download from: {}", firmwareUrl);
        bool downloadSuccess = net::downloadFile(firmwareUrl, firmwarePath);
        
        // Check if download was interrupted by user
        bool wasInterrupted = ProgressEvent::instance().getInterupt();
        
        if (wasInterrupted) {
            brls::Logger::info("Download cancelled by user");
            std::filesystem::remove(firmwarePath);
            this->extractFinished = true;
            this->wasCancelled = true;
            ProgressEvent::instance().setInterupt(false);  // Reset the flag
            return;
        }
        
        this->downloadFinished = true;
        
        if (!downloadSuccess) {
            brls::Logger::error("Download failed for {}", firmwareName);
            
            brls::sync([this]() {
                brls::Dialog* dialog = new brls::Dialog("menu/firmware_download/download_failed"_i18n);
                dialog->addButton("hints/ok"_i18n, [this]() {
                    this->dismiss();
                });
                dialog->setCancelable(false);
                dialog->open();
            });
            
            this->extractFinished = true;
            return;
        }
        
        ProgressEvent::instance().reset();
        
        // Update UI for extraction
        brls::sync([this]() {
            this->download_spinner->animate(false);
            this->download_progressBar->setProgress(1.0f);
            this->download_percent->setText("100%");
            
            this->extract_spinner->animate(true);
            this->extract_progressBar->setProgress(0);
            this->extract_percent->setText("0%");
        });
        
        // Clean existing firmware directory before extraction to avoid mixed files
        try {
            if (std::filesystem::exists("/firmware")) {
                brls::Logger::info("Removing existing /firmware directory before extraction");
                std::filesystem::remove_all("/firmware");
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to clean /firmware directory: {}", e.what());
        }

        // Extract firmware
        brls::Logger::info("Starting extraction");
        bool extractSuccess = extract::extractCFW(firmwarePath, "/firmware");
        
        // Check if extraction was interrupted by user
        bool extractInterrupted = ProgressEvent::instance().getInterupt();
        
        if (extractInterrupted) {
            brls::Logger::info("Extraction cancelled by user");
            std::filesystem::remove(firmwarePath);
            std::filesystem::remove_all("/firmware");
            this->extractFinished = true;
            this->wasCancelled = true;
            ProgressEvent::instance().setInterupt(false);  // Reset the flag
            return;
        }
        
        if (!extractSuccess) {
            brls::Logger::error("Extraction failed for {}", firmwareName);
            
            brls::sync([this]() {
                brls::Dialog* dialog = new brls::Dialog("menu/firmware_download/extract_failed"_i18n);
                dialog->addButton("hints/ok"_i18n, [this]() {
                    this->dismiss();
                });
                dialog->setCancelable(false);
                dialog->open();
            });
            
            this->extractFinished = true;
            std::filesystem::remove(firmwarePath);
            return;
        }
        
        std::filesystem::remove(firmwarePath);
        
        // Success
        brls::sync([this]() {
            this->extract_spinner->animate(false);
            this->extract_progressBar->setProgress(1.0f);
            this->extract_percent->setText("100%");
            this->extract_text->setText(fmt::format("menu/firmware_download/extracted"_i18n, firmwareName));
        });
        
        brls::Logger::info("Firmware {} extraction completed", firmwareName);
        
        // Ask to launch Daybreak
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        
        brls::sync([this]() {
            brls::Dialog* dialog = new brls::Dialog("menu/firmware_download/launch_daybreak"_i18n);
            dialog->addButton("hints/yes"_i18n, []() {
                envSetNextLoad(DAYBREAK_PATH, fmt::format("\"{}\" \"/firmware\"", DAYBREAK_PATH).c_str());
                brls::Application::quit();
            });
            dialog->addButton("hints/no"_i18n, [this]() {
                this->dismiss();
            });
            dialog->setCancelable(false);
            dialog->open();
        });
        
        this->extractFinished = true;
        
    } catch (const std::exception& e) {
        brls::Logger::error("Exception in download thread: {}", e.what());
        std::string errorMsg = e.what();
        
        brls::sync([this, errorMsg]() {
            brls::Dialog* dialog = new brls::Dialog("Error: " + errorMsg);
            dialog->addButton("hints/ok"_i18n, [this]() {
                this->dismiss();
            });
            dialog->setCancelable(false);
            dialog->open();
        });
        
        this->extractFinished = true;
    }
}

void FirmwareDownloadView::updateProgress() {
    brls::Logger::info("Update progress thread started");
  
    // Set AppletFrame title
    brls::sync([this]() {
        getAppletFrame()->setTitle("menu/firmware_download/title"_i18n);
    });

    // Monitor download progress
    while(!downloadFinished && !shouldStopThreads) {
        {
            double now = ProgressEvent::instance().getNow();
            double total = ProgressEvent::instance().getTotal();
            double speed = ProgressEvent::instance().getSpeed();
            
            if (total > 0 && !shouldStopThreads) {
                brls::sync([this, now, total, speed]() {
                    this->download_progressBar->setProgress((float)(now / total));
                    this->download_percent->setText(fmt::format("{}%", (int)(now / total * 100)));
                    
                    if (speed > 1024 * 1024) {
                        this->download_speed->setText(fmt::format("{:.2f} MB/s", speed / (1024 * 1024)));
                    } else if (speed > 1024) {
                        this->download_speed->setText(fmt::format("{:.2f} KB/s", speed / 1024));
                    } else {
                        this->download_speed->setText(fmt::format("{:.0f} B/s", speed));
                    }
                    
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
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Monitor extract progress (only if not stopped)
    while(!extractFinished && !shouldStopThreads) {
        {
            int current = ProgressEvent::instance().getStep();
            int max = ProgressEvent::instance().getMax();
            
            if (max > 0 && !shouldStopThreads) {
                brls::sync([this, current, max]() {
                    this->extract_progressBar->setProgress((float)current / max);
                    this->extract_percent->setText(fmt::format("{}%", (int)(current / (float)max * 100)));
                });
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    brls::Logger::info("Update progress thread exiting");
}
