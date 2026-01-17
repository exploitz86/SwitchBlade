#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <string>
#include <borealis/views/cells/cell_radio.hpp>

using json = nlohmann::ordered_json;

class CheatSlipsDownloadView : public brls::AppletFrame {
public:
    CheatSlipsDownloadView(uint64_t tid, const std::string& gameName);
    ~CheatSlipsDownloadView() {
        if (loadThread.joinable())
            loadThread.join();
    }

    static brls::View* create();

private:
    uint64_t titleId;
    std::string gameTitle;
    std::string buildId;

    void loadCheats();
    void getBuildId();
    void getBuildIdFromDmnt();
    void getBuildIdFromVersions();
    void displayCheats(const json& cheatsInfo);
    void downloadSelectedCheats();
    void showCheatsContent(const json& titles);

    std::thread loadThread;
    std::vector<std::pair<brls::RadioCell*, int>> toggles;
};

