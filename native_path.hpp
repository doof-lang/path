#pragma once

#include "doof_runtime.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#endif

namespace doof_path {

namespace detail {

inline std::string dirname(const std::string& path) {
    const size_t separator = path.find_last_of('/');
    if (separator == std::string::npos) {
        return ".";
    }
    if (separator == 0) {
        return "/";
    }
    return path.substr(0, separator);
}

inline doof::Result<std::string, std::string> executablePath() {
#if defined(__APPLE__)
    uint32_t size = PATH_MAX;
    std::vector<char> path(size);
    if (::_NSGetExecutablePath(path.data(), &size) != 0) {
        path.resize(size);
        if (::_NSGetExecutablePath(path.data(), &size) != 0) {
            return doof::Result<std::string, std::string>::failure("Failed to determine executable path");
        }
    }

    std::vector<char> resolved(PATH_MAX);
    if (::realpath(path.data(), resolved.data()) != nullptr) {
        return doof::Result<std::string, std::string>::success(std::string(resolved.data()));
    }

    return doof::Result<std::string, std::string>::success(std::string(path.data()));
#elif defined(__linux__) || defined(__GNU__)
    size_t size = 256;
    while (true) {
        std::vector<char> path(size);
        ssize_t length = ::readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (length < 0) {
            return doof::Result<std::string, std::string>::failure(
                std::string("Failed to determine executable path: ") + std::strerror(errno)
            );
        }
        if (static_cast<size_t>(length) < path.size() - 1) {
            path[static_cast<size_t>(length)] = '\0';
            return doof::Result<std::string, std::string>::success(std::string(path.data()));
        }
        size *= 2;
    }
#else
    return doof::Result<std::string, std::string>::failure("Executable path is not supported on this platform");
#endif
}

#if defined(__APPLE__)
inline doof::Result<std::string, std::string> bundleResourcesDirectory() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return doof::Result<std::string, std::string>::failure("Failed to get main bundle");
    }

    CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
    if (url == nullptr) {
        return doof::Result<std::string, std::string>::failure("Failed to get bundle resources directory");
    }

    std::vector<char> path(PATH_MAX);
    const Boolean ok = CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path.data()), path.size());
    CFRelease(url);

    if (!ok) {
        return doof::Result<std::string, std::string>::failure("Failed to convert bundle resources directory to a path");
    }

    return doof::Result<std::string, std::string>::success(std::string(path.data()));
}
#endif

}  // namespace detail

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

inline doof::Result<std::string, std::string> resourcesDirectory() {
#if defined(__APPLE__)
    auto bundled = detail::bundleResourcesDirectory();
    if (bundled.isSuccess()) {
        return bundled;
    }
#endif

    auto executable = detail::executablePath();
    if (!executable.isSuccess()) {
        return doof::Result<std::string, std::string>::failure(executable.error());
    }

    return doof::Result<std::string, std::string>::success(detail::dirname(executable.value()));
}

}  // namespace doof_path
