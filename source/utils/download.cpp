#include "utils/download.hpp"
#include <curl/curl.h>
#include <borealis.hpp>

namespace download {

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    bool getRequest(const std::string& url, nlohmann::json& res) {
        CURL* curl = curl_easy_init();
        std::string readBuffer;
        bool success = false;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade/1.0");

            CURLcode result = curl_easy_perform(curl);

            if (result == CURLE_OK) {
                try {
                    res = nlohmann::json::parse(readBuffer);
                    success = true;
                } catch (nlohmann::json::parse_error& e) {
                    brls::Logger::error("JSON parse error: {}", e.what());
                    success = false;
                }
            } else {
                brls::Logger::error("CURL error: {}", curl_easy_strerror(result));
                success = false;
            }

            curl_easy_cleanup(curl);
        }

        return success;
    }

    bool getRequest(const std::string& url, nlohmann::json& res, const std::vector<std::string>& headers) {
        CURL* curl = curl_easy_init();
        std::string readBuffer;
        bool success = false;

        if (curl) {
            struct curl_slist* header_list = nullptr;
            for (const auto& h : headers) {
                header_list = curl_slist_append(header_list, h.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade/1.0");

            CURLcode result = curl_easy_perform(curl);

            if (result == CURLE_OK) {
                try {
                    res = nlohmann::json::parse(readBuffer);
                    success = true;
                } catch (nlohmann::json::parse_error& e) {
                    brls::Logger::error("JSON parse error: {}", e.what());
                    success = false;
                }
            } else {
                brls::Logger::error("CURL error: {}", curl_easy_strerror(result));
                success = false;
            }

            curl_slist_free_all(header_list);
            curl_easy_cleanup(curl);
        }

        return success;
    }

    bool postRequest(const std::string& url, const std::vector<std::string>& headers,
                     const std::string& body, nlohmann::json& res) {
        CURL* curl = curl_easy_init();
        std::string readBuffer;
        bool success = false;

        if (curl) {
            struct curl_slist* header_list = nullptr;
            for (const auto& h : headers) {
                header_list = curl_slist_append(header_list, h.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade/1.0");

            CURLcode result = curl_easy_perform(curl);

            if (result == CURLE_OK) {
                try {
                    res = nlohmann::json::parse(readBuffer);
                    success = true;
                } catch (nlohmann::json::parse_error& e) {
                    brls::Logger::error("JSON parse error: {}", e.what());
                    success = false;
                }
            } else {
                brls::Logger::error("CURL error: {}", curl_easy_strerror(result));
                success = false;
            }

            curl_slist_free_all(header_list);
            curl_easy_cleanup(curl);
        }

        return success;
    }

    std::vector<std::pair<std::string, std::string>> getLinksFromJson(const nlohmann::json& json_object) {
        std::vector<std::pair<std::string, std::string>> links;

        if (json_object.is_object()) {
            for (auto it = json_object.begin(); it != json_object.end(); ++it) {
                std::string key = it.key();
                std::string value;

                if (it.value().is_string()) {
                    value = it.value().get<std::string>();
                } else if (it.value().is_object() && it.value().contains("url")) {
                    value = it.value()["url"].get<std::string>();
                }

                if (!value.empty()) {
                    links.push_back({key, value});
                }
            }
        }

        return links;
    }

}
