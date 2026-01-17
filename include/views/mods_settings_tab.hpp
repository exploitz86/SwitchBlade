#pragma once

#include <borealis.hpp>

class ModsSettingsTab : public brls::Box {
public:
    ModsSettingsTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::BooleanCell, strict_cell, "strict_cell");
};
