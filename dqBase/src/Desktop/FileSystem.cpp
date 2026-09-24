// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Desktop/FileSystem.h
// DanQing dqBase — 桌面文件系统操作实现
#include "dqBase/Desktop/FileSystem.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
    #include <direct.h>
#elif defined(__APPLE__)
    #include <unistd.h>
    #include <sys/statvfs.h>
    #include <mach-o/dyld.h>
#else
    #include <unistd.h>
    #include <sys/statvfs.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

namespace Desktop {

BeFileNameStatus FileSystem::BeGetTempPath(DqString& tempPath) {
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buf);
    if (len == 0) return BeFileNameStatus::UnknownError;
    tempPath = DqString(buf, len);
#else
    tempPath = "/tmp";
#endif
    return BeFileNameStatus::Success;
}

BeFileNameStatus FileSystem::BeGetTempFileName(DqString& tempFileName, const DqString& pathName, const char* prefixString) {
    DqString dirPath = pathName.empty() ? "/tmp" : pathName;
    DqString pref = (prefixString == nullptr || prefixString[0] == '\0') ? "zo" : DqString(prefixString);
#if defined(_WIN32)
    char buf[MAX_PATH];
    UINT uResult = GetTempFileNameA(dirPath.c_str(), pref.c_str(), 0, buf);
    if (uResult == 0) return BeFileNameStatus::UnknownError;
    tempFileName = buf;
#else
    DqString tmpl = dirPath + "/" + pref + "XXXXXX";
    char buf[1024];
    strncpy(buf, tmpl.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int fd = mkstemp(buf);
    if (fd < 0) return BeFileNameStatus::CantCreate;
    close(fd);
    tempFileName = buf;
#endif
    return BeFileNameStatus::Success;
}

BeFileNameStatus FileSystem::BeGetDiskFreeSpace(uint64_t& freeBytes, const DqString& dirName) {
#if defined(_WIN32)
    ULARGE_INTEGER freeAvail;
    if (!GetDiskFreeSpaceExA(dirName.c_str(), &freeAvail, nullptr, nullptr))
        return BeFileNameStatus::UnknownError;
    freeBytes = freeAvail.QuadPart;
#else
    struct statvfs st;
    if (statvfs(dirName.c_str(), &st) != 0)
        return BeFileNameStatus::UnknownError;
    freeBytes = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
#endif
    return BeFileNameStatus::Success;
}

// Ported from: imodel-native Desktop/FileSystem.cpp GetCwd
BeFileNameStatus FileSystem::GetCwd(DqString& currentDirectory) {
    char buf[1024];
#if defined(_WIN32)
    if (_getcwd(buf, sizeof(buf))) {
        currentDirectory = buf;
        return BeFileNameStatus::Success;
    }
#else
    if (getcwd(buf, sizeof(buf))) {
        currentDirectory = buf;
        return BeFileNameStatus::Success;
    }
#endif
    return BeFileNameStatus::UnknownError;
}

// Ported from: imodel-native Desktop/FileSystem.cpp GetExecutableDir
BeFileName FileSystem::GetExecutableDir(const DqString* moduleName) {
    DqString dir;
    (void)moduleName; // 仅 Windows 使用该参数获取模块目录
#if defined(__APPLE__)
    char buf[1024];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        dir = DqString(buf);
        auto pos = dir.find_last_of('/');
        if (pos != DqString::npos)
            dir = dir.substr(0, pos);
    }
#elif defined(__linux__)
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        dir = DqString(buf);
        auto pos = dir.find_last_of('/');
        if (pos != DqString::npos)
            dir = dir.substr(0, pos);
    }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    HMODULE hMod = (moduleName != nullptr) ? GetModuleHandleA(moduleName->c_str()) : nullptr;
    GetModuleFileNameA(hMod, buf, MAX_PATH);
    dir = DqString(buf);
    auto pos = dir.find_last_of('\\');
    if (pos != DqString::npos)
        dir = dir.substr(0, pos);
#else
#endif
    return BeFileName(dir);
}

// Ported from: imodel-native Desktop/FileSystem.cpp GetLibraryDir
BeFileName FileSystem::GetLibraryDir() {
    // ref: on platforms without a distinct library dir, falls back to executable dir.
    return GetExecutableDir(nullptr);
}

} // namespace Desktop

END_DQ_BASE_NAMESPACE
