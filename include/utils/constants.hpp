#pragma once

constexpr const char ROOT_PATH[] = "/";
constexpr const char NRO_PATH[] = "/switch/SwitchBlade/SwitchBlade.nro";
constexpr const char CONFIG_PATH[] = "/config/SwitchBlade/";

constexpr const char RCM_PAYLOAD_PATH[] = "romfs:/switchblade_rcm.bin";

constexpr const char APP_URL[] = "https://github.com/exploitz86/SwitchBlade/releases/latest/download/SwitchBlade.zip";
constexpr const char TAGS_INFO[] = "https://api.github.com/repos/exploitz86/SwitchBlade/releases/latest";
constexpr const char APP_FILENAME[] = "/config/SwitchBlade/app.zip";

constexpr const char NXLINKS_URL[] = "https://raw.githubusercontent.com/exploitz86/nx-links/master/nx-links.json";

constexpr const char CUSTOM_FILENAME[] = "/config/SwitchBlade/custom.zip";
constexpr const char HEKATE_IPL_PATH[] = "/bootloader/hekate_ipl.ini";

constexpr const char BOOTLOADER_FILENAME[] = "/config/SwitchBlade/bootloader.zip";
constexpr const char AMS_FILENAME[] = "/config/SwitchBlade/ams.zip";

constexpr const char CUSTOM_PACKS_PATH[] = "/config/SwitchBlade/custom_packs.json";

constexpr const char CHEATS_URL_TITLES[] = "https://github.com/exploitz86/switch-cheats-db/releases/latest/download/titles.zip";
constexpr const char CHEATS_URL_CONTENTS[] = "https://github.com/exploitz86/switch-cheats-db/releases/latest/download/contents.zip";
constexpr const char GFX_CHEATS_URL_TITLES[] = "https://github.com/exploitz86/switch-cheats-db/releases/latest/download/titles_60fps-res-gfx.zip";
constexpr const char GFX_CHEATS_URL_CONTENTS[] = "https://github.com/exploitz86/switch-cheats-db/releases/latest/download/contents_60fps-res-gfx.zip";
constexpr const char CHEATS_URL_VERSION[] = "https://github.com/exploitz86/switch-cheats-db/releases/latest/download/VERSION";
constexpr const char LOOKUP_TABLE_URL[] = "https://raw.githubusercontent.com/exploitz86/switch-cheats-db/master/versions.json";
constexpr const char VERSIONS_DIRECTORY[] = "https://raw.githubusercontent.com/exploitz86/switch-cheats-db/master/versions/";
constexpr const char CHEATS_DIRECTORY_GBATEMP[] = "https://raw.githubusercontent.com/exploitz86/switch-cheats-db/master/cheats_gbatemp/";
constexpr const char CHEATS_DIRECTORY_GFX[] = "https://raw.githubusercontent.com/exploitz86/switch-cheats-db/master/cheats_gfx/";
constexpr const char CHEATSLIPS_CHEATS_URL[] = "https://www.cheatslips.com/api/v1/cheats/";
constexpr const char CHEATSLIPS_TOKEN_URL[] = "https://www.cheatslips.com/api/v1/token";
constexpr const char TOKEN_PATH[] = "/config/SwitchBlade/token.json";
constexpr const char CHEATS_FILENAME[] = "/config/SwitchBlade/cheats.zip";
constexpr const char CHEATS_EXCLUDE[] = "/config/SwitchBlade/exclude.txt";
constexpr const char FILES_IGNORE[] = "/config/SwitchBlade/preserve.txt";
constexpr const char CHEATS_VERSION[] = "/config/SwitchBlade/cheats_version.dat";
constexpr const char AMS_CONTENTS[] = "/atmosphere/contents/";
constexpr const char REINX_CONTENTS[] = "/ReiNX/contents/";
constexpr const char SXOS_TITLES[] = "/sxos/titles/";
constexpr const char AMS_PATH[] = "/atmosphere/";
constexpr const char SXOS_PATH[] = "/sxos/";
constexpr const char REINX_PATH[] = "/ReiNX/";
constexpr const char CONTENTS_PATH[] = "contents/";
constexpr const char TITLES_PATH[] = "titles/";

constexpr const char COLOR_PICKER_URL[] = "https://exploitz86.github.io";
constexpr const char JC_COLOR_URL[] = "https://raw.githubusercontent.com/exploitz86/SwitchBlade/master/jc_profiles.json";
constexpr const char JC_COLOR_PATH[] = "/config/SwitchBlade/jc_profiles.json";
constexpr const char PC_COLOR_PATH[] = "/config/SwitchBlade/pc_profiles.json";

constexpr const char PAYLOAD_PATH[] = "/payloads/";
constexpr const char BOOTLOADER_PATH[] = "/bootloader/";
constexpr const char BOOTLOADER_PL_PATH[] = "/bootloader/payloads/";
constexpr const char UPDATE_BIN_PATH[] = "/bootloader/update.bin";
constexpr const char REBOOT_PAYLOAD_PATH[] = "/atmosphere/reboot_payload.bin";

constexpr const char COPY_FILES_TXT[] = "/config/SwitchBlade/copy_files.txt";

constexpr const char ROMFS_FORWARDER[] = "romfs:/forwarder.nro";
constexpr const char FORWARDER_PATH[] = "/config/SwitchBlade/forwarder.nro";

constexpr const char DAYBREAK_PATH[] = "/switch/daybreak.nro";

enum class CFW
{
    rnx,
    sxos,
    ams,
};
