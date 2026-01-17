#pragma once

#include <borealis.hpp>

class CheatsToolsTab : public brls::Box {
public:
    CheatsToolsTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, contentBox, "content_box");
};
