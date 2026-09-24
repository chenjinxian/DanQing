// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Logging.h
//              ConsoleLogger impl (ref:36-48), Logging facade (ref:53-89).
// DanQing dqBase — faithful 1:1 port of the NativeLogging surface (impl).
//
// Char-only (§7.2): no WChar variants. ConsoleLogger routes to stderr.
#include "dqBase/Logging.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>

BEGIN_DQ_BASE_NAMESPACE

namespace NativeLogging {

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:36-48 — ConsoleLogger implementation
// ---------------------------------------------------------------------------
void ConsoleLogger::LogMessage(Utf8CP category, SEVERITY sev, Utf8CP msg) {
    // Aligns with ref's console sink; DanQing uses fprintf(stderr) (UTF-8).
    std::lock_guard<DqMutex> lock(m_lock);
    const char* sevStr = "?";
    switch (sev) {
        case LOG_FATAL:   sevStr = "FATAL";   break;
        case LOG_ERROR:   sevStr = "ERROR";   break;
        case LOG_WARNING: sevStr = "WARNING"; break;
        case LOG_INFO:    sevStr = "INFO";    break;
        case LOG_DEBUG:   sevStr = "DEBUG";   break; // LOG_TRACE aliases LOG_DEBUG (-4)
        default:          sevStr = "?";       break;
    }
    std::fprintf(stderr, "[%s] %s: %s\n",
                 sevStr,
                 category ? category : "",
                 msg ? msg : "");
}

// Ported from: ref:42 — IsSeverityEnabled. Category-specific severity wins;
// otherwise falls back to m_defaultSeverity. The threshold is the *most verbose*
// severity that is enabled; more-severe messages (numerically LARGER — FATAL=0
// is most severe, DEBUG=-4 is most verbose) are also enabled.
//   threshold=INFO(-3): ERROR(-1) enabled (-1 >= -3), DEBUG(-4) disabled (-4 < -3).
// LOG_NEVER (ref:20) can never be enabled.
bool ConsoleLogger::IsSeverityEnabled(Utf8CP category, SEVERITY sev) {
    std::lock_guard<DqMutex> lock(m_lock);
    SEVERITY threshold = m_defaultSeverity;
    if (category) {
        auto it = m_severity.find(Utf8String(category));
        if (it != m_severity.end())
            threshold = it->second;
    }
    if (sev == LOG_NEVER)
        return false;
    return sev >= threshold;
}

// Ported from: ref:46
void ConsoleLogger::SetSeverity(CharCP category, SEVERITY severity) {
    std::lock_guard<DqMutex> lock(m_lock);
    if (category)
        m_severity[Utf8String(category)] = severity;
}

// Ported from: ref:47 — singleton ConsoleLogger instance.
ConsoleLogger& ConsoleLogger::GetLogger() {
    static ConsoleLogger s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:54-55 — Logging facade
// Default Logger is the ConsoleLogger singleton (matches ref behavior).
// ---------------------------------------------------------------------------
namespace {
Logger*& GlobalLoggerRef() {
    static Logger* s_logger = &ConsoleLogger::GetLogger();
    return s_logger;
}
} // namespace

Logger& Logging::GetLogger() {
    return *GlobalLoggerRef();
}

void Logging::SetLogger(Logger* logger) {
    GlobalLoggerRef() = logger ? logger : &ConsoleLogger::GetLogger();
}

// Ported from: ref:75-78. UTF-8 formatting via vsnprintf.
void Logging::LogMessageVa(Utf8CP category, SEVERITY sev, Utf8CP fmt, va_list args) {
    if (GetLogger().IsSeverityEnabled(category, sev)) {
        char buf[2048];
        va_list args2;
        va_copy(args2, args);
        std::vsnprintf(buf, sizeof(buf), fmt ? fmt : "", args2);
        va_end(args2);
        GetLogger().LogMessage(category, sev, buf);
    }
}

} // namespace NativeLogging

END_DQ_BASE_NAMESPACE
