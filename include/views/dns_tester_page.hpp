#pragma once

#include <borealis.hpp>
#include <switch.h>

typedef enum DNS_RESOLVER_STATUS {
    DNS_BLOCKED,
    DNS_RESOLVED,
    DNS_UNRESOLVED
} DNS_RESOLVER_STATUS;

class DNSTesterPage : public brls::AppletFrame {
private:
    DNS_RESOLVER_STATUS resolveHostname(const char* hostname);
    
public:
    DNSTesterPage();
};
