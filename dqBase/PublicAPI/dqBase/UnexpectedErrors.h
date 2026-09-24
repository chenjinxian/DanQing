// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/UnexpectedErrors.ts
// DanQing dqBase — 全局错误处理器
//
// 1:1 对齐 itwinjs-core UnexpectedErrors. Reference surface:
//   - 4 predefined handlers: ConsoleLog / ErrorLog / ReThrowImmediate /
//     ReThrowDeferred (ref: reThrowImmediate / reThrowDeferred / consoleLog /
//     errorLog).
//   - AddTelemetry(tracker) returns a remover; telemetry fans out on Handle.
//   - Handle(error, notifyTelemetry = true): false suppresses telemetry.
//   - SetHandler(handler) RETURNS the previous handler (temporary swap).
//
// --- -fno-exceptions adaptation (CLAUDE.md §9: -fno-exceptions -fno-rtti) ---
// The reference's reThrowImmediate / reThrowDeferred THROW. DanQing is built with
// -fno-exceptions and therefore CANNOT throw. The two handlers are the
// reference's contract (an "immediate unrecoverable" path and a "deferred"
// path), so they are retained with adapted semantics:
//
//   * ReThrowImmediate -> Logger::LogError("unhandled", error) + std::abort().
//     Rationale: the reference's `throw e` aborts the synchronous call stack
//     for an unrecoverable error; in a no-exceptions C++ program the honest
//     equivalent is terminate (std::abort), preceded by a Logger error so the
//     failure is observable.
//
//   * ReThrowDeferred -> Logger::LogError("unhandled", error).
//     Rationale: the reference uses setTimeout(..., 0) to re-throw off the
//     current stack (a JS async concept). C++ has no built-in deferred-throw
//     analog under -fno-exceptions; we log the error so it is observable and
//     document that the "deferred throw" semantics do not transfer. Callers
//     that need deferred reporting should chain their own scheduler.
//
// HandleException(const std::exception&) was a §6 self-addition (the reference
// has no such method; exceptions are also banned by §9) and has been REMOVED.
#pragma once

#include "Export.h"

#include <functional>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// UnexpectedErrors — 全局错误处理器
// Ported from: itwinjs-core UnexpectedErrors.ts
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT UnexpectedErrors {
public:
    using ErrorHandler = std::function<void(const std::string& error)>;
    using TelemetryRemover = std::function<void()>;

    /// 建立/替换全局 *unexpected error* 处理器。
    /// 返回上一个处理器（用于临时替换后恢复）。
    /// 默认处理器为 ErrorLog。
    /// Ported from: itwinjs-core UnexpectedErrors.setHandler
    static ErrorHandler SetHandler(ErrorHandler handler);

    /// 报告一个 unexpected error，由全局处理器处理。
    /// notifyTelemetry=false 时不通知 telemetry 追踪器（例如来自第三方代码的异常）。
    /// Ported from: itwinjs-core UnexpectedErrors.handle(error, notifyTelemetry = true)
    static void Handle(const std::string& error, bool notifyTelemetry = true);

    /// 注册一个 telemetry 追踪器。返回的函数用于注销。
    /// 追踪器中报告的错误不会中断 fan-out（对齐 ref 的 try/catch 包裹）。
    /// Ported from: itwinjs-core UnexpectedErrors.addTelemetry
    static TelemetryRemover AddTelemetry(ErrorHandler tracker);

    // --- 预定义处理器（对齐 ref 的 4 个静态成员） ---
    /// 控制台输出（对齐 ref consoleLog: console.error）
    static void ConsoleLog(const std::string& error);

    /// Logger.logError("unhandled", error)
    /// Ported from: itwinjs-core UnexpectedErrors.errorLog
    static void ErrorLog(const std::string& error);

    /// ref reThrowImmediate 的 -fno-exceptions 适配：
    /// Logger::LogError + std::abort（详见文件顶部适配说明）。
    static void ReThrowImmediate(const std::string& error);

    /// ref reThrowDeferred 的 -fno-exceptions 适配：
    /// Logger::LogError（"deferred-throw" 在 -fno-exceptions 下无对应语义；详见文件顶部）。
    static void ReThrowDeferred(const std::string& error);
};

END_DQ_BASE_NAMESPACE
