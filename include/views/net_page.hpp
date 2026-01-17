#pragma once

#include <borealis.hpp>
#include <switch.h>

constexpr int AF_INET = 2;

class NetPage : public brls::AppletFrame {
private:
    std::string ipToString(unsigned char* ip);
    int stringToIp(const std::string& ip, unsigned char* out);

public:
    NetPage();
};
