// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — BilinearPatch (双线性四角面片，UVSurface)
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/BilinearPatch.ts
// 最小子集（PolyfaceBuilder.addBox 的六面 UV-grid 用）：create/uvFractionToPoint/
// uvFractionToPointAndTangents。其余（射线求交/法线曲线等）Phase-N 待移植。
#pragma once

#include "GeometryHandler.h"   // UVSurface
#include "Plane3dByOriginAndVectors.h"
#include "Point3d.h"
#include "DqGeom.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// BilinearPatch — 1:1 BilinearPatch.ts:50-66（字段 + create）
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT BilinearPatch : public UVSurface {
public:
    Point3d point00;   // corner at parametric coordinate (0,0)
    Point3d point10;   // corner at parametric coordinate (1,0)
    Point3d point01;   // corner at parametric coordinate (0,1)
    Point3d point11;   // corner at parametric coordinate (1,1)

    BilinearPatch(Point3d const& p00, Point3d const& p10, Point3d const& p01, Point3d const& p11) noexcept
        : point00(p00), point10(p10), point01(p01), point11(p11) {}

    /// Ported from: BilinearPatch.ts:77 create（clone 语义由值拷贝承载）
    static BilinearPatch Create(Point3d const& p00, Point3d const& p10, Point3d const& p01, Point3d const& p11) noexcept {
        return BilinearPatch(p00, p10, p01, p11);
    }

    /// Ported from: BilinearPatch.ts:146 uvFractionToPoint
    Point3d UVFractionToPoint(double u, double v) const override {
        double const f00 = (1.0 - u) * (1.0 - v);
        double const f10 = u * (1.0 - v);
        double const f01 = (1.0 - u) * v;
        double const f11 = u * v;
        return Point3d::From(
            f00 * point00.x + f10 * point10.x + f01 * point01.x + f11 * point11.x,
            f00 * point00.y + f10 * point10.y + f01 * point01.y + f11 * point11.y,
            f00 * point00.z + f10 * point10.z + f01 * point01.z + f11 * point11.z);
    }

    /// Ported from: BilinearPatch.ts:163 uvFractionToPointAndTangents
    Plane3dByOriginAndVectors UVFractionToPointAndTangents(double u, double v) const override {
        double const u0 = 1.0 - u;
        double const v0 = 1.0 - v;
        double const f00 = u0 * v0;
        double const f10 = u * v0;
        double const f01 = u0 * v;
        double const f11 = u * v;
        return Plane3dByOriginAndVectors::createCapture(   // ← createOriginAndVectors 等价（:149 非 result 分支）
            Point3d::From(
                f00 * point00.x + f10 * point10.x + f01 * point01.x + f11 * point11.x,
                f00 * point00.y + f10 * point10.y + f01 * point01.y + f11 * point11.y,
                f00 * point00.z + f10 * point10.z + f01 * point01.z + f11 * point11.z),
            // u derivative
            Vector3d::From(
                v0 * (point10.x - point00.x) + v * (point11.x - point01.x),
                v0 * (point10.y - point00.y) + v * (point11.y - point01.y),
                v0 * (point10.z - point00.z) + v * (point11.z - point01.z)),
            // v derivative
            Vector3d::From(
                u0 * (point01.x - point00.x) + u * (point11.x - point10.x),
                u0 * (point01.y - point00.y) + u * (point11.y - point10.y),
                u0 * (point01.z - point00.z) + u * (point11.z - point10.z)));
    }
};

END_DQ_GEOM_NAMESPACE
