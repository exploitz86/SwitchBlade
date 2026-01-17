#pragma once

#include <borealis.hpp>

class UpdateAtmosphereTab : public brls::Box
{
public:
    UpdateAtmosphereTab();

    static brls::View* create();

private:
    void createAtmosphereUpdateUI();
    void fetchCFWLinks();

    BRLS_BIND(brls::Label, atmosphereDescriptionLabel, "atmosphere_description");
    BRLS_BIND(brls::Label, currentAtmosphereLabel, "current_atmosphere");
    BRLS_BIND(brls::Label, revisionLabel, "revision");
    BRLS_BIND(brls::Label, ascentDescriptionLabel, "ascent_description");
    BRLS_BIND(brls::Label, currentAscentLabel, "current_ascent");
    BRLS_BIND(brls::Label, latestHOSLabel, "latest_hos");
    BRLS_BIND(brls::Box, atmosphereVersionsBox, "atmosphere_versions");
    BRLS_BIND(brls::Box, ascentVersionsBox, "ascent_versions");
};
