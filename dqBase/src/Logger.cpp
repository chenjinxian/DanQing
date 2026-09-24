// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/Logger.ts
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/Logging.h
// DanQing dqBase — 完整日志系统实现
#include "dqBase/Logger.h"
#include "dqBase/bmap.h"

#include <cstdio>
#include <cstring>
#include <mutex>

BEGIN_DQ_BASE_NAMESPACE

namespace {
Logger::LogFunction g_logFunc;
LogLevel g_defaultLevel = LogLevel::None;
// §7.3: category→level lookup uses btree-backed bmap (not std::unordered_map).
bmap<std::string, LogLevel> g_categoryLevels;
std::mutex g_logMutex;
bool g_initialized = false;

void DefaultLogFunc(const char* category, const char* message) {
    fprintf(stderr, "[%s] %s\n", category ? category : "", message);
}
} // namespace

void Logger::Initialize() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized) return;
    g_initialized = true;
    g_defaultLevel = LogLevel::None;
    g_logFunc = DefaultLogFunc;
}

void Logger::InitializeToConsole() {
    Initialize();
}

void Logger::SetLogFunction(LogFunction func) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logFunc = std::move(func);
}

void Logger::SetLevelDefault(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_defaultLevel = level;
}

void Logger::SetLevel(const char* category, LogLevel level) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_categoryLevels[category] = level;
}

LogLevel Logger::GetLevel(const char* category) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    auto it = g_categoryLevels.find(category);
    if (it != g_categoryLevels.end())
        return it->second;
    return g_defaultLevel;
}

bool Logger::IsEnabled(const char* category, LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(GetLevel(category));
}

void Logger::TurnOffLevelDefault() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_defaultLevel = LogLevel::None;
}

void Logger::TurnOffCategories(const char* categories) {
    // 简单实现：关闭指定分类
    if (!categories) return;
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_categoryLevels[categories] = LogLevel::None;
}

void Logger::ConfigureLevels(const char* config) {
    // 简单实现：解析 "category=level" 格式
    if (!config) return;
    // TODO: 完整的配置解析
}

static void DoLog(const char* category, LogLevel level, const char* message) {
    if (!Logger::IsEnabled(category, level)) return;
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFunc) {
        g_logFunc(category, message);
    }
}

void Logger::LogError(const char* category, const char* message) {
    DoLog(category, LogLevel::Error, message);
}

void Logger::LogWarning(const char* category, const char* message) {
    DoLog(category, LogLevel::Warning, message);
}

void Logger::LogInfo(const char* category, const char* message) {
    DoLog(category, LogLevel::Info, message);
}

void Logger::LogTrace(const char* category, const char* message) {
    DoLog(category, LogLevel::Trace, message);
}

// --- PerfLogger ---

PerfLogger::PerfLogger(const char* operation)
    : m_operation(operation ? operation : "")
    , m_start(DqTimePoint::Now())
{
}

PerfLogger::~PerfLogger() {
    Dispose();
}

void PerfLogger::Dispose() {
    if (m_disposed) return;
    m_disposed = true;
    auto elapsed = DqTimePoint::Now() - m_start;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %.3f ms",
             m_operation.c_str(), elapsed.ToMilliseconds() / 1000.0);
    Logger::LogInfo("Performance", buf);
}

END_DQ_BASE_NAMESPACE
