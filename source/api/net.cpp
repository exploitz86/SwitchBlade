#include "api/net.hpp"
#include "utils/progress_event.hpp"

#include <curl/curl.h>
#include <borealis.hpp>
#include <fstream>

namespace net {
    std::chrono::_V2::steady_clock::time_point time_old;
    double dlold;

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

    size_t WriteCallbackFile(void* ptr, size_t size, size_t nmemb, std::ofstream *stream) {
        stream->write(static_cast<char *>(ptr), size * nmemb);
        if(stream->bad()) {
            brls::Logger::error("Error writing to file");
            return 0;
        }
        return size * nmemb;
    }
    
    int downloadFileProgress(void* p, double dltotal, double dlnow, double ultotal, double ulnow) {
        //brls::Logger::debug("Downloaded: {} / {}", dlnow, dltotal);
        if(dltotal < 0.0) return 0;

        // Check if download was interrupted
        if (ProgressEvent::instance().getInterupt()) {
            brls::Logger::error("Download interrupted by user");
            return 1;  // Return non-zero to abort CURL transfer
        }

        double progress = dlnow / dltotal;
        int counter = (int)(progress * ProgressEvent::instance().getMax());
        ProgressEvent::instance().setStep(std::min(ProgressEvent::instance().getMax() - 1, counter));
        ProgressEvent::instance().setNow(dlnow);
        ProgressEvent::instance().setTotalCount(dltotal);
        auto time_now = std::chrono::steady_clock::now();
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

        auto curl = curl_easy_init();

        if(!curl) {
            brls::Logger::error("Failed to initialize curl");
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, downloadFileProgress);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512000L);
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
        curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) {
            brls::Logger::error("Failed to open file for writing: {}", path);
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);

        auto res = curl_easy_perform(curl);

        ofs.close();

        // Check if aborted by callback (user cancelled)
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            brls::Logger::info("Download aborted by user");
            std::filesystem::remove(path);
            curl_easy_cleanup(curl);
            return false;
        }

        // Check for CURL errors
        if(res != CURLE_OK) {
            brls::Logger::error("Download failed: {}", curl_easy_strerror(res));
            std::filesystem::remove(path);
            curl_easy_cleanup(curl);
            return false;
        }

        // Check HTTP status code
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if(http_code >= 400) {
            brls::Logger::error("HTTP error: {}", http_code);
            std::filesystem::remove(path);
            return false;
        }

        return true;
    }

}