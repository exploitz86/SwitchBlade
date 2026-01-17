#include "views/cheat_download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include <atomic>
#include <filesystem>

using namespace brls::literals;

CheatDownloadView::CheatDownloadView(const std::string& url, const std::string& version, bool isGraphicsCheats, bool extractAll)
    : cheatUrl(url), cheatVersion(version), isGfxCheats(isGraphicsCheats), extractAll(extractAll) {
    brls::Logger::debug("CheatDownloadView constructor START");
    this->inflateFromXMLRes("xml/views/atmosphere_download_view.xml");
    brls::Logger::debug("XML inflated");

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

    // Set appropriate text based on cheat type and mode
    if (isGfxCheats) {
        download_text->setText("menu/cheats_menu/downloading_gfx_cheats"_i18n);
        extract_text->setText(extractAll ? "menu/cheats_menu/extracting_all_gfx_cheats"_i18n : "menu/cheats_menu/extracting_gfx_cheats"_i18n);
    } else {
        download_text->setText("menu/cheats_menu/downloading_gbatemp_cheats"_i18n);
        extract_text->setText(extractAll ? "menu/cheats_menu/extracting_all_cheats"_i18n : "menu/cheats_menu/extracting_cheats"_i18n);
    }

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    brls::Logger::debug("CheatDownloadView constructor END");
}

void CheatDownloadView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);

    brls::Logger::debug("willAppear called");

    brls::Logger::debug("Starting threads...");
    downloadThread = std::thread(&CheatDownloadView::downloadFile, this);
    updateThread = std::thread(&CheatDownloadView::updateProgress, this);
    brls::Logger::debug("Threads started");

    brls::Logger::debug("willAppear END");
}

void CheatDownloadView::downloadFile() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }

    // Download the cheat archive
    std::string archivePath = std::string(CHEATS_FILENAME);
    bool downloadSuccess = net::downloadFile(cheatUrl, archivePath);
    this->downloadFinished = true;

    // Handle download failure
    if (!downloadSuccess) {
        brls::Logger::error("Download failed for cheats");

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            brls::Dialog* dialog = new brls::Dialog("menu/cheats_menu/download_failed"_i18n);
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

    bool extractSuccess = false;
    if (extractAll) {
        // Extract all cheats without filtering
        extractSuccess = extract::extractAllCheats(archivePath, CFW::ams, cheatVersion);
    } else {
        // Get list of installed titles
        std::vector<std::string> titles = extract::getInstalledTitles();
        brls::Logger::info("Found {} installed games", titles.size());

        // Exclude titles from exclude list
        if (std::filesystem::exists(CHEATS_EXCLUDE)) {
            titles = extract::excludeTitles(CHEATS_EXCLUDE, titles);
            brls::Logger::info("{} titles after exclusions", titles.size());
        }

        // Extract cheats for installed titles only
        // Using CFW::ams for Atmosphere (the most common CFW)
        extractSuccess = extract::extractCheats(archivePath, titles, CFW::ams, cheatVersion, false);
    }

    // Handle extraction failure
    if (!extractSuccess) {
        brls::Logger::error("Extraction failed for cheats");

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            brls::Dialog* dialog = new brls::Dialog("menu/cheats_menu/extract_failed"_i18n);
            dialog->addButton("hints/ok"_i18n
                , [this]() {
                this->dismiss();
            });
            dialog->setCancelable(false);
            dialog->open();
        });

        this->extractFinished = true;
        return;
    }

    brls::Logger::info("Cheat extraction completed successfully");
    this->extractFinished = true;
}

void CheatDownloadView::updateProgress() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }

    // Set AppletFrame title and disable B button
    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        if (this->isGfxCheats) {
            getAppletFrame()->setTitle(this->extractAll ? "menu/cheats_menu/downloading_all_gfx_cheats"_i18n : "menu/cheats_menu/downloading_gfx_cheats"_i18n);
        } else {
            getAppletFrame()->setTitle(this->extractAll ? "menu/cheats_menu/downloading_all_cheats"_i18n : "menu/cheats_menu/downloading_cheats"_i18n);
        }
    });

    // Download progress - wait for total to be set
    {
        while(ProgressEvent::instance().getTotal() == 0) {
            if(downloadFinished)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        while(ProgressEvent::instance().getNow() < ProgressEvent::instance().getTotal() && !downloadFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN]() {
                ASYNC_RELEASE
                double now = ProgressEvent::instance().getNow();
                double total = ProgressEvent::instance().getTotal();
                double speed = ProgressEvent::instance().getSpeed();

                this->download_progressBar->setProgress((float)(now / total));
                this->download_percent->setText(fmt::format("{}%", (int)(now / total * 100)));

                // Display speed in MB/s or KB/s
                if (speed > 1024 * 1024) {
                    this->download_speed->setText(fmt::format("{:.2f} MB/s", speed / (1024 * 1024)));
                } else if (speed > 1024) {
                    this->download_speed->setText(fmt::format("{:.2f} KB/s", speed / 1024));
                } else {
                    this->download_speed->setText(fmt::format("{:.0f} B/s", speed));
                }

                // Calculate and display ETA
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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            this->download_progressBar->setProgress(1.0f);
            this->download_percent->setText("100%");
            this->download_spinner->animate(false);
        });
    }

    // Extract progress - wait for max to be set
    {
        while(ProgressEvent::instance().getMax() == 0) {
            if(extractFinished)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        while(ProgressEvent::instance().getStep() < ProgressEvent::instance().getMax() && !extractFinished) {
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN]() {
                ASYNC_RELEASE
                int step = ProgressEvent::instance().getStep();
                int max = ProgressEvent::instance().getMax();

                this->extract_progressBar->setProgress((float)step / max);
                this->extract_percent->setText(fmt::format("{}%", (step * 100 / max)));
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            this->extract_progressBar->setProgress(1.0f);
            this->extract_percent->setText("100%");
            this->extract_spinner->animate(false);
        });
    }

    // Wait for extraction to complete
    while(!extractFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Show completion dialog
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        std::string dialogText;
        if (this->isGfxCheats) {
            dialogText = fmt::format("menu/cheats_menu/graphics_cheats_downloaded"_i18n, this->cheatVersion.empty() ? "Unknown" : this->cheatVersion);
        } else {
            dialogText = fmt::format("menu/cheats_menu/cheats_downloaded"_i18n, this->cheatVersion.empty() ? "Unknown" : this->cheatVersion);
        }

        brls::Dialog* dialog = new brls::Dialog(dialogText);
        dialog->addButton("hints/ok"_i18n, [this]() {
            this->dismiss();
        });
        dialog->setCancelable(false);
        dialog->open();

        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
    });
}
