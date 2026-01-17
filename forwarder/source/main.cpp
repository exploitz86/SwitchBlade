#include <filesystem>
#include <string>

#include <switch.h>

#define PATH        "/switch/SwitchBlade/"
#define FULL_PATH   "/switch/SwitchBlade/SwitchBlade.nro"
#define CONFIG_PATH "/config/SwitchBlade/switch/SwitchBlade/SwitchBlade.nro"
#define PREFIX      "/switch/SwitchBlade/SwitchBlade-v"
#define FORWARDER_PATH      "/config/SwitchBlade/forwarder.nro"
#define CONFIG_SWITCH       "/config/SwitchBlade/switch/"
#define HIDDEN_FILE "/config/SwitchBlade/.switchblade"

int removeDir(const char* path)
{
    Result ret = 0;
    FsFileSystem *fs = fsdevGetDeviceFileSystem("sdmc");
    if (R_FAILED(ret = fsFsDeleteDirectoryRecursively(fs, path))) {
        return ret;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    std::filesystem::create_directory(PATH);
    for (const auto & entry : std::filesystem::directory_iterator(PATH)){
        if(entry.path().string().find(PREFIX) != std::string::npos) {
            std::filesystem::remove(entry.path().string());
            std::filesystem::remove(entry.path().string() + ".star");
        }
    }
    std::filesystem::remove(HIDDEN_FILE);

    if(std::filesystem::exists(CONFIG_PATH)){
        std::filesystem::create_directory(PATH);
        std::filesystem::remove(FULL_PATH);
        std::filesystem::rename(CONFIG_PATH, FULL_PATH);
        removeDir(CONFIG_SWITCH);
    }

    std::filesystem::remove(FORWARDER_PATH);

    envSetNextLoad(FULL_PATH, FULL_PATH);
    return 0;
}
