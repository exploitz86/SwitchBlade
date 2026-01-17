#include "views/cheats_settings_tab.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <switch.h>

using namespace brls::literals;

static constexpr const char* CONFIG_PATH = "/atmosphere/config/system_settings.ini";
static constexpr const char* REBOOT_PAYLOAD_PATH = "/atmosphere/reboot_payload.bin";

// Global BPC service for proper lifecycle management
static Service g_amsBpcSrv;
static bool g_amsBpcInitialized = false;

// Helper function to initialize Atmosphère BPC service
static Result amsBpcInitialize() {
    if (g_amsBpcInitialized) {
        return 0;
    }

    Handle h;
    Result rc = svcConnectToNamedPort(&h, "bpc:ams");
    if (R_SUCCEEDED(rc)) {
        serviceCreate(&g_amsBpcSrv, h);
        g_amsBpcInitialized = true;
    }
    return rc;
}

// Helper function to cleanup Atmosphère BPC service
static void amsBpcExit() {
    if (g_amsBpcInitialized) {
        serviceClose(&g_amsBpcSrv);
        g_amsBpcInitialized = false;
    }
}

// Helper function to set reboot payload via Atmosphère BPC
static Result amsBpcSetRebootPayload(const void* src, size_t src_size) {
    if (!g_amsBpcInitialized) {
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    }

    return serviceDispatch(&g_amsBpcSrv, 65001,
        .buffer_attrs = {SfBufferAttr_In | SfBufferAttr_HipcMapAlias},
        .buffers = {{src, src_size}},
    );
}

// Helper function to reboot to payload
static void rebootToPayload() {
    brls::Logger::info("Preparing to reboot to RCM payload");

    // Load payload from file
    alignas(0x1000) static u8 g_reboot_payload[0x24000];
    std::string payloadPath = REBOOT_PAYLOAD_PATH;

    FILE* f = fopen(payloadPath.c_str(), "rb");
    if (!f) {
        brls::Logger::error("Failed to open payload file: {}", payloadPath);
        brls::Application::notify("menu/ams_update/rcm_not_found"_i18n);
        return;
    }

    size_t payloadSize = fread(g_reboot_payload, 1, sizeof(g_reboot_payload), f);
    fclose(f);

    if (payloadSize == 0) {
        brls::Logger::error("Failed to read payload file");
        brls::Application::notify("menu/ams_update/rcm_cant_read"_i18n);
        return;
    }

    brls::Logger::info("Loaded payload: {} bytes", payloadSize);

    // Initialize SPSM first (required for both paths)
    Result rc = spsmInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("Failed to initialize SPSM: 0x{:X}", rc);
        brls::Application::notify("menu/ams_update/power_service_init_error"_i18n);
        return;
    }

    // Exit SM before attempting BPC operations
    smExit();

    // Try modern Atmosphère path first
    rc = amsBpcInitialize();
    if (R_SUCCEEDED(rc)) {
        brls::Logger::info("Using Atmosphère BPC service for reboot");

        rc = amsBpcSetRebootPayload(g_reboot_payload, sizeof(g_reboot_payload));
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("Payload set successfully, rebooting...");
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
        } else {
            brls::Logger::error("Failed to set reboot payload: 0x{:X}", rc);
            amsBpcExit(); // Clean up on failure
            brls::Application::notify("Error: Failed to set reboot payload");
        }
    } else {
        // Fallback to legacy SPL path
        brls::Logger::info("Atmosphère BPC not available, using legacy SPL path");

        rc = splInitialize();
        if (R_FAILED(rc)) {
            brls::Logger::error("Failed to initialize SPL: 0x{:X}", rc);
            brls::Application::notify("menu/ams_update/spl_init_error"_i18n);
            return;
        }

        rc = splSetConfig((SplConfigItem)65001, 2);
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("SPL configured, rebooting...");
            splExit();
            spsmShutdown(true); // true = reboot
            // If we reach here, reboot failed
            brls::Logger::error("spsmShutdown returned unexpectedly");
        } else {
            brls::Logger::error("Failed to set SPL config: 0x{:X}", rc);
            splExit();
            brls::Application::notify("menu/ams_update/reboot_config_error"_i18n);
        }
    }
}

CheatsSettingsTab::CheatsSettingsTab() {
    this->inflateFromXMLRes("xml/tabs/cheats_settings_tab.xml");

    // Ensure config file exists and is properly formatted when tab is opened
    ensureConfigExists();

    // Status header
    auto* statusLabel = new brls::Label();
    statusLabel->setText("menu/cheats_menu/current_status"_i18n + std::string(": ") + "menu/cheats_menu/config_ready"_i18n);
    statusLabel->setFontSize(18);
    statusLabel->setTextColor(nvgRGB(150, 150, 150));
    statusLabel->setMarginBottom(20);
    statusLabel->setFocusable(false);
    contentBox->addView(statusLabel);

    // Auto-enable cheats toggle - read fresh values after ensuring config exists
    autoEnableItem = new brls::BooleanCell();
    bool autoEnabled = getCurrentSetting("dmnt_cheats_enabled_by_default");
    autoEnableItem->init("menu/cheats_menu/auto_enable_cheats"_i18n, autoEnabled, [this](bool value) {
        updateConfigSetting("dmnt_cheats_enabled_by_default", value);
    });
    contentBox->addView(autoEnableItem);

    // Remember cheat state toggle - read fresh values after ensuring config exists
    rememberStateItem = new brls::BooleanCell();
    bool rememberEnabled = getCurrentSetting("dmnt_always_save_cheat_toggles");
    rememberStateItem->init("menu/cheats_menu/remember_state"_i18n, rememberEnabled, [this](bool value) {
        updateConfigSetting("dmnt_always_save_cheat_toggles", value);
    });
    contentBox->addView(rememberStateItem);

    // Add small spacing
    brls::Label* spacer = new brls::Label();
    spacer->setText("");
    spacer->setHeight(20);
    spacer->setFocusable(false);
    contentBox->addView(spacer);

    // Add restart to payload button
    auto* restartButton = new brls::RadioCell();
    restartButton->title->setText("menu/cheats_menu/restart_to_payload"_i18n);
    restartButton->setSelected(false);
    restartButton->registerClickAction([](brls::View* view) {
        // Show confirmation dialog before rebooting
        brls::Dialog* dialog = new brls::Dialog("menu/cheats_menu/restart_confirm"_i18n);
        dialog->addButton("hints/ok"_i18n, []() {
            rebootToPayload();
        });
        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->open();
        return true;
    });
    contentBox->addView(restartButton);

    // Add info section
    brls::Label* infoLabel = new brls::Label();
    infoLabel->setText("menu/cheats_menu/info"_i18n + std::string(": ") + "menu/cheats_menu/info_desc"_i18n);
    infoLabel->setFontSize(16);
    infoLabel->setTextColor(nvgRGB(150, 150, 150));
    infoLabel->setMarginTop(20);
    infoLabel->setFocusable(false);
    contentBox->addView(infoLabel);
}

void CheatsSettingsTab::refreshUI() {
    // BooleanCell manages its own state through the init() method and callbacks
    // No manual refresh needed since we read fresh values after ensureConfigExists()
}

void CheatsSettingsTab::ensureConfigExists() {
    // Ensure directory exists
    std::filesystem::create_directories("/atmosphere/config");

    std::vector<std::string> lines;
    bool needsWrite = false;

    // Try to load existing config or template
    if (std::filesystem::exists(CONFIG_PATH)) {
        std::ifstream configFile(CONFIG_PATH);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                lines.push_back(line);
            }
            configFile.close();
        }
    } else {
        // Try to load from template first
        const char* TEMPLATE_PATH = "/atmosphere/config_templates/system_settings.ini";
        
        if (std::filesystem::exists(TEMPLATE_PATH)) {
            try {
                std::ifstream templateFile(TEMPLATE_PATH);
                if (templateFile.is_open()) {
                    std::string line;
                    while (std::getline(templateFile, line)) {
                        lines.push_back(line);
                    }
                    templateFile.close();
                }
            } catch (...) {
                // If reading template fails, use default
                lines.clear();
            }
        }
        
        // If template wasn't used or was empty, create default structure
        if (lines.empty()) {
            lines.push_back("; Atmosphere Configuration");
            lines.push_back("; This file was created by SwitchBlade");
            lines.push_back("");
            lines.push_back("[atmosphere]");
        }
        
        needsWrite = true;
    }

    // Check if both settings exist in the config (commented or uncommented)
    bool hasAutoEnable = false;
    bool hasRememberState = false;
    int atmosphereSectionIndex = -1;

    for (size_t i = 0; i < lines.size(); i++) {
        const auto& line = lines[i];
        
        if (line.find("[atmosphere]") != std::string::npos) {
            atmosphereSectionIndex = i;
        }
        
        // Check for settings (both commented and uncommented)
        if (line.find("dmnt_cheats_enabled_by_default") != std::string::npos) {
            hasAutoEnable = true;
        }
        if (line.find("dmnt_always_save_cheat_toggles") != std::string::npos) {
            hasRememberState = true;
        }
    }

    // Add missing settings
    if (!hasAutoEnable || !hasRememberState) {
        if (atmosphereSectionIndex == -1) {
            // No [atmosphere] section, add it at the end
            lines.push_back("");
            lines.push_back("[atmosphere]");
            atmosphereSectionIndex = lines.size() - 1;
        }
        
        if (!hasAutoEnable) {
            lines.insert(lines.begin() + atmosphereSectionIndex + 1, 
                        "dmnt_cheats_enabled_by_default = u8!0x1");
        }
        
        if (!hasRememberState) {
            // Insert after atmosphere section (or after auto-enable if we just added it)
            int insertIndex = atmosphereSectionIndex + 1;
            if (!hasAutoEnable) insertIndex++;
            lines.insert(lines.begin() + insertIndex, 
                        "dmnt_always_save_cheat_toggles = u8!0x0");
        }
        
        needsWrite = true;
    }

    // Write if needed
    if (needsWrite) {
        try {
            std::ofstream configFile(CONFIG_PATH);
            if (configFile.is_open()) {
                for (const auto& line : lines) {
                    configFile << line << std::endl;
                }
                configFile.close();
            }
        } catch (...) {
            // Silently fail, will use defaults on read
        }
    }
}

bool CheatsSettingsTab::getCurrentSetting(const std::string& settingName) {
    if (!std::filesystem::exists(CONFIG_PATH)) {
        // Return Atmosphere's actual defaults when config doesn't exist
        if (settingName == "dmnt_cheats_enabled_by_default") return true;  // 0x1
        if (settingName == "dmnt_always_save_cheat_toggles") return false; // 0x0
        return false;
    }

    std::ifstream configFile(CONFIG_PATH);
    if (!configFile.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(configFile, line)) {
        // Skip commented lines
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos && line[firstNonSpace] == ';') {
            continue;
        }
        
        if (line.find(settingName) != std::string::npos) {
            if (line.find("0x1") != std::string::npos) {
                return true;
            } else if (line.find("0x0") != std::string::npos) {
                return false;
            }
        }
    }

    // Return Atmosphere's actual defaults when setting is commented out or missing
    if (settingName == "dmnt_cheats_enabled_by_default") return true;  // 0x1
    if (settingName == "dmnt_always_save_cheat_toggles") return false; // 0x0
    return false;
}

bool CheatsSettingsTab::updateConfigSetting(const std::string& settingName, bool value) {
    std::vector<std::string> lines;
    
    // Ensure directory exists
    std::filesystem::create_directories("/atmosphere/config");

    // Try to load existing config or template
    if (std::filesystem::exists(CONFIG_PATH)) {
        std::ifstream configFile(CONFIG_PATH);
        if (configFile.is_open()) {
            std::string line;
            while (std::getline(configFile, line)) {
                lines.push_back(line);
            }
            configFile.close();
        }
    } else {
        // Try to copy from template first
        const char* TEMPLATE_PATH = "/atmosphere/config_templates/system_settings.ini";
        
        if (std::filesystem::exists(TEMPLATE_PATH)) {
            try {
                std::ifstream templateFile(TEMPLATE_PATH);
                if (templateFile.is_open()) {
                    std::string line;
                    while (std::getline(templateFile, line)) {
                        lines.push_back(line);
                    }
                    templateFile.close();
                }
            } catch (...) {
                // If reading template fails, create default structure
                lines.clear();
            }
        }
        
        // If template wasn't used or was empty, create default structure
        if (lines.empty()) {
            lines.push_back("; Atmosphere Configuration");
            lines.push_back("; This file was created by SwitchBlade");
            lines.push_back("");
            lines.push_back("[atmosphere]");
        }
    }

    bool found = false;
    int atmosphereSectionIndex = -1;

    // First pass: look for the setting (commented or uncommented) and find [atmosphere] section
    for (size_t i = 0; i < lines.size(); i++) {
        auto& line = lines[i];
        
        // Track [atmosphere] section
        if (line.find("[atmosphere]") != std::string::npos) {
            atmosphereSectionIndex = i;
        }
        
        // Check if this line contains the setting name (could be commented)
        if (line.find(settingName) != std::string::npos) {
            // Uncomment and update the value
            line = settingName + " = u8!" + (value ? "0x1" : "0x0");
            found = true;
            break;
        }
    }

    // If not found, add it under [atmosphere] section
    if (!found) {
        if (atmosphereSectionIndex != -1) {
            // Insert after [atmosphere] section header
            lines.insert(lines.begin() + atmosphereSectionIndex + 1, 
                        settingName + " = u8!" + (value ? "0x1" : "0x0"));
        } else {
            // No [atmosphere] section found, add it at the end
            lines.push_back("");
            lines.push_back("[atmosphere]");
            lines.push_back(settingName + " = u8!" + (value ? "0x1" : "0x0"));
        }
    }

    try {
        std::ofstream configFile(CONFIG_PATH);
        if (!configFile.is_open()) {
            return false;
        }

        for (const auto& line : lines) {
            configFile << line << std::endl;
        }

        configFile.close();
        return true;
    } catch (...) {
        return false;
    }
}

brls::View* CheatsSettingsTab::create() {
    return new CheatsSettingsTab();
}
