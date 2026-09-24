// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl graphics creation (minimal pass)
// Ported from: itwinjs-core core/frontend/src/common/imdl/ImdlGraphicsCreator.ts
//              (decodeImdlGraphics :414-437 — node walk → mesh graphics) and
//              the VertexTable quantized-position decoding the reference
//              performs on the GPU (VertexTable.ts layout); the DanQing pass
//              decodes on the CPU into IndexedPolyface (H-3, layout verified
//              byte-level against the recorded v1.1 fixtures 2026-09-23:
//              first three u16 per vertex = quantized x/y/z, surface indices
//              24-bit LE, world = decodedMin + q*(decodedMax-min)/65535).
#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/RealityTileTree.h"
#include "dqRender/RenderSystem.h"

#include "TilesetJson.h"

#include <dqGeom/IndexedPolyface.h>

#include <cmath>
#include <cstring>
#include <memory>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Locate a bufferView's byte span inside the imdl document's binary section.
bool findBufferView(tilejson::JsonValue const& doc, std::string const& name,
                    std::vector<uint8_t> const& binary,
                    uint8_t const*& outData, size_t& outSize)
{
    tilejson::JsonValue const* views = doc.find("bufferViews");
    if (!views)
        return false;
    tilejson::JsonValue const* view = views->find(name.c_str());
    if (!view)
        return false;
    tilejson::JsonValue const* off = view->find("byteOffset");
    tilejson::JsonValue const* len = view->find("byteLength");
    if (!off || !len)
        return false;
    size_t const offset = static_cast<size_t>(off->number);
    size_t const length = static_cast<size_t>(len->number);
    if (offset + length > binary.size())
        return false;
    outData = binary.data() + offset;
    outSize = length;
    return true;
}

}  // namespace

std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> DQ_RENDER_EXPORT
decodeImdlGraphics(ImdlDocument const& doc)
{
    std::vector<dqBase::RefPtr<dqGeom::IndexedPolyface>> meshes;

    auto json = tilejson::parseJsonDocument(doc.sceneJson);
    if (!json)
        return meshes;

    // Reparse the JSON: the document's sceneJson is re-read by find callers;
    // parseImdlMeshPrimitives wants the parsed tree.
    auto primitives = tilejson::parseImdlMeshPrimitives(*json);
    for (auto const& prim : primitives) {
        uint8_t const* vertData = nullptr;
        size_t vertSize = 0;
        if (!findBufferView(*json, prim.vertices.bufferView, doc.binary, vertData, vertSize))
            continue;
        size_t const bytesPerVertex = prim.vertices.numRgbaPerVertex * 4;
        if (bytesPerVertex == 0 || vertSize < prim.vertices.count * bytesPerVertex)
            continue;

        uint8_t const* indexData = nullptr;
        size_t indexSize = 0;
        if (!findBufferView(*json, prim.surface.indicesView, doc.binary, indexData, indexSize))
            continue;
        size_t const numIndices = indexSize / 3;  // 24-bit LE each
        if (numIndices < 3 || numIndices % 3 != 0)
            continue;

        auto polyface = dqGeom::IndexedPolyface::create(true /*needNormals*/);
        // Quantized positions → world (decodedMin/decodedMax affine) + oct normals.
        double const range[3] = {
            prim.vertices.decodedMax[0] - prim.vertices.decodedMin[0],
            prim.vertices.decodedMax[1] - prim.vertices.decodedMin[1],
            prim.vertices.decodedMax[2] - prim.vertices.decodedMin[2],
        };
        for (uint32_t v = 0; v < prim.vertices.count; ++v) {
            uint8_t const* base = vertData + v * bytesPerVertex;
            uint16_t const q[3] = {
                static_cast<uint16_t>(base[0] | (base[1] << 8)),
                static_cast<uint16_t>(base[2] | (base[3] << 8)),
                static_cast<uint16_t>(base[4] | (base[5] << 8)),
            };
            double const p[3] = {
                prim.vertices.decodedMin[0] + range[0] * q[0] / 65535.0,
                prim.vertices.decodedMin[1] + range[1] * q[1] / 65535.0,
                prim.vertices.decodedMin[2] + range[2] * q[2] / 65535.0,
            };
            polyface->AddPoint(dqGeom::Point3d(p[0], p[1], p[2]));

            // Oct-encoded normal at bytes 6-7 (2×u8, Surface.ts:383-398
            // octDecodeNormal — e/255*2-1, hemisphere fix for z<0).
            double const ex = base[6] / 255.0 * 2.0 - 1.0;
            double const ey = base[7] / 255.0 * 2.0 - 1.0;
            double nx = ex, ny = ey;
            double nz = 1.0 - std::abs(nx) - std::abs(ny);
            if (nz < 0.0) {
                // Hemisphere fix (Surface.ts:389-392).
                double const sx = nx >= 0.0 ? 1.0 : -1.0;
                double const sy = ny >= 0.0 ? 1.0 : -1.0;
                double const tx = (1.0 - std::abs(ny)) * sx;
                double const ty = (1.0 - std::abs(nx)) * sy;
                nx = tx;
                ny = ty;
                nz = 0.0;
            }
            double const len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-12)
                polyface->AddNormal(dqGeom::Vector3d(nx / len, ny / len, nz / len));
        }
        // Surface indices: triangles of consecutive 24-bit LE indices, each
        // facet terminated via point-index chain (AddPointIndex).
        size_t const numTriangles = numIndices / 3;
        for (size_t t = 0; t < numTriangles; ++t) {
            size_t const base = t * 9;
            uint32_t const ia = indexData[base] | (indexData[base + 1] << 8) | (static_cast<uint32_t>(indexData[base + 2]) << 16);
            uint32_t const ib = indexData[base + 3] | (indexData[base + 4] << 8) | (static_cast<uint32_t>(indexData[base + 5]) << 16);
            uint32_t const ic = indexData[base + 6] | (indexData[base + 7] << 8) | (static_cast<uint32_t>(indexData[base + 8]) << 16);
            // 24-bit surface indices are 0-based; IndexedPolyface uses
            // 1-based point indices (same conversion as the glTF reader —
            // GltfReader.cpp "cgltf indices are 0-based" note; index 0 is
            // the skip sentinel in PolyfaceGraphic::buildFromPolyface, the
            // TD-19 root cause: 0-based indices skipped EVERY corner).
            polyface->AddPointIndex(static_cast<int32_t>(ia) + 1);
            polyface->AddPointIndex(static_cast<int32_t>(ib) + 1);
            polyface->AddPointIndex(static_cast<int32_t>(ic) + 1);
            // Per-corner normal indices (1-based, same as points).
            polyface->AddNormalIndex(static_cast<int32_t>(ia) + 1);
            polyface->AddNormalIndex(static_cast<int32_t>(ib) + 1);
            polyface->AddNormalIndex(static_cast<int32_t>(ic) + 1);
            polyface->TerminateFacet();
        }
        if (polyface && polyface->FacetCount() > 0)
            meshes.push_back(std::move(polyface));
    }

    return meshes;
}

END_DQ_RENDER_NAMESPACE
