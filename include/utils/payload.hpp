#pragma once

#include <string>
#include <vector>

namespace Payload {
    std::vector<std::string> fetchPayloads();
    int rebootToPayload(const std::string& path);
}
