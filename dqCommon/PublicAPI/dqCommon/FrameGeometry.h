// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text annotation frame geometry
// Ported from: itwinjs-core core/backend/src/annotations/FrameGeometry.ts
#pragma once

#include "DqCommon.h"
#include "GeometryStreamBuilder.h"
#include "TextStyle.h"

#include <dqGeom/Arc3d.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Path.h>
#include <dqGeom/Range3d.h>  // dqGeom::Range2d 宿主
#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/Transform.h>

#include <optional>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Arguments for computeFrame.
// Ported from: itwinjs-core ComputeFrameArgs (FrameGeometry.ts:65-72)
struct ComputeFrameArgs {
    // Frame shape to be calculated（None 臂由 appendFrameToBuilder 早退）。
    TextAnnotationFrameShape frame = TextAnnotationFrameShape::Rectangle;
    dqGeom::Range2d range;       // Range to be enclosed
    dqGeom::Transform transform; // Translates/rotates the range to world coordinates
};

// Frame geometry result——参考 computeFrame 返回 Loop | Path 联合（line 臂为
// Path，其余 Loop）；C++ 以查询指针 + isLoop 标签承载（§3.4 适配）。
// curves = 变换后子原语清单（参考 collectCurvePrimitives 的消费面——
// LeaderGeometry KeyPoint/Nearest/TextPoint 三臂与 computeIntervalPoints；
// 帧形曲线集封闭[LineString/Arc 解析臂]，List 化免去 CurveChain 泛型遍历缝）。
struct FrameGeometryResult {
    dqBase::RefPtr<dqGeom::GeometryQuery> geometry;
    bool isLoop = true;
    std::vector<dqBase::RefPtr<dqGeom::CurvePrimitive>> curves;
};

// Computes the frame geometry for a frame shape and range.
// Ported from: itwinjs-core computeFrame (FrameGeometry.ts:79-94)
DQ_COMMON_EXPORT FrameGeometryResult computeFrame(ComputeFrameArgs const& args);

// Computes points along the edges of the frame geometry (snapping/leader
// attachment).
// Ported from: itwinjs-core computeIntervalPoints (FrameGeometry.ts:113-124)
DQ_COMMON_EXPORT std::optional<std::vector<dqGeom::Point3d>> computeIntervalPoints(
    ComputeFrameArgs const& args, std::optional<double> lineIntervalFactor = 0.5,
    std::optional<double> arcIntervalFactor = 0.25);

// Based on the frame style, appends GeometryParams (line style) and the frame
// shape to the builder. DanQing builder = GeometryStreamBuilder（参考
// ElementGeometry.Builder 的对应面——appendGeometryParamsChange+appendGeometry）。
// Ported from: itwinjs-core appendFrameToBuilder (FrameGeometry.ts:22-59)
DQ_COMMON_EXPORT bool appendFrameToBuilder(GeometryStreamBuilder& builder,
                                           TextFrameStyleProps const& frame,
                                           dqGeom::Range2d const& range,
                                           dqGeom::Transform const& transform,
                                           GeometryAppearanceProps const& geomParams);

END_DQ_COMMON_NAMESPACE
