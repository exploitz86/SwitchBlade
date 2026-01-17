#include "utils/utils.hpp"
#include "utils/constants.hpp"
#include "utils/download.hpp"

#include <switch.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <regex>
#include <fstream>
#include <curl/curl.h>
#include <borealis.hpp>
#include <SimpleIniParser.hpp>


using namespace simpleIniParser;

namespace utils {

    Result nacpGetLanguageEntrySpecialLanguage(NacpStruct* nacp, NacpLanguageEntry** langentry, const SetLanguage LanguageChoosen) {
        Result rc=0;
        SetLanguage Language = LanguageChoosen;
        NacpLanguageEntry *entry = NULL;
        u32 i=0;

        if (nacp==NULL || langentry==NULL)
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);

        *langentry = NULL;

        entry = &nacp->lang[SetLanguage_ENUS];

        if (entry->name[0]==0 && entry->author[0]==0) {
            for(i=0; i<16; i++) {
                entry = &nacp->lang[i];
                if (entry->name[0] || entry->author[0]) break;
            }
        }

        if (entry->name[0]==0 && entry->author[0]==0)
            return rc;

        *langentry = entry;

        return rc;
    }


    std::string formatApplicationId(u64 ApplicationId)
    {
        std::stringstream strm;
        strm << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << ApplicationId;
        return strm.str();
    }

    std::vector<std::pair<std::string, std::string>> getInstalledGames() {
        std::vector<std::pair<std::string, std::string>> games;

        NsApplicationRecord* records = new NsApplicationRecord[64000]();
        uint64_t tid;
        NsApplicationControlData controlData;
        NacpLanguageEntry* langEntry = nullptr;
        const char* desiredLanguageCode = "en";
        
        Result rc;
        int recordCount = 0;
        size_t controlSize = 0;

        nlohmann::json json = nlohmann::json::array();

        rc = nsListApplicationRecord(records, 64000, 0, &recordCount);
        for (auto i = 0; i < recordCount; ++i) {
            tid = records[i].application_id;
            rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &controlData, sizeof(controlData), &controlSize);
            if (R_FAILED(rc)) {
                continue; // Ou break je sais pas trop
            }

            rc = nacpGetLanguageEntrySpecialLanguage(&controlData.nacp, &langEntry, SetLanguage_ENUS);
            
            //rc = nsGetApplicationDesiredLanguage(&controlData.nacp, &langEntry);    
            if (R_FAILED(rc)) {
                continue; // Ou break je sais pas trop
            }

            if (!langEntry->name) {
                continue;
            }

            std::string appName = langEntry->name;
            std::string titleId = formatApplicationId(tid);
            json.push_back({
                {"name", appName},
                {"tid", titleId}
            });
        }

        delete[] records;

        for(auto i : json) {
            games.push_back(std::pair<std::string, std::string>(i.at("name").get<std::string>(), i.at("tid").get<std::string>()));
        }

        return games;
    }

    uint8_t* getIconFromTitleId(const std::string& titleId) {
        if(titleId.empty()) return nullptr;

        uint8_t* icon = nullptr;
        NsApplicationControlData controlData;
        size_t controlSize  = 0;
        uint64_t tid;

        std::istringstream buffer(titleId);
        buffer >> std::hex >> tid;

        if (R_FAILED(nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &controlData, sizeof(controlData), &controlSize))){ return nullptr; }

        icon = new uint8_t[0x20000];
        memcpy(icon, controlData.icon, 0x20000);
        return icon;
    }

    std::string removeHtmlTags(const std::string& input) {
        //Replace <br> by \n
        std::regex brRegex("<br>");
        std::string output = std::regex_replace(input, brRegex, "\n");
        std::regex nbspRegex("&nbsp;");
        output = std::regex_replace(output, nbspRegex, " ");
        std::regex tagsRegex("<.*?>");
        return std::regex_replace(output, tagsRegex, "");
    }

    bool ends_with(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) {
            return false;
        }
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    bool starts_with(const std::string& str, const std::string& prefix) {
        if (prefix.length() > str.length()) {
            return false;
        }
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    std::string getModInstallPath() {
        std::string config_file = "sdmc:" + std::string(CONFIG_PATH) + "parameters.ini";
        std::filesystem::path path(config_file);
        if(!std::filesystem::exists(path)) {
            return "mods";
        }
        Ini* config = Ini::parseFile(path.string());
        IniOption* path_mods = config->findFirstOption("stored-mods-base-folder");
        std::string pathString = path_mods->value;
        brls::Logger::debug("Mod install path: {}", pathString);
        if(ends_with(pathString, "/"))
            pathString.pop_back();
        if(starts_with(pathString, "/"))
            pathString.erase(0, 1);
        return pathString;
    }

    std::string timestamp_to_date(time_t timestamp) {
        std::tm* timeinfo = std::gmtime(&timestamp);
        std::ostringstream os;
        os << std::setfill('0') << std::setw(2) << timeinfo->tm_mday << "/"
        << std::setfill('0') << std::setw(2) << (timeinfo->tm_mon + 1) << "/"
        << (timeinfo->tm_year + 1900);
        return os.str();
    }

    std::string file_size_to_string(int file_size) {
        const double kb = 1024.0;
        const double mb = std::pow(kb, 2);
        const double gb = std::pow(kb, 3);

        std::ostringstream os;
        if (file_size >= gb) {
            os << std::fixed << std::setprecision(2) << file_size / gb << " GB";
        } else if (file_size >= mb) {
            os << std::fixed << std::setprecision(2) << file_size / mb << " MB";
        } else if (file_size >= kb) {
            os << std::fixed << std::setprecision(2) << file_size / kb << " KB";
        } else {
            os << file_size << " B";
        }

        return os.str();
    }

    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }

        std::string content;
        std::getline(file, content);
        file.close();
        return content;
    }

    // Static callback for CURL to write data to string
    static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string downloadFileToString(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            brls::Logger::error("Failed to initialize curl for {}", url);
            return "";
        }

        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SwitchBlade");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            brls::Logger::error("Failed to download {}: {}", url, curl_easy_strerror(res));
            response.clear();
        } else {
            // Trim whitespace/newlines
            response.erase(response.find_last_not_of(" \n\r\t") + 1);
        }

        curl_easy_cleanup(curl);
        return response;
    }

    std::string getCheatsVersion() {
        return downloadFileToString(CHEATS_URL_VERSION);
    }

    std::string getContentsPath() {
        // Atmosphere uses /atmosphere/contents/
        return AMS_CONTENTS;
    }

    void saveToFile(const std::string& text, const std::string& path) {
        std::ofstream file(path);
        if (file.is_open()) {
            file << text << std::endl;
            file.close();
        }
    }

    u32 getInstalledVersion(uint64_t title_id) {
        u32 res = 0;
        NsApplicationContentMetaStatus* MetaStatus = new NsApplicationContentMetaStatus[100U];
        s32 out;
        nsListApplicationContentMetaStatus(title_id, 0, MetaStatus, 100, &out);
        for (int i = 0; i < out; i++) {
            if (res < MetaStatus[i].version) res = MetaStatus[i].version;
        }
        delete[] MetaStatus;
        return res;
    }

    bool isDlcInstalled(uint64_t dlc_title_id) {
        NsApplicationContentMetaStatus meta;
        s32 out = 0;
        Result rc = nsListApplicationContentMetaStatus(dlc_title_id, 0, &meta, 1, &out);
        return R_SUCCEEDED(rc) && out > 0 && meta.meta_type == NcmContentMetaType_AddOnContent;
    }
    
    std::string getLatestTag(const std::string& url) {
        nlohmann::json tag;
        download::getRequest(url, tag, {"accept: application/vnd.github.v3+json"});
        if (tag.contains("tag_name")) {
            return tag["tag_name"];
        }
        return "";
    }
    
    bool isUpdateAvailable(const std::string& currentVersion) {
        std::string latestTag = getLatestTag(TAGS_INFO);
        return !latestTag.empty() && latestTag != currentVersion;
    }
}