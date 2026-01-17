#pragma once

#include <borealis.hpp>

class CheatMenuTab : public brls::Box {
public:
    CheatMenuTab();

    static brls::View* create();

private:
    BRLS_BIND(brls::Box, contentBox, "content_box");
};
