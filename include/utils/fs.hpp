#pragma once

#include <string>

namespace fs {
    bool removeDir(const std::string& path);
    bool copyFile(const std::string& src, const std::string& dest);
}
