#pragma once

#include <string>
#include <borealis.hpp>

namespace ModStatus {
    // Canonical tokens - used for cache storage and internal comparisons
    // These NEVER change and are NOT localized
    const std::string CANONICAL_INACTIVE = "INACTIVE";
    const std::string CANONICAL_ACTIVE = "ACTIVE";
    const std::string CANONICAL_UNCHECKED = "UNCHECKED";
    const std::string CANONICAL_PARTIAL_PREFIX = "PARTIAL";  // Used for "PARTIAL (x/y)" format
    const std::string CANONICAL_NO_FILE = "NO FILE";

    /**
     * Convert a canonical status token to a localized display string
     * @param canonicalStatus The canonical status (e.g., "ACTIVE", "INACTIVE", "PARTIAL (2/5)")
     * @return Localized string for display in UI
     */
    std::string toDisplayString(const std::string& canonicalStatus);

    /**
     * Check if a status is active (either ACTIVE or PARTIAL)
     * @param status The status to check (canonical format)
     * @return true if the mod is active or partially active
     */
    bool isActive(const std::string& status);

    /**
     * Check if a status is the ACTIVE token
     * @param status The status to check (canonical format)
     * @return true if exactly ACTIVE
     */
    bool isFullyActive(const std::string& status);

    /**
     * Check if a status is the INACTIVE token
     * @param status The status to check (canonical format)
     * @return true if INACTIVE
     */
    bool isInactive(const std::string& status);

    /**
     * Check if a status is the UNCHECKED token
     * @param status The status to check (canonical format)
     * @return true if UNCHECKED
     */
    bool isUnchecked(const std::string& status);

    /**
     * Check if a status starts with PARTIAL
     * @param status The status to check (canonical format)
     * @return true if starts with "PARTIAL"
     */
    bool isPartial(const std::string& status);

    /**
     * Check if a status is the NO FILE token
     * @param status The status to check (canonical format)
     * @return true if NO FILE
     */
    bool isNoFile(const std::string& status);

    /**
     * Create a canonical PARTIAL status with count
     * @param current Current number of files
     * @param total Total number of files
     * @return Canonical "PARTIAL (x/y)" string
     */
    std::string createPartial(int current, int total);
}
