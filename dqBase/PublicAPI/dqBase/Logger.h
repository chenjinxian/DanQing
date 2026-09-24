// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Logger.ts
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/Logging.h
// DanQing dqBase — 完整日志系统
//
// 1:1 对齐 itwinjs-core Logger 类 + imodel-native CategoryLogger。
// 替代之前的简化 Logging.h。
#pragma once

#include "Export.h"
#include "DqTime.h"

#include <cstdint>
#include <functional>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// LogLevel — 日志级别（对齐 itwinjs-core LogLevel）
// Ported from: itwinjs-core Logger.ts LogLevel
// ---------------------------------------------------------------------------
enum class LogLevel : int {
    Trace   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    None    = 4,
};

// ---------------------------------------------------------------------------
// Logger — 完整日志设施（对齐 itwinjs-core Logger）
// Ported from: itwinjs-core Logger.ts Logger
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT Logger {
public:
    using LogFunction = std::function<void(const char* category, const char* message)>;

    /// 初始化日志系统
    static void Initialize();

    /// 初始化为控制台输出
    static void InitializeToConsole();

    /// 设置日志输出函数
    static void SetLogFunction(LogFunction func);

    /// 设置默认日志级别
    static void SetLevelDefault(LogLevel level);

    /// 设置特定分类的日志级别
    static void SetLevel(const char* category, LogLevel level);

    /// 获取特定分类的日志级别
    static LogLevel GetLevel(const char* category);

    /// 检查指定分类和级别是否启用
    static bool IsEnabled(const char* category, LogLevel level);

    /// 关闭默认级别
    static void TurnOffLevelDefault();

    /// 关闭特定分类
    static void TurnOffCategories(const char* categories);

    /// 配置日志级别（从字符串解析）
    static void ConfigureLevels(const char* config);

    // --- 日志输出 ---
    static void LogError(const char* category, const char* message);
    static void LogWarning(const char* category, const char* message);
    static void LogInfo(const char* category, const char* message);
    static void LogTrace(const char* category, const char* message);
};

// ---------------------------------------------------------------------------
// PerfLogger — 性能日志（对齐 itwinjs-core PerfLogger）
// Ported from: itwinjs-core Logger.ts PerfLogger
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT PerfLogger {
public:
    explicit PerfLogger(const char* operation);
    ~PerfLogger();

    void Dispose();

private:
    std::string m_operation;
    DqTimePoint m_start;
    bool m_disposed = false;
};

// ---------------------------------------------------------------------------
// CategoryLogger — 便捷分类日志（对齐 imodel-native CategoryLogger）
// Ported from: imodel-native Logging.h CategoryLogger
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT CategoryLogger {
public:
    explicit CategoryLogger(const char* category) : m_category(category) {}

    void error(const char* msg) const   { Logger::LogError(m_category, msg); }
    void warning(const char* msg) const { Logger::LogWarning(m_category, msg); }
    void info(const char* msg) const    { Logger::LogInfo(m_category, msg); }
    void trace(const char* msg) const   { Logger::LogTrace(m_category, msg); }

    bool IsEnabled(LogLevel level) const { return Logger::IsEnabled(m_category, level); }

private:
    const char* m_category;
};

END_DQ_BASE_NAMESPACE
