#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <vector>
#include <string>

class CheatSlipsGameCell : public brls::RecyclerCell {
public:
    CheatSlipsGameCell();

    BRLS_BIND(brls::Label, label, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, image, "image");

    static CheatSlipsGameCell* create();
};

class CheatSlipsGameData : public brls::RecyclerDataSource {
public:
    CheatSlipsGameData();
    int numberOfSections(brls::RecyclerFrame* recycler) override;
    int numberOfRows(brls::RecyclerFrame* recycler, int section) override;
    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override;
    void didSelectRowAt(brls::RecyclerFrame* recycler, brls::IndexPath indexPath) override;
    std::string titleForHeader(brls::RecyclerFrame* recycler, int section) override;

private:
    struct GameInfo {
        std::string name;
        u64 tid;
        std::string tidFormatted;
    };
    std::vector<GameInfo> games;
};

class CheatSlipsPage : public brls::AppletFrame {
public:
    CheatSlipsPage();

    static brls::View* create();

private:
    CheatSlipsGameData* gameData;
};
