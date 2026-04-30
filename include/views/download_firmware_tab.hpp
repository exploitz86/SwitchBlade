#pragma once

#include <borealis.hpp>
#include <string>

class DownloadFirmwareTab : public brls::Box
{
public:
    DownloadFirmwareTab();

    static brls::View* create();

private:
    void setDescription();
    void fetchFirmwareLinks();
    bool isFirmwareOlderThanCurrent(const std::string& selectedFirmwareName) const;
    void openFirmwareDownloadView(const std::string& name, const std::string& url);

    std::string currentFirmwareVersion;
    bool hasCurrentFirmwareVersion = false;

    BRLS_BIND(brls::Label, firmwareDescLabel, "firmware_desc");
    BRLS_BIND(brls::Label, firmwareWarningLabel, "firmware_warning");
    BRLS_BIND(brls::Label, firmwareVersionLabel, "firmware_version");
    BRLS_BIND(brls::Label, boxTitleLabel, "box_title");
    BRLS_BIND(brls::Box, firmwareListBox, "firmware_list");
};
