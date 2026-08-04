#pragma once

#include "doof_runtime.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#ifdef small
#undef small
#endif
#else
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#endif

namespace doof_path {

namespace detail {

inline bool isWindowsDriveRoot(const std::string& path) {
    if (path.size() < 3 || path[1] != ':' || (path[2] != '/' && path[2] != '\\')) {
        return false;
    }
    const char drive = path[0];
    return (drive >= 'A' && drive <= 'Z') || (drive >= 'a' && drive <= 'z');
}

#if defined(_WIN32)
inline std::string windowsError(const char* operation, DWORD error = ::GetLastError()) {
    return std::string(operation) + " (Windows error " + std::to_string(error) + ")";
}

inline doof::Result<std::wstring, std::string> utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return doof::Success<std::wstring>{std::wstring()};
    }
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size == 0) {
        return doof::Failure<std::string>{windowsError("Failed to decode UTF-8 path")};
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size) == 0) {
        return doof::Failure<std::string>{windowsError("Failed to decode UTF-8 path")};
    }
    return doof::Success<std::wstring>{result};
}

inline doof::Result<std::string, std::string> wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return doof::Success<std::string>{std::string()};
    }
    const int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return doof::Failure<std::string>{windowsError("Failed to encode Windows path as UTF-8")};
    }
    std::string result(static_cast<size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) == 0) {
        return doof::Failure<std::string>{windowsError("Failed to encode Windows path as UTF-8")};
    }
    for (char& character : result) {
        if (character == '\\') character = '/';
    }
    return doof::Success<std::string>{result};
}

inline doof::Result<std::string, std::string> windowsEnvironmentPath(const wchar_t* name) {
    const wchar_t* value = ::_wgetenv(name);
    if (value == nullptr || value[0] == L'\0') {
        return doof::Failure<std::string>{"Windows environment path is unavailable"};
    }
    return wideToUtf8(std::wstring(value));
}

inline std::string windowsTempFallback() {
    UINT size = MAX_PATH;
    while (true) {
        std::vector<wchar_t> path(size);
        const UINT length = ::GetWindowsDirectoryW(path.data(), size);
        if (length == 0) {
            break;
        }
        if (length < size) {
            auto converted = wideToUtf8(std::wstring(path.data(), length));
            if (doof::is_success(converted)) {
                return doof::success_value(converted) + "/Temp";
            }
            break;
        }
        size = length + 1;
    }

    const DWORD currentSize = ::GetCurrentDirectoryW(0, nullptr);
    if (currentSize > 0) {
        std::vector<wchar_t> path(currentSize);
        const DWORD length = ::GetCurrentDirectoryW(currentSize, path.data());
        if (length > 0 && length < currentSize) {
            auto converted = wideToUtf8(std::wstring(path.data(), length));
            if (doof::is_success(converted)) {
                return doof::success_value(converted);
            }
        }
    }
    return "/";
}
#endif

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
#if defined(_WIN32)
    DWORD size = 256;
    while (true) {
        std::vector<wchar_t> path(size);
        const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), size);
        if (length == 0) {
            return doof::Failure<std::string>{windowsError("Failed to determine executable path")};
        }
        if (length < size - 1) {
            return wideToUtf8(std::wstring(path.data(), length));
        }
        size *= 2;
    }
#elif defined(__APPLE__)
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
        if (appId.value().find('/') != std::string::npos || appId.value().find('\\') != std::string::npos || appId.value().find('\0') != std::string::npos) {
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
#if defined(_WIN32)
    auto widePath = utf8ToWide(path);
    if (!doof::is_success(widePath)) {
        return doof::Failure<std::string>{doof::failure_error(widePath)};
    }
    const std::wstring& nativePath = doof::success_value(widePath);
    const DWORD attributes = ::GetFileAttributesW(nativePath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            return doof::Success<void>{};
        }
        return doof::Failure<std::string>{path + " exists but is not a directory"};
    }
    const DWORD inspectionError = ::GetLastError();
    if (inspectionError != ERROR_FILE_NOT_FOUND && inspectionError != ERROR_PATH_NOT_FOUND) {
        return doof::Failure<std::string>{windowsError((std::string("Failed to inspect directory ") + path).c_str(), inspectionError)};
    }
    if (::CreateDirectoryW(nativePath.c_str(), nullptr) != 0) {
        return doof::Success<void>{};
    }
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        return ensureSingleDirectory(path);
    }
    return doof::Failure<std::string>{windowsError((std::string("Failed to create directory ") + path).c_str())};
#else
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
#endif
}

inline doof::Result<void, std::string> ensureDirectory(const std::string& path) {
    if (path.empty()) {
        return doof::Failure<std::string>{"Directory path cannot be empty"};
    }
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Directory path contains a NUL byte"};
    }

    size_t searchFrom = path[0] == '/' ? 1 : (isWindowsDriveRoot(path) ? 3 : 0);
#if defined(_WIN32)
    if (path.rfind("//", 0) == 0) {
        const size_t serverEnd = path.find('/', 2);
        if (serverEnd != std::string::npos && serverEnd > 2) {
            const size_t shareEnd = path.find('/', serverEnd + 1);
            const bool hasShare = shareEnd == std::string::npos
                ? serverEnd + 1 < path.size()
                : shareEnd > serverEnd + 1;
            if (hasShare) {
                searchFrom = shareEnd == std::string::npos ? path.size() : shareEnd + 1;
            }
        }
    }
#endif
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
#if defined(_WIN32)
    auto profile = windowsEnvironmentPath(L"USERPROFILE");
    if (doof::is_success(profile)) {
        return profile;
    }
    const wchar_t* drive = ::_wgetenv(L"HOMEDRIVE");
    const wchar_t* relativePath = ::_wgetenv(L"HOMEPATH");
    if (drive != nullptr && relativePath != nullptr && drive[0] != L'\0' && relativePath[0] != L'\0') {
        return wideToUtf8(std::wstring(drive) + relativePath);
    }
    return doof::Failure<std::string>{"Failed to determine the home directory"};
#else
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return doof::Success<std::string>{std::string(home)};
    }

    const passwd* entry = ::getpwuid(::getuid());
    if (entry != nullptr && entry->pw_dir != nullptr && entry->pw_dir[0] != '\0') {
        return doof::Success<std::string>{std::string(entry->pw_dir)};
    }

    return doof::Failure<std::string>{"Failed to determine the home directory"};
#endif
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

#if defined(_WIN32)
    const wchar_t* environmentName = std::strcmp(xdgEnvironmentName, "XDG_CACHE_HOME") == 0 ? L"LOCALAPPDATA" : L"APPDATA";
    auto base = windowsEnvironmentPath(environmentName);
    if (!doof::is_success(base)) {
        return doof::Failure<std::string>{doof::failure_error(base)};
    }
    const std::string path = doof::success_value(base) + "/" + doof::success_value(identifier);
#elif defined(__APPLE__)
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
#if defined(_WIN32)
    DWORD size = ::GetTempPathW(0, nullptr);
    if (size == 0) {
        return detail::windowsTempFallback();
    }
    std::vector<wchar_t> path(size);
    const DWORD length = ::GetTempPathW(size, path.data());
    if (length == 0 || length >= size) {
        return detail::windowsTempFallback();
    }
    auto converted = detail::wideToUtf8(std::wstring(path.data(), length));
    return doof::is_success(converted) ? doof::success_value(converted) : detail::windowsTempFallback();
#else
    const char* temp = std::getenv("TMPDIR");
    if (temp != nullptr && temp[0] != '\0') {
        return std::string(temp);
    }

    return "/tmp";
#endif
}

inline doof::Result<std::string, std::string> dataDirectory(const std::optional<std::string>& appId = std::nullopt) {
    return detail::applicationDirectory(appId, "Application Support", "XDG_DATA_HOME", ".local/share");
}

inline doof::Result<std::string, std::string> cacheDirectory(const std::optional<std::string>& appId = std::nullopt) {
    return detail::applicationDirectory(appId, "Caches", "XDG_CACHE_HOME", ".cache");
}

inline doof::Result<std::string, std::string> currentWorkingDirectory() {
#if defined(_WIN32)
    const DWORD size = ::GetCurrentDirectoryW(0, nullptr);
    if (size == 0) {
        return doof::Failure<std::string>{detail::windowsError("Failed to get current working directory")};
    }
    std::vector<wchar_t> buffer(size);
    const DWORD length = ::GetCurrentDirectoryW(size, buffer.data());
    if (length == 0 || length >= size) {
        return doof::Failure<std::string>{detail::windowsError("Failed to get current working directory")};
    }
    return detail::wideToUtf8(std::wstring(buffer.data(), length));
#else
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
#endif
}

inline doof::Result<std::string, std::string> absolute(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Path contains a NUL byte"};
    }
#if defined(_WIN32)
    if ((!path.empty() && path.front() == '/') || detail::isWindowsDriveRoot(path)) {
        return doof::Success<std::string>{path};
    }

    auto widePath = detail::utf8ToWide(path);
    if (!doof::is_success(widePath)) {
        return doof::Failure<std::string>{doof::failure_error(widePath)};
    }

    const std::wstring& nativePath = doof::success_value(widePath);
    const DWORD size = ::GetFullPathNameW(nativePath.c_str(), 0, nullptr, nullptr);
    if (size == 0) {
        return doof::Failure<std::string>{detail::windowsError("Failed to resolve absolute path")};
    }
    std::vector<wchar_t> resolved(size);
    const DWORD length = ::GetFullPathNameW(nativePath.c_str(), size, resolved.data(), nullptr);
    if (length == 0 || length >= size) {
        return doof::Failure<std::string>{detail::windowsError("Failed to resolve absolute path")};
    }
    return detail::wideToUtf8(std::wstring(resolved.data(), length));
#else
    if ((!path.empty() && path.front() == '/') || detail::isWindowsDriveRoot(path)) {
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
#endif
}

inline doof::Result<void, std::string> setCurrentWorkingDirectory(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        return doof::Failure<std::string>{"Working directory path contains a NUL byte"};
    }

#if defined(_WIN32)
    auto widePath = detail::utf8ToWide(path);
    if (!doof::is_success(widePath)) {
        return doof::Failure<std::string>{doof::failure_error(widePath)};
    }
    if (::SetCurrentDirectoryW(doof::success_value(widePath).c_str()) != 0) {
        return doof::Success<void>{};
    }
    return doof::Failure<std::string>{detail::windowsError("Failed to set current working directory")};
#else
    if (::chdir(path.c_str()) == 0) {
        return doof::Success<void>{};
    }

    return doof::Failure<std::string>{std::string("Failed to set current working directory: ") + std::strerror(errno)};
#endif
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
