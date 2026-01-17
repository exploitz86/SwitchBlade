#include "utils/mod_status.hpp"
#include <fmt/core.h>

using namespace brls::literals;

namespace ModStatus {

std::string toDisplayString(const std::string& canonicalStatus) {
    // Check for exact matches first
    if (canonicalStatus == CANONICAL_INACTIVE) {
        return "menu/mods/inactive"_i18n;
    }
    if (canonicalStatus == CANONICAL_ACTIVE) {
        return "menu/mods/active"_i18n;
    }
    if (canonicalStatus == CANONICAL_UNCHECKED) {
        return "menu/mods/unchecked"_i18n;
    }
    if (canonicalStatus == CANONICAL_NO_FILE) {
        return "menu/mods/no_file"_i18n;
    }
    
    // Check for PARTIAL with count
    if (canonicalStatus.find(CANONICAL_PARTIAL_PREFIX) == 0) {
        // Extract numbers from "PARTIAL (x/y)"
        size_t openParen = canonicalStatus.find('(');
        size_t slash = canonicalStatus.find('/');
        size_t closeParen = canonicalStatus.find(')');
        
        if (openParen != std::string::npos && 
            slash != std::string::npos && 
            closeParen != std::string::npos) {
            try {
                int current = std::stoi(canonicalStatus.substr(openParen + 1, slash - openParen - 1));
                int total = std::stoi(canonicalStatus.substr(slash + 1, closeParen - slash - 1));
                return fmt::format("menu/mods/partial"_i18n, current, total);
            } catch (...) {
                // If parsing fails, return the raw string
                return canonicalStatus;
            }
        }
        // If no count, just return generic partial
        return "menu/mods/partial_find"_i18n;
    }
    
    // Unknown status, return as-is
    return canonicalStatus;
}

bool isActive(const std::string& status) {
    return status == CANONICAL_ACTIVE || status.find(CANONICAL_PARTIAL_PREFIX) == 0;
}

bool isFullyActive(const std::string& status) {
    return status == CANONICAL_ACTIVE;
}

bool isInactive(const std::string& status) {
    return status == CANONICAL_INACTIVE;
}

bool isUnchecked(const std::string& status) {
    return status == CANONICAL_UNCHECKED;
}

bool isPartial(const std::string& status) {
    return status.find(CANONICAL_PARTIAL_PREFIX) == 0;
}

bool isNoFile(const std::string& status) {
    return status == CANONICAL_NO_FILE;
}

std::string createPartial(int current, int total) {
    return fmt::format("PARTIAL ({}/{})", current, total);
}

}  // namespace ModStatus
