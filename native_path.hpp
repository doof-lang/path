#pragma once

#include "doof_runtime.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace doof_path {

inline doof::Result<std::string, std::string> homeDirectory() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return doof::Result<std::string, std::string>::success(std::string(home));
    }

    const passwd* entry = ::getpwuid(::getuid());
    if (entry != nullptr && entry->pw_dir != nullptr && entry->pw_dir[0] != '\0') {
        return doof::Result<std::string, std::string>::success(std::string(entry->pw_dir));
    }

    return doof::Result<std::string, std::string>::failure("Failed to determine the home directory");
}

inline std::string tempDirectory() {
    const char* temp = std::getenv("TMPDIR");
    if (temp != nullptr && temp[0] != '\0') {
        return std::string(temp);
    }

    return "/tmp";
}

inline doof::Result<std::string, std::string> currentWorkingDirectory() {
    size_t size = 256;
    while (true) {
        std::vector<char> buffer(size);
        errno = 0;
        if (::getcwd(buffer.data(), buffer.size()) != nullptr) {
            return doof::Result<std::string, std::string>::success(std::string(buffer.data()));
        }

        if (errno != ERANGE) {
            return doof::Result<std::string, std::string>::failure(
                std::string("Failed to get current working directory: ") + std::strerror(errno)
            );
        }

        size *= 2;
    }
}

inline doof::Result<void, std::string> setCurrentWorkingDirectory(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        return doof::Result<void, std::string>::failure("Working directory path contains a NUL byte");
    }

    if (::chdir(path.c_str()) == 0) {
        return doof::Result<void, std::string>::success();
    }

    return doof::Result<void, std::string>::failure(
        std::string("Failed to set current working directory: ") + std::strerror(errno)
    );
}

}  // namespace doof_path