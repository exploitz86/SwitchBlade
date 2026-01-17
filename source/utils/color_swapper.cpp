#include "utils/color_swapper.hpp"
#include "utils/constants.hpp"
#include "api/net.hpp"
#include <borealis.hpp>
#include <switch.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace ColorSwapper {

int hexToBGR(const std::string& hex) {
    std::string color = hex;
    if (color[0] == '#') {
        color = color.substr(1);
    }

    int r = std::stoi(color.substr(0, 2), nullptr, 16);
    int g = std::stoi(color.substr(2, 2), nullptr, 16);
    int b = std::stoi(color.substr(4, 2), nullptr, 16);

    return (b << 16) | (g << 8) | r;
}

std::string BGRToHex(int v) {
    int r = v & 0xFF;
    int g = (v >> 8) & 0xFF;
    int b = (v >> 16) & 0xFF;

    char hex[7];
    snprintf(hex, sizeof(hex), "%02X%02X%02X", r, g, b);
    return std::string(hex);
}

} // namespace ColorSwapper

namespace JC {

int setColor(const std::vector<int>& colors) {
    Result res = 0;

    // Initialize hidsys and hiddbg
    res = hidsysInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hidsys: 0x{:X}", res);
        return res;
    }

    res = hiddbgInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hiddbg: 0x{:X}", res);
        hidsysExit();
        return res;
    }

    // Get unique pad IDs for handheld mode
    HidsysUniquePadId UniquePadIds[2] = {0};
    s32 nbEntries = 0;
    res = hidsysGetUniquePadsFromNpad(HidNpadIdType_Handheld, UniquePadIds, 2, &nbEntries);

    if (R_FAILED(res) || nbEntries < 2) {
        brls::Logger::error("Failed to get Joy-Con unique pad IDs");
        hiddbgExit();
        hidsysExit();
        return -1;
    }

    // Update colors: colors[0] = left body, colors[1] = left buttons,
    //                colors[2] = right body, colors[3] = right buttons
    Result ljc = hiddbgUpdateControllerColor(colors[0], colors[1], UniquePadIds[0]);
    Result rjc = hiddbgUpdateControllerColor(colors[2], colors[3], UniquePadIds[1]);

    hiddbgExit();
    hidsysExit();

    if (R_FAILED(ljc) || R_FAILED(rjc)) {
        brls::Logger::error("Failed to update Joy-Con colors: Left=0x{:X}, Right=0x{:X}", ljc, rjc);
        return -1;
    }

    return 0;
}

void changeJCColor(const std::vector<int>& values) {
    int res = setColor(values);

    if (res == 0) {
        auto* dialog = new brls::Dialog("Joy-Con colors updated successfully!");
        dialog->addButton("OK", []() {});
        dialog->open();
    } else {
        auto* dialog = new brls::Dialog("Failed to update Joy-Con colors. Make sure Joy-Cons are attached.");
        dialog->addButton("OK", []() {});
        dialog->open();
    }
}

bool backupJCColor(const std::string& path) {
    Result res = hidInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hid: 0x{:X}", res);
        return false;
    }

    HidNpadControllerColor color_left;
    HidNpadControllerColor color_right;
    res = hidGetNpadControllerColorSplit(HidNpadIdType_Handheld, &color_left, &color_right);
    hidExit();

    if (R_FAILED(res)) {
        brls::Logger::error("Failed to read Joy-Con colors: 0x{:X}", res);
        return false;
    }

    // Convert colors to hex
    std::string leftBody = ColorSwapper::BGRToHex(color_left.main);
    std::string leftButtons = ColorSwapper::BGRToHex(color_left.sub);
    std::string rightBody = ColorSwapper::BGRToHex(color_right.main);
    std::string rightButtons = ColorSwapper::BGRToHex(color_right.sub);

    // Create backup JSON
    json backupData;
    std::string timestamp = std::to_string(std::time(nullptr));
    std::string backupName = "Backup " + timestamp;

    backupData[backupName] = {
        {"L_JC", leftBody},
        {"L_BTN", leftButtons},
        {"R_JC", rightBody},
        {"R_BTN", rightButtons}
    };

    // Load existing profiles
    json existingProfiles;
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                file >> existingProfiles;
            } catch (...) {
                existingProfiles = json::array();
            }
            file.close();
        }
    }

    // If the existing file is an array format, convert to array and append
    if (!existingProfiles.is_array()) {
        existingProfiles = json::array();
    }

    // Create backup entry in array format
    json backupEntry = {
        {"name", backupName},
        {"L_JC", leftBody},
        {"L_BTN", leftButtons},
        {"R_JC", rightBody},
        {"R_BTN", rightButtons}
    };

    // Add backup to the beginning of the array
    existingProfiles.insert(existingProfiles.begin(), backupEntry);

    // Write back to file
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream outFile(path);
    if (outFile.is_open()) {
        outFile << existingProfiles.dump(4);
        outFile.close();
        brls::Logger::info("Joy-Con colors backed up successfully");
        return true;
    } else {
        brls::Logger::error("Failed to save backup");
        return false;
    }
}

std::vector<std::pair<std::string, std::vector<int>>> getProfiles(const std::string& path) {
    std::vector<std::pair<std::string, std::vector<int>>> profiles;

    // Load backups from local file first
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                json localProfiles;
                file >> localProfiles;

                if (localProfiles.is_array()) {
                    for (const auto& profile : localProfiles) {
                        if (profile.contains("name")) {
                            std::string name = profile["name"].get<std::string>();
                            // Only load backups from file
                            if (name.find("Backup") != std::string::npos) {
                                if (profile.contains("L_JC") && profile.contains("L_BTN") &&
                                    profile.contains("R_JC") && profile.contains("R_BTN")) {

                                    std::string leftBody = profile["L_JC"].get<std::string>();
                                    std::string leftButtons = profile["L_BTN"].get<std::string>();
                                    std::string rightBody = profile["R_JC"].get<std::string>();
                                    std::string rightButtons = profile["R_BTN"].get<std::string>();

                                    std::vector<int> colorValues = {
                                        ColorSwapper::hexToBGR(leftBody),
                                        ColorSwapper::hexToBGR(leftButtons),
                                        ColorSwapper::hexToBGR(rightBody),
                                        ColorSwapper::hexToBGR(rightButtons)
                                    };

                                    profiles.push_back({name, colorValues});
                                }
                            }
                        }
                    }
                }
                brls::Logger::info("Loaded {} backups from local file", profiles.size());
            } catch (...) {
                brls::Logger::error("Failed to parse local backup file");
            }
            file.close();
        }
    }

    // Add all hard-coded profiles
    profiles.push_back({"Gray", {
        ColorSwapper::hexToBGR("828282"),
        ColorSwapper::hexToBGR("0F0F0F"),
        ColorSwapper::hexToBGR("828282"),
        ColorSwapper::hexToBGR("0F0F0F")
    }});

    profiles.push_back({"White (OLED)", {
        ColorSwapper::hexToBGR("E6E6E6"),
        ColorSwapper::hexToBGR("323232"),
        ColorSwapper::hexToBGR("E6E6E6"),
        ColorSwapper::hexToBGR("323232")
    }});

    profiles.push_back({"Black (devkit)", {
        ColorSwapper::hexToBGR("313131"),
        ColorSwapper::hexToBGR("0F0F0F"),
        ColorSwapper::hexToBGR("313131"),
        ColorSwapper::hexToBGR("0F0F0F")
    }});

    profiles.push_back({"Neon Blue (L) & Neon Red (R)", {
        ColorSwapper::hexToBGR("0AB9E6"),
        ColorSwapper::hexToBGR("001E1E"),
        ColorSwapper::hexToBGR("FF3C28"),
        ColorSwapper::hexToBGR("1E0A0A")
    }});

    profiles.push_back({"Neon Red (L) & Neon Blue (R)", {
        ColorSwapper::hexToBGR("FF3C28"),
        ColorSwapper::hexToBGR("1E0A0A"),
        ColorSwapper::hexToBGR("0AB9E6"),
        ColorSwapper::hexToBGR("001E1E")
    }});

    profiles.push_back({"Neon Blue", {
        ColorSwapper::hexToBGR("0AB9E6"),
        ColorSwapper::hexToBGR("001E1E"),
        ColorSwapper::hexToBGR("0AB9E6"),
        ColorSwapper::hexToBGR("001E1E")
    }});

    profiles.push_back({"Neon Red", {
        ColorSwapper::hexToBGR("FF3C28"),
        ColorSwapper::hexToBGR("1E0A0A"),
        ColorSwapper::hexToBGR("FF3C28"),
        ColorSwapper::hexToBGR("1E0A0A")
    }});

    profiles.push_back({"Neon Yellow", {
        ColorSwapper::hexToBGR("E6FF00"),
        ColorSwapper::hexToBGR("142800"),
        ColorSwapper::hexToBGR("E6FF00"),
        ColorSwapper::hexToBGR("142800")
    }});

    profiles.push_back({"Neon Pink", {
        ColorSwapper::hexToBGR("FF3278"),
        ColorSwapper::hexToBGR("28001E"),
        ColorSwapper::hexToBGR("FF3278"),
        ColorSwapper::hexToBGR("28001E")
    }});

    profiles.push_back({"Neon Green", {
        ColorSwapper::hexToBGR("1EDC00"),
        ColorSwapper::hexToBGR("002800"),
        ColorSwapper::hexToBGR("1EDC00"),
        ColorSwapper::hexToBGR("002800")
    }});

    profiles.push_back({"Neon Purple", {
        ColorSwapper::hexToBGR("B400E6"),
        ColorSwapper::hexToBGR("140014"),
        ColorSwapper::hexToBGR("B400E6"),
        ColorSwapper::hexToBGR("140014")
    }});

    profiles.push_back({"Neon Orange", {
        ColorSwapper::hexToBGR("FAA005"),
        ColorSwapper::hexToBGR("0F0A00"),
        ColorSwapper::hexToBGR("FAA005"),
        ColorSwapper::hexToBGR("0F0A00")
    }});

    profiles.push_back({"Red", {
        ColorSwapper::hexToBGR("E10F00"),
        ColorSwapper::hexToBGR("280A0A"),
        ColorSwapper::hexToBGR("E10F00"),
        ColorSwapper::hexToBGR("280A0A")
    }});

    profiles.push_back({"Blue", {
        ColorSwapper::hexToBGR("4655F5"),
        ColorSwapper::hexToBGR("00000A"),
        ColorSwapper::hexToBGR("4655F5"),
        ColorSwapper::hexToBGR("00000A")
    }});

    profiles.push_back({"Splatoon 2 Neon Green & Neon Pink", {
        ColorSwapper::hexToBGR("1EDC00"),
        ColorSwapper::hexToBGR("002800"),
        ColorSwapper::hexToBGR("FF3278"),
        ColorSwapper::hexToBGR("28001E")
    }});

    profiles.push_back({"Fortnite Wildcat", {
        ColorSwapper::hexToBGR("FFCC00"),
        ColorSwapper::hexToBGR("1A1100"),
        ColorSwapper::hexToBGR("0084FF"),
        ColorSwapper::hexToBGR("000F1E")
    }});

    profiles.push_back({"Fortnite Fleet Force", {
        ColorSwapper::hexToBGR("0084FF"),
        ColorSwapper::hexToBGR("000F1E"),
        ColorSwapper::hexToBGR("FFCC00"),
        ColorSwapper::hexToBGR("1A1100")
    }});

    profiles.push_back({"Mario Red & Blue", {
        ColorSwapper::hexToBGR("F04614"),
        ColorSwapper::hexToBGR("1E1914"),
        ColorSwapper::hexToBGR("F04614"),
        ColorSwapper::hexToBGR("1E1914")
    }});

    profiles.push_back({"Pokemon Let's Go! Eevee and Pikachu", {
        ColorSwapper::hexToBGR("C88C32"),
        ColorSwapper::hexToBGR("281900"),
        ColorSwapper::hexToBGR("FFDC00"),
        ColorSwapper::hexToBGR("322800")
    }});

    profiles.push_back({"Nintendo Labo Creators Contest Edition", {
        ColorSwapper::hexToBGR("D7AA73"),
        ColorSwapper::hexToBGR("1E1914"),
        ColorSwapper::hexToBGR("D7AA73"),
        ColorSwapper::hexToBGR("1E1914")
    }});

    profiles.push_back({"Animal Crossing: New Horizons", {
        ColorSwapper::hexToBGR("82FF96"),
        ColorSwapper::hexToBGR("0A1E0A"),
        ColorSwapper::hexToBGR("96F5F5"),
        ColorSwapper::hexToBGR("0A1E28")
    }});

    profiles.push_back({"Dragon Quest XI S Lotto Edition Royal-Blue", {
        ColorSwapper::hexToBGR("1473FA"),
        ColorSwapper::hexToBGR("00000F"),
        ColorSwapper::hexToBGR("1473FA"),
        ColorSwapper::hexToBGR("00000F")
    }});

    profiles.push_back({"Disney Tsum Tsum Festival", {
        ColorSwapper::hexToBGR("B400E6"),
        ColorSwapper::hexToBGR("140014"),
        ColorSwapper::hexToBGR("FF3278"),
        ColorSwapper::hexToBGR("28001E")
    }});

    profiles.push_back({"Monster Hunter Rise Edition Gray", {
        ColorSwapper::hexToBGR("818282"),
        ColorSwapper::hexToBGR("0F0F0F"),
        ColorSwapper::hexToBGR("818282"),
        ColorSwapper::hexToBGR("0F0F0F")
    }});

    profiles.push_back({"The Legend of Zelda: Skyward Sword HD", {
        ColorSwapper::hexToBGR("2D50F0"),
        ColorSwapper::hexToBGR("1E0F46"),
        ColorSwapper::hexToBGR("500FC8"),
        ColorSwapper::hexToBGR("00051E")
    }});

    profiles.push_back({"Splatoon 3 OLED Edition", {
        ColorSwapper::hexToBGR("6455F5"),
        ColorSwapper::hexToBGR("28282D"),
        ColorSwapper::hexToBGR("C3FA05"),
        ColorSwapper::hexToBGR("1E1E28")
    }});

    profiles.push_back({"Pokémon: Scarlet × Violet", {
        ColorSwapper::hexToBGR("F04614"),
        ColorSwapper::hexToBGR("1E1914"),
        ColorSwapper::hexToBGR("B400E6"),
        ColorSwapper::hexToBGR("140014")
    }});

    profiles.push_back({"Legend of Zelda: Tears of the Kingdom Edition Gold Joy-Con", {
        ColorSwapper::hexToBGR("D2BE69"),
        ColorSwapper::hexToBGR("32322D"),
        ColorSwapper::hexToBGR("D2BE69"),
        ColorSwapper::hexToBGR("32322D")
    }});

    profiles.push_back({"Pastel Pink", {
        ColorSwapper::hexToBGR("FFAFAF"),
        ColorSwapper::hexToBGR("372D2D"),
        ColorSwapper::hexToBGR("FFAFAF"),
        ColorSwapper::hexToBGR("372D2D")
    }});

    brls::Logger::info("Loaded {} Joy-Con color profiles", profiles.size());
    return profiles;
}

} // namespace JC

namespace PC {

int setColor(const std::vector<int>& colors) {
    Result res = 0;

    // Initialize hidsys and hiddbg
    res = hidsysInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hidsys: 0x{:X}", res);
        return res;
    }

    res = hiddbgInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hiddbg: 0x{:X}", res);
        hidsysExit();
        return res;
    }

    // Get unique pad ID for Pro Controller (Player 1)
    HidsysUniquePadId UniquePadId = {0};
    s32 nbEntries = 0;
    res = hidsysGetUniquePadsFromNpad(HidNpadIdType_No1, &UniquePadId, 1, &nbEntries);

    if (R_FAILED(res) || nbEntries < 1) {
        brls::Logger::error("Failed to get Pro Controller unique pad ID");
        hiddbgExit();
        hidsysExit();
        return -1;
    }

    // Update colors: colors[0] = body, colors[1] = buttons
    Result pc = hiddbgUpdateControllerColor(colors[0], colors[1], UniquePadId);

    hiddbgExit();
    hidsysExit();

    if (R_FAILED(pc)) {
        brls::Logger::error("Failed to update Pro Controller colors: 0x{:X}", pc);
        return -1;
    }

    return 0;
}

void changePCColor(const std::vector<int>& values) {
    int res = setColor(values);

    if (res == 0) {
        auto* dialog = new brls::Dialog("Pro Controller colors updated successfully!");
        dialog->addButton("OK", []() {});
        dialog->open();
    } else {
        auto* dialog = new brls::Dialog("Failed to update Pro Controller colors. Make sure Pro Controller is connected to Player 1.");
        dialog->addButton("OK", []() {});
        dialog->open();
    }
}

bool backupPCColor(const std::string& path) {
    Result res = hidInitialize();
    if (R_FAILED(res)) {
        brls::Logger::error("Failed to initialize hid: 0x{:X}", res);
        return false;
    }

    HidNpadControllerColor color;
    res = hidGetNpadControllerColorSingle(HidNpadIdType_No1, &color);
    hidExit();

    if (R_FAILED(res)) {
        brls::Logger::error("Failed to read Pro Controller colors: 0x{:X}", res);
        return false;
    }

    // Convert colors to hex
    std::string body = ColorSwapper::BGRToHex(color.main);
    std::string buttons = ColorSwapper::BGRToHex(color.sub);

    // Create backup JSON
    std::string timestamp = std::to_string(std::time(nullptr));
    std::string backupName = "Backup " + timestamp;

    // Load existing profiles
    json existingProfiles;
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                file >> existingProfiles;
            } catch (...) {
                existingProfiles = json::array();
            }
            file.close();
        }
    }

    // If the existing file is not an array, convert it
    if (!existingProfiles.is_array()) {
        existingProfiles = json::array();
    }

    // Create backup entry in array format
    json backupEntry = {
        {"name", backupName},
        {"BODY", body},
        {"BTN", buttons}
    };

    // Add backup to the beginning of the array
    existingProfiles.insert(existingProfiles.begin(), backupEntry);

    // Write back to file
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream outFile(path);
    if (outFile.is_open()) {
        outFile << existingProfiles.dump(4);
        outFile.close();
        brls::Logger::info("Pro Controller colors backed up successfully");
        return true;
    } else {
        brls::Logger::error("Failed to save backup");
        return false;
    }
}

std::vector<std::pair<std::string, std::vector<int>>> getProfiles(const std::string& path) {
    std::vector<std::pair<std::string, std::vector<int>>> profiles;

    // Load backups from local file first
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                json localProfiles;
                file >> localProfiles;

                if (localProfiles.is_array()) {
                    for (const auto& profile : localProfiles) {
                        if (profile.contains("name")) {
                            std::string name = profile["name"].get<std::string>();
                            // Only load backups from file
                            if (name.find("Backup") != std::string::npos) {
                                if (profile.contains("BODY") && profile.contains("BTN")) {

                                    std::string body = profile["BODY"].get<std::string>();
                                    std::string buttons = profile["BTN"].get<std::string>();

                                    std::vector<int> colorValues = {
                                        ColorSwapper::hexToBGR(body),
                                        ColorSwapper::hexToBGR(buttons)
                                    };

                                    profiles.push_back({name, colorValues});
                                }
                            }
                        }
                    }
                }
                brls::Logger::info("Loaded {} backups from local file", profiles.size());
            } catch (...) {
                brls::Logger::error("Failed to parse local backup file");
            }
            file.close();
        }
    }

    // Add all hard-coded profiles
    profiles.push_back({"Default Black", {
        ColorSwapper::hexToBGR("2D2D2D"),
        ColorSwapper::hexToBGR("E6E6E6")
    }});

    profiles.push_back({"ACNH Green", {
        ColorSwapper::hexToBGR("82FF96"),
        ColorSwapper::hexToBGR("0A1E0A")
    }});

    profiles.push_back({"ACNH Blue", {
        ColorSwapper::hexToBGR("96F5F5"),
        ColorSwapper::hexToBGR("0A1E28")
    }});

    brls::Logger::info("Loaded {} Pro Controller color profiles", profiles.size());
    return profiles;
}

} // namespace PC
