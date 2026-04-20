#include "views/dns_tester_page.hpp"
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>

using namespace brls::literals;

const char* hostnames[] = {
    "nintendo.com",
    "nintendo.net",
    "nintendo.jp",
    "nintendo.co.jp",
    "nintendo.co.uk",
    "nintendo-europe.com",
    "nintendowifi.net",
    "nintendo.es",
    "nintendo.co.kr",
    "nintendo.tw",
    "nintendo.com.hk",
    "nintendo.com.au",
    "nintendo.co.nz",
    "nintendo.at",
    "nintendo.be",
    "nintendods.cz",
    "nintendo.dk",
    "nintendo.de",
    "nintendo.fi",
    "nintendo.fr",
    "nintendo.gr",
    "nintendo.hu",
    "nintendo.it",
    "nintendo.nl",
    "nintendo.no",
    "nintendo.pt",
    "nintendo.ru",
    "nintendo.co.za",
    "nintendo.se",
    "nintendo.ch",
    "nintendo.pl",
    "nintendoswitch.com",
    "nintendoswitch.com.cn",
    "nintendoswitch.cn",
    "sun.hac.lp1.d4c.nintendo.net",
};

const int HOSTNAME_COUNT = sizeof(hostnames) / sizeof(hostnames[0]);

DNS_RESOLVER_STATUS DNSTesterPage::resolveHostname(const char* hostname)
{
    struct hostent *he;
    struct in_addr a;
    
    he = gethostbyname(hostname);
    if (he) {
        while (*he->h_addr_list) {
            std::memcpy(&a, *he->h_addr_list++, sizeof(a));
            
            std::string ip_str = inet_ntoa(a);
            if (ip_str == "127.0.0.1" || ip_str == "0.0.0.0") {
                return DNS_BLOCKED;
            }
        }
        return DNS_RESOLVED;
    }
    return DNS_UNRESOLVED;
}

DNSTesterPage::DNSTesterPage() {
    
    auto* scrollFrame = new brls::ScrollingFrame();
    scrollFrame->setWidth(brls::View::AUTO);
    scrollFrame->setHeight(brls::View::AUTO);
    
    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::COLUMN);
    contentBox->setWidth(brls::View::AUTO);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(20, 20, 20, 20);
    
    auto* titleLabel = new brls::Label();
    titleLabel->setText("menu/dns_tester/description"_i18n);
    titleLabel->setFontSize(16);
    titleLabel->setMarginBottom(20);
    titleLabel->setFocusable(false);
    contentBox->addView(titleLabel);
    
    Result net_rc = nifmGetInternetConnectionStatus(NULL, NULL, NULL);
    
    if (R_FAILED(net_rc)) {
        auto* errorLabel = new brls::Label();
        errorLabel->setText(std::string("menu/dns_tester/no_connection"_i18n));
        errorLabel->setTextColor(nvgRGB(220, 100, 100));
        errorLabel->setFontSize(18);
        errorLabel->setFocusable(false);
        contentBox->addView(errorLabel);
    } else {
        int blocked_count = 0;
        int resolved_count = 0;
        int unresolved_count = 0;
        
        for (int i = 0; i < HOSTNAME_COUNT; i++) {
            int result = resolveHostname(hostnames[i]);
            
            auto* item = new brls::DetailCell();
            item->setText(hostnames[i]);
            
            switch(result) {
                case DNS_BLOCKED:
                    item->setDetailText(std::string("menu/dns_tester/blocked"_i18n));
                    item->setDetailTextColor(nvgRGB(88, 195, 169));
                    blocked_count++;
                    break;
                case DNS_RESOLVED:
                    item->setDetailText(std::string("menu/dns_tester/unblocked"_i18n));
                    item->setDetailTextColor(nvgRGB(220, 100, 100));
                    resolved_count++;
                    break;
                case DNS_UNRESOLVED:
                    item->setDetailText(std::string("menu/dns_tester/unresolved"_i18n));
                    item->setDetailTextColor(nvgRGB(208, 168, 50));
                    unresolved_count++;
                    break;
            }
            item->setFocusable(false);
            contentBox->addView(item);
        }
        
        auto* summaryLabel = new brls::Label();
        summaryLabel->setText("\n" + std::string("menu/dns_tester/summary"_i18n));
        summaryLabel->setFontSize(18);
        summaryLabel->setMarginTop(20);
        summaryLabel->setMarginBottom(10);
        summaryLabel->setFocusable(false);
        contentBox->addView(summaryLabel);
        
        auto* blockedLabel = new brls::Label();
        blockedLabel->setText(std::string("menu/dns_tester/blocked_label"_i18n) + ": " + std::to_string(blocked_count));
        blockedLabel->setFontSize(16);
        blockedLabel->setMarginBottom(5);
        blockedLabel->setFocusable(false);
        contentBox->addView(blockedLabel);
        
        auto* resolvedLabel = new brls::Label();
        resolvedLabel->setText(std::string("menu/dns_tester/resolved_label"_i18n) + ": " + std::to_string(resolved_count));
        resolvedLabel->setFontSize(16);
        resolvedLabel->setMarginBottom(5);
        resolvedLabel->setFocusable(false);
        contentBox->addView(resolvedLabel);
        
        auto* unresolvedLabel = new brls::Label();
        unresolvedLabel->setText(std::string("menu/dns_tester/unresolved_label"_i18n) + ": " + std::to_string(unresolved_count));
        unresolvedLabel->setFontSize(16);
        unresolvedLabel->setFocusable(false);
        contentBox->addView(unresolvedLabel);
    }
    
    scrollFrame->setContentView(contentBox);
    scrollFrame->setFocusable(true);
    this->setContentView(scrollFrame);
    
    brls::Application::giveFocus(scrollFrame);
    
    this->setTitle("menu/dns_tester/title"_i18n);

    this->registerAction("hints/back"_i18n, brls::ControllerButton::BUTTON_B,
        [](brls::View* view) {
            brls::Application::popActivity();
            return true;
        },
        false);
    
}
