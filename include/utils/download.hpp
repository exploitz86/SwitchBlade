#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace download {
    // Fetch JSON data from a URL
    bool getRequest(const std::string& url, nlohmann::json& res);
    // Fetch JSON data with custom headers
    bool getRequest(const std::string& url, nlohmann::json& res, const std::vector<std::string>& headers);

    // POST JSON with custom headers, parse JSON response
    bool postRequest(const std::string& url, const std::vector<std::string>& headers,
                     const std::string& body, nlohmann::json& res);

    // Extract download links from JSON object
    std::vector<std::pair<std::string, std::string>> getLinksFromJson(const nlohmann::json& json_object);
}
