#pragma once

#include <borealis.hpp>
#include <functional>

class ModOptionsTab : public brls::Box {
public:
    ModOptionsTab(const std::string& gameName,
                  const std::string& gamePath,
                  const std::string& titleId,
                  std::function<void()> onModsChanged = nullptr);

    static brls::View* create();
    void willAppear(bool resetState = false) override;

private:
    BRLS_BIND(brls::Box, optionsList, "options_list");

    std::string gameName;
    std::string gamePath;
    std::string titleId;
    std::function<void()> onModsChangedCallback;

    void populateOptions();
    void recheckAllMods();
    void disableAllMods();
};
