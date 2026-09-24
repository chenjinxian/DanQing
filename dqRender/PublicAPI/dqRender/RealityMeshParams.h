// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RealityMeshParams (geometry for a reality mesh, type surface).
//
// Ported from: itwinjs-core core/frontend/src/render/RealityMeshParams.ts
//
// Type-only port: RealityMeshParams struct + RealityMeshParamsBuilderOptions.
// The Builder class (incremental construction) is intentionally NOT ported —
// it depends on Uint16ArrayBuilder / UintArrayBuilder / QPoint*BufferBuilder,
// none of which are ported yet (§6 — no invention). Logic methods on the
// RealityMeshParams namespace (fromGltfMesh / toPolyface) are TODO.
#pragma once

#include "Export.h"
#include "RenderMaterial.h"  // RenderTexture

#include <cstdint>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Faithful placeholders for unported core-common quantized-buffer types.
// §6: no half-implementation. TODO: replace with real QPoint3dBuffer /
// QPoint2dBuffer once core-common lands them.
// TODO: QPoint3dBuffer not yet ported (core-common).
struct QPoint3dBuffer {
    std::vector<uint16_t> points;  // raw quantized coords, 3 per vertex
    // quantization params elided — TODO when QPoint3dBuffer lands
};
// TODO: QPoint2dBuffer not yet ported (core-common).
struct QPoint2dBuffer {
    std::vector<uint16_t> points;  // raw quantized coords, 2 per vertex
    // quantization params elided — TODO when QPoint2dBuffer lands
};

// Geometry for a reality mesh submitted to the RenderSystem.
// Ported from: itwinjs-core RealityMeshParams (public interface)
struct DQ_RENDER_EXPORT RealityMeshParams {
    // 3d position of each vertex, indexed by `indices`.
    QPoint3dBuffer positions;

    // 2d texture coordinates of each vertex, indexed by `indices`.
    QPoint2dBuffer uvs;

    // Optional per-vertex normal as OctEncodedNormal (uint16).
    // Ported from: itwinjs-core RealityMeshParams.normals
    std::optional<std::vector<uint16_t>> normals;

    // Integer indices of each triangle. Length must be a multiple of 3.
    std::vector<uint32_t> indices;

    // alpha — unused by terrain meshes.
    uint32_t featureID = 0;

    // alpha — unused by terrain meshes.
    RenderTexture* texture = nullptr;

    // internal — MapLayer tile data, opaque (not ported).
    // TODO: LayerTileData not yet ported (core/frontend internal).
};

// Options to construct a RealityMeshParamsBuilder.
// Ported from: itwinjs-core RealityMeshParamsBuilderOptions (beta)
struct DQ_RENDER_EXPORT RealityMeshParamsBuilderOptions {
    // Bounding box fully containing all vertex positions (for quantization).
    // TODO: dqGeom Range3d not yet ported; use 6 doubles faithfully.
    double positionRangeMinX = 0.0, positionRangeMinY = 0.0, positionRangeMinZ = 0.0;
    double positionRangeMaxX = 0.0, positionRangeMaxY = 0.0, positionRangeMaxZ = 0.0;

    // Range fully containing all texture coords (for quantization). Default [0,1].
    // TODO: dqGeom Range2d not yet ported; use 4 doubles faithfully.
    double uvRangeMinX = 0.0, uvRangeMinY = 0.0;
    double uvRangeMaxX = 1.0, uvRangeMaxY = 1.0;

    // If true, RealityMeshParams.normals will be populated.
    bool wantNormals = false;

    // If defined, preallocate memory for this many vertices.
    std::optional<uint32_t> initialVertexCapacity;

    // If defined, preallocate memory for this many indices.
    std::optional<uint32_t> initialIndexCapacity;
};

// TODO: RealityMeshParamsBuilder not ported — depends on UintArrayBuilder /
// Uint16ArrayBuilder / QPoint3dBufferBuilder / QPoint2dBufferBuilder, none of
// which are ported into DanQing yet. Port together with those builders.

END_DQ_RENDER_NAMESPACE
