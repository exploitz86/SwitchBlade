#pragma once

#include <borealis.hpp>

class CheatsModsTab : public brls::Box {
public:
    CheatsModsTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::RadioCell, modsMenuButton, "mods_menu_button");
    BRLS_BIND(brls::RadioCell, cheatsMenuButton, "cheats_menu_button");
    BRLS_BIND(brls::Label, cheatmenuDescriptionLabel, "cheat_menu_desc");
    BRLS_BIND(brls::Label, modmenuDescriptionLabel, "mod_menu_desc");
};
