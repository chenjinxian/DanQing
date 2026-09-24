// Ported from: itwinjs-core core/frontend/src/test/render/primitives/VertexTable.test.ts
//              describe("VertexLUT") > it("should produce correct VertexLUT.Params
//              from unquantized MeshArgs") (transposed byte verification).
//              + Authored round-trip / uniform-color / cumulative-distance tests
//              (no reference test exists in itwinjs-core for the PolylineBuilder's
//              6-texel layout or uniform-color-no-table path; permitted under
//              CLAUDE.md §5(f)).
// SPDX-License-Identifier: Apache-2.0
//
// Verifies VertexTableBuilder (port of VertexTableBuilder.ts Unquantized.
// PolylineBuilder) byte-for-byte against the reference test's transposed
// expected output, plus Authored invariants for the polyline-specific 6th
// texel (cumulative distance) and the uniform-color-no-color-table path.

#include "render/VertexTableBuilder.h"

#include <dqBase/DqTypes.h>
#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

using dqBase::DqVector;
using dqRender::BuiltVertexTable;
using dqRender::VertexTableBuilder;
using dqGeom::Point3d;

namespace {
// Re-implement the shader's unquantized gather on CPU to validate the pack
// (spec §3.5 / Vertex.ts computeUnquantizedPositionFromLUT). Reads 4 byte-
// interleaved texels and reconstructs the float32 by gathering byte i of each
// of x/y/z across the 4 texels.
static float gatherFloat(uint8_t const* vert, int byteOffsets[4]) {
    uint32_t u = static_cast<uint32_t>(vert[byteOffsets[0]])
               | (static_cast<uint32_t>(vert[byteOffsets[1]]) << 8)
               | (static_cast<uint32_t>(vert[byteOffsets[2]]) << 16)
               | (static_cast<uint32_t>(vert[byteOffsets[3]]) << 24);
    float f;
    std::memcpy(&f, &u, 4u);
    return f;
}
} // namespace

// Ported from: itwinjs-core core/frontend/src/test/render/primitives/VertexTable.test.ts
//              describe("VertexLUT") > it("should produce correct VertexLUT.Params
//              from unquantized MeshArgs") (lines 181-228). The reference test
//              transposes a contiguous-float layout via swap(1,4)/swap(2,8)/
//              swap(3,12)/swap(6,9)/swap(7,13)/swap(11,14); the resulting 16 bytes
//              per vertex are what appendTransposePosAndFeatureNdx emits and what
//              we assert here. (Reference test exercises MeshBuilder; the same
//              appendTransposePosAndFeatureNdx routine is shared by PolylineBuilder,
//              so the first 16 bytes of each 24-byte polyline vertex match.)
TEST(VertexTableBuilder, SimpleBuilderTransposeMatchesReferenceBytes) {
    // Reference points (VertexTable.test.ts:166-169).
    Point3d pts[3] = {
        Point3d::From(0.0, 1.0, 2.0),
        Point3d::From(-1.0, 0.0, 1.0),
        Point3d::From(12.34, 99999.9, -98.76),
    };
    auto vt = VertexTableBuilder::buildFromPolylines(pts, 3u, dqCommon::ColorDef::white);

    // computeDimensions(3 vertices, 6 rgba/vert, 0 extra, 2048) = 18 rgba <= 2048
    // -> width=18, height=1.
    ASSERT_EQ(1u, vt.height);
    ASSERT_EQ(18u, vt.width);
    ASSERT_EQ(6u, vt.numRgbaPerVertex);
    ASSERT_EQ(3u, vt.numVertices);

    // Reference transposed expected bytes (first 16 bytes = pos+featID, 4 texels).
    // clang-format off
    static const uint8_t kExpectedV0[16] = {
        0x00, 0x00, 0x00, 0x00,   // texel 0: x.b0, y.b0, z.b0, featID.b0  (x=0,y=1,z=2,f=0)
        0x00, 0x00, 0x00, 0x00,   // texel 1: x.b1, y.b1, z.b1, featID.b1
        0x00, 0x80, 0x00, 0x00,   // texel 2: x.b2, y.b2(=0x80), z.b2, featID.b2
        0x00, 0x3f, 0x40, 0x00,   // texel 3: x.b3, y.b3(=0x3f), z.b3(=0x40), featID.b3
    };
    static const uint8_t kExpectedV1[16] = {
        0x00, 0x00, 0x00, 0x00,   // x=-1 -> 00 00 80 bf; y=0; z=1 -> 00 00 80 3f; f=0
        0x00, 0x00, 0x00, 0x00,
        0x80, 0x00, 0x80, 0x00,   // x.b2=80, y.b2=00, z.b2=80, featID.b2=00
        0xbf, 0x00, 0x3f, 0x00,   // x.b3=bf, y.b3=00, z.b3=3f, featID.b3=00
    };
    static const uint8_t kExpectedV2[16] = {
        // x=12.34 -> a4 70 45 41; y=99999.9 -> f3 4f c3 47; z=-98.76 -> 1f 85 c5 c2; f=0
        0xa4, 0xf3, 0x1f, 0x00,   // x.b0, y.b0, z.b0, featID.b0
        0x70, 0x4f, 0x85, 0x00,   // x.b1, y.b1, z.b1, featID.b1
        0x45, 0xc3, 0xc5, 0x00,   // x.b2, y.b2, z.b2, featID.b2
        0x41, 0x47, 0xc2, 0x00,   // x.b3, y.b3, z.b3, featID.b3
    };
    // clang-format on
    const uint8_t* expected[3] = { kExpectedV0, kExpectedV1, kExpectedV2 };

    for (uint32_t v = 0; v < 3u; ++v) {
        uint8_t const* vert = vt.data.data() + (v * 24u);  // height==1 -> stride = 24
        for (int i = 0; i < 16; ++i) {
            EXPECT_EQ(expected[v][i], vert[i])
                << "vertex " << v << " byte " << i << " mismatch";
        }
    }
}

// Authored: no reference test exists in itwinjs-core for VertexTableBuilder
// PolylineBuilder byte layout (the reference VertexTable.test.ts covers only
// the MeshBuilder path). Round-trips the transposed SoA layout through the
// shader's gather convention (spec §3.5) to validate byte offsets {0,4,8,12}
// for x, {1,5,9,13} for y, {2,6,10,14} for z.
TEST(VertexTableBuilder, PacksTransposedUnquantizedPositions) {
    Point3d pts[3] = {
        Point3d::From(1.5, -2.25, 3.0),
        Point3d::From(10.0, 0.0, -7.5),
        Point3d::From(0.0, 0.0, 0.0),
    };
    auto vt = VertexTableBuilder::buildFromPolylines(pts, 3u, dqCommon::ColorDef::white);

    EXPECT_EQ(6u, vt.numRgbaPerVertex);
    EXPECT_EQ(3u, vt.numVertices);
    EXPECT_EQ(vt.data.size(), vt.width * vt.height * 4u);
    EXPECT_TRUE(vt.usesUnquantizedPositions);
    // width must be divisible by numRgbaPerVertex so a vertex never wraps rows.
    EXPECT_EQ(0u, vt.width % vt.numRgbaPerVertex);

    // Each vertex occupies 24 contiguous bytes; transpose: x at texel-byte 0,4,8,12.
    for (uint32_t v = 0; v < 3u; ++v) {
        uint8_t const* vert = vt.data.data() + (v * 24u);  // height==1 for small counts
        int xOff[4] = {0, 4, 8, 12};
        int yOff[4] = {1, 5, 9, 13};
        int zOff[4] = {2, 6, 10, 14};
        EXPECT_FLOAT_EQ(static_cast<float>(pts[v].x), gatherFloat(vert, xOff))
            << "vertex " << v << " x";
        EXPECT_FLOAT_EQ(static_cast<float>(pts[v].y), gatherFloat(vert, yOff))
            << "vertex " << v << " y";
        EXPECT_FLOAT_EQ(static_cast<float>(pts[v].z), gatherFloat(vert, zOff))
            << "vertex " << v << " z";
    }
}

// Authored: no reference test exists in itwinjs-core for the uniform-color
// polyline VertexTable (MeshBuilder reference test always uses a uniform
// ColorIndex but does not assert the table size / absence of color table).
// Verifies the uniform path: hasNonUniformColor=false and data.size() ==
// width*height*4 with no appended color table.
TEST(VertexTableBuilder, UniformColorEmitsNoColorTable) {
    Point3d pts[2] = { Point3d::From(0.0, 0.0, 0.0), Point3d::From(1.0, 0.0, 0.0) };
    auto vt = VertexTableBuilder::buildFromPolylines(pts, 2u, dqCommon::ColorDef::white);

    EXPECT_FALSE(vt.hasNonUniformColor);
    // 2 vertices * 6 rgba/vert = 12 rgba; width=12, height=1; data = 12*1*4 = 48 bytes.
    EXPECT_EQ(12u, vt.width);
    EXPECT_EQ(1u, vt.height);
    EXPECT_EQ(48u, vt.data.size());
    EXPECT_EQ(vt.data.size(), vt.width * vt.height * 4u);  // no appended color table
}

// Authored: no reference test exists in itwinjs-core for the PolylineBuilder
// 6th texel (cumulative distance). Verifies the polyline-specific 24-byte
// layout: texel 5 (bytes 20-23) holds the float32 LE cumulative distance.
TEST(VertexTableBuilder, PolylineAppendsCumulativeDistance) {
    // Sequential polyline [0 -> 1 -> 2] with unit-length segments along x:
    // cumDist = [0.0, 1.0, 2.0].
    Point3d pts[3] = {
        Point3d::From(0.0, 0.0, 0.0),
        Point3d::From(1.0, 0.0, 0.0),
        Point3d::From(2.0, 0.0, 0.0),
    };
    auto vt = VertexTableBuilder::buildFromPolylines(pts, 3u, dqCommon::ColorDef::white);

    ASSERT_EQ(1u, vt.height);  // small count -> single row
    const float expectedCum[3] = { 0.0f, 1.0f, 2.0f };
    for (uint32_t v = 0; v < 3u; ++v) {
        uint8_t const* vert = vt.data.data() + (v * 24u);
        // Texel 5 = bytes 20-23 (float32 LE cumulative distance).
        int cumOff[4] = {20, 21, 22, 23};
        EXPECT_FLOAT_EQ(expectedCum[v], gatherFloat(vert, cumOff))
            << "vertex " << v << " cumulative distance";
    }
}

// Authored: no reference test exists in itwinjs-core for VertexTableBuilder
// computeDimensions width-divisibility invariant on the polyline path.
// Verifies width % numRgbaPerVertex == 0 so a vertex's 6 texels never wrap
// rows (computeDimensions pads width up to the next multiple of 6 once the
// sqrt-fallback branch is entered).
TEST(VertexTableBuilder, WidthAlwaysDivisibleByNumRgbaPerVertex) {
    // 2048/6 = 341.33 -> at 342 vertices the single-row case still holds
    // (342*6=2052 > 2048 -> enters sqrt fallback). Use a count that forces
    // the multi-row path.
    const uint32_t n = 600u;
    std::vector<Point3d> pts(n, Point3d::From(0.0, 0.0, 0.0));
    auto vt = VertexTableBuilder::buildFromPolylines(pts.data(), n,
                                                    dqCommon::ColorDef::white,
                                                    /*maxDimension*/ 2048u);
    EXPECT_EQ(n, vt.numVertices);
    EXPECT_EQ(6u, vt.numRgbaPerVertex);
    EXPECT_EQ(0u, vt.width % vt.numRgbaPerVertex)
        << "width=" << vt.width << " must be divisible by 6";
    EXPECT_EQ(vt.data.size(), vt.width * vt.height * 4u);
    EXPECT_GE(vt.width * vt.height, n * vt.numRgbaPerVertex);
}
