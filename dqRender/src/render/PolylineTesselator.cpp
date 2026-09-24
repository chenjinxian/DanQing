// Ported from: itwinjs-core core/frontend/src/common/internal/render/PolylineParams.ts
// SPDX-License-Identifier: Apache-2.0
//
// Faithful 1:1 C++ port of PolylineTesselator (PolylineParams.ts:105-275).
// See PolylineTesselator.h for the field/layout contract.

#include "PolylineTesselator.h"

#include <dqGeom/Vector3d.h>  // Vector3d::FromStartEnd, Vector3d::DotProduct

namespace dqRender {

namespace {
// Ported from: VertexIndices.encodeIndex (VertexIndices.ts:40-45).
// 24-bit little-endian write of `index` into `bytes` at byte offset `byteIndex`.
void encodeIndex(uint32_t index, DqVector<uint8_t>& bytes, size_t byteIndex) {
    bytes[byteIndex + 0] = static_cast<uint8_t>((index & 0x000000ffu));
    bytes[byteIndex + 1] = static_cast<uint8_t>((index & 0x0000ff00u) >> 8);
    bytes[byteIndex + 2] = static_cast<uint8_t>((index & 0x00ff0000u) >> 16);
}

// Ported from: VertexIndices.fromArray (VertexIndices.ts:32-38) -- packs a
// uint32 array into a 3-bytes-per-index little-endian byte buffer.
DqVector<uint8_t> indicesFromArray(DqVector<uint32_t> const& indices) {
    DqVector<uint8_t> bytes(indices.size() * 3);
    for (size_t i = 0; i < indices.size(); ++i)
        encodeIndex(indices[i], bytes, i * 3);
    return bytes;
}
} // namespace

// Ported from: PolylineTesselator constructor (PolylineParams.ts:115-125).
PolylineTesselator::PolylineTesselator(DqVector<DqVector<uint32_t>> const& polylines,
                                        dqGeom::Point3d const* points, uint32_t numPoints,
                                        bool doJointTriangles)
    : m_polylines(polylines), m_doJoints(doJointTriangles) {
    m_position.assign(points, points + numPoints);
}

// Ported from: tesselate (PolylineParams.ts:140-159).
TesselatedPolyline PolylineTesselator::tesselate() {
    _tesselate();

    DqVector<uint8_t> vertIndex = indicesFromArray(m_vertIndex);
    DqVector<uint8_t> prevIndex = indicesFromArray(m_prevIndex);

    DqVector<uint8_t> nextIndexAndParam(static_cast<size_t>(m_numIndices) * 4);
    for (uint32_t i = 0; i < m_numIndices; ++i) {
        uint32_t const index = m_nextIndex[i];
        size_t const j = static_cast<size_t>(i) * 4;
        encodeIndex(index, nextIndexAndParam, j);
        nextIndexAndParam[j + 3] = static_cast<uint8_t>(m_nextParam[i] & 0x000000ff);
    }

    return TesselatedPolyline{
        std::move(vertIndex),
        std::move(prevIndex),
        std::move(nextIndexAndParam),
    };
}

// Ported from: tesselatePolyline test-only export (PolylineParams.ts:243-246).
TesselatedPolyline PolylineTesselator::tesselatePolyline(DqVector<DqVector<uint32_t>> const& polylines,
                                                          dqGeom::Point3d const* points, uint32_t numPoints,
                                                          bool doJointTriangles) {
    PolylineTesselator tesselator(polylines, points, numPoints, doJointTriangles);
    return tesselator.tesselate();
}

// Ported from: `create` + `tesselatePolylineList` (PolylineParams.ts:56-64,
// 131-138). Single-polyline sequential-indices entry point.
TesselatedPolyline PolylineTesselator::tesselate(dqGeom::Point3d const* points, uint32_t numVertices,
                                                  float weight, bool is2d, bool /*disjoint*/) {
    DqVector<uint32_t> sequential;
    sequential.reserve(numVertices);
    for (uint32_t i = 0; i < numVertices; ++i)
        sequential.push_back(i);
    DqVector<DqVector<uint32_t>> polylines;
    polylines.push_back(std::move(sequential));

    bool const doJoints = wantJointTriangles(weight, is2d);
    PolylineTesselator tesselator(polylines, points, numVertices, doJoints);
    return tesselator.tesselate();
}

// Ported from: _tesselate (PolylineParams.ts:161-215).
void PolylineTesselator::_tesselate() {
    PolylineVertex v0, v1;
    constexpr double maxJointDot = -0.7;

    for (DqVector<uint32_t> const& line : m_polylines) {
        if (line.size() < 2)
            continue;

        uint32_t const last = static_cast<uint32_t>(line.size() - 1);
        bool const isClosed = (line[0] == line[last]);

        for (uint32_t i = 0; i < last; ++i) {
            uint32_t const idx0 = line[i];
            uint32_t const idx1 = line[i + 1];
            bool const isStart = (0 == i);
            bool const isEnd = (last - 1 == i);
            uint32_t const prevIdx0 = isStart ? (isClosed ? line[last - 1] : idx0) : line[i - 1];
            uint32_t const nextIdx1 = isEnd ? (isClosed ? line[1] : idx1) : line[i + 2];

            v0.init(true, isStart && !isClosed, idx0, prevIdx0, idx1);
            v1.init(false, isEnd && !isClosed, idx1, nextIdx1, idx0);

            bool const jointAt0 = m_doJoints && (isClosed || !isStart) && _dotProduct(v0) > maxJointDot;
            bool const jointAt1 = m_doJoints && (isClosed || !isEnd) && _dotProduct(v1) > maxJointDot;

            if (jointAt0 || jointAt1) {
                _addVertex(v0, v0.computeParam(true, jointAt0, false, false));
                _addVertex(v1, v1.computeParam(false, jointAt1, false, false));
                _addVertex(v0, v0.computeParam(false, jointAt0, false, true));
                _addVertex(v0, v0.computeParam(false, jointAt0, false, true));
                _addVertex(v1, v1.computeParam(false, jointAt1, false, false));
                _addVertex(v1, v1.computeParam(false, jointAt1, false, true));
                _addVertex(v0, v0.computeParam(false, jointAt0, false, true));
                _addVertex(v1, v1.computeParam(false, jointAt1, false, true));
                _addVertex(v0, v0.computeParam(false, jointAt0, false, false));
                _addVertex(v0, v0.computeParam(false, jointAt0, false, false));
                _addVertex(v1, v1.computeParam(false, jointAt1, false, true));
                _addVertex(v1, v1.computeParam(true, jointAt1, false, false));

                if (jointAt0)
                    addJointTriangles(v0, v0.computeParam(false, true, false, true), v0);

                if (jointAt1)
                    addJointTriangles(v1, v1.computeParam(false, true, false, true), v1);
            } else {
                _addVertex(v0, v0.computeParam(true));
                _addVertex(v1, v1.computeParam(false));
                _addVertex(v0, v0.computeParam(false));
                _addVertex(v0, v0.computeParam(false));
                _addVertex(v1, v1.computeParam(false));
                _addVertex(v1, v1.computeParam(true));
            }
        }
    }
}

// Ported from: addJointTriangles (PolylineParams.ts:217-224).
void PolylineTesselator::addJointTriangles(PolylineVertex const& v0, int32_t p0, PolylineVertex const& v1) {
    int32_t const param = v1.computeParam(false, false, true);
    for (int i = 0; i < 3; i++) {
        _addVertex(v0, p0);
        _addVertex(v1, param + i + 1);
        _addVertex(v1, param + i);
    }
}

// Ported from: _dotProduct (PolylineParams.ts:226-231).
double PolylineTesselator::_dotProduct(PolylineVertex const& v) const {
    dqGeom::Point3d const& pos = m_position[v.vertexIndex];
    dqGeom::Vector3d const prevDir = dqGeom::Vector3d::FromStartEnd(m_position[v.prevIndex], pos);
    dqGeom::Vector3d const nextDir = dqGeom::Vector3d::FromStartEnd(m_position[v.nextIndex], pos);
    return prevDir.DotProduct(nextDir);
}

// Ported from: _addVertex (PolylineParams.ts:233-239).
void PolylineTesselator::_addVertex(PolylineVertex const& vertex, int32_t param) {
    // The reference indexes these arrays by _numIndices (JS sparse-array growth).
    // C++ push_back is the faithful equivalent.
    m_vertIndex.push_back(vertex.vertexIndex);
    m_prevIndex.push_back(vertex.prevIndex);
    m_nextIndex.push_back(vertex.nextIndex);
    m_nextParam.push_back(param);
    ++m_numIndices;
}

} // namespace dqRender
