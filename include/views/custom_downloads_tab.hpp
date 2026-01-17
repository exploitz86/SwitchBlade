#pragma once

#include <borealis.hpp>
#include <nlohmann/json.hpp>

class CustomDownloadsTab : public brls::Box
{
public:
    CustomDownloadsTab();

    static brls::View* create();

private:
    void loadCustomDownloads();
    void refreshDownloadsList();
    void addCustomDownload(const std::string& category);
    void deleteCustomDownload(const std::string& category, const std::string& name);
    void downloadCustomPack(const std::string& category, const std::string& name, const std::string& url);
    void saveCustomDownloads();

    nlohmann::json customPacks;

    BRLS_BIND(brls::Box, customDownloadsBox, "custom_downloads_box");
    BRLS_BIND(brls::Label, descriptionLabel, "description_label");
    BRLS_BIND(brls::Label, warningLabel, "warning_label");
};
