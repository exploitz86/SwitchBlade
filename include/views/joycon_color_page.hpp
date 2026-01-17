#pragma once

#include <borealis.hpp>

class JoyConColorPage : public brls::AppletFrame {
public:
    JoyConColorPage();

private:
    void loadColorProfiles();
};
