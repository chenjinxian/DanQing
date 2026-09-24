// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeSystemInfo.h
// DanQing dqBase — 系统信息实现
#include "dqBase/BeSystemInfo.h"

#include <cstdio>
#include <thread>

#if defined(__APPLE__)
    #include <sys/sysctl.h>
    #include <unistd.h>
    #include <mach/mach.h>
#elif defined(__linux__)
    #include <sys/sysinfo.h>
    #include <unistd.h>
    #include <fstream>
#elif defined(_WIN32)
    #include <windows.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

// Ported from: imodel-native BeSystemInfo.cpp CacheAndroidDeviceId
// Android-only; no-op on desktop/mobile platforms for ABI alignment.
void BeSystemInfo::CacheAndroidDeviceId(const DqString& /*deviceId*/) {
    // ref caches the id for Android; no-op elsewhere.
}

// Ported from: imodel-native BeSystemInfo.cpp GetModelName
// ref: detailed model only on iOS; returns empty elsewhere (matches reference).
DqString BeSystemInfo::GetModelName() {
#if defined(__APPLE__) && (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
    // iOS: read model identifier via sysctl hw.machine.
    char buf[64] = {0};
    size_t len = sizeof(buf);
    sysctlbyname("hw.machine", buf, &len, nullptr, 0);
    return DqString(buf);
#else
    return DqString();
#endif
}

uint64_t BeSystemInfo::GetAmountOfPhysicalMemory() {
#if defined(__APPLE__)
    int64_t mem;
    size_t len = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &len, nullptr, 0);
    return static_cast<uint64_t>(mem);
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0)
        return si.totalram * si.mem_unit;
    return 0;
#elif defined(_WIN32)
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        return ms.ullTotalPhys;
    return 0;
#else
    return 0;
#endif
}

uint32_t BeSystemInfo::GetNumberOfCpus() {
    return std::thread::hardware_concurrency();
}

DqString BeSystemInfo::GetOSName() {
#if defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Unknown";
#endif
}

DqString BeSystemInfo::GetOSVersion() {
#if defined(__APPLE__)
    char buf[64];
    size_t len = sizeof(buf);
    sysctlbyname("kern.osproductversion", buf, &len, nullptr, 0);
    return DqString(buf);
#elif defined(__linux__)
    std::ifstream f("/proc/version");
    DqString line;
    if (std::getline(f, line)) return line;
    return "Unknown";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Unknown";
#endif
}

DqString BeSystemInfo::GetMachineName() {
    char buf[256];
#if defined(_WIN32)
    DWORD len = sizeof(buf);
    if (GetComputerNameA(buf, &len)) return DqString(buf);
#else
    if (gethostname(buf, sizeof(buf)) == 0) return DqString(buf);
#endif
    return "Unknown";
}

DqString BeSystemInfo::GetDeviceId() {
    // 简化实现：使用机器名称作为设备 ID
    return GetMachineName();
}

END_DQ_BASE_NAMESPACE
