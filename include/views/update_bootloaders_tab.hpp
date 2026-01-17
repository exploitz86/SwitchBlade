#pragma once

#include <borealis.hpp>
#include <string>

class UpdateBootloadersTab : public brls::Box {
public:
    UpdateBootloadersTab();
    static brls::View* create();

private:
    BRLS_BIND(brls::Label, bootloaders_title, "bootloaders_title");
    BRLS_BIND(brls::Label, bootloaders_desc, "bootloaders_desc");
    BRLS_BIND(brls::Box, bootloader_list, "bootloader_list");

    BRLS_BIND(brls::Label, hekate_ipl_title, "hekate_ipl_title");
    BRLS_BIND(brls::Box, hekate_ipl_list, "hekate_ipl_list");

    BRLS_BIND(brls::Label, payloads_title, "payloads_title");
    BRLS_BIND(brls::Box, payloads_list, "payloads_list");

    void fetchBootloaderLinks();
    void fetchHekateIplLinks();
    void fetchPayloadLinks();
    void setDescription();
};
