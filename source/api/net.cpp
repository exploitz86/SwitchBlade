#include "api/net.hpp"
#include "utils/progress_event.hpp"

#include <curl/curl.h>
#include <borealis.hpp>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace net {
    std::chrono::_V2::steady_clock::time_point time_old;
    std::chrono::_V2::steady_clock::time_point progress_update_old;
    double dlold;
    constexpr size_t FILE_WRITE_BUFFER_SIZE = 0x100000;

    struct DownloadChunk {
        unsigned char* data = nullptr;
        size_t data_size = 0;
        size_t offset = 0;
        FILE* out = nullptr;
    };

    size_t WriteCallback(void* content, size_t size, size_t nmemb, std::string* buffer) {
        buffer->append((char*)content, size * nmemb);
        return size * nmemb;
    }

    nlohmann::json downloadRequest(std::string url) {
        auto curl = curl_easy_init();

        brls::Logger::debug("Requesting: " + url);
        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return nlohmann::json();
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        std::string response;

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        auto res = curl_easy_perform(curl);

        nlohmann::json json;

        if(res != CURLE_OK) {
            brls::Logger::error("Failed to perform request: " + std::string(curl_easy_strerror(res)));
        } else {
            json = nlohmann::json::parse(response);
        }

        curl_easy_cleanup(curl);

        return json;
    }

    size_t WriteCallbackImage(char* ptr, size_t size, size_t nmemb, void* userdata) {
        if (ProgressEvent::instance().getInterupt()) {
            return 0;
        }
        std::vector<unsigned char>* buffer = static_cast<std::vector<unsigned char>*>(userdata);
        size_t total_size = size * nmemb;
        buffer->insert(buffer->end(), ptr, ptr + total_size);
        return total_size;
    }

    void downloadImage(const std::string& url, std::vector<unsigned char>& buffer) {
        auto curl = curl_easy_init();

        brls::Logger::debug("Downloading image: {}", url);
        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackImage);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 120000L);

        auto res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            brls::Logger::error(curl_easy_strerror(res));
        curl_easy_cleanup(curl);
    }

    size_t WriteCallbackFile(void* ptr, size_t size, size_t nmemb, void* userdata) {
        if (ProgressEvent::instance().getInterupt()) {
            return 0;
        }

        auto* chunk = static_cast<DownloadChunk*>(userdata);
        size_t totalSize = size * nmemb;

        if (totalSize >= chunk->data_size) {
            if (chunk->offset > 0) {
                if (fwrite(chunk->data, 1, chunk->offset, chunk->out) != chunk->offset) {
                    brls::Logger::error("Error writing buffered chunk to file");
                    return 0;
                }
                chunk->offset = 0;
            }

            if (fwrite(ptr, 1, totalSize, chunk->out) != totalSize) {
                brls::Logger::error("Error writing large chunk to file");
                return 0;
            }
            return totalSize;
        }

        if (chunk->offset + totalSize >= chunk->data_size) {
            if (fwrite(chunk->data, 1, chunk->offset, chunk->out) != chunk->offset) {
                brls::Logger::error("Error flushing download buffer to file");
                return 0;
            }
            chunk->offset = 0;
        }

        std::memcpy(chunk->data + chunk->offset, ptr, totalSize);
        chunk->offset += totalSize;
        return totalSize;
    }
    
    int downloadFileProgress(void* p, double dltotal, double dlnow, double ultotal, double ulnow) {
        //brls::Logger::debug("Downloaded: {} / {}", dlnow, dltotal);
        if(dltotal < 0.0) return 0;

        // Check if download was interrupted
        if (ProgressEvent::instance().getInterupt()) {
            brls::Logger::error("Download interrupted by user");
            return 1;  // Return non-zero to abort CURL transfer
        }

        auto now = std::chrono::steady_clock::now();
        double progress_elapsed = ((std::chrono::duration<double>)(now - progress_update_old)).count();
        if (progress_elapsed < 0.1) {
            return 0;
        }
        progress_update_old = now;

        double progress = dlnow / dltotal;
        int counter = (int)(progress * ProgressEvent::instance().getMax());
        ProgressEvent::instance().setStep(std::min(ProgressEvent::instance().getMax() - 1, counter));
        ProgressEvent::instance().setNow(dlnow);
        ProgressEvent::instance().setTotalCount(dltotal);
        auto time_now = now;
        double elapsed_time = ((std::chrono::duration<double>)(time_now - time_old)).count();
        if(elapsed_time > 1.2f) {
            ProgressEvent::instance().setSpeed((dlnow - dlold) / elapsed_time);
            dlold = dlnow;
            time_old = time_now;
        }

        return 0;
    }

    bool downloadFile(const std::string& url, const std::string& path) {
        brls::Logger::debug("Downloading file: {}, in the location : {}", url, path);

        time_old = std::chrono::steady_clock::now();
        progress_update_old = time_old;
        dlold = 0.0;

        auto curl = curl_easy_init();

        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return false;
        }

        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) {
            brls::Logger::error("Failed to open file for writing: {}", path);
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, downloadFileProgress);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

        DownloadChunk chunk;
        chunk.data = static_cast<unsigned char*>(std::malloc(FILE_WRITE_BUFFER_SIZE));
        chunk.data_size = FILE_WRITE_BUFFER_SIZE;
        chunk.out = fp;

        if (!chunk.data) {
            brls::Logger::error("Failed to allocate download buffer");
            std::fclose(fp);
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        auto res = curl_easy_perform(curl);

        bool fileWriteError = false;
        if (chunk.offset > 0) {
            if (fwrite(chunk.data, 1, chunk.offset, fp) != chunk.offset) {
                brls::Logger::error("Error writing final buffered chunk to file");
                fileWriteError = true;
            }
        }

        std::fclose(fp);

        // Check HTTP status code before cleaning up curl handle
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        std::free(chunk.data);
        curl_easy_cleanup(curl);

        // Check if aborted by callback (user cancelled)
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            brls::Logger::info("Download aborted by user");
            std::filesystem::remove(path);
            return false;
        }

        // Check for CURL errors
        if(res != CURLE_OK) {
            brls::Logger::error("Download failed: {}", curl_easy_strerror(res));
            std::filesystem::remove(path);
            return false;
        }

        if (fileWriteError) {
            brls::Logger::error("Download failed due to file write error");
            std::filesystem::remove(path);
            return false;
        }

        if(http_code >= 400) {
            brls::Logger::error("HTTP error: {}", http_code);
            std::filesystem::remove(path);
            return false;
        }

        return true;
    }

}