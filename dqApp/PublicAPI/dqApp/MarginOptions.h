// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Margin/padding options for view fitting (lookAt)
// Ported from: itwinjs-core core/frontend/src/ViewAnimation.ts (MarginOptions, :73-84)
//              + core/frontend/src/MarginPercent.ts (MarginPercent :22-30, PaddingPercent :45-50)
#pragma once

#include "Export.h"

#include <algorithm>
#include <optional>

namespace dqApp {

// Ported from: itwinjs-core PaddingPercent (MarginPercent.ts:45-50) — per-side padding
// fractions; may be negative (shrinks the view).
struct DQ_APP_EXPORT PaddingPercent {
    double left = 0.0;
    double right = 0.0;
    double top = 0.0;
    double bottom = 0.0;
};

// Ported from: itwinjs-core MarginPercent (MarginPercent.ts:22-30) — per-side margin
// fractions, each clamped to [0.0, 0.25] at construction (MarginPercent.ts:24).
struct DQ_APP_EXPORT MarginPercent {
    double left;
    double top;
    double right;
    double bottom;

    MarginPercent(double l, double t, double r, double b)
        : left(clampSide(l)), top(clampSide(t)), right(clampSide(r)), bottom(clampSide(b)) {}

private:
    // Ported from: itwinjs-core MarginPercent limitMargin (MarginPercent.ts:24).
    static double clampSide(double v) noexcept { return std::max(0.0, std::min(0.25, v)); }
};

// Ported from: itwinjs-core MarginOptions (ViewAnimation.ts:73-84). paddingPercent may be
// a uniform number or a per-side PaddingPercent; marginPercent is a MarginPercent.
// Precedence in lookAtViewAlignedVolume (ViewState.ts:1059-1095): paddingPercent wins.
struct DQ_APP_EXPORT MarginOptions {
    std::optional<double> paddingPercentUniform;    // paddingPercent: number
    std::optional<PaddingPercent> paddingPercent;   // paddingPercent: PaddingPercent
    std::optional<MarginPercent> marginPercent;

    // Ported from: itwinjs-core `undefined !== options?.paddingPercent` (ViewState.ts:1059).
    bool hasPaddingPercent() const noexcept {
        return paddingPercentUniform.has_value() || paddingPercent.has_value();
    }
};

}  // namespace dqApp
