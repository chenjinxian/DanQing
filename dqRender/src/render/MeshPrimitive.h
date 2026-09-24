// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/MeshPrimitive.ts
// DanQing dqRender — MeshPrimitiveType + Point3dList + MeshPointList
//
// 保真依据：逐类型移植 MeshPrimitive.ts。MeshPrimitiveType 枚举（Mesh/Polyline/Point）；
// Point3dList = Point3d[] + range + add（DanQing struct）。MeshPointList = Point3dList | QPoint3dList（联合），
// DanQing 暂用 Point3dList 别名（QPoint 量化路径在 MeshBuilderMap quantizePositions，PR E）。@internal。
#pragma once

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 MeshPrimitiveType (MeshPrimitive.ts:13-17).
enum class MeshPrimitiveType : uint8_t {
    Mesh = 0,
    Polyline = 1,
    Point = 2,
};

// A Point3d[] with an add method (compatible with QPoint3dList) plus the range enclosing all points.
// 1:1 Point3dList (MeshPrimitive.ts:25-30). The Mesh ctor (MeshPrimitives.ts:166-170, non-quantized
// path) assigns a per-instance `add` that recenters each point about `range.center`; DanQing encodes
// that as a `center` offset (default zero = identity, matching a fresh standalone Point3dList).
struct Point3dList {
    std::vector<dqGeom::Point3d> points;
    dqGeom::Range3d range;
    dqGeom::Point3d center = dqGeom::Point3d::FromZero();  // 1:1 Mesh ctor closure: add → push(pt - center)

    void add(const dqGeom::Point3d& point)
    {
        points.push_back(dqGeom::Point3d::From(point.x - center.x, point.y - center.y, point.z - center.z));
    }
    size_t length() const noexcept { return points.size(); }
};

// 1:1 MeshPointList (MeshPrimitive.ts:36) — Point3dList | QPoint3dList. DanQing uses the Point3dList
// variant (QPoint quantization path belongs to MeshBuilderMap, PR E).
using MeshPointList = Point3dList;

END_DQ_RENDER_NAMESPACE
