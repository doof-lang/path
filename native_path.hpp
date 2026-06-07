#pragma once

#include "doof_runtime.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
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
inline doof::Result<std::string, std::string> cfStringToString(CFStringRef value, const char* description) {
    if (value == nullptr) {
        return doof::Result<std::string, std::string>::failure(std::string("Failed to get ") + description);
    }

    const CFIndex length = CFStringGetLength(value);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(static_cast<size_t>(maxSize));
    if (!CFStringGetCString(value, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
        return doof::Result<std::string, std::string>::failure(std::string("Failed to convert ") + description + " to UTF-8");
    }

    return doof::Result<std::string, std::string>::success(std::string(buffer.data()));
}

inline doof::Result<std::string, std::string> bundleIdentifier() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return doof::Result<std::string, std::string>::failure("Failed to get main bundle");
    }

    CFStringRef identifier = CFBundleGetIdentifier(bundle);
    if (identifier == nullptr) {
        return doof::Result<std::string, std::string>::failure("Application identifier is required for console applications");
    }

    return cfStringToString(identifier, "bundle identifier");
}

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

inline doof::Result<std::string, std::string> resolveApplicationIdentifier(const std::optional<std::string>& appId) {
    if (appId.has_value()) {
        if (appId.value().empty()) {
            return doof::Result<std::string, std::string>::failure("Application identifier cannot be empty");
        }
        if (appId.value().find('/') != std::string::npos || appId.value().find('\0') != std::string::npos) {
            return doof::Result<std::string, std::string>::failure("Application identifier cannot contain a path separator or NUL byte");
        }
    }

#if defined(__APPLE__)
    auto bundledIdentifier = bundleIdentifier();
    if (bundledIdentifier.isSuccess()) {
        if (appId.has_value() && appId.value() != bundledIdentifier.value()) {
            return doof::Result<std::string, std::string>::failure("Application identifier must match the bundle identifier");
        }
        return bundledIdentifier;
    }
#endif

    if (!appId.has_value()) {
        return doof::Result<std::string, std::string>::failure("Application identifier is required for console applications");
    }

    return doof::Result<std::string, std::string>::success(appId.value());
}

inline doof::Result<void, std::string> ensureSingleDirectory(const std::string& path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return doof::Result<void, std::string>::success();
        }
        return doof::Result<void, std::string>::failure(path + " exists but is not a directory");
    }

    if (errno != ENOENT) {
        return doof::Result<void, std::string>::failure(
            std::string("Failed to inspect directory ") + path + ": " + std::strerror(errno)
        );
    }

    if (::mkdir(path.c_str(), 0777) == 0) {
        return doof::Result<void, std::string>::success();
    }

    if (errno == EEXIST) {
        return ensureSingleDirectory(path);
    }

    return doof::Result<void, std::string>::failure(
        std::string("Failed to create directory ") + path + ": " + std::strerror(errno)
    );
}

inline doof::Result<void, std::string> ensureDirectory(const std::string& path) {
    if (path.empty()) {
        return doof::Result<void, std::string>::failure("Directory path cannot be empty");
    }
    if (path.find('\0') != std::string::npos) {
        return doof::Result<void, std::string>::failure("Directory path contains a NUL byte");
    }

    size_t searchFrom = path[0] == '/' ? 1 : 0;
    while (true) {
        const size_t separator = path.find('/', searchFrom);
        const std::string current = separator == std::string::npos ? path : path.substr(0, separator);
        if (!current.empty() && current != ".") {
            auto result = ensureSingleDirectory(current);
            if (!result.isSuccess()) {
                return result;
            }
        }
        if (separator == std::string::npos) {
            break;
        }
        searchFrom = separator + 1;
    }

    return doof::Result<void, std::string>::success();
}

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

inline doof::Result<std::string, std::string> applicationDirectory(
    const std::optional<std::string>& appId,
    const char* appleRelativeBase,
    const char* xdgEnvironmentName,
    const char* unixRelativeBase
) {
    auto identifier = resolveApplicationIdentifier(appId);
    if (!identifier.isSuccess()) {
        return doof::Result<std::string, std::string>::failure(identifier.error());
    }

#if defined(__APPLE__)
    auto home = homeDirectory();
    if (!home.isSuccess()) {
        return doof::Result<std::string, std::string>::failure(home.error());
    }

    const std::string path = home.value() + "/Library/" + appleRelativeBase + "/" + identifier.value();
#else
    std::string path;
    const char* xdgBase = std::getenv(xdgEnvironmentName);
    if (xdgBase != nullptr && xdgBase[0] != '\0') {
        path = std::string(xdgBase) + "/" + identifier.value();
    } else {
        auto home = homeDirectory();
        if (!home.isSuccess()) {
            return doof::Result<std::string, std::string>::failure(home.error());
        }

        path = home.value() + "/" + unixRelativeBase + "/" + identifier.value();
    }
#endif

    auto directory = ensureDirectory(path);
    if (!directory.isSuccess()) {
        return doof::Result<std::string, std::string>::failure(directory.error());
    }
    return doof::Result<std::string, std::string>::success(path);
}

}  // namespace detail

inline doof::Result<std::string, std::string> homeDirectory() {
    return detail::homeDirectory();
}

inline std::string tempDirectory() {
    const char* temp = std::getenv("TMPDIR");
    if (temp != nullptr && temp[0] != '\0') {
        return std::string(temp);
    }

    return "/tmp";
}

inline doof::Result<std::string, std::string> dataDirectory(const std::optional<std::string>& appId = std::nullopt) {
    return detail::applicationDirectory(appId, "Application Support", "XDG_DATA_HOME", ".local/share");
}

inline doof::Result<std::string, std::string> cacheDirectory(const std::optional<std::string>& appId = std::nullopt) {
    return detail::applicationDirectory(appId, "Caches", "XDG_CACHE_HOME", ".cache");
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
