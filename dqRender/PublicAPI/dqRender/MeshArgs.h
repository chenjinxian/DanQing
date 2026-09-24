// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — MeshArgs and PolylineArgs
//
// Ported from: itwinjs-core core/frontend/src/render/MeshArgs.ts
//              core/frontend/src/render/PolylineArgs.ts
// Arguments for creating triangle meshes and polylines.
#pragma once

#include "Export.h"
#include "RenderMaterial.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureIndex.h>
#include <dqCommon/FillFlags.h>
#include <dqCommon/Image.h>
#include <dqCommon/LinePixels.h>
#include <dqCommon/QPoint.h>

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// Oct-encoded normal (2 bytes per normal).
// Ported from: itwinjs-core OctEncodedNormal
using OctEncodedNormal = uint16_t;

// Polyline flags.
// Ported from: itwinjs-core PolylineFlags
enum class PolylineFlags : uint8_t {
    None = 0,
    Edge = 1 << 0,
    Silhouette = 1 << 1,
    Planar = 1 << 2,
    Undefined = 1 << 7,
};

// Indices for a single polyline (array of vertex indices).
using PolylineIndices = std::vector<uint32_t>;

// Texture mapping arguments for a mesh.
struct MeshTextureMapping {
    RenderTexture* texture = nullptr;
    std::vector<double> uParams;  // per-vertex U coordinates
    std::vector<double> vParams;  // per-vertex V coordinates
    bool useConstantLod = false;
};

// Arguments for creating a triangle mesh.
// Ported from: itwinjs-core MeshArgs
struct DQ_RENDER_EXPORT MeshArgs {
    // Triangle indices (each consecutive 3 = one triangle).
    std::vector<uint32_t> vertIndices;

    // Vertex positions (quantized).
    dqCommon::QPoint3dList points;

    // Per-vertex normals (oct-encoded).
    std::vector<OctEncodedNormal> normals;

    // Color(s) of the mesh.
    dqCommon::ColorIndex colors;

    // Features contained in the mesh.
    dqCommon::FeatureIndex features;

    // Fill flags for planar regions in wireframe.
    dqCommon::FillFlags fillFlags = dqCommon::FillFlags::ByView;

    // Whether the mesh represents a planar region.
    bool isPlanar = false;

    // Whether the mesh is 2D (all points have same z).
    bool is2d = false;

    // Whether the mesh has baked lighting.
    bool hasBakedLighting = false;

    // Material applied to the mesh.
    RenderMaterial* material = nullptr;

    // Texture mapping.
    std::optional<MeshTextureMapping> textureMapping;
};

// Arguments for creating polylines (line strings or point strings).
// Ported from: itwinjs-core PolylineArgs
struct DQ_RENDER_EXPORT PolylineArgs {
    // Color(s) of the vertices.
    dqCommon::ColorIndex colors;

    // Features contained in the polylines.
    dqCommon::FeatureIndex features;

    // Width of lines or radius of points, in pixels.
    int width = 1;

    // Pixel pattern for line strings.
    dqCommon::LinePixels linePixels = dqCommon::LinePixels::Solid;

    // Flags describing how to draw.
    PolylineFlags flags = PolylineFlags::None;

    // Vertex positions (quantized).
    dqCommon::QPoint3dList points;

    // Set of polylines (each entry = series of vertex indices).
    std::vector<PolylineIndices> polylines;
};

END_DQ_RENDER_NAMESPACE
