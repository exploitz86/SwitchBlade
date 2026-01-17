#pragma once

#include <borealis.hpp>

class DownloadFirmwareTab : public brls::Box
{
public:
    DownloadFirmwareTab();

    static brls::View* create();

private:
    void setDescription();
    void fetchFirmwareLinks();

    BRLS_BIND(brls::Label, firmwareDescLabel, "firmware_desc");
    BRLS_BIND(brls::Label, firmwareVersionLabel, "firmware_version");
    BRLS_BIND(brls::Label, boxTitleLabel, "box_title");
    BRLS_BIND(brls::Box, firmwareListBox, "firmware_list");
};
