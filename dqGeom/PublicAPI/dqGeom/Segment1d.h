// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Segment1d (1D interval with fractional interpolation)
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Segment1d.ts
// 最小子集（PolyfaceBuilder.addUVGridBody 的 uMap/vMap 用）：create/fractionToPoint/
// shift/set。其余（clip/negate/...）Phase-N 待移植。
#pragma once

#include "Geometry.h"
#include "DqGeom.h"

BEGIN_DQ_GEOM_NAMESPACE

// Segment1d — a 1D interval [x0, x1] (1:1 Segment1d.ts:30-53 字段/工厂)
struct DQ_GEOM_EXPORT Segment1d {
    double x0 = 0.0;
    double x1 = 1.0;

    Segment1d() noexcept = default;
    Segment1d(double x0_, double x1_) noexcept : x0(x0_), x1(x1_) {}

    /// Ported from: Segment1d.ts:41 set
    void Set(double x0_, double x1_) noexcept { x0 = x0_; x1 = x1_; }
    /// Ported from: Segment1d.ts:46 shift
    void Shift(double dx) noexcept { x0 += dx; x1 += dx; }
    /// Ported from: Segment1d.ts:53 create
    static Segment1d Create(double x0_ = 0.0, double x1_ = 1.0) noexcept { return Segment1d(x0_, x1_); }
    /// Ported from: Segment1d.ts:80 fractionToPoint — Geometry.interpolate(x0, fraction, x1)
    double FractionToPoint(double fraction) const noexcept { return interpolate(x0, fraction, x1); }
};

END_DQ_GEOM_NAMESPACE
