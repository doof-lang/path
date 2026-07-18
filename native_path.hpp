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
            return doof::Failure<std::string>{"Failed to determine executable path"};
        }
    }

    std::vector<char> resolved(PATH_MAX);
    if (::realpath(path.data(), resolved.data()) != nullptr) {
        return doof::Success<std::string>{std::string(resolved.data())};
    }

    return doof::Success<std::string>{std::string(path.data())};
#elif defined(__linux__) || defined(__GNU__)
    size_t size = 256;
    while (true) {
        std::vector<char> path(size);
        ssize_t length = ::readlink("/proc/self/exe", path.data(), path.size() - 1);
        if (length < 0) {
            return doof::Failure<std::string>{std::string("Failed to determine executable path: ") + std::strerror(errno)};
        }
        if (static_cast<size_t>(length) < path.size() - 1) {
            path[static_cast<size_t>(length)] = '\0';
            return doof::Success<std::string>{std::string(path.data())};
        }
        size *= 2;
    }
#else
    return doof::Failure<std::string>{"Executable path is not supported on this platform"};
#endif
}

#if defined(__APPLE__)
inline doof::Result<std::string, std::string> cfStringToString(CFStringRef value, const char* description) {
    if (value == nullptr) {
        return doof::Failure<std::string>{std::string("Failed to get ") + description};
    }

    const CFIndex length = CFStringGetLength(value);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(static_cast<size_t>(maxSize));
    if (!CFStringGetCString(value, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
        return doof::Failure<std::string>{std::string("Failed to convert ") + description + " to UTF-8"};
    }

    return doof::Success<std::string>{std::string(buffer.data())};
}

inline doof::Result<std::string, std::string> bundleIdentifier() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return doof::Failure<std::string>{"Failed to get main bundle"};
    }

    CFStringRef identifier = CFBundleGetIdentifier(bundle);
    if (identifier == nullptr) {
        return doof::Failure<std::string>{"Application identifier is required for console applications"};
    }

    return cfStringToString(identifier, "bundle identifier");
}

inline doof::Result<std::string, std::string> bundleResourcesDirectory() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle == nullptr) {
        return doof::Failure<std::string>{"Failed to get main bundle"};
    }

    CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
    if (url == nullptr) {
        return doof::Failure<std::string>{"Failed to get bundle resources directory"};
    }

    std::vector<char> path(PATH_MAX);
    const Boolean ok = CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path.data()), path.size());
    CFRelease(url);

    if (!ok) {
        return doof::Failure<std::string>{"Failed to convert bundle resources directory to a path"};
    }

    return doof::Success<std::string>{std::string(path.data())};
}
#endif

inline doof::Result<std::string, std::string> resolveApplicationIdentifier(const std::optional<std::string>& appId) {
    if (appId.has_value()) {
        if (appId.value().empty()) {
            return doof::Failure<std::string>{"Application identifier cannot be empty"};
        }
        if (appId.value().find('/') != std::string::npos || appId.value().find('\0') != std::string::npos) {
            return doof::Failure<std::string>{"Application identifier cannot contain a path separator or NUL byte"};
        }
    }

#if defined(__APPLE__)
    auto bundledIdentifier = bundleIdentifier();
    if (doof::is_success(bundledIdentifier)) {
        if (appId.has_value() && appId.value() != doof::success_value(bundledIdentifier)) {
            return doof::Failure<std::string>{"Application identifier must match the bundle identifier"};
        }
        return bundledIdentifier;
    }
#endif

    if (!appId.has_value()) {
        return doof::Failure<std::string>{"Application identifier is required for console applications"};
    }

    return doof::Success<std::string>{appId.value()};
}

inline doof::Result<void, std::string> ensureSingleDirectory(const std::string& path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return doof::Success<void>{};
        }
        return doof::Failure<std::string>{path + " exists but is not a directory"};
    }

    if (errno != ENOENT) {
        return doof::Failure<std::string>{std::string("Failed to inspect directory ") + path + ": " + std::strerror(errno)};
    }

    if (::mkdir(path.c_str(), 0777) == 0) {
        return doof::Success<void>{};
    }

    if (errno == EEXIST) {
        return ensureSingleDirectory(path);
    }

    return doof::Failure<std::string>{std::string("Failed to create directory ") + path + ": " + std::strerror(errno)};
}

inline doof::Result<void, std::string> ensureDirectory(const std::string& path) {
    if (path.empty()) {
        return doof::Failure<std::string>{"Directory path cannot be empty"};
    }
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Directory path contains a NUL byte"};
    }

    size_t searchFrom = path[0] == '/' ? 1 : 0;
    while (true) {
        const size_t separator = path.find('/', searchFrom);
        const std::string current = separator == std::string::npos ? path : path.substr(0, separator);
        if (!current.empty() && current != ".") {
            auto result = ensureSingleDirectory(current);
            if (!doof::is_success(result)) {
                return result;
            }
        }
        if (separator == std::string::npos) {
            break;
        }
        searchFrom = separator + 1;
    }

    return doof::Success<void>{};
}

inline doof::Result<std::string, std::string> homeDirectory() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return doof::Success<std::string>{std::string(home)};
    }

    const passwd* entry = ::getpwuid(::getuid());
    if (entry != nullptr && entry->pw_dir != nullptr && entry->pw_dir[0] != '\0') {
        return doof::Success<std::string>{std::string(entry->pw_dir)};
    }

    return doof::Failure<std::string>{"Failed to determine the home directory"};
}

inline doof::Result<std::string, std::string> applicationDirectory(
    const std::optional<std::string>& appId,
    const char* appleRelativeBase,
    const char* xdgEnvironmentName,
    const char* unixRelativeBase
) {
    auto identifier = resolveApplicationIdentifier(appId);
    if (!doof::is_success(identifier)) {
        return doof::Failure<std::string>{doof::failure_error(identifier)};
    }

#if defined(__APPLE__)
    auto home = homeDirectory();
    if (!doof::is_success(home)) {
        return doof::Failure<std::string>{doof::failure_error(home)};
    }

    const std::string path = doof::success_value(home) + "/Library/" + appleRelativeBase + "/" + doof::success_value(identifier);
#else
    std::string path;
    const char* xdgBase = std::getenv(xdgEnvironmentName);
    if (xdgBase != nullptr && xdgBase[0] != '\0') {
        path = std::string(xdgBase) + "/" + doof::success_value(identifier);
    } else {
        auto home = homeDirectory();
        if (!doof::is_success(home)) {
            return doof::Failure<std::string>{doof::failure_error(home)};
        }

        path = doof::success_value(home) + "/" + unixRelativeBase + "/" + doof::success_value(identifier);
    }
#endif

    auto directory = ensureDirectory(path);
    if (!doof::is_success(directory)) {
        return doof::Failure<std::string>{doof::failure_error(directory)};
    }
    return doof::Success<std::string>{path};
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
            return doof::Success<std::string>{std::string(buffer.data())};
        }

        if (errno != ERANGE) {
            return doof::Failure<std::string>{std::string("Failed to get current working directory: ") + std::strerror(errno)};
        }

        size *= 2;
    }
}

inline doof::Result<std::string, std::string> absolute(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Path contains a NUL byte"};
    }
    if (!path.empty() && path.front() == '/') {
        return doof::Success<std::string>{path};
    }

    auto workingDirectory = currentWorkingDirectory();
    if (!doof::is_success(workingDirectory)) {
        return workingDirectory;
    }
    const std::string& base = doof::success_value(workingDirectory);
    if (path.empty()) {
        return doof::Success<std::string>{base};
    }
    return doof::Success<std::string>{base + "/" + path};
}

inline doof::Result<void, std::string> setCurrentWorkingDirectory(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Working directory path contains a NUL byte"};
    }

    if (::chdir(path.c_str()) == 0) {
        return doof::Success<void>{};
    }

    return doof::Failure<std::string>{std::string("Failed to set current working directory: ") + std::strerror(errno)};
}

inline doof::Result<std::string, std::string> resourcesDirectory() {
#if defined(__APPLE__)
    auto bundled = detail::bundleResourcesDirectory();
    if (doof::is_success(bundled)) {
        return bundled;
    }
#endif

    auto executable = detail::executablePath();
    if (!doof::is_success(executable)) {
        return doof::Failure<std::string>{doof::failure_error(executable)};
    }

    return doof::Success<std::string>{detail::dirname(doof::success_value(executable))};
}

}  // namespace doof_path
