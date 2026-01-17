#include "views/app_update_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include "utils/fs.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <switch.h>

using namespace brls::literals;

AppUpdateView::AppUpdateView(const std::string& newVersion)
    : version(newVersion)
{
    brls::Logger::info("AppUpdateView::constructor - start");
    
    // Create vertical container
    this->setAxis(brls::Axis::COLUMN);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setAlignItems(brls::AlignItems::CENTER);
    
    // Download status
    download_text = new brls::Label();
    download_text->setText("menu/main/downloading_update"_i18n);
    download_text->setFontSize(24);
    download_text->setMarginBottom(10);
    this->addView(download_text);
    
    download_percent = new brls::Label();
    download_percent->setText("0%");
    download_percent->setFontSize(32);
    download_percent->setMarginBottom(30);
    this->addView(download_percent);
    
    // Extract status
    extract_text = new brls::Label();
    extract_text->setText("");
    extract_text->setFontSize(24);
    extract_text->setMarginBottom(10);
    this->addView(extract_text);
    
    extract_percent = new brls::Label();
    extract_percent->setText("");
    extract_percent->setFontSize(32);
    this->addView(extract_percent);
    
    this->setFocusable(true);
    
    brls::Logger::info("AppUpdateView::constructor - complete");
}

void AppUpdateView::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);

    this->setActionAvailable(brls::ControllerButton::BUTTON_B, false);

    // Start progress monitoring thread
    updateThread = std::thread(&AppUpdateView::updateProgress, this);
    
    // Start download thread
    downloadThread = std::thread(&AppUpdateView::downloadAndUpdate, this);
    
    brls::Logger::info("AppUpdateView::willAppear - threads started");
}

void AppUpdateView::downloadAndUpdate()
{
    brls::Logger::info("AppUpdateView::downloadAndUpdate - thread started");
    
    downloadFinished = false;
    extractFinished = false;
    
    // Download update
    try {
        brls::Logger::info("Starting app update download from: {}", APP_URL);
        net::downloadFile(APP_URL, APP_FILENAME);
        downloadFinished = true;
        brls::Logger::info("Download completed successfully");
    } catch (const std::exception& e) {
        brls::Logger::error("Download failed: {}", e.what());
        brls::sync([this, e]() {
            auto* errorDialog = new brls::Dialog(fmt::format("Update download failed: {}", e.what()));
            errorDialog->addButton("hints/ok"_i18n, [this]() {
                this->dismiss();
            });
            errorDialog->open();
        });
        return;
    }

    // Extract to config path
    brls::Logger::info("Extracting update to: {}", CONFIG_PATH);
    
    brls::sync([this]() {
        if (download_percent) download_percent->setText("100%");
        if (extract_text) extract_text->setText("menu/main/extracting_update"_i18n);
    });

    try {
        std::string dummyPayload;
        brls::Logger::info("Calling extractCFW with file: {}", APP_FILENAME);
        bool extractSuccess = extract::extractCFW(APP_FILENAME, CONFIG_PATH, false, &dummyPayload);
        extractFinished = true;
        brls::Logger::info("extractCFW returned: {}", extractSuccess);

        if (!extractSuccess) {
            brls::Logger::error("Extraction failed");
            brls::sync([this]() {
                auto* errorDialog = new brls::Dialog("Update extraction failed. Please try again.");
                errorDialog->addButton("OK", [this]() {
                    this->dismiss();
                });
                errorDialog->open();
            });
            return;
        }

        brls::Logger::info("Extraction completed successfully");
        
        brls::sync([this]() {
            if (extract_percent) extract_percent->setText("100%");
            if (extract_text) extract_text->setText("menu/main/finalizing_update"_i18n);
        });

        // Copy forwarder from romfs
        brls::Logger::info("Copying forwarder from {} to {}", ROMFS_FORWARDER, FORWARDER_PATH);
        try {
            // Read forwarder from romfs
            std::ifstream src(ROMFS_FORWARDER, std::ios::binary);
            if (!src.is_open()) {
                throw std::runtime_error("Cannot open source forwarder");
            }
            
            // Write to destination
            std::ofstream dest(FORWARDER_PATH, std::ios::binary);
            if (!dest.is_open()) {
                throw std::runtime_error("Cannot open destination forwarder path");
            }
            
            dest << src.rdbuf();
            src.close();
            dest.close();
            
            brls::Logger::info("Forwarder copied successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to copy forwarder: {}", e.what());
            throw;
        }

        // Show success and prepare to relaunch
        brls::sync([this]() {
            auto* successDialog = new brls::Dialog(
                fmt::format("menu/main/update_successful"_i18n, version));
            successDialog->addButton("Restart", [this]() {
                // Clean exit procedures before launching forwarder
                brls::Logger::info("Launching forwarder at {}", FORWARDER_PATH);
                
                // Set next load to forwarder
                int ret = envSetNextLoad(FORWARDER_PATH, FORWARDER_PATH);
                brls::Logger::info("envSetNextLoad returned: {}", ret);
                
                // Exit romfs before app quit
                romfsExit();
                
                // Quit the application
                brls::Application::quit();
            });
            successDialog->setCancelable(false);
            successDialog->open();
        });

    } catch (const std::exception& e) {
        brls::Logger::error("Update process exception: {}", e.what());
        brls::sync([this, e]() {
            auto* errorDialog = new brls::Dialog(fmt::format("menu/main/update_failed"_i18n, e.what()));
            errorDialog->addButton("hints/ok"_i18n, [this]() {
                this->dismiss();
            });
            errorDialog->open();
        });
    }

    // Clean up downloaded archive
    try {
        if (std::filesystem::exists(APP_FILENAME)) {
            std::filesystem::remove(APP_FILENAME);
        }
    } catch (...) {
        // Ignore cleanup errors
    }
}

void AppUpdateView::updateProgress()
{
    brls::Logger::info("AppUpdateView::updateProgress - thread started");
    
    // DOWNLOAD PHASE
    while(!downloadFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto now = ProgressEvent::instance().getNow();
        auto total = ProgressEvent::instance().getTotal();

        if(total > 0) {
            double progress = static_cast<double>(now) / static_cast<double>(total);
            
            brls::sync([this, progress]() {
                if (download_percent) download_percent->setText(fmt::format("{:.1f}%", progress * 100.0));
            });
        }
    }

    brls::sync([this]() {
        if (download_percent) download_percent->setText("100%");
    });

    ProgressEvent::instance().reset();

    // EXTRACT PHASE
    while(!extractFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto now = ProgressEvent::instance().getNow();
        auto total = ProgressEvent::instance().getTotal();

        if(total > 0) {
            double progress = static_cast<double>(now) / static_cast<double>(total);
            
            brls::sync([this, progress]() {
                if (extract_percent) extract_percent->setText(fmt::format("{:.1f}%", progress * 100.0));
            });
        }
    }

    brls::sync([this]() {
        if (extract_percent) extract_percent->setText("100%");
        if (extract_text) extract_text->setText("menu/main/update_complete"_i18n);
    });
}
