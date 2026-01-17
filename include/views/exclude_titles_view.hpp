#pragma once

#include <borealis.hpp>
#include <switch.h>
#include <string>
#include <vector>
#include <set>

class ExcludeTitlesView : public brls::AppletFrame {
public:
    ExcludeTitlesView();

private:
    void loadExcludedTitles();
    void saveExcludedTitles();
    void populateList();

    std::set<std::string> excludedTitles;
    brls::Box* list = nullptr;
};
