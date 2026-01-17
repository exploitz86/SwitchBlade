#include "views/atmosphere_download_view.hpp"
#include "api/net.hpp"
#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/constants.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <switch.h>

using namespace brls::literals;

// Helper function to remove sysmodule flags
static void removeSysmodulesFlags(const std::string& directory) {
    for (const auto& e : std::filesystem::recursive_directory_iterator(directory)) {
        if (e.path().string().find("boot2.flag") != std::string::npos) {
            std::filesystem::remove(e.path());
        }
    }
}

// Global BPC service for proper lifecycle management
static Service g_amsBpcSrv;
static bool g_amsBpcInitialized = false;

// Helper function to initialize Atmosphère BPC service
static Result amsBpcInitialize() {
    if (g_amsBpcInitialized) {
        return 0;
    }

    Handle h;
    Result rc = svcConnectToNamedPort(&h, "bpc:ams");
    if (R_SUCCEEDED(rc)) {
        serviceCreate(&g_amsBpcSrv, h);
        g_amsBpcInitialized = true;
    }
    return rc;
}

// Helper function to cleanup Atmosphère BPC service
static void amsBpcExit() {
    if (g_amsBpcInitialized) {
        serviceClose(&g_amsBpcSrv);
        g_amsBpcInitialized = false;
    }
}

// Helper function to set reboot payload via Atmosphère BPC
static Result amsBpcSetRebootPayload(const void* src, size_t src_size) {
    if (!g_amsBpcInitialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    return serviceDispatch(&g_amsBpcSrv, 65001,
        .buffer_attrs = {SfBufferAttr_In | SfBufferAttr_HipcMapAlias},
        .buffers = {{src, src_size}},
    );
}

// Helper function to reboot to RCM payload
static void rebootToPayload() {
    brls::Logger::info("Preparing to reboot to RCM payload");

    // Load payload from file
    alignas(0x1000) static u8 g_reboot_payload[0x24000];
    std::string payloadPath = std::string(RCM_PAYLOAD_PATH);

    FILE* f = fopen(payloadPath.c_str(), "rb");
    if (!f) {
        brls::Logger::error("Failed to open payload file: {}", payloadPath);
        brls::Application::notify("menu/ams_update/rcm_not_found"_i18n);
        return;
    }

    size_t payloadSize = fread(g_reboot_payload, 1, sizeof(g_reboot_payload), f);
    fclose(f);

    if (payloadSize == 0) {
        brls::Logger::error("Failed to read payload file");
        brls::Application::notify("Error: Could not read RCM payload");
        return;
    }

    brls::Logger::info("Loaded payload: {} bytes", payloadSize);

    // Initialize SPSM first (required for both paths)
    Result rc = spsmInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("Failed to initialize SPSM: 0x{:X}", rc);
        brls::Application::notify("menu/ams_update/power_service_init_error"_i18n);
        return;
    }

    // Exit SM before attempting BPC operations
    smExit();

    // Try modern Atmosphère path first
    rc = amsBpcInitialize();
    if (R_SUCCEEDED(rc)) {
        brls::Logger::info("Using Atmosphère BPC service for reboot");

        rc = amsBpcSetRebootPayload(g_reboot_payload, sizeof(g_reboot_payload));
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("Payload set successfully, rebooting...");
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
        } else {
            brls::Logger::error("Failed to set reboot payload: 0x{:X}", rc);
            amsBpcExit(); // Clean up on failure
            brls::Application::notify("menu/ams_update/reboot_payload_set_failed"_i18n);
        }
    } else {
        // Fallback to legacy SPL path
        brls::Logger::info("Atmosphère BPC not available, using legacy SPL path");

        rc = splInitialize();
        if (R_FAILED(rc)) {
            brls::Logger::error("Failed to initialize SPL: 0x{:X}", rc);
            brls::Application::notify("menu/ams_update/spl_init_error"_i18n);
            return;
        }

        rc = splSetConfig((SplConfigItem)65001, 2);
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("SPL configured, rebooting...");
            splExit();
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
        } else {
            brls::Logger::error("Failed to set SPL config: 0x{:X}", rc);
            splExit();
            brls::Application::notify("menu/ams_update/reboot_config_error"_i18n);
        }
    }
}

// Helper function to copy files based on copy_files.txt
static void processCopyFiles(const std::string& copyFilesPath) {
    if (!std::filesystem::exists(copyFilesPath)) {
        return;
    }

    std::ifstream file(copyFilesPath);
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse format: source|destination
        size_t pos = line.find('|');
        if (pos == std::string::npos) {
            continue;
        }

        std::string source = line.substr(0, pos);
        std::string dest = line.substr(pos + 1);

        // Copy file if source exists
        if (std::filesystem::exists(source)) {
            try {
                // Create destination directory if needed
                std::filesystem::create_directories(std::filesystem::path(dest).parent_path());
                
                std::ifstream sourceFile(source, std::ios::binary);
                if (!sourceFile.is_open()) {
                    brls::Logger::error("Failed to open source file: {}", source);
                } else {
                    std::ofstream destFile(dest, std::ios::binary | std::ios::trunc);
                    if (!destFile.is_open()) {
                        brls::Logger::error("Failed to open destination file: {}", dest);
                    } else {
                        destFile << sourceFile.rdbuf();
                        destFile.close();
                        sourceFile.close();
                        brls::Logger::debug("Copied {} to {}", source, dest);
                    }
                }
            } catch (const std::exception& e) {
                brls::Logger::error("Failed to copy {} to {}: {}", source, dest, e.what());
            }
        }
    }
}

AtmosphereDownloadView::AtmosphereDownloadView(const std::string& name, const std::string& url, bool offerHekateDownload, const std::string& hekateUrl)
    : cfwName(name), cfwUrl(url), shouldOfferHekate(offerHekateDownload), hekateDownloadUrl(hekateUrl) {
    brls::Logger::debug("AtmosphereDownloadView constructor START");
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

    download_text->setText(fmt::format("menu/ams_update/downloading"_i18n, cfwName));
    extract_text->setText("menu/ams_update/extracting"_i18n);

    this->setFocusable(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);

    brls::Logger::debug("AtmosphereDownloadView constructor END");
}

void AtmosphereDownloadView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);

    brls::Logger::debug("willAppear called");

    brls::Logger::debug("Starting threads...");
    downloadThread = std::thread(&AtmosphereDownloadView::downloadFile, this);
    updateThread = std::thread(&AtmosphereDownloadView::updateProgress, this);
    brls::Logger::debug("Threads started");

    brls::Logger::debug("willAppear END");
}

void AtmosphereDownloadView::downloadFile() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }

    // Download the CFW archive
    std::string archivePath = std::string(AMS_FILENAME);
    bool downloadSuccess = net::downloadFile(cfwUrl, archivePath);
    this->downloadFinished = true;

    // Handle download failure
    if (!downloadSuccess) {
        brls::Logger::error("Download failed for {}", cfwName);

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/download_failed"_i18n);
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

    // Ask user about preserving .ini files
    bool preserveInis = false;
    {
        std::atomic<int> dialogResult{-1};

        brls::sync([&dialogResult]() {
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/overwrite_ini"_i18n);
            dialog->addButton("hints/yes"_i18n, [&dialogResult]() {
                dialogResult.store(0);
            });
            dialog->addButton("hints/no"_i18n, [&dialogResult]() {
                dialogResult.store(1);
            });
            dialog->open();
        });

        // Wait for user response
        while(dialogResult.load() == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        preserveInis = (dialogResult.load() == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    // Ask user about deleting sysmodules flags
    bool deleteSysmoduleFlags = false;
    {
        std::atomic<int> dialogResult{-1};

        brls::sync([&dialogResult]() {
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/sysmodule_flag_delete"_i18n);
            dialog->addButton("hints/no"_i18n, [&dialogResult]() {
                dialogResult.store(0);
            });
            dialog->addButton("hints/yes"_i18n, [&dialogResult]() {
                dialogResult.store(1);
            });
            dialog->open();
        });

        // Wait for user response
        while(dialogResult.load() == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        deleteSysmoduleFlags = (dialogResult.load() == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    // Delete sysmodule flags if requested
    if (deleteSysmoduleFlags) {
        removeSysmodulesFlags(AMS_CONTENTS);
    }

    // Extract to root and check for Hekate payload
    std::string hekatePayloadPath;
    bool extractSuccess = extract::extractCFW(archivePath, "/", preserveInis, &hekatePayloadPath);

    // Handle extraction failure
    if (!extractSuccess) {
        brls::Logger::error("Extraction failed for {}", cfwName);

        ASYNC_RETAIN
        brls::sync([ASYNC_TOKEN]() {
            ASYNC_RELEASE
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/extract_failed"_i18n);
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

    // Process copy_files.txt for additional file copying
    processCopyFiles(COPY_FILES_TXT);

    // If Hekate was not found in the archive and we should offer to download it
    if (hekatePayloadPath.empty() && shouldOfferHekate && !hekateDownloadUrl.empty()) {
        brls::Logger::info("Hekate not found in archive, offering to download separately");

        // Show "Download Hekate?" dialog
        std::atomic<int> dialogResult{-1};

        brls::sync([&dialogResult]() {
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/download_hekate"_i18n);
            dialog->addButton("hints/yes"_i18n, [&dialogResult]() {
                dialogResult.store(0);
            });
            dialog->addButton("hints/no"_i18n, [&dialogResult]() {
                dialogResult.store(1);
            });
            dialog->open();
        });

        // Wait for user response
        while(dialogResult.load() == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // If user selected Yes, download and extract Hekate
        if (dialogResult.load() == 0) {
            brls::Logger::info("User chose to download Hekate");
            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            // Reset progress bars and labels for Hekate download
            ASYNC_RETAIN
            brls::sync([ASYNC_TOKEN]() {
                ASYNC_RELEASE
                this->download_progressBar->setProgress(0);
                this->download_percent->setText("0%");
                this->download_speed->setText("");
                this->download_eta->setText("");
                this->download_spinner->animate(true);
                
                this->extract_progressBar->setProgress(0);
                this->extract_percent->setText("0%");
                this->extract_spinner->animate(true);
                
                this->download_text->setText("menu/ams_update/downloading_hekate"_i18n);
                this->extract_text->setText("menu/ams_update/extracting"_i18n);
            });

            ProgressEvent::instance().reset();

            // Download Hekate
            std::string hekateArchivePath = std::string(BOOTLOADER_FILENAME);
            bool hekateDownloadSuccess = net::downloadFile(hekateDownloadUrl, hekateArchivePath);

            // Update download progress bars during Hekate download
            {
                while(ProgressEvent::instance().getTotal() == 0 && !hekateDownloadSuccess) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                while(ProgressEvent::instance().getNow() < ProgressEvent::instance().getTotal()) {
                    ASYNC_RETAIN
                    brls::sync([ASYNC_TOKEN]() {
                        ASYNC_RELEASE
                        double now = ProgressEvent::instance().getNow();
                        double total = ProgressEvent::instance().getTotal();
                        double speed = ProgressEvent::instance().getSpeed();

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

            if (hekateDownloadSuccess) {
                ProgressEvent::instance().reset();

                // Ask about preserving .ini files for Hekate
                bool preserveHekateInis = false;
                {
                    std::atomic<int> iniDialogResult{-1};

                    brls::sync([&iniDialogResult]() {
                        brls::Dialog* dialog = new brls::Dialog("menu/ams_update/hekate_ini_overwrite"_i18n);
                        dialog->addButton("hints/yes"_i18n, [&iniDialogResult]() {
                            iniDialogResult.store(0);
                        });
                        dialog->addButton("hints/no"_i18n, [&iniDialogResult]() {
                            iniDialogResult.store(1);
                        });
                        dialog->open();
                    });

                    while(iniDialogResult.load() == -1) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    preserveHekateInis = (iniDialogResult.load() == 1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                }

                // Extract Hekate and check for payload
                std::string hekatePayload;
                bool hekateExtractSuccess = extract::extractCFW(hekateArchivePath, "/", preserveHekateInis, &hekatePayload);

                // Update extraction progress bars during Hekate extraction
                {
                    while(ProgressEvent::instance().getMax() == 0 && !hekateExtractSuccess) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    while(ProgressEvent::instance().getStep() < ProgressEvent::instance().getMax()) {
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

                if (hekateExtractSuccess && !hekatePayload.empty()) {
                    brls::Logger::info("Hekate extracted successfully, payload: {}", hekatePayload);
                    hekatePayloadPath = hekatePayload;
                } else {
                    brls::Logger::error("Hekate extraction failed or no payload found");
                    brls::Application::notify("menu/ams_update/hekate_extract_failed"_i18n);
                }
            } else {
                brls::Logger::error("Hekate download failed");
                brls::Application::notify("menu/ams_update/hekate_download_failed"_i18n);
            }
        } else {
            brls::Logger::info("User declined Hekate download");
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }
    }

    // If Hekate payload was found (either in archive or downloaded separately), ask user about copying to reboot_payload.bin
    if (!hekatePayloadPath.empty()) {
        brls::Logger::info("Hekate payload detected, showing copy dialog");

        // First, copy to update.bin (always done)
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
                    brls::Logger::info("Copied Hekate payload to {}", UPDATE_BIN_PATH);
                }
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Failed to copy to {}: {}", UPDATE_BIN_PATH, e.what());
        }

        // Ask user about copying to reboot_payload.bin
        std::atomic<int> dialogResult{-1};

        brls::sync([&dialogResult]() {
            brls::Dialog* dialog = new brls::Dialog("menu/ams_update/hekate_copy_reboot_payload"_i18n);
            dialog->addButton("hints/yes"_i18n, [&dialogResult]() {
                dialogResult.store(0);
            });
            dialog->addButton("hints/no"_i18n, [&dialogResult]() {
                dialogResult.store(1);
            });
            dialog->open();
        });

        // Wait for user response
        while(dialogResult.load() == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Copy to reboot_payload.bin if user selected Yes
        if (dialogResult.load() == 0) {
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

    // Set extractFinished at the very end after all dialogs are complete
    this->extractFinished = true;
}

void AtmosphereDownloadView::updateProgress() {
    {
        std::unique_lock<std::mutex> lock(threadMutex);
    }

    // Set AppletFrame title and disable B button
    brls::sync([this]() {
        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, false);
        getAppletFrame()->setTitle("menu/ams_update/title"_i18n);
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

    // Wait for all post-extraction work (Hekate dialogs, etc.) to complete
    while(!extractFinished) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Show reboot confirmation dialog
    ASYNC_RETAIN
    brls::sync([ASYNC_TOKEN]() {
        ASYNC_RELEASE
        brls::Dialog* dialog = new brls::Dialog("menu/ams_update/update_complete_reboot"_i18n);
        dialog->addButton("menu/ams_update/reboot"_i18n, []() {
            rebootToPayload();
        });
        dialog->setCancelable(false);
        dialog->open();

        getAppletFrame()->setActionAvailable(brls::ControllerButton::BUTTON_B, true);
    });
}
