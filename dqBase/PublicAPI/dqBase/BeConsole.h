// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeConsole.h
// DanQing dqBase — 控制台输出
//
// 1:1 对齐 imodel-native BeConsole 的 sink 契约：Printf 不写 stdout，
// 而是通过日志系统以 "BeConsole" 分类输出（ref 调
// NativeLogging::Logging::LogMessageV("BeConsole", LOG_DEBUG, fmt, args)，
// 即 LOG_DEBUG 通道）。DanQing 的等价 sink 是 Logger::LogTrace。
#pragma once

#include "Export.h"
#include "Logger.h"

#include <cstdio>
#include <cstdarg>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeConsole — 调试控制台输出（对齐 imodel-native BeConsole）
// Ported from: imodel-native BeConsole.h
//
// ref 行为：Printf -> NativeLogging LOG_DEBUG 通道 "BeConsole"。
// DanQing 对应：Logger::LogTrace("BeConsole", <formatted>)。
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeConsole {
    /// 格式化输出到日志系统（"BeConsole" 分类，Trace 级别）。
    /// 对齐 ref 返回类型 int（ref 恒返回 1）。
    static int Printf(const char* fmt, ...) noexcept {
        va_list args;
        va_start(args, fmt);
        int rc = VPrintf(fmt, args);
        va_end(args);
        return rc;
    }

    /// 格式化输出到日志系统（va_list 版本）。
    /// 注意：ref BeConsole 没有 VPrintf（DanQing 扩展），保持与 Printf 相同的 sink。
    static int VPrintf(const char* fmt, va_list args) noexcept {
        char buffer[1024];
        va_list argsCopy;
        va_copy(argsCopy, args);
        int needed = vsnprintf(buffer, sizeof(buffer), fmt, argsCopy);
        va_end(argsCopy);
        // vsnprintf 已写出的内容（受 sizeof(buffer) 限制）即被记录；
        // needed < 0 表示格式化错误，此时记录空串。安全：buffer 始终以 \0 结尾。
        Logger::LogTrace("BeConsole", buffer);
        // 对齐 ref：恒返回 1 表示"已分发一条日志消息"。
        (void)needed;
        return 1;
    }
};

END_DQ_BASE_NAMESPACE
