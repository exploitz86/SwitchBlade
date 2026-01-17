#include "utils/fs.hpp"
#include <switch.h>
#include <filesystem>
#include <fstream>

namespace fs {
    bool removeDir(const std::string& path) {
        Result ret = 0;
        FsFileSystem* fs = fsdevGetDeviceFileSystem("sdmc");
        if (R_FAILED(ret = fsFsDeleteDirectoryRecursively(fs, path.c_str()))) {
            return false;
        }
        return true;
    }
    
    bool copyFile(const std::string& src, const std::string& dest) {
        try {
            // Create parent directories if they don't exist
            auto destPath = std::filesystem::path(dest);
            if (destPath.has_parent_path()) {
                std::filesystem::create_directories(destPath.parent_path());
            }
            
            std::ifstream sourceFile(src, std::ios::binary);
            if (!sourceFile.is_open()) {
                return false;
            }
            
            std::ofstream destFile(dest, std::ios::binary | std::ios::trunc);
            if (!destFile.is_open()) {
                sourceFile.close();
                return false;
            }
            
            destFile << sourceFile.rdbuf();
            destFile.close();
            sourceFile.close();
            return true;
        } catch (...) {
            return false;
        }
    }
}
