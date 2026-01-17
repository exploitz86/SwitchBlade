#include "utils/payload.hpp"
#include "utils/constants.hpp"
#include <borealis.hpp>
#include <switch.h>
#include <filesystem>
#include <fstream>
#include <cstring>

extern "C" {
    // AMS BPC service functions
    Result amsBpcInitialize();
    void amsBpcExit();
    Result amsBpcSetRebootPayload(const void* src, size_t src_size);
}

namespace {
    constexpr size_t IRAM_PAYLOAD_MAX_SIZE = 0x2F000;
    constexpr size_t IRAM_PAYLOAD_BASE = 0x40010000;
    alignas(0x1000) u8 g_reboot_payload[IRAM_PAYLOAD_MAX_SIZE];

    void copy_to_iram(uintptr_t iram_addr, void* buf, size_t size) {
        SecmonArgs args = {0};
        args.X[0] = 0xF0000201;              // smcAmsIramCopy
        args.X[1] = (u64)buf;                // DRAM buffer
        args.X[2] = (u64)iram_addr;          // IRAM address
        args.X[3] = size;                    // Size
        args.X[4] = 1;                       // Write operation
        svcCallSecureMonitor(&args);
    }
}

namespace Payload {

std::vector<std::string> fetchPayloads() {
    std::vector<std::string> searchDirectories;

    // Build list of directories to search (only if they exist)
    searchDirectories.push_back(ROOT_PATH);
    if (std::filesystem::exists(PAYLOAD_PATH)) searchDirectories.push_back(PAYLOAD_PATH);
    if (std::filesystem::exists(AMS_PATH)) searchDirectories.push_back(AMS_PATH);
    if (std::filesystem::exists(BOOTLOADER_PATH)) searchDirectories.push_back(BOOTLOADER_PATH);
    if (std::filesystem::exists(BOOTLOADER_PL_PATH)) searchDirectories.push_back(BOOTLOADER_PL_PATH);

    std::vector<std::string> payloadFiles;

    for (const auto& dir : searchDirectories) {
        try {
            // Use non-recursive iterator - only scan the specific directory
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().string();
                    std::string ext = entry.path().extension().string();

                    // Only include .bin files, exclude system files
                    if (ext == ".bin") {
                        std::string filename = entry.path().filename().string();
                        if (filename != "fusee-secondary.bin" &&
                            filename != "fusee-mtc.bin" &&
                            filename != "update.bin" &&
                            filename != "reboot_payload.bin") {
                            payloadFiles.push_back(path);
                        }
                    }
                }
            }
        } catch (...) {
            brls::Logger::error("Failed to scan directory: {}", dir);
        }
    }

    // Sort alphabetically
    std::sort(payloadFiles.begin(), payloadFiles.end());

    brls::Logger::info("Found {} payload files", payloadFiles.size());
    return payloadFiles;
}

int rebootToPayload(const std::string& path) {
    brls::Logger::info("Preparing to reboot to payload: {}", path);

    // Validate file exists and is accessible
    if (!std::filesystem::exists(path)) {
        brls::Logger::error("Payload file does not exist: {}", path);
        brls::Application::notify("Error: Payload file not found");
        return -1;
    }

    // Check file size
    size_t fileSize = std::filesystem::file_size(path);
    if (fileSize == 0) {
        brls::Logger::error("Payload file is empty: {}", path);
        brls::Application::notify("Error: Payload file is empty");
        return -1;
    }

    if (fileSize > IRAM_PAYLOAD_MAX_SIZE) {
        brls::Logger::error("Payload file too large ({} bytes, max {}): {}", fileSize, IRAM_PAYLOAD_MAX_SIZE, path);
        brls::Application::notify("Error: Payload file too large");
        return -1;
    }

    // Read payload file
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        brls::Logger::error("Failed to open payload file: {}", path);
        brls::Application::notify("Error: Could not open payload file");
        return -1;
    }

    std::memset(g_reboot_payload, 0, IRAM_PAYLOAD_MAX_SIZE);
    size_t bytesRead = fread(g_reboot_payload, 1, sizeof(g_reboot_payload), f);
    fclose(f);

    if (bytesRead == 0) {
        brls::Logger::error("Failed to read payload file: {}", path);
        brls::Application::notify("Error: Could not read payload file");
        return -1;
    }

    brls::Logger::info("Loaded payload: {} bytes", bytesRead);

    // Initialize SPSM first (required for both paths)
    Result rc = spsmInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("Failed to initialize SPSM: 0x{:X}", rc);
        brls::Application::notify("Error: Failed to initialize power service");
        return -1;
    }

    // Exit SM before attempting BPC operations
    smExit();

    // Try modern Atmosphère BPC path first
    rc = amsBpcInitialize();
    if (R_SUCCEEDED(rc)) {
        brls::Logger::info("Using Atmosphère BPC service for reboot");

        rc = amsBpcSetRebootPayload(g_reboot_payload, 0x24000);
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("Payload set successfully, rebooting...");
            amsBpcExit();
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
            brls::Application::notify("Error: Reboot failed");
            return -1;
        } else {
            brls::Logger::error("Failed to set reboot payload: 0x{:X}", rc);
            amsBpcExit();
            brls::Application::notify("Error: Failed to set reboot payload");
            return -1;
        }
    } else {
        // Fallback to legacy SPL path
        brls::Logger::info("Atmosphère BPC not available, using legacy SPL path");

        rc = splInitialize();
        if (R_FAILED(rc)) {
            brls::Logger::error("Failed to initialize SPL: 0x{:X}", rc);
            brls::Application::notify("Error: Failed to initialize SPL service");
            return -1;
        }

        // Copy payload to IRAM
        copy_to_iram(IRAM_PAYLOAD_BASE, g_reboot_payload, IRAM_PAYLOAD_MAX_SIZE);

        rc = splSetConfig((SplConfigItem)65001, 2);
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("SPL configured, rebooting...");
            splExit();
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
            brls::Application::notify("Error: Reboot failed");
            return -1;
        } else {
            brls::Logger::error("Failed to set SPL config: 0x{:X}", rc);
            splExit();
            brls::Application::notify("Error: Failed to configure reboot");
            return -1;
        }
    }

    return 0;
}

} // namespace Payload
