#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <vector>
#include <string>

class CheatGameCell : public brls::RecyclerCell {
public:
    CheatGameCell();

    BRLS_BIND(brls::Label, label, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    BRLS_BIND(brls::Image, image, "image");

    static CheatGameCell* create();
};

class CheatGameData : public brls::RecyclerDataSource {
public:
    CheatGameData(bool isGfxMode = false);
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
    bool isGfxMode;
};

class CheatAppPage : public brls::AppletFrame {
public:
    CheatAppPage(bool isGfxCheats = false);

    static brls::View* create();

private:
    CheatGameData* gameData;
    bool isGfxCheats;
};
