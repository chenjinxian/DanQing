// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/Strokes.ts
// DanQing dqRender — StrokesPrimitive + point-list/list types (output of Geometry.getStrokes)
//
// 保真依据：逐类型移植 Strokes.ts。StrokesPrimitive 持 displayParams/isDisjoint/isPlanar +
// StrokesPrimitivePointLists（每条 stroke 一组 Point3d）。transform 用 Transform::MultiplyPoint3dArrayInPlace
// （PR A）。@internal，Geometry.getStrokes 返回 StrokesPrimitiveList（C2）。
#pragma once

#include "DisplayParams.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>

#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 StrokesPrimitivePointList (Strokes.ts:13-16).
struct StrokesPrimitivePointList {
    std::vector<dqGeom::Point3d> points;
    StrokesPrimitivePointList() = default;
    explicit StrokesPrimitivePointList(const std::vector<dqGeom::Point3d>& pts) : points(pts) {}
};

// 1:1 StrokesPrimitivePointLists (Array<StrokesPrimitivePointList>).
using StrokesPrimitivePointLists = std::vector<StrokesPrimitivePointList>;

// 1:1 StrokesPrimitive (Strokes.ts:26-48).
class StrokesPrimitive {
public:
    static StrokesPrimitive create(const DisplayParams& params, bool isDisjoint, bool isPlanar) {
        return StrokesPrimitive(params, isDisjoint, isPlanar);
    }

    DisplayParams displayParams;
    const bool isDisjoint;
    const bool isPlanar;
    StrokesPrimitivePointLists strokes;

    // 1:1 StrokesPrimitive.transform — multiply each stroke's points in place.
    void transform(const dqGeom::Transform& trans) {
        for (auto& strk : strokes)
            trans.MultiplyPoint3dArrayInPlace(strk.points);
    }

private:
    StrokesPrimitive(const DisplayParams& params, bool isDisjoint, bool isPlanar)
        : displayParams(params), isDisjoint(isDisjoint), isPlanar(isPlanar) {}
};

// 1:1 StrokesPrimitiveList (Array<StrokesPrimitive>).
using StrokesPrimitiveList = std::vector<StrokesPrimitive>;

END_DQ_RENDER_NAMESPACE
