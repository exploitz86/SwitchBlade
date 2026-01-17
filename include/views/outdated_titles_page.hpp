#pragma once

#include <borealis.hpp>
#include <nlohmann/json.hpp>
#include <switch.h>
#include <string>
#include <vector>

struct OutdatedTitle {
    std::string titleId;
    std::string name;
    u32 currentVersion;
    u32 latestVersion;
    std::string latestBuildId;
    int missingDlcCount;
    std::vector<std::pair<std::string, std::string>> missingDlcInfo;  // {name, id}
};

class OutdatedTitleCell : public brls::RecyclerCell {
public:
    OutdatedTitleCell();

    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, image, "image");

    static OutdatedTitleCell* create();
};

class OutdatedTitleDetailsPage : public brls::AppletFrame {
public:
    OutdatedTitleDetailsPage(const std::string& gameName,
                             u32 currentVersion,
                             u32 latestVersion,
                             const std::string& latestBuildId,
                             const std::vector<std::pair<std::string, std::string>>& missingDlcInfo);
};

class OutdatedTitlesData : public brls::RecyclerDataSource {
public:
    OutdatedTitlesData(const std::vector<OutdatedTitle>& titles);

    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) override;
    std::string titleForHeader(brls::RecyclerFrame* recycler, int section) override;

private:
    std::vector<OutdatedTitle> outdatedTitles;
};

class OutdatedTitlesPage : public brls::AppletFrame
{
public:
    OutdatedTitlesPage();

private:
    nlohmann::json versionsData;
    std::vector<OutdatedTitle> outdatedTitles;

    void loadVersionsData();
    void checkForOutdatedTitles();
};
