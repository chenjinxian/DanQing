// Ported from: itwinjs-core core/frontend/src/common/internal/render/PolylineParams.ts
// SPDX-License-Identifier: Apache-2.0
//
// Faithful 1:1 C++ port of itwinjs PolylineParams.ts (PolylineParam enum,
// PolylineVertex, PolylineTesselator, TesselatedPolyline, wantJointTriangles).
// CPU-only: expands an ordered polyline vertex list into a triangle-list
// "corner buffer" (per-corner 24-bit a_pos, 24-bit a_prevIndex, 24-bit
// a_nextIndex + 8-bit a_param). Consumed by the GPU polyline technique
// (PolylineShaderBuilder / Task 4-5 MeshGraphic) and drawn as
// glDrawArrays(GL_TRIANGLES) with no element index buffer.
//
// Reference lines (PolylineParams.ts):
//   - PolylineParam enum      :45-54
//   - TesselatedPolyline      :24-31
//   - PolylineVertex          :66-103
//   - PolylineTesselator      :105-240
//   - tesselatePolyline       :243-246 (test-only export)
//   - wantJointTriangles      :271-275
//
// C++ adaptations (§3.4): TS `_field` -> `m_field`; TS `number[]` -> `DqVector<uint32_t>`;
// `Point3d[]` -> `dqGeom::Point3d const* + count`; no GC -> value/member storage.
// `PolylineVertex` is a private nested struct (implementation detail; reference
// has it as a top-level non-exported class).

#pragma once

#include <dqBase/DqTypes.h>      // DqVector
#include <dqGeom/Point3d.h>

#include <cstdint>

namespace dqRender {

// Bring DqVector (dqBase thin wrapper for std::vector) into this namespace so
// the brief's TesselatedPolyline contract reads verbatim (`DqVector<uint8_t>`).
using dqBase::DqVector;

// Ported from: PolylineParam enum (PolylineParams.ts:45-54).
// Parameter associated with each vertex index of a tesselated polyline. Values
// are *3-scaled so the shader can do param/3 to recover the base enum and
// param%3 to recover the joint-triangle slot index (see addJointTriangles,
// PolylineParams.ts:217-224).
enum class PolylineParam : int32_t {
    kNone = 0,
    kSquare = 1 * 3,
    kMiter = 2 * 3,
    kMiterInsideOnly = 3 * 3,
    kJointBase = 4 * 3,
    kNegatePerp = 8 * 3,
    kNegateAlong = 16 * 3,
    kNoneAdjustWeight = 32 * 3,
};

// Ported from: TesselatedPolyline (PolylineParams.ts:24-31).
// Cross-task contract consumed by Task 5 (MeshGraphic). Field byte-layouts are
// fixed: `indices`/`prevIndices` are 3 bytes/corner (24-bit LE vertex index);
// `nextIndicesAndParams` is 4 bytes/corner ([nextIdx bytes 0..2, param byte 3]).
struct TesselatedPolyline {
    DqVector<uint8_t> indices;              // 3 bytes/corner -- a_pos 24-bit index
    DqVector<uint8_t> prevIndices;          // 3 bytes/corner -- a_prevIndex 24-bit index
    DqVector<uint8_t> nextIndicesAndParams; // 4 bytes/corner -- [nextIdx x3 (bytes 0..2), param (byte 3)]

    uint32_t numCorners() const noexcept { return static_cast<uint32_t>(indices.size() / 3); }
};

// Ported from: PolylineTesselator class (PolylineParams.ts:105-240).
class PolylineTesselator {
public:
    // Ported from: PolylineTesselator constructor (PolylineParams.ts:115-125).
    // `polylines` = list of index lists (one per polyline); `points` = the
    // vertex pool referenced by the index lists; `doJointTriangles` = the
    // pre-computed joint gate (see wantJointTriangles).
    PolylineTesselator(DqVector<DqVector<uint32_t>> const& polylines,
                        dqGeom::Point3d const* points, uint32_t numPoints,
                        bool doJointTriangles);

    // Ported from: tesselate (PolylineParams.ts:140-159). Runs _tesselate then
    // packs _vertIndex/_prevIndex/_nextIndex/_nextParam into the 3-byte/4-byte
    // corner buffer layout. May only be called once per instance (matches
    // reference, which does no reset).
    TesselatedPolyline tesselate();

    // Ported from: wantJointTriangles (PolylineParams.ts:271-275).
    // Joints are expensive; in 3D only emit them once the line is wide enough
    // to be noticeable. In 2D always emit.
    static bool wantJointTriangles(float weight, bool is2d) noexcept {
        constexpr int32_t jointWidthThreshold = 3;
        return is2d || weight >= jointWidthThreshold;
    }

    // Ported from: tesselatePolyline test-only export (PolylineParams.ts:243-246).
    // Used by the ported reference test.
    static TesselatedPolyline tesselatePolyline(DqVector<DqVector<uint32_t>> const& polylines,
                                                 dqGeom::Point3d const* points, uint32_t numPoints,
                                                 bool doJointTriangles);

    // Brief Task 2 minimal entry point (mirrors reference `create` +
    // `tesselatePolylineList`: PolylineParams.ts:56-64, 131-138). Builds a
    // single sequential-indices polyline [0..numVertices-1] and computes the
    // joint gate from weight+is2d. `disjoint` mirrors PolylineArgs.flags
    // .isDisjoint (reference asserts !isDisjoint at line 250; tessellation
    // algorithm itself does not branch on it -- ignored here).
    static TesselatedPolyline tesselate(dqGeom::Point3d const* points, uint32_t numVertices,
                                         float weight, bool is2d, bool disjoint);

private:
    // Ported from: PolylineVertex class (PolylineParams.ts:66-103). Internal
    // helper describing one endpoint of the segment currently being emitted.
    struct PolylineVertex {
        bool isSegmentStart = false;
        bool isPolylineStartOrEnd = false;
        uint32_t vertexIndex = 0;
        uint32_t prevIndex = 0;
        uint32_t nextIndex = 0;

        void init(bool isSegStart, bool isPolyStartOrEnd, uint32_t vertIdx,
                  uint32_t prevIdx, uint32_t nextIdx) noexcept {
            isSegmentStart = isSegStart;
            isPolylineStartOrEnd = isPolyStartOrEnd;
            vertexIndex = vertIdx;
            prevIndex = prevIdx;
            nextIndex = nextIdx;
        }

        // Ported from: PolylineVertex.computeParam (PolylineParams.ts:83-102).
        int32_t computeParam(bool negatePerp, bool adjacentToJoint = false,
                             bool joint = false, bool noDisplacement = false) const noexcept {
            if (joint)
                return static_cast<int32_t>(PolylineParam::kJointBase);

            int32_t param;
            if (noDisplacement)
                param = static_cast<int32_t>(PolylineParam::kNoneAdjustWeight);
            else if (adjacentToJoint)
                param = static_cast<int32_t>(PolylineParam::kMiterInsideOnly);
            else
                param = isPolylineStartOrEnd ? static_cast<int32_t>(PolylineParam::kSquare)
                                             : static_cast<int32_t>(PolylineParam::kMiter);

            int32_t adjust = 0;
            if (negatePerp)
                adjust = static_cast<int32_t>(PolylineParam::kNegatePerp);
            if (!isSegmentStart)
                adjust += static_cast<int32_t>(PolylineParam::kNegateAlong);

            return param + adjust;
        }
    };

    // Ported from: _tesselate (PolylineParams.ts:161-215).
    void _tesselate();

    // Ported from: addJointTriangles (PolylineParams.ts:217-224). Note: the
    // reference names the third parameter `v1` (same as the second `v1`) --
    // here we keep the reference call shape (`v0`, `p0`, `v1`).
    void addJointTriangles(PolylineVertex const& v0, int32_t p0, PolylineVertex const& v1);

    // Ported from: _dotProduct (PolylineParams.ts:226-231).
    double _dotProduct(PolylineVertex const& v) const;

    // Ported from: _addVertex (PolylineParams.ts:233-239).
    void _addVertex(PolylineVertex const& vertex, int32_t param);

private:
    // Ported from: PolylineTesselator members (PolylineParams.ts:106-113).
    DqVector<DqVector<uint32_t>> m_polylines;
    bool m_doJoints = false;
    uint32_t m_numIndices = 0;
    DqVector<uint32_t> m_vertIndex;
    DqVector<uint32_t> m_prevIndex;
    DqVector<uint32_t> m_nextIndex;
    DqVector<int32_t> m_nextParam;
    DqVector<dqGeom::Point3d> m_position;
};

} // namespace dqRender
