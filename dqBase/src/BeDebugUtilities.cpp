// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDebugUtilities.h
// DanQing dqBase — 调试工具实现
#include "dqBase/BeDebugUtilities.h"

#include <cstdio>
#include <cstring>

#if defined(__APPLE__)
    #include <mach/mach.h>
#elif defined(__linux__)
    #include <fstream>
#elif defined(_WIN32)
    #include <windows.h>
    #include <psapi.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

DqString BeDebugUtilities::GetStackTraceDescription(size_t maxFrames) {
    // 简化实现：平台相关的堆栈跟踪需要 execinfo.h (Linux/macOS) 或 CaptureStackBackTrace (Windows)
    (void)maxFrames;
    return "stack trace not available";
}

BeDebugUtilities::StackFrameInfo BeDebugUtilities::GetStackFrameInfoAt(size_t /*index*/) {
    return {};
}

// Ported from: imodel-native BeDebugUtilities.cpp GetStackFrameInfosAt
// ref: implemented for Windows x64 only; returns empty vector elsewhere.
std::vector<BeDebugUtilities::StackFrameInfo>
BeDebugUtilities::GetStackFrameInfosAt(size_t /*frameIndex*/, size_t /*frameCount*/) {
    return {};
}

size_t BeDebugUtilities::GetMemoryUsed() {
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return info.resident_size;
    }
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    DqString line;
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            return static_cast<uint64_t>(std::stoul(line.substr(6))) * 1024;
        }
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif
    return 0;
}

END_DQ_BASE_NAMESPACE
