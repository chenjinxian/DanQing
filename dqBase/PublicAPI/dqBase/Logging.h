// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Logging.h
// DanQing dqBase — faithful 1:1 port of the imodel-native NativeLogging surface.
//
// Replaces the previously invented DqLogLevel/DqLog/DQ_LOG_* API (§6 violation).
// Everything lives inside `dqBase::NativeLogging` to match the reference's
// `BentleyApi::NativeLogging` namespace, so it coexists WITHOUT collision with
// the itwinjs-ported top-level `dqBase::Logger` / `dqBase::CategoryLogger`
// (in Logger.h) — different namespaces, different reference sources.
//
// Char-only adaptation (§7.2 POSIX): the reference's WChar/WString wide
// variants (LogMessageW/VW/VaW, message/fatal/.../trace WCharCP overloads) are
// omitted — DanQing is UTF-8 with no WCHAR type. Each omission is marked inline.
#pragma once

#include "Export.h"
#include "DqTypes.h"   // bmap<>, DqString
#include "DqSync.h"    // DqMutex (recursive, aligns with imodel-native BeMutex)

#include <cstdarg>
#include <cstdio>

// Aligns with ref:12 `#define USING_NAMESPACE_BENTLEY_LOGGING`.
// DanQing namespace convention → dqBase::NativeLogging.
#define USING_NAMESPACE_DQ_BASE_LOGGING using namespace dqBase::NativeLogging;

BEGIN_DQ_BASE_NAMESPACE

namespace NativeLogging {

// Local ref-faithful aliases (const char*). Match ref Utf8CP/CharCP usage.
// Scoped INSIDE NativeLogging to avoid colliding with the same aliases defined
// at the dqBase top level by Base64Utilities.h (C++ "redefinition of using").
using Utf8CP   = const char*;
using CharCP   = const char*;
using Utf8String   = std::string;
using Utf8StringCR = const std::string&;

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:18-27
// Logger message severity levels. Plain enum with EXACT reference values.
// NOTE: LOG_TRACE and LOG_DEBUG share value -4 (alias), per ref:25-26.
// ---------------------------------------------------------------------------
typedef enum {
    LOG_NEVER   =  1, // cannot be enabled.
    LOG_FATAL   =  0, // errors that will terminate the application
    LOG_ERROR   = -1,
    LOG_WARNING = -2,
    LOG_INFO    = -3,
    LOG_DEBUG   = -4, // for debugging
    LOG_TRACE   = -4  // alias for debug
} SEVERITY;

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:30-33
// Base class for Logger implementation. Default implementation does nothing.
// ---------------------------------------------------------------------------
struct Logger {
    virtual void LogMessage(Utf8CP category, SEVERITY sev, Utf8CP msg) { (void)category; (void)sev; (void)msg; }
    virtual bool IsSeverityEnabled(Utf8CP category, SEVERITY sev) { (void)category; (void)sev; return false; }
    virtual ~Logger() = default;
};

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:36-48
// Logs to the console (stderr in DanQing's UTF-8/POSIX adaptation).
// m_severity uses bmap<Utf8String, SEVERITY> per §7.3 (ref uses bmap).
// m_lock uses DqMutex (recursive) — aligns with ref's BeMutex.
// ---------------------------------------------------------------------------
struct ConsoleLogger : Logger {
protected:
    bmap<Utf8String, SEVERITY> m_severity;
    DqMutex m_lock;
    ConsoleLogger() {}
    DQ_BASE_EXPORT virtual void LogMessage(Utf8CP category, SEVERITY sev, Utf8CP msg) override;
    DQ_BASE_EXPORT virtual bool IsSeverityEnabled(Utf8CP category, SEVERITY sev) override;

public:
    SEVERITY m_defaultSeverity = LOG_ERROR;
    DQ_BASE_EXPORT void SetSeverity(CharCP category, SEVERITY severity);
    DQ_BASE_EXPORT static ConsoleLogger& GetLogger();
};

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:53-89
// Static logging facade. Char variants ported faithfully; W (wide) variants
// omitted (DanQing is UTF-8, §7.2 POSIX adaptation — no WCharCP/WCHAR).
// ---------------------------------------------------------------------------
struct Logging {
    DQ_BASE_EXPORT static Logger& GetLogger();
    DQ_BASE_EXPORT static void SetLogger(Logger*);

    // Ported from: ref:58-60
    static bool isSeverityEnabled(Utf8CP category, SEVERITY sev) {
        return GetLogger().IsSeverityEnabled(category, sev);
    }
    // Ported from: ref:61-64
    static void LogMessage(Utf8CP category, SEVERITY sev, Utf8CP msg) {
        if (GetLogger().IsSeverityEnabled(category, sev))
            GetLogger().LogMessage(category, sev, msg);
    }
    // ref LogMessageW (ref:65-68) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
    // Ported from: ref:69-74
    static void LogMessageV(Utf8CP category, SEVERITY sev, Utf8CP fmt, ...) {
        va_list args;
        va_start(args, fmt);
        LogMessageVa(category, sev, fmt, args);
        va_end(args);
    }
    // Ported from: ref:75-78. Uses vsnprintf (UTF-8) in place of ref's
    // Utf8PrintfString::CreateFromVaList.
    DQ_BASE_EXPORT static void LogMessageVa(Utf8CP category, SEVERITY sev, Utf8CP fmt, va_list args);
    // ref LogMessageVW (ref:79-84) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
    // ref LogMessageVaW (ref:85-88) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
};

// ---------------------------------------------------------------------------
// Ported from: imodel-native Logging.h:91-246
// Per-category logger. Char variants ported faithfully; W (wide) overloads
// (message/fatal/.../trace taking WCharCP, *v WCharCP) omitted (DanQing is
// UTF-8, §7.2 POSIX adaptation — no WCharCP).
// ---------------------------------------------------------------------------
struct CategoryLogger {
    Utf8CP m_category;
    CategoryLogger(Utf8CP category) : m_category(category) {}

    // Ported from: ref:95-97
    void LogMessage(SEVERITY sev, Utf8CP msg) const {
        Logging::LogMessage(m_category, sev, msg);
    }
    // ref LogMessageW (ref:98-100) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
    // Ported from: ref:101-106
    void LogMessageV(SEVERITY sev, Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, sev, fmt, args);
        va_end(args);
    }
    // ref LogMessageVW (ref:107-112) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)

    // Ported from: ref:114-116
    bool isSeverityEnabled(SEVERITY sev) const {
        return Logging::isSeverityEnabled(m_category, sev);
    }
    // ref message(SEVERITY, WCharCP) (ref:117-119) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
    // Ported from: ref:120-122
    void message(SEVERITY sev, Utf8CP msg) const {
        LogMessage(sev, msg);
    }
    // ref messagev(SEVERITY, WCharCP, ...) (ref:123-128) omitted: DanQing is UTF-8 (§7.2 POSIX adaptation)
    // Ported from: ref:129-134
    void messagev(SEVERITY sev, Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, sev, fmt, args);
        va_end(args);
    }

    // Ported from: ref:139-141
    void fatal(Utf8CP msg) const {
        LogMessage(LOG_FATAL, msg);
    }
    // Ported from: ref:148-153
    void fatalv(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_FATAL, fmt, args);
        va_end(args);
    }

    // Ported from: ref:158-160
    void error(Utf8CP msg) const {
        LogMessage(LOG_ERROR, msg);
    }
    // Ported from: ref:167-172
    void errorv(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_ERROR, fmt, args);
        va_end(args);
    }

    // Ported from: ref:177-179
    void warning(Utf8CP msg) const {
        LogMessage(LOG_WARNING, msg);
    }
    // Ported from: ref:186-191
    void warningv(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_WARNING, fmt, args);
        va_end(args);
    }

    // Ported from: ref:195-197
    void info(Utf8CP msg) const {
        LogMessage(LOG_INFO, msg);
    }
    // Ported from: ref:204-209
    void infov(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_INFO, fmt, args);
        va_end(args);
    }

    // Ported from: ref:213-215
    void debug(Utf8CP msg) const {
        LogMessage(LOG_DEBUG, msg);
    }
    // Ported from: ref:222-227
    void debugv(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_DEBUG, fmt, args);
        va_end(args);
    }

    // Ported from: ref:231-233
    void trace(Utf8CP msg) const {
        LogMessage(LOG_TRACE, msg);
    }
    // Ported from: ref:240-245
    void tracev(Utf8CP fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        Logging::LogMessageVa(m_category, LOG_TRACE, fmt, args);
        va_end(args);
    }
};

} // namespace NativeLogging

END_DQ_BASE_NAMESPACE
