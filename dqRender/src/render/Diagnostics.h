// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Conditional diagnostic utilities
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Diagnostics.ts
//
// Static Debug class providing gated diagnostic output and expression
// evaluation.  All facilities are disabled by default; they must be
// explicitly enabled via the static flags.
#pragma once

#include <cstdio>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Debug — conditional diagnostic utilities
// (Ported from: itwinjs-core Diagnostics.ts Debug)
//
// In itwinjs, print() takes a lazy lambda and evaluate() takes a lazy lambda.
// In C++ we use templates to preserve lazy evaluation — the lambda is only
// called when the corresponding flag is true.
// ---------------------------------------------------------------------------
class Debug {
public:
    /// When true, Debug::print() produces output.
    static bool sPrintEnabled;

    /// When true, Debug::evaluate() actually calls the evaluation function.
    static bool sEvaluateEnabled;

    /// If sPrintEnabled is true, output the message to stderr.
    /// The message parameter is a callable returning const char*.
    /// Ported from: itwinjs-core Diagnostics.ts Debug.print
    template <typename TMessage>
    static void print(TMessage const& message)
    {
        if (sPrintEnabled)
            std::fprintf(stderr, "%s\n", message());
    }

    /// Convenience overload for plain C strings.
    static void print(char const* message);

    /// If sEvaluateEnabled is true, call eval() and return its result;
    /// otherwise return defaultValue without evaluating.
    /// Ported from: itwinjs-core Diagnostics.ts Debug.evaluate
    template <typename TFunc>
    static auto evaluate(TFunc eval, decltype(eval()) defaultValue)
        -> decltype(eval())
    {
        return sEvaluateEnabled ? eval() : defaultValue;
    }

    /// Returns true if the currently-bound framebuffer is complete.
    /// When sEvaluateEnabled is false, returns true (assumes complete).
    /// Ported from: itwinjs-core Diagnostics.ts Debug.isValidFrameBuffer
    static bool isValidFrameBuffer();
};

END_DQ_RENDER_NAMESPACE
