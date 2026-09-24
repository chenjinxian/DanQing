// Ported from: itwinjs-core core/frontend/src/test/render/primitives/PolylineTesselator.test.ts
// SPDX-License-Identifier: Apache-2.0
//
// Verifies PolylineTesselator (port of PolylineParams.ts) byte-for-byte against
// the reference test's expected index/prev/next/param arrays, plus Authored
// invariants for the no-joints simple case (no reference test covers that path;
// permitted under CLAUDE.md §5(f)).

#include "render/PolylineTesselator.h"

#include <dqBase/DqTypes.h>
#include <dqGeom/Point3d.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace dqBase;  // DqVector
using dqRender::PolylineParam;
using dqRender::PolylineTesselator;
using dqRender::TesselatedPolyline;
using dqGeom::Point3d;

namespace {
// Mirror of itwinjs VertexIndices.decodeIndex (VertexIndices.ts:51-55):
// 24-bit little-endian read of one corner's 3-byte index buffer slot.
uint32_t decodeIndex(DqVector<uint8_t> const& buf, size_t corner) {
    size_t i = corner * 3;
    return static_cast<uint32_t>(buf[i])
         | (static_cast<uint32_t>(buf[i + 1]) << 8)
         | (static_cast<uint32_t>(buf[i + 2]) << 16);
}

// Mirror of the reference test's `new Uint32Array(buffer)` reinterpret
// (PolylineTesselator.test.ts:23): 32-bit little-endian, low 24 bits = next
// index, high 8 bits = param.
uint32_t decodeNextAndParam(DqVector<uint8_t> const& buf, size_t corner) {
    size_t i = corner * 4;
    return static_cast<uint32_t>(buf[i])
         | (static_cast<uint32_t>(buf[i + 1]) << 8)
         | (static_cast<uint32_t>(buf[i + 2]) << 16)
         | (static_cast<uint32_t>(buf[i + 3]) << 24);
}
} // namespace

// Ported from: itwinjs-core core/frontend/src/test/render/primitives/PolylineTesselator.test.ts
//              describe("PolylineTesselator") > it("produces joint triangles")
TEST(PolylineTesselator, ProducesJointTriangles) {
    std::vector<Point3d> pts = {
        Point3d::From(0, 0, 0),
        Point3d::From(0, 10, 0),
        Point3d::From(10, 10, 0),
        Point3d::From(10, 20, 0),
    };
    DqVector<DqVector<uint32_t>> polylines = { {0, 1, 2, 3} };

    TesselatedPolyline tesselated = PolylineTesselator::tesselatePolyline(
        polylines, pts.data(), static_cast<uint32_t>(pts.size()), /*doJointTriangles*/ true);

    // Reference: expect(tesselated.indices.length).toEqual(72);
    EXPECT_EQ(72u, tesselated.numCorners());

    // Reference: expect(tesselated.indices.decodeIndices()).toEqual([...]);
    const std::vector<uint32_t> expectedIndices = {
        0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 2, 1, 1, 2, 2, 1, 2, 1, 1, 2, 2, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2,
        2, 3, 3, 2, 3, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    };
    ASSERT_EQ(expectedIndices.size(), tesselated.numCorners());
    for (size_t i = 0; i < expectedIndices.size(); ++i) {
        EXPECT_EQ(expectedIndices[i], decodeIndex(tesselated.indices, i)) << "indices[" << i << "]";
    }

    // Reference: expect(tesselated.prevIndices.decodeIndices()).toEqual([...]);
    const std::vector<uint32_t> expectedPrev = {
        0, 2, 0, 0, 2, 2, 0, 2, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 0, 3, 0, 0, 3, 3, 0, 3, 0, 0, 3, 3, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 1,
        1, 3, 3, 1, 3, 1, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    for (size_t i = 0; i < expectedPrev.size(); ++i) {
        EXPECT_EQ(expectedPrev[i], decodeIndex(tesselated.prevIndices, i)) << "prevIndices[" << i << "]";
    }

    // Reference: nextIndices = (x & 0x00ffffff) >>> 0
    const std::vector<uint32_t> expectedNext = {
        1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 2, 1, 2, 2, 1, 1, 2, 1, 2, 2, 1, 1, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 3,
        3, 2, 2, 3, 2, 3, 3, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    };
    // Reference: params = (x & 0xff000000) >>> 24
    const std::vector<uint32_t> expectedParams = {
        27, 57, 96, 96, 57, 144, 96, 144, 3, 3, 144, 81, 144, 13, 12, 144, 14, 13,
        144, 15, 14, 33, 57, 96, 96, 57, 144, 96, 144, 9, 9, 144, 81, 96, 13, 12,
        96, 14, 13, 96, 15, 14, 144, 13, 12, 144, 14, 13, 144, 15, 14, 33, 51, 96,
        96, 51, 144, 96, 144, 9, 9, 144, 75, 96, 13, 12, 96, 14, 13, 96, 15, 14,
    };
    for (size_t i = 0; i < expectedNext.size(); ++i) {
        uint32_t const v = decodeNextAndParam(tesselated.nextIndicesAndParams, i);
        EXPECT_EQ(expectedNext[i], (v & 0x00ffffffu)) << "next[" << i << "]";
        EXPECT_EQ(expectedParams[i], ((v & 0xff000000u) >> 24)) << "param[" << i << "]";
    }
}

// Authored: no reference test exists in itwinjs-core for PolylineTesselator
// no-joints quad count. (The sole reference test exercises the joint path.)
// Verifies the cross-task contract: 1 quad = 6 corners per segment, 3-byte/
// 4-byte buffer layouts, and numCorners() == indices.size()/3.
TEST(PolylineTesselator, TwoPointPolylineProducesOneQuad) {
    Point3d pts[2] = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0) };
    auto t = PolylineTesselator::tesselate(pts, 2, /*weight*/ 1.0f, /*is2d*/ false, /*disjoint*/ false);
    EXPECT_EQ(6u, t.numCorners());  // 1 segment -> 1 quad -> 6 corners (2 triangles)
    EXPECT_EQ(t.indices.size(), t.prevIndices.size());
    EXPECT_EQ(t.nextIndicesAndParams.size(), t.numCorners() * 4u);
}

// Authored: no reference test exists in itwinjs-core for PolylineTesselator
// no-joints multi-segment corner count. (Sole reference test uses joints.)
TEST(PolylineTesselator, NSegmentsProduce6NCornersWithoutJoints) {
    Point3d pts[4] = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0),
                       Point3d::From(2, 0, 0), Point3d::From(3, 0, 0) };
    auto t = PolylineTesselator::tesselate(pts, 4, 1.0f, false, false);
    EXPECT_EQ(18u, t.numCorners());  // 3 segments x 6 (weight 1 -> no joints)
}

// Authored: no reference test exists in itwinjs-core for PolylineTesselator
// index-range / param-divisible-by-3 invariants in the no-joints path.
TEST(PolylineTesselator, AllIndicesInRangeAndParamsValid) {
    Point3d pts[4] = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0),
                       Point3d::From(2, 0, 0), Point3d::From(3, 0, 0) };
    auto t = PolylineTesselator::tesselate(pts, 4, 1.0f, false, false);
    for (uint32_t c = 0; c < t.numCorners(); ++c) {
        EXPECT_LT(decodeIndex(t.indices, c), 4u);
        EXPECT_LT(decodeIndex(t.prevIndices, c), 4u);
        uint32_t next = decodeNextAndParam(t.nextIndicesAndParams, c) & 0x00ffffffu;
        EXPECT_LT(next, 4u);
        uint8_t param = t.nextIndicesAndParams[c * 4 + 3];
        // No-joints path: every emitted param is a *3-scaled PolylineParam
        // (kSquare/kMiter + kNegatePerp/kNegateAlong adjustments) -> divisible by 3.
        EXPECT_EQ(0, param % 3) << "corner " << c;
    }
}

// Authored: no reference test exists in itwinjs-core for the PolylineTesselator
// endpoint prev/next self-reference convention (open polyline start/end
// vertices use their own index as prev/next respectively — see
// PolylineParams.ts:177-178). Verifies that the quad for the first segment
// carries prevIndex == idx0 and the quad for the last segment carries
// nextIndex == idx1.
TEST(PolylineTesselator, EndpointsSelfReferenceAtPolylineBounds) {
    Point3d pts[3] = { Point3d::From(0, 0, 0), Point3d::From(1, 0, 0), Point3d::From(2, 0, 0) };
    auto t = PolylineTesselator::tesselate(pts, 3, 1.0f, false, false);
    ASSERT_EQ(12u, t.numCorners());  // 2 segments x 6, no joints
    // Segment 0 (corners 0..5): v0 is at corners 0, 2, 3; prevIdx0 = idx0 = 0
    // (open-polyline start self-reference).
    EXPECT_EQ(0u, decodeIndex(t.prevIndices, 0));
    EXPECT_EQ(0u, decodeIndex(t.prevIndices, 2));
    EXPECT_EQ(0u, decodeIndex(t.prevIndices, 3));
    // Segment 1 (corners 6..11, last segment): v1 at corners 7, 10, 11 has
    // prevIndex = nextIdx1 = idx1 = 2 (open-polyline end self-reference; note
    // v1.init stores nextIdx1 in the prev slot -- PolylineParams.ts:181).
    EXPECT_EQ(2u, decodeIndex(t.prevIndices, 7));
    EXPECT_EQ(2u, decodeIndex(t.prevIndices, 10));
    EXPECT_EQ(2u, decodeIndex(t.prevIndices, 11));
}
