// Ported from: itwinjs-core core/frontend/src/common/internal/render/VertexTableBuilder.ts
//                - Unquantized.PolylineBuilder        (:508-527)
//                - Unquantized.SimpleBuilder          (:422-506)
//                - appendTransposePosAndFeatureNdx    (:455-479)
//                - convertFloat32                     (:450-453)
//                - computePolylineCumulativeDistances (:32-75)
//                - build / buildFromPolylines         (:178-217)
//              + core/frontend/src/common/internal/render/VertexTable.ts
//                - computeDimensions                  (:53-81)
// SPDX-License-Identifier: Apache-2.0
//
// Faithful 1:1 C++ port of itwinjs's unquantized polyline VertexTable packer.
// Packs each polyline vertex into 6 RGBA8 texels (24 bytes) with the position
// TRANSPOSED (SoA byte-interleave) across the first 4 texels -- the layout the
// GPU shader samples from u_vertLUT (spec §3.3 / Vertex.ts
// computeUnquantizedPositionFromLUT). Pure CPU; no GL/RHI/Qt.
//
// Layout (per vertex, 6 texels = 24 bytes):
//   texel 0 (bytes  0..3):  x.byte0,  y.byte0,  z.byte0,  featID.byte0
//   texel 1 (bytes  4..7):  x.byte1,  y.byte1,  z.byte1,  featID.byte1
//   texel 2 (bytes  8..11): x.byte2,  y.byte2,  z.byte2,  featID.byte2
//   texel 3 (bytes 12..15): x.byte3,  y.byte3,  z.byte3,  featID.byte3
//   texel 4 (bytes 16..19): colorIndex.lo, colorIndex.hi, unused, unused
//                            (uniform color path: left zero -- u_color resolves)
//   texel 5 (bytes 20..23): cumulative distance (float32 LE)
//
// C++ adaptations (§3.4): TS `_field` -> `m_field`; TS `Point3d[]` ->
// `Point3d const* + count`; TS `Float32Array`/`Uint32Array` reinterpret ->
// `std::memcpy` of the IEEE-754 bits (NOT a cast); TS class hierarchy folded
// into a single static entry point (only the uniform-color PolylineBuilder
// path is required for the ACS thick-line use case; MeshBuilder/Quantized/
// non-uniform color remain reference-shaped TODOs for future tasks).
#pragma once

#include <dqBase/DqTypes.h>      // DqVector
#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Bring DqVector (dqBase thin wrapper for std::vector) into this namespace so
// the cross-task contract reads verbatim (`DqVector<uint8_t>`).
using dqBase::DqVector;

// Ported from: itwinjs-core VertexTableBuilder.ts Unquantized.PolylineBuilder
// + VertexTable.ts VertexTable interface.
//
// Cross-task contract consumed by Task 5 (MeshGraphic) -> VertexLutTexture::create.
// Field byte-layout is fixed: 6 RGBA texels (24 bytes) per vertex, position
// transposed SoA across texels 0..3, texel 4 = colorIndex (left zero for the
// uniform color path), texel 5 = cumulative distance (float32 LE).
struct BuiltVertexTable {
    DqVector<uint8_t> data;             // width*height*4 bytes, RGBA8, transposed layout
    uint32_t width = 0;                 // in RGBA units; divisible by numRgbaPerVertex
    uint32_t height = 0;                // rows
    uint32_t numRgbaPerVertex = 6;      // PolylineBuilder override (SimpleBuilder=5)
    uint32_t numVertices = 0;
    bool usesUnquantizedPositions = true;   // Unquantized.* -> always true
    bool hasNonUniformColor = false;        // false for ACS uniform color path
    // uniformColor is resolved by the caller via u_color; not stored in the
    // table for the uniform path (matches reference `uniformColor: colorIndex.uniform`).
};

// Ported from: itwinjs-core VertexTableBuilder.ts class VertexTableBuilder
// (buildFromPolylines :210-217 + Unquantized.PolylineBuilder :508-527).
//
// Single static entry point that specialises the reference's generic
// PolylineArgs-based builder to the uniform-color sequential-polyline case
// required by the ACS thick-line port. The byte layout, computeDimensions,
// convertFloat32, and computePolylineCumulativeDistances routines are 1:1
// ports; only the API shape is folded (reference polymorphism -> one function).
class VertexTableBuilder {
public:
    // Pack one uniform-color polyline's unique vertices into the transposed LUT layout.
    // Ported from: VertexTableBuilder.ts buildFromPolylines + Unquantized.PolylineBuilder
    //              (numRgbaPerVertex=6, appendTransposePosAndFeatureNdx,
    //               appendColorIndex[uniform->advance], appendCumulativeDistance).
    // `points`         = the sequential polyline vertex pool [0..numVertices-1];
    // `numVertices`    = number of vertices in the single sequential polyline;
    // `uniformColor`   = color resolved by caller via u_color (no color table
    //                     appended; hasNonUniformColor=false);
    // `maxDimension`   = max texture edge (itwinjs IModelApp.renderSystem.maxTextureSize
    //                     default 2048 -- VertexTable.test.ts uses this).
    static BuiltVertexTable buildFromPolylines(dqGeom::Point3d const* points, uint32_t numVertices,
                                               dqCommon::ColorDef const& uniformColor,
                                               uint32_t maxDimension = 2048u);

private:
    // Ported from: VertexTable.ts computeDimensions (:53-81).
    // Picks (width, height) so width*height >= nEntries*nRgbaPerEntry + nExtraRgba
    // and width % nRgbaPerEntry == 0 (a vertex's texels never wrap rows).
    static void computeDimensions(uint32_t nEntries, uint32_t nRgbaPerEntry,
                                  uint32_t nExtraRgba, uint32_t maxSize,
                                  uint32_t& width, uint32_t& height);

    // Ported from: VertexTableBuilder.ts convertFloat32 (:450-453).
    // Reinterprets IEEE-754 float bits as uint32 via memcpy (no conversion).
    static uint32_t convertFloat32(float val) noexcept;

    // Ported from: VertexTableBuilder.ts computePolylineCumulativeDistances (:32-75).
    // Sequential-chain specialization (single polyline [0..numVertices-1]):
    // cumDist[0]=0; cumDist[i]=cumDist[i-1]+|p[i]-p[i-1]|.
    static DqVector<float> computePolylineCumulativeDistances(dqGeom::Point3d const* points,
                                                              uint32_t numVertices);
};

END_DQ_RENDER_NAMESPACE
