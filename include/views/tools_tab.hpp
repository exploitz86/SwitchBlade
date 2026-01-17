#pragma once

#include <borealis.hpp>

class ToolsTab : public brls::Box {
public:
    ToolsTab();

    static brls::View* create();
private:
    BRLS_BIND(brls::SelectorCell, language_selector, "language_selector");
    BRLS_BIND(brls::Box, tools_box, "tools_box");
};