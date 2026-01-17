#include "api/extract.hpp"
#include "utils/progress_event.hpp"
#include "utils/utils.hpp"
#include "utils/constants.hpp"

#include <minizip/unzip.h>
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <set>
#include <switch.h>
#include <borealis.hpp>

const std::string smash_tid = "01006A800016E000";

// Helper to read lines from a file into a set
static std::set<std::string> readPreserveList(const std::string& filePath) {
    std::set<std::string> result;
    if (!std::filesystem::exists(filePath)) {
        return result;
    }
    std::ifstream file(filePath);
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        // Skip lines that are only whitespace
        bool isWhitespace = true;
        for (char c : line) {
            if (!std::isspace(c)) {
                isWhitespace = false;
                break;
            }
        }
        if (isWhitespace) {
            continue;
        }
        result.insert(line);
    }
    return result;
}

// Helper to check if a path should be preserved
static bool shouldPreservePath(const std::string& filePath, const std::set<std::string>& preserveList) {
    for (const auto& preserved : preserveList) {
        // Normalize paths for comparison (remove leading slashes for matching)
        std::string normalizedFile = filePath;
        std::string normalizedPreserve = preserved;
        
        // Remove leading slash from file path
        if (!normalizedFile.empty() && normalizedFile[0] == '/') {
            normalizedFile = normalizedFile.substr(1);
        }
        // Remove leading slash from preserved path
        if (!normalizedPreserve.empty() && normalizedPreserve[0] == '/') {
            normalizedPreserve = normalizedPreserve.substr(1);
        }
        
        // Check if file path starts with preserved path
        if (normalizedFile.find(normalizedPreserve) == 0) {
            return true;
        }
    }
    return false;
}

// Helper to process copy_files.txt
static void processCopyFiles(const std::string& copyFilesPath) {
    if (!std::filesystem::exists(copyFilesPath)) {
        brls::Logger::debug("copy_files.txt not found at {}", copyFilesPath);
        return;
    }

    std::ifstream file(copyFilesPath);
    std::string line;
    int copiedCount = 0;
    int failedCount = 0;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse format: source|destination
        size_t pos = line.find('|');
        if (pos == std::string::npos) {
            brls::Logger::error("Invalid copy_files.txt format (expected source|dest): {}", line);
            continue;
        }

        std::string source = line.substr(0, pos);
        std::string dest = line.substr(pos + 1);

        // Copy file if source exists
        if (std::filesystem::exists(source)) {
            try {
                // Create destination directory if needed
                std::filesystem::create_directories(std::filesystem::path(dest).parent_path());
                
                // Manual file copy using streams (more reliable across filesystems)
                std::ifstream infile(source, std::ios::binary);
                if (!infile.is_open()) {
                    brls::Logger::error("Failed to open source file: {}", source);
                    failedCount++;
                } else {
                    std::ofstream outfile(dest, std::ios::binary | std::ios::trunc);
                    if (!outfile.is_open()) {
                        brls::Logger::error("Failed to open destination file: {}", dest);
                        failedCount++;
                    } else {
                        outfile << infile.rdbuf();
                        outfile.close();
                        infile.close();
                        brls::Logger::debug("Copied {} to {}", source, dest);
                        copiedCount++;
                    }
                }
            } catch (const std::exception& e) {
                brls::Logger::error("Failed to copy {} to {}: {}", source, dest, e.what());
                failedCount++;
            }
        } else {
            brls::Logger::debug("Source file not found for copy: {}", source);
        }
    }
    
    if (copiedCount > 0 || failedCount > 0) {
        brls::Logger::info("Copy files complete: {} succeeded, {} failed", copiedCount, failedCount);
    }
}

namespace extract {
    constexpr u32 MaxTitleCount = 64000;

    int getFileCount(const std::string& archivePath) {
        unzFile zipFile = unzOpen(archivePath.c_str());
        if (!zipFile) return 0;

        int fileCount = 0;
        if (unzGoToFirstFile(zipFile) == UNZ_OK) {
            do {
                fileCount++;
            } while (unzGoToNextFile(zipFile) == UNZ_OK);
        }

        unzClose(zipFile);
        return fileCount;
    }

    s64 getTotalArchiveSize(const std::string& archivePath) {
        unzFile zipFile = unzOpen(archivePath.c_str());
        if (!zipFile) return 0;

        s64 totalSize = 0;
        if (unzGoToFirstFile(zipFile) == UNZ_OK) {
            do {
                unz_file_info fileInfo;
                if (unzGetCurrentFileInfo(zipFile, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) == UNZ_OK) {
                    totalSize += fileInfo.uncompressed_size;
                }
            } while (unzGoToNextFile(zipFile) == UNZ_OK);
        }

        unzClose(zipFile);
        return totalSize;
    }

bool extractEntry(const std::string& archiveFile, const std::string& outputDir, const std::string& tid) {
        chdir("sdmc:/");
        struct archive* archive = archive_read_new();

        brls::Logger::debug("Extracting {} to {}", archiveFile, outputDir);

        archive_read_support_format_all(archive);
        int result = archive_read_open_filename(archive, archiveFile.c_str(), 10240);
        if (result != ARCHIVE_OK) {
            brls::Logger::error("Failed to open archive: {}", archiveFile);
            archive_read_free(archive);
            //std::filesystem::remove(archiveFile);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            return false;
        }
        struct archive_entry* entry;
        ProgressEvent::instance().setTotalSteps(getFileCount(archiveFile));
        ProgressEvent::instance().setStep(0);

        s64 freeStorage;
        if(R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_SdCard, &freeStorage)) && getTotalArchiveSize(archiveFile) * 1.1 > freeStorage) {
            brls::Logger::error("sd is full");
            archive_read_free(archive);
            std::filesystem::remove(archiveFile);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            brls::Application::crash("full");
            std::this_thread::sleep_for(std::chrono::microseconds(2000000));
            brls::Application::quit();
            return false;
        }

        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
            if (ProgressEvent::instance().getInterupt()) {
                archive_read_close(archive);
                archive_read_free(archive);
                std::filesystem::remove(archiveFile);
                ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                return false;
            }
            const char* entryName = archive_entry_pathname(entry);
            
            if ((tid != smash_tid)) {
                if (std::string(entryName).find("romfs/") != std::string::npos || std::string(entryName).find("exefs/") != std::string::npos || std::string(entryName).find("exefs_patches/") != std::string::npos) {
                    
                    std::string outputFilePath;


                    if (std::string(entryName).find("romfs/") != std::string::npos) //romfs
                        outputFilePath = fmt::format("{}/contents/{}/{}", outputDir, tid, std::string(entryName).substr(std::string(entryName).find("romfs/")));
                    else if (std::string(entryName).find("exefs_patches/") != std::string::npos)//exefs_patches
                        outputFilePath = fmt::format("{}/{}", outputDir, std::string(entryName).substr(std::string(entryName).find("exefs_patches/")));
                    else //Exefs
                        outputFilePath = fmt::format("{}/contents/{}/{}", outputDir, tid, std::string(entryName).substr(std::string(entryName).find("exefs/")));

                    if (std::string(entryName).find("|") != std::string::npos)
                        outputFilePath = outputFilePath.substr(0, outputFilePath.find("|"));
                  
                    brls::Logger::debug("Extracting file {} to {}", entryName,outputFilePath);
                    std::filesystem::path outputPath(outputFilePath);
                    std::filesystem::create_directories(outputPath.parent_path());

                    if (archive_entry_filetype(entry) == AE_IFDIR) {
                        ProgressEvent::instance().incrementStep(1);
                        // Skip directories
                        continue;
                    }


                    std::ofstream outputFile(outputFilePath, std::ios::binary);
                    if (!outputFile) {
                        brls::Logger::error("Failed to create output file: {}", outputFilePath);
                        archive_read_free(archive);
                        std::filesystem::remove(archiveFile);
                        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                        return false;
                    }

                    const size_t bufferSize = 100000;
                    char buffer[bufferSize];
                    ssize_t bytesRead;
                    while ((bytesRead = archive_read_data(archive, buffer, bufferSize)) > 0) {
                        outputFile.write(buffer, bytesRead);
                    }

                    outputFile.close();

                    ProgressEvent::instance().incrementStep(1);
                } else {
                    brls::Logger::debug("Skipping {}", entryName);
                }
            } else {
                // Smash bros mods
                std::string outputFilePath = fmt::format("sdmc:/ultimate/mods/{}",std::string(entryName));
                std::filesystem::path outputPath(outputFilePath);
                std::filesystem::create_directories(outputPath.parent_path());
                if (archive_entry_filetype(entry) == AE_IFDIR) {
                    // Create the directory
                    if (!std::filesystem::create_directory(outputPath)) {
                        brls::Logger::error("Failed to create directory: {}", outputFilePath);
                    }
                    ProgressEvent::instance().incrementStep(1);
                    continue;
                }

                std::ofstream outputFile(outputFilePath, std::ios::binary);
                if (!outputFile) {
                    brls::Logger::error("Failed to create output file: {}", outputFilePath);
                    archive_read_free(archive);
                    std::filesystem::remove(archiveFile);
                    ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                    return false;
                }

                const size_t bufferSize = 100000;
                char buffer[bufferSize];
                ssize_t bytesRead;
                while ((bytesRead = archive_read_data(archive, buffer, bufferSize)) > 0) {
                    outputFile.write(buffer, bytesRead);
                }

                outputFile.close();

                brls::Logger::debug("Extracted file: {}", outputFilePath);
                ProgressEvent::instance().incrementStep(1);
            }
        }

        archive_read_close(archive);
        archive_read_free(archive);
        std::filesystem::remove(archiveFile);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
        return true;
    }

    bool extractCFW(const std::string& archiveFile, const std::string& outputDir, bool preserveInis, std::string* hekatePayloadOut) {
        chdir("sdmc:/");
        struct archive* archive = archive_read_new();

        brls::Logger::debug("Extracting CFW {} to {}", archiveFile, outputDir);

        // Load preserve.txt if it exists
        std::set<std::string> preserveList = readPreserveList(FILES_IGNORE);
        if (!preserveList.empty()) {
            brls::Logger::info("Loaded {} paths from preserve.txt", preserveList.size());
            for (const auto& path : preserveList) {
                brls::Logger::debug("  - Preserve: {}", path);
            }
        }

        archive_read_support_format_all(archive);
        archive_read_support_filter_all(archive);
        int result = archive_read_open_filename(archive, archiveFile.c_str(), 16384);
        if (result != ARCHIVE_OK) {
            brls::Logger::error("Failed to open archive: {}", archiveFile);
            archive_read_free(archive);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            return false;
        }

        struct archive_entry* entry;
        int fileCount = getFileCount(archiveFile);
        ProgressEvent::instance().setTotalSteps(fileCount);
        ProgressEvent::instance().setStep(0);
        int currentFile = 0;
        std::string hekatePayloadPath = "";  // Track if we find a Hekate payload

        // Check free storage
        s64 freeStorage;
        if(R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_SdCard, &freeStorage)) && getTotalArchiveSize(archiveFile) * 1.1 > freeStorage) {
            brls::Logger::error("SD card is full");
            archive_read_free(archive);
            std::filesystem::remove(archiveFile);
            ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
            brls::Application::notify("Error: SD card is full");
            return false;
        }

        while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
            if (ProgressEvent::instance().getInterupt()) {
                archive_read_close(archive);
                archive_read_free(archive);
                std::filesystem::remove(archiveFile);
                ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
                return false;
            }

            const char* entryName = archive_entry_pathname(entry);
            std::string entryPath = std::string(entryName);

            // Debug: log all extracted files when preserve list is active
            if (!preserveList.empty() && entryPath.find("bootloader") != std::string::npos) {
                brls::Logger::debug("Processing file: {}", entryPath);
            }

            // Skip .ini files if preserveInis is true
            if (preserveInis && entryPath.find(".ini") != std::string::npos) {
                currentFile++;
                continue;
            }

            // Check if file is in preserve list; if so, skip extraction if it already exists
            // Do this BEFORE removing leading slash so paths with / match properly
            if (shouldPreservePath(entryPath, preserveList)) {
                // Build the output path to check existence
                std::string outputFilePath;
                if (outputDir == "/") {
                    outputFilePath = entryPath;
                    if (outputFilePath[0] != '/') {
                        outputFilePath = "/" + outputFilePath;
                    }
                } else {
                    outputFilePath = outputDir;
                    if (outputFilePath.back() != '/') {
                        outputFilePath += "/";
                    }
                    outputFilePath += (entryPath[0] == '/') ? entryPath.substr(1) : entryPath;
                }
                if (std::filesystem::exists(outputFilePath)) {
                    brls::Logger::info("Preserving existing file: {}", outputFilePath);
                    currentFile++;
                    continue;
                } else {
                    brls::Logger::debug("Preserve list matched {} but file doesn't exist yet, extracting", entryPath);
                }
            }

            // Remove leading slash from entryPath if present (for normal extraction)
            if (!entryPath.empty() && entryPath[0] == '/') {
                entryPath = entryPath.substr(1);
            }

            // Build output file path, ensuring proper path joining
            std::string outputFilePath;
            if (outputDir == "/") {
                outputFilePath = "/" + entryPath;
            } else {
                outputFilePath = outputDir + "/" + entryPath;
            }

            // Rename critical system files with .sb extension
            // These files cannot be overwritten while the system is running
            // The RCM payload will rename them back after extraction completes
            if (entryPath == "atmosphere/fusee-secondary.bin" ||
                entryPath == "sept/payload.bin" ||
                entryPath == "atmosphere/stratosphere.romfs" ||
                entryPath == "atmosphere/package3" ||
                entryPath == "payload.bin") {
                outputFilePath += ".sb";
                brls::Logger::debug("Renaming {} to {}", entryPath, outputFilePath);
            }

            // Detect Hekate payload - filenames starting with "/hekate_ctcaer" (root of SD)
            if (entryPath.substr(0, 14) == "/hekate_ctcaer" || entryPath.substr(0, 13) == "hekate_ctcaer") {
                hekatePayloadPath = outputFilePath;
                brls::Logger::info("Found Hekate payload: {}", entryPath);
            }

            // Create directory structure
            std::filesystem::path path(outputFilePath);
            if (!std::filesystem::exists(path.parent_path())) {
                std::filesystem::create_directories(path.parent_path());
            }

            // Extract file
            if (archive_entry_filetype(entry) == AE_IFREG) {
                std::ofstream outputFile(outputFilePath, std::ios::binary);
                if (!outputFile.is_open()) {
                    brls::Logger::error("Failed to create file: {}", outputFilePath);
                    currentFile++;
                    continue;
                }

                const size_t bufferSize = 65536; // Increased to 64KB for faster extraction
                char buffer[bufferSize];
                ssize_t bytesRead;

                while ((bytesRead = archive_read_data(archive, buffer, bufferSize)) > 0) {
                    outputFile.write(buffer, bytesRead);
                }

                outputFile.close();
            }

            // Update progress every 10 files to reduce overhead
            currentFile++;
            if (currentFile % 10 == 0 || currentFile == fileCount) {
                ProgressEvent::instance().setStep(currentFile);
            }
        }

        archive_read_close(archive);
        archive_read_free(archive);
        std::filesystem::remove(archiveFile);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());

        // Process copy_files.txt after successful extraction
        processCopyFiles(COPY_FILES_TXT);

        // Return hekate payload path if found and requested
        if (hekatePayloadOut && !hekatePayloadPath.empty()) {
            *hekatePayloadOut = hekatePayloadPath;
            brls::Logger::info("Hekate payload extracted to: {}", hekatePayloadPath);
        }

        return true;
    }

    // Helper function for case-insensitive string comparison
    static bool caselessCompare(const std::string& a, const std::string& b) {
        return strcasecmp(a.c_str(), b.c_str()) == 0;
    }

    // Get contents path based on CFW and compute offset for path extraction
    static int computeOffset(CFW cfw) {
        switch (cfw) {
            case CFW::ams:
                std::filesystem::create_directories(AMS_PATH);
                std::filesystem::create_directories(AMS_CONTENTS);
                chdir(AMS_PATH);
                return std::string(CONTENTS_PATH).length();
            case CFW::rnx:
                std::filesystem::create_directories(REINX_PATH);
                std::filesystem::create_directories(REINX_CONTENTS);
                chdir(REINX_PATH);
                return std::string(CONTENTS_PATH).length();
            case CFW::sxos:
                std::filesystem::create_directories(SXOS_PATH);
                std::filesystem::create_directories(SXOS_TITLES);
                chdir(SXOS_PATH);
                return std::string(TITLES_PATH).length();
            default:
                return 0;
        }
    }

    // Helper to extract a single entry from the archive (minizip version)
    static void extractArchiveEntry(const std::string& filename, unzFile& zfile) {
        if (filename.back() == '/') {
            std::filesystem::create_directories(filename);
            return;
        }
        
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
        
        constexpr size_t WRITE_BUFFER_SIZE = 0x10000;
        void* buf = malloc(WRITE_BUFFER_SIZE);
        FILE* outfile = fopen(filename.c_str(), "wb");
        
        if (!outfile) {
            brls::Logger::error("Failed to create file: {}", filename);
            free(buf);
            return;
        }
        
        for (int j = unzReadCurrentFile(zfile, buf, WRITE_BUFFER_SIZE); j > 0; 
             j = unzReadCurrentFile(zfile, buf, WRITE_BUFFER_SIZE)) {
            fwrite(buf, 1, j, outfile);
        }
        
        free(buf);
        fclose(outfile);
    }

    std::vector<std::string> getInstalledTitles() {
        std::vector<std::string> titles;
        NsApplicationRecord* records = new NsApplicationRecord[MaxTitleCount]();
        NsApplicationControlData* controlData = nullptr;
        s32 recordCount = 0;
        u64 controlSize = 0;

        if (R_SUCCEEDED(nsListApplicationRecord(records, MaxTitleCount, 0, &recordCount))) {
            for (s32 i = 0; i < recordCount; i++) {
                controlSize = 0;
                free(controlData);
                controlData = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
                if (controlData == nullptr) {
                    break;
                }
                memset(controlData, 0, sizeof(NsApplicationControlData));

                if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage,
                    records[i].application_id, controlData, sizeof(NsApplicationControlData), &controlSize))) {
                    continue;
                }

                if (controlSize < sizeof(controlData->nacp)) {
                    continue;
                }

                titles.push_back(utils::formatApplicationId(records[i].application_id));
            }
            free(controlData);
        }
        delete[] records;
        std::sort(titles.begin(), titles.end());
        return titles;
    }

    std::vector<std::string> excludeTitles(const std::string& path, const std::vector<std::string>& listedTitles) {
        std::vector<std::string> titles;
        std::ifstream file(path);

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::transform(line.begin(), line.end(), line.begin(), ::toupper);
                for (size_t i = 0; i < listedTitles.size(); i++) {
                    if (line == listedTitles[i]) {
                        titles.push_back(line);
                        break;
                    }
                }
            }
            file.close();
        }

        std::sort(titles.begin(), titles.end());

        // Return titles that are NOT in the exclude list
        std::vector<std::string> result;
        std::set_difference(listedTitles.begin(), listedTitles.end(),
                           titles.begin(), titles.end(),
                           std::back_inserter(result));
        return result;
    }

    bool extractCheats(const std::string& archivePath, const std::vector<std::string>& titles,
                      CFW cfw, const std::string& version, bool extractAll) {
        chdir("sdmc:/");

        // Check free storage
        s64 freeStorage;
        if (R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_SdCard, &freeStorage)) &&
            getTotalArchiveSize(archivePath) * 1.1 > freeStorage) {
            brls::Logger::error("SD card is full");
            return false;
        }

        unzFile zfile = unzOpen(archivePath.c_str());
        if (!zfile) {
            brls::Logger::error("Failed to open archive: {}", archivePath);
            return false;
        }

        unz_global_info gi;
        unzGetGlobalInfo(zfile, &gi);

        ProgressEvent::instance().setTotalSteps(gi.number_entry);
        ProgressEvent::instance().setStep(0);

        int offset = computeOffset(cfw);
        int matchedCount = 0;

        for (uLong i = 0; i < gi.number_entry; ++i) {
            if (ProgressEvent::instance().getInterupt()) {
                unzCloseCurrentFile(zfile);
                break;
            }

            char szFilename[0x301] = "";
            unzOpenCurrentFile(zfile);
            unzGetCurrentFileInfo(zfile, NULL, szFilename, sizeof(szFilename), NULL, 0, NULL, 0);
            std::string filename = szFilename;

            // Check if this is a cheat file
            if ((int)filename.size() > offset + 16 + 7 && 
                caselessCompare(filename.substr(offset + 16, 7), "/cheats")) {
                
                if (extractAll) {
                    // Extract all cheats
                    extractArchiveEntry(filename, zfile);
                    matchedCount++;
                } else {
                    // Check if this title is installed
                    std::string titleId = filename.substr(offset, 16);
                    for (const auto& installedTitle : titles) {
                        if (caselessCompare(installedTitle, titleId)) {
                            extractArchiveEntry(filename, zfile);
                            matchedCount++;
                            break;
                        }
                    }
                }
            }

            ProgressEvent::instance().setStep(i);
            unzCloseCurrentFile(zfile);
            unzGoToNextFile(zfile);
        }

        unzClose(zfile);
        std::filesystem::remove(archivePath);

        // Save version if valid
        if (version != "offline" && !version.empty()) {
            utils::saveToFile(version, CHEATS_VERSION);
        }

        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
        return true;
    }

    bool extractAllCheats(const std::string& archivePath, CFW cfw, const std::string& version) {
        return extractCheats(archivePath, {}, cfw, version, true);
    }
}