#pragma once

#include <borealis.hpp>
#include <vector>
#include <string>

class ModGameCell : public brls::RecyclerCell {
public:
    ModGameCell();

    BRLS_BIND(brls::Label, label, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, image, "image");

    static ModGameCell* create();
};

class ModGameData : public brls::RecyclerDataSource {
public:
    ModGameData();
    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) override;
    std::string titleForHeader(brls::RecyclerFrame* recycler, int section) override;

private:
    std::vector<std::pair<std::string, std::string>> gameFolders; // <gameName, titleId>

    // Keep selected game strings alive for static pointers
    std::string selectedGameName;
    std::string selectedGamePath;
    std::string selectedTitleId;
};

class ModInstallTab : public brls::Box {
public:
    ModInstallTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::RecyclerFrame, recycler, "recycler");
    ModGameData* gameData;
};
