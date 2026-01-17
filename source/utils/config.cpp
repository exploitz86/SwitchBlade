#include "utils/config.hpp"
#include "utils/constants.hpp"

#include <switch.h>
#include <filesystem>
#include <fstream>
#include <vector>

#include <borealis.hpp>

bool cp(const char *filein, const char *fileout) {
    FILE *exein, *exeout;
    exein = fopen(filein, "rb");
    if (exein == NULL) {
        /* handle error */
        perror("file open for reading");
        return false;
    }
    exeout = fopen(fileout, "wb");
    if (exeout == NULL) {
        /* handle error */
        perror("file open for writing");
        return false;
    }
    size_t n, m;
    unsigned char buff[8192];
    do {
        n = fread(buff, 1, sizeof buff, exein);
        if (n) m = fwrite(buff, 1, n, exeout);
        else   m = 0;
    }
    while ((n > 0) && (n == m));
    if (m) {
        perror("copy");
        return false;
    }
    if (fclose(exeout)) {
        perror("close output file");
        return false;
    }
    if (fclose(exein)) {
        perror("close input file");
        return false;
    }
    return true;
} 

namespace cfg {
    Config::Config() {
        this->loadConfig();
        this->parseConfig();
    }

    void Config::loadConfig() {
        std::string settings_path = std::string(CONFIG_PATH) + "settings.json";
        std::string full_path = "sdmc:" + settings_path;
        
        if(!std::filesystem::exists(full_path)) {
            chdir("sdmc:/");
            std::filesystem::create_directories("sdmc:" + std::string(CONFIG_PATH));
            cp("romfs:/json/settings.json", (char*)full_path.c_str());
        }

        std::ifstream file(full_path);
        config = nlohmann::json::parse(file);
        file.close();
    }

    void Config::parseConfig() {
        try {
            app_language = config.contains("language") ? config["language"].get<std::string>() : "en-US";
            is_strict = config.contains("is_strict") ? config["is_strict"].get<bool>() : true;
            wireframe = config.contains("wireframe") ? config["wireframe"].get<bool>() : false;
        } catch (const std::exception& e) {
            brls::Logger::error("Error parsing config: {}", e.what());
            app_language = "en-US";
            is_strict = true;
            wireframe = false;
        }
    }

    std::string Config::getAppLanguage() {
        return app_language;
    }

    void Config::setAppLanguage(const std::string& app_language) {
        this->app_language = app_language;
    }

    bool Config::getStrictSearch() {
        return this->is_strict;
    }
    void Config::setStringSearch(bool strict) {
        this->is_strict = strict;
    }

    void Config::saveConfig() {
        this->config["language"] = app_language;
        this->config["is_strict"] = is_strict;
        this->config["wireframe"] = wireframe;

        std::string settings_path = std::string(CONFIG_PATH) + "settings.json";
        std::string full_path = "sdmc:" + settings_path;
        std::ofstream file(full_path);
        file << this->config.dump(4);
        file.close();
    }
}