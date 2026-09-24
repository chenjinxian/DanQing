// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/multiclip/BuildAverageNormalsContext.ts
// DanQing dqGeom — BuildAverageNormalsContext (per-vertex normal averaging; @internal)
//
// 保真依据：移植 BuildAverageNormalsContext.ts（184 行）。buildFastAverageNormals：每 facet 算 areaNormal
// → 每个 sector 记 (facetData, sectorIndex, vertexIndex) → 按 vertexIndex 排序 → 同顶点近平行（angle ≤
// tolerance，默认 31°）的 facet 法向等权平均 → 写回 polyface.data.{normal,normalIndex}。buildPerFaceNormals：
// 每 facet 一个法向。@internal；PolyfaceQuery::BuildAverageNormals 消费。
#pragma once

#include "dqGeom/IndexedPolyface.h"

BEGIN_DQ_GEOM_NAMESPACE

// 1:1 BuildAverageNormalsContext (helper context; all static). @internal.
class BuildAverageNormalsContext {
public:
    // 1:1 buildFastAverageNormals(polyface, toleranceAngle). Averages near-parallel facet normals per vertex.
    static void BuildFastAverageNormals(IndexedPolyface& polyface, double toleranceAngleRadians);

    // 1:1 buildPerFaceNormals(polyface). One normal per facet (all facet vertices share it).
    static void BuildPerFaceNormals(IndexedPolyface& polyface);
};

END_DQ_GEOM_NAMESPACE
