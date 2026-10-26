// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text annotation geometry assembly
// Ported from: itwinjs-core core/backend/src/annotations/TextAnnotationGeometry.ts
//
// 参考的 ElementGeometry.Builder 面 = DanQing GeometryStreamBuilder（§3.4 载体
// 适配）；GeometryParams → GeometryAppearanceProps（elmPriority→displayPriority
// 映射——2d z 序面，DanQing 3d 常渲染无 2d priority 通道，登记）。debug 臂的
// PointString3d（dqGeom 缺）以每点十字短线视觉等价承载（TODO 随 PointString3d
// 落地复核）。
#pragma once

#include "DqCommon.h"
#include "FrameGeometry.h"
#include "LeaderGeometry.h"
#include "TextAnnotation.h"
#include "TextBlockLayout.h"

BEGIN_DQ_COMMON_NAMESPACE

// Render priorities for different parts of a text annotation.
// Ported from: itwinjs-core RenderPriority (TextAnnotationGeometry.ts:26-29)
struct RenderPriority {
    std::optional<int> annotation;
    std::optional<int> annotationLabels;
};

// Properties required to compute the geometry of a text annotation.
// Ported from: itwinjs-core AppendTextAnnotationGeometryArgs (:35-54) —
// annotationProps 携实例（Props EQUIVALENCE 同 TextBlock.h 注）。
struct AppendTextAnnotationGeometryArgs {
    TextAnnotation const* annotation = nullptr;  // 需含带 runs 的 TextBlock
    TextBlockLayout const* layout = nullptr;
    TextStyleResolver const* textStyleResolver = nullptr;
    double scaleFactor = 1.0;
    GeometryStreamBuilder* builder = nullptr;
    dqBase::DqId categoryId;
    std::optional<dqBase::DqId> subCategoryId;
    bool wantDebugGeometry = false;
    RenderPriority renderPriority;
};

// Constructs the TextBlockGeometry + frame + leaders and appends to the builder.
// Ported from: itwinjs-core appendTextAnnotationGeometry (:60-114)
DQ_COMMON_EXPORT bool appendTextAnnotationGeometry(AppendTextAnnotationGeometryArgs& args);

END_DQ_COMMON_NAMESPACE
