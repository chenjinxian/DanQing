// Ported from: itwinjs-core core/frontend/src/common/internal/render/VertexTableBuilder.ts
//                - Unquantized.PolylineBuilder        (:508-527)
//                - Unquantized.SimpleBuilder          (:422-506)
//                - appendTransposePosAndFeatureNdx    (:455-479)
//                - convertFloat32                     (:450-453)
//                - computePolylineCumulativeDistances (:32-75)
//                - build / buildFromPolylines         (:178-217)
//                - appendColorTable                   (:118-124, 190)
//              + core/frontend/src/common/internal/render/VertexTable.ts
//                - computeDimensions                  (:53-81)
// SPDX-License-Identifier: Apache-2.0

#include "render/VertexTableBuilder.h"

#include <dqBase/DqTypes.h>
#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>

#include <cmath>
#include <cstring>

namespace {
// Local write cursor used while packing vertices into the RGBA8 buffer.
// Ported from: VertexTableBuilder.ts _curIndex (:109) + advance/append8/
// append16/append32 (:126-147). The reference uses asserts to check bounds;
// we rely on the pre-sized buffer (data.assign(width*height*4, 0)).
struct Cursor {
    dqBase::DqVector<uint8_t>& data;
    uint32_t index = 0;
    explicit Cursor(dqBase::DqVector<uint8_t>& d) : data(d) {}

    // Ported from: VertexTableBuilder.ts advance (:126-130).
    void advance(uint32_t nBytes) { index += nBytes; }
    // Ported from: VertexTableBuilder.ts append8 (:132-139).
    void append8(uint8_t v) { data[index++] = v; }
    // Ported from: VertexTableBuilder.ts append16 (:140-143) -- little-endian.
    void append16(uint16_t v) {
        append8(static_cast<uint8_t>(v & 0x00ffu));
        append8(static_cast<uint8_t>((v >> 8) & 0x00ffu));
    }
    // Ported from: VertexTableBuilder.ts append32 (:144-147) -- little-endian.
    void append32(uint32_t v) {
        append16(static_cast<uint16_t>(v & 0x0000ffffu));
        append16(static_cast<uint16_t>((v >> 16) & 0x0000ffffu));
    }
};
} // namespace

BEGIN_DQ_RENDER_NAMESPACE

// Ported from: VertexTable.ts computeDimensions (:53-81).
void VertexTableBuilder::computeDimensions(uint32_t nEntries, uint32_t nRgbaPerEntry,
                                           uint32_t nExtraRgba, uint32_t maxSize,
                                           uint32_t& width, uint32_t& height) {
    // Reference: nRgba = Math.ceil(nEntries * nRgbaPerEntry) + nExtraRgba.
    // nEntries*nRgbaPerEntry is exact in uint32; Math.ceil is a no-op for integers.
    const uint32_t nRgba = nEntries * nRgbaPerEntry + nExtraRgba;

    if (nRgba <= maxSize) {
        width = nRgba;
        height = 1u;
        return;
    }

    // Make roughly square to reduce unused space in last row.
    uint32_t w = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(nRgba))));

    // Ensure a given entry's RGBA values all fit on the same row.
    const uint32_t remainder = w % nRgbaPerEntry;
    if (remainder != 0u)
        w += nRgbaPerEntry - remainder;

    // Compute height.
    const uint32_t h = static_cast<uint32_t>(std::ceil(static_cast<double>(nRgba) / static_cast<double>(w)));

    width = w;
    height = h;
}

// Ported from: VertexTableBuilder.ts convertFloat32 (:450-453).
//   const u32Array = new Uint32Array(1);
//   const f32Array = new Float32Array(u32Array.buffer);
//   f32Array[0] = val; return u32Array[0];
// Reinterprets the IEEE-754 single-precision bit pattern as uint32 (no
// numeric conversion). In C++ this is std::memcpy (the TS reinterpret is via
// a shared ArrayBuffer; memcpy is the standard-compliant equivalent).
uint32_t VertexTableBuilder::convertFloat32(float val) noexcept {
    uint32_t u = 0u;
    std::memcpy(&u, &val, 4u);
    return u;
}

// Ported from: VertexTableBuilder.ts computePolylineCumulativeDistances (:32-75).
// Specialised for a single sequential polyline [0..numVertices-1] (the ACS
// thick-line case). The reference walks `args.polylines[i]` index lists and
// keeps the first assignment per vertex (NaN guard); here every vertex is
// visited exactly once by the single sequential line, so the first-assignment
// rule is equivalent to direct assignment. Unvisited vertices (none in this
// sequential specialisation) would be set to 0 -- the loop below initialises
// everything to 0 first to mirror that reference behaviour exactly.
DqVector<float> VertexTableBuilder::computePolylineCumulativeDistances(
    dqGeom::Point3d const* points, uint32_t numVertices) {
    DqVector<float> cumDist(numVertices, 0.0f);  // reference: NaN fill, then NaN->0.

    if (numVertices < 2u)
        return cumDist;  // reference: line.length < 2 -> continue (no distance emitted).

    // Single sequential polyline = [0, 1, ..., numVertices-1].
    // Ported from: the reference inner loop (:54-66) specialised to one line.
    double dist = 0.0;
    // cumDist[0] already 0 (matches reference: first vertex of each line -> dist=0).
    for (uint32_t i = 1u; i < numVertices; ++i) {
        dist += points[i - 1u].Distance(points[i]);
        cumDist[i] = static_cast<float>(dist);
    }
    return cumDist;
}

// Ported from: VertexTableBuilder.ts buildFromPolylines (:210-217) +
//              build (:178-208) +
//              Unquantized.PolylineBuilder.appendVertex (:518-521) +
//              Unquantized.SimpleBuilder.appendTransposePosAndFeatureNdx (:455-479) +
//              Unquantized.SimpleBuilder.appendColorIndex (:502-505, uniform path) +
//              Unquantized.PolylineBuilder.appendCumulativeDistance (:523-526) +
//              appendColorTable (:190 -- no-op for uniform color).
//
// The reference routes the call through createPolylineBuilder + a builder
// instance with virtual appendVertex; here we inline the Unquantized.
// PolylineBuilder path (the only path required by the ACS use case) into the
// static entry point, preserving byte-for-byte output.
BuiltVertexTable VertexTableBuilder::buildFromPolylines(dqGeom::Point3d const* points,
                                                       uint32_t numVertices,
                                                       dqCommon::ColorDef const& uniformColor,
                                                       uint32_t maxDimension) {
    (void)uniformColor;  // uniform color is resolved by caller via u_color; not
                          // stored in the table (matches reference
                          // `uniformColor: colorIndex.uniform`).

    BuiltVertexTable result;
    result.numVertices = numVertices;
    result.numRgbaPerVertex = 6u;          // Unquantized.PolylineBuilder override (:516).
    result.usesUnquantizedPositions = true;  // Unquantized.* -> true (:437).
    result.hasNonUniformColor = false;     // uniform color path: no color table.

    // build (:178-208): computeDimensions(numVertices, numRgbaPerVertex,
    // numColors=0 [colorIndex.isUniform -> 0], maxDimension).
    const uint32_t numColors = 0u;  // reference: colorIndex.isUniform ? 0 : numColors.
    uint32_t width = 0u, height = 0u;
    computeDimensions(numVertices, result.numRgbaPerVertex, numColors, maxDimension, width, height);
    result.width = width;
    result.height = height;

    // Reference: const data = new Uint8Array(width * height * 4).
    result.data.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0u);

    // Pre-compute cumulative distances for the 6th texel (appendCumulativeDistance).
    const DqVector<float> cumDist = computePolylineCumulativeDistances(points, numVertices);

    // Reference: for (let i = 0; i < numVertices; i++) this.appendVertex(i);
    Cursor cursor(result.data);
    for (uint32_t v = 0u; v < numVertices; ++v) {
        // --- Unquantized.SimpleBuilder.appendTransposePosAndFeatureNdx (:455-479) ---
        // transpose position xyz vals into [0].xyz - [3].xyz, and add feature
        // index at .w -- this is to order things to let shader code access much
        // more efficiently.
        const dqGeom::Point3d pt = points[v];
        const uint32_t x = convertFloat32(static_cast<float>(pt.x));
        const uint32_t y = convertFloat32(static_cast<float>(pt.y));
        const uint32_t z = convertFloat32(static_cast<float>(pt.z));
        // featID: reference reads args.features.featureIDs[vertIndex] or 0
        // (uniform feature path). ACS uses uniform feature -> featID = 0.
        const uint32_t featID = 0u;

        cursor.append8(static_cast<uint8_t>(x & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>(y & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>(z & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>(featID & 0x000000ffu));

        cursor.append8(static_cast<uint8_t>((x >> 8) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((y >> 8) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((z >> 8) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((featID >> 8) & 0x000000ffu));

        cursor.append8(static_cast<uint8_t>((x >> 16) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((y >> 16) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((z >> 16) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((featID >> 16) & 0x000000ffu));

        cursor.append8(static_cast<uint8_t>((x >> 24) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((y >> 24) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((z >> 24) & 0x000000ffu));
        cursor.append8(static_cast<uint8_t>((featID >> 24) & 0x000000ffu));

        // --- Unquantized.SimpleBuilder.appendColorIndex (:502-505) ---
        // _appendColorIndex (:495-500): uniform color -> advance(2); else
        // append16(colorIndex). appendColorIndex then advance(2) more (unused).
        // Uniform path total: advance(4) -- texel 4 left zero.
        cursor.advance(4u);

        // --- Unquantized.PolylineBuilder.appendCumulativeDistance (:523-526) ---
        // const dist = vertIndex < this._cumDist.length ? this._cumDist[vertIndex] : 0.0;
        // this.append32(floatToUint32(dist));
        const float d = (v < cumDist.size()) ? cumDist[v] : 0.0f;
        cursor.append32(convertFloat32(d));
    }

    // Reference: this.appendColorTable(colorIndex) (:190) -- for uniform color
    // (colorIndex.nonUniform === undefined) the table is empty, no bytes
    // appended. hasNonUniformColor stays false.

    return result;
}

END_DQ_RENDER_NAMESPACE
