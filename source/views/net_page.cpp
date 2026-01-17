#include "views/net_page.hpp"
#include <arpa/inet.h>
#include <switch.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <nlohmann/json.hpp>

using namespace brls::literals;
using json = nlohmann::ordered_json;

NetPage::NetPage() {
    this->setTitle("menu/net/title"_i18n);

    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::COLUMN);
    contentBox->setWidth(brls::View::AUTO);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(20, 20, 20, 20);

    nifmInitialize(NifmServiceType_User);
    NifmNetworkProfileData profile;
    nifmGetCurrentNetworkProfile(&profile);
    nifmExit();

    int uuid = std::accumulate(profile.uuid.uuid, profile.uuid.uuid + 16, 0);

    std::string labelText;

    if (!uuid || !profile.ip_setting_data.mtu) {
        auto* noConnectionLabel = new brls::Label();
        noConnectionLabel->setText("menu/net/no_internet"_i18n);
        noConnectionLabel->setFontSize(16);
        contentBox->addView(noConnectionLabel);
    } else {
        // Display current network configuration
        if (profile.ip_setting_data.ip_address_setting.is_automatic) {
            labelText = "menu/net/ip_auto"_i18n;
        } else {
            labelText = fmt::format(
                "menu/net/ip_subnet_gateway"_i18n,
                ipToString(profile.ip_setting_data.ip_address_setting.current_addr.addr),
                ipToString(profile.ip_setting_data.ip_address_setting.subnet_mask.addr),
                ipToString(profile.ip_setting_data.ip_address_setting.gateway.addr));
        }
        labelText = fmt::format("menu/net/local_ip_mtu"_i18n, labelText, std::string(inet_ntoa({(in_addr_t)gethostid()})), std::to_string(unsigned(profile.ip_setting_data.mtu)));

        if (profile.ip_setting_data.dns_setting.is_automatic) {
            labelText = fmt::format("menu/net/dns_auto"_i18n, labelText);
        } else {
            labelText = fmt::format(
                "menu/net/primary_secondary_dns"_i18n,
                labelText,
                ipToString(profile.ip_setting_data.dns_setting.primary_dns_server.addr),
                ipToString(profile.ip_setting_data.dns_setting.secondary_dns_server.addr));
        }

        auto* currentSettingsLabel = new brls::Label();
        currentSettingsLabel->setText(labelText);
        currentSettingsLabel->setFontSize(16);
        currentSettingsLabel->setMarginBottom(20);
        contentBox->addView(currentSettingsLabel);

        // Build preset profiles
        json profiles = json::array();

        profiles.push_back(
            json::object({{"name", "LAN-Play"},
                          {"ip_addr", fmt::format("10.13.{}.{}", std::rand() % 256, std::rand() % 253 + 2)},
                          {"subnet_mask", "255.255.0.0"},
                          {"gateway", "10.13.37.1"}}));

        profiles.push_back(
            json::object({{"name", "Automatic IP Address"},
                          {"ip_auto", true}}));

        profiles.push_back(
            json::object({{"name", "Automatic DNS"},
                          {"dns_auto", true}}));

        profiles.push_back(
            json::object({{"name", "90DNS (Europe)"},
                          {"dns1", "163.172.141.219"},
                          {"dns2", "207.246.121.77"}}));

        profiles.push_back(
            json::object({{"name", "90DNS (USA)"},
                          {"dns1", "207.246.121.77"},
                          {"dns2", "163.172.141.219"}}));

        profiles.push_back(
            json::object({{"name", "Google DNS"},
                          {"dns1", "8.8.8.8"},
                          {"dns2", "8.8.4.4"}}));

        profiles.push_back(
            json::object({{"name", "ACNH MTU"},
                          {"mtu", 1500}}));

        auto* header = new brls::Header();
        header->setTitle("menu/net/network_presets"_i18n);
        contentBox->addView(header);

        for (const auto& p : profiles.items()) {
            json values = p.value();
            std::string name = values.contains("name") ? values["name"] : "Unnamed";

            auto* item = new brls::RadioCell();
            item->title->setText(name);

            item->registerClickAction([this, values, name](brls::View* view) {
                auto* dialog = new brls::Dialog("menu/net/apply_preset_confirm"_i18n + values.dump(2));

                auto callbackOk = [this, dialog, values]() {
                    nifmExit();  // Exit the global User service first!
                    nifmInitialize(NifmServiceType_Admin);
                    NifmNetworkProfileData profile;
                    nifmGetCurrentNetworkProfile(&profile);

                    unsigned char buf[sizeof(struct in6_addr)];

                    if (values.contains("ip_addr")) {
                        if (inet_pton(AF_INET, std::string(values["ip_addr"]).c_str(), buf)) {
                            profile.ip_setting_data.ip_address_setting.is_automatic = u8(0);
                            stringToIp(std::string(values["ip_addr"]), profile.ip_setting_data.ip_address_setting.current_addr.addr);
                        }
                    }
                    if (values.contains("subnet_mask")) {
                        if (inet_pton(AF_INET, std::string(values["subnet_mask"]).c_str(), buf)) {
                            stringToIp(std::string(values["subnet_mask"]), profile.ip_setting_data.ip_address_setting.subnet_mask.addr);
                        }
                    }
                    if (values.contains("gateway")) {
                        if (inet_pton(AF_INET, std::string(values["gateway"]).c_str(), buf)) {
                            stringToIp(std::string(values["gateway"]), profile.ip_setting_data.ip_address_setting.gateway.addr);
                        }
                    }
                    if (values.contains("dns1")) {
                        if (inet_pton(AF_INET, std::string(values["dns1"]).c_str(), buf)) {
                            profile.ip_setting_data.dns_setting.is_automatic = u8(0);
                            stringToIp(std::string(values["dns1"]), profile.ip_setting_data.dns_setting.primary_dns_server.addr);
                        }
                    }
                    if (values.contains("dns2")) {
                        if (inet_pton(AF_INET, std::string(values["dns2"]).c_str(), buf)) {
                            profile.ip_setting_data.dns_setting.is_automatic = u8(0);
                            stringToIp(std::string(values["dns2"]), profile.ip_setting_data.dns_setting.secondary_dns_server.addr);
                        }
                    }
                    if (values.contains("mtu")) {
                        profile.ip_setting_data.mtu = u16(values["mtu"]);
                    }
                    if (values.contains("ip_auto")) {
                        profile.ip_setting_data.ip_address_setting.is_automatic = u8(values["ip_auto"]);
                    }
                    if (values.contains("dns_auto")) {
                        profile.ip_setting_data.dns_setting.is_automatic = u8(values["dns_auto"]);
                    }

                    nifmSetNetworkProfile(&profile, &profile.uuid);
                    nifmSetWirelessCommunicationEnabled(true);
                    nifmExit();
                    nifmInitialize(NifmServiceType_User);  // Reinitialize immediately
                    
                    dialog->close();
                };

                auto callbackNo = [dialog]() {
                    dialog->close();
                };

                dialog->addButton("menu/net/apply"_i18n, callbackOk);
                dialog->addButton("hints/cancel"_i18n, callbackNo);
                dialog->setCancelable(false);
                dialog->open();

                return true;
            });

            contentBox->addView(item);
        }
    }

    scrollFrame->setContentView(contentBox);
    this->setContentView(scrollFrame);

    this->setTitle("menu/net/title"_i18n);
    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });
}

std::string NetPage::ipToString(unsigned char* ip) {
    std::string res = "";
    for (size_t i = 0; i < 3; i++) {
        res += std::to_string(unsigned(ip[i]));
        res += ".";
    }
    res += std::to_string(unsigned(ip[3]));
    return res;
}

int NetPage::stringToIp(const std::string& ip, unsigned char* out) {
    size_t start;
    size_t end = 0;
    int i = 0;
    while ((start = ip.find_first_not_of(".", end)) != std::string::npos) {
        end = ip.find(".", start);
        out[i] = u8(std::stoi(ip.substr(start, end - start)));
        i++;
    }
    return 0;
}
