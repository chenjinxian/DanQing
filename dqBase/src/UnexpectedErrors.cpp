// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/UnexpectedErrors.ts
// DanQing dqBase — 全局错误处理器实现
#include "dqBase/UnexpectedErrors.h"

#include "dqBase/Logger.h"
#include "dqBase/DqTypes.h"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

namespace {

// Default handler is ErrorLog, matching the reference's
//   private static _handler = this.errorLog;
UnexpectedErrors::ErrorHandler g_handler = &UnexpectedErrors::ErrorLog;

DqVector<UnexpectedErrors::ErrorHandler>& TelemetryTrackers() {
    static DqVector<UnexpectedErrors::ErrorHandler> trackers;
    return trackers;
}

} // namespace

UnexpectedErrors::ErrorHandler UnexpectedErrors::SetHandler(ErrorHandler handler) {
    ErrorHandler previous = std::move(g_handler);
    g_handler = std::move(handler);
    return previous;
}

void UnexpectedErrors::Handle(const std::string& error, bool notifyTelemetry) {
    if (g_handler) {
        g_handler(error);
    }

    if (notifyTelemetry) {
        // Take a snapshot — a tracker may itself call Handle (recursion) or
        // call its own remover during fan-out. Iterating by index over a copy
        // of the vector is the equivalent of the reference's
        // `this._telemetry.forEach`; trackers that fail (the reference wraps
        // each in try/catch) are isolated by the no-exceptions invariant —
        // there is nothing to throw, so fan-out continues uninterrupted.
        auto& trackers = TelemetryTrackers();
        DqVector<UnexpectedErrors::ErrorHandler> snapshot(trackers);
        for (auto& tracker : snapshot) {
            if (tracker) {
                tracker(error);
            }
        }
    }
}

UnexpectedErrors::TelemetryRemover UnexpectedErrors::AddTelemetry(ErrorHandler tracker) {
    auto& trackers = TelemetryTrackers();
    auto pos = static_cast<std::size_t>(trackers.size());
    trackers.push_back(std::move(tracker));
    return [pos]() {
        auto& t = TelemetryTrackers();
        if (pos < t.size()) {
            t[pos] = nullptr; // null out; preserve indices of other trackers
        }
    };
}

void UnexpectedErrors::ConsoleLog(const std::string& error) {
    // Ported from: itwinjs-core UnexpectedErrors.consoleLog -> console.error(e)
    fprintf(stderr, "UnexpectedError: %s\n", error.c_str());
}

void UnexpectedErrors::ErrorLog(const std::string& error) {
    // Ported from: itwinjs-core UnexpectedErrors.errorLog -> Logger.logError("unhandled", e)
    Logger::LogError("unhandled", error.c_str());
}

void UnexpectedErrors::ReThrowImmediate(const std::string& error) {
    // -fno-exceptions adaptation of ref reThrowImmediate (`throw e`).
    // See header top-of-file note: log a fatal, then abort.
    Logger::LogError("unhandled", error.c_str());
    fprintf(stderr, "FATAL (ReThrowImmediate): %s\n", error.c_str());
    std::abort();
}

void UnexpectedErrors::ReThrowDeferred(const std::string& error) {
    // -fno-exceptions adaptation of ref reThrowDeferred
    // (`setTimeout(() => { throw e; }, 0)`). The JS deferred-throw has no
    // C++ analog under -fno-exceptions; we log a fatal error so it is
    // observable. See header top-of-file note.
    Logger::LogError("unhandled", error.c_str());
}

END_DQ_BASE_NAMESPACE
