// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mesh graphic implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Mesh.ts
#include "MeshGraphic.h"
#include "MeshPrimitives.h"
#include "PolylineTesselator.h"
#include "VertexLutTexture.h"
#include "VertexTableBuilder.h"
#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Driver.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/FeatureIndex.h>

#include <cstring>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MeshGraphic dtor — release the shared vertex-side GL resources. The
// per-geometry ibh + primitive are released by each geometry's own dtor when
// m_surfaces / m_polylines / m_pointStrings auto-destruct (member destruction
// runs after this body). GL-safe regardless of order: VAO/VBO/IBO deletes are
// independent name frees; the primitives are not mid-draw at dispose time.
// Mirrors PolyfaceGraphic::~PolyfaceGraphic shared-resource teardown.
// ---------------------------------------------------------------------------
MeshGraphic::~MeshGraphic()
{
    m_driver.destroyBufferObject(m_vbo);
    m_driver.destroyVertexBufferInfo(m_vbih);
    m_driver.destroyVertexBuffer(m_vbh);
}

// ---------------------------------------------------------------------------
// MeshRenderGeometry::create — create GPU-ready mesh from raw data
// Ported from: itwinjs-core Mesh.ts MeshRenderGeometry.create()
//
// Creates VBO/IBO from MeshData and wraps in MeshGraphic.
// Follows the PolyfaceGraphic pattern for GPU buffer creation.
// ---------------------------------------------------------------------------
std::unique_ptr<MeshGraphic> MeshRenderGeometry::create(
    rhi::Driver& driver, MeshData const& meshData, uint32_t defaultColor)
{
    uint32_t vertexCount = meshData.getVertexCount();
    uint32_t indexCount = meshData.getIndexCount();
    if (vertexCount == 0 || indexCount == 0) {
        return nullptr;
    }

    auto mesh = std::make_unique<MeshGraphic>(driver);

    // Create interleaved vertex data: position(3xfloat) + normal(3xfloat) + color(4xfloat) + featureId(1xuint)
    // Layout matches PolyfaceGraphic::Vertex (48 bytes per vertex)
    struct Vertex {
        float pos[3];
        float normal[3];
        float color[4];
        uint32_t featureId;
    };

    std::vector<Vertex> vertices(vertexCount);
    auto const& positions = meshData.getPositions();
    auto const& normals = meshData.getNormals();
    auto const& colors = meshData.getColors();
    auto const& featureIndices = meshData.getFeatureIndices();

    // Default quantization params (normalized range 0..1)
    const float scale = 1.0f / 65535.0f;

    for (uint32_t i = 0; i < vertexCount; ++i) {
        Vertex& v = vertices[i];

        // Position (quantized uint16 → float)
        if (positions.size() >= (i + 1) * 3) {
            v.pos[0] = positions[i * 3 + 0] * scale;
            v.pos[1] = positions[i * 3 + 1] * scale;
            v.pos[2] = positions[i * 3 + 2] * scale;
        } else {
            v.pos[0] = v.pos[1] = v.pos[2] = 0.0f;
        }

        // normal (oct-encoded uint16 → float)
        if (normals.size() >= (i + 1) * 2) {
            // Oct decode: convert [0,65535] to [-1,1]
            float nx = normals[i * 2 + 0] * scale * 2.0f - 1.0f;
            float ny = normals[i * 2 + 1] * scale * 2.0f - 1.0f;
            float nz = 1.0f - std::abs(nx) - std::abs(ny);
            if (nz < 0.0f) {
                float tx = (1.0f - std::abs(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                float ty = (1.0f - std::abs(nx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                nx = tx;
                ny = ty;
            }
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0f) {
                v.normal[0] = nx / len;
                v.normal[1] = ny / len;
                v.normal[2] = nz / len;
            } else {
                v.normal[0] = 0.0f;
                v.normal[1] = 0.0f;
                v.normal[2] = 1.0f;
            }
        } else {
            v.normal[0] = 0.0f;
            v.normal[1] = 0.0f;
            v.normal[2] = 1.0f;
        }

        // Color (RGBA8 uint32 → float4)
        if (colors.size() > i) {
            uint32_t c = colors[i];
            v.color[0] = ((c >> 0) & 0xFF) / 255.0f;
            v.color[1] = ((c >> 8) & 0xFF) / 255.0f;
            v.color[2] = ((c >> 16) & 0xFF) / 255.0f;
            v.color[3] = ((c >> 24) & 0xFF) / 255.0f;
        } else {
            // Use default color
            v.color[0] = ((defaultColor >> 0) & 0xFF) / 255.0f;
            v.color[1] = ((defaultColor >> 8) & 0xFF) / 255.0f;
            v.color[2] = ((defaultColor >> 16) & 0xFF) / 255.0f;
            v.color[3] = ((defaultColor >> 24) & 0xFF) / 255.0f;
        }

        // Feature ID
        v.featureId = (featureIndices.size() > i) ? featureIndices[i] : 0;
    }

    // Create VBO
    size_t vertexDataSize = vertexCount * sizeof(Vertex);
    auto vbo = driver.createBufferObject(
        static_cast<uint32_t>(vertexDataSize),
        rhi::BufferObjectBinding::VERTEX,
        rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor vboDesc(vertices.data(), vertexDataSize);
    driver.updateBufferObject(vbo, std::move(vboDesc), 0);

    // Create vertex buffer info (attribute layout)
    rhi::AttributeArray attrs = {};
    attrs[0].buffer = 0;
    attrs[0].offset = 0;
    attrs[0].type = rhi::ElementType::FLOAT3;  // position
    attrs[1].buffer = 0;
    attrs[1].offset = 12;
    attrs[1].type = rhi::ElementType::FLOAT3;  // normal
    attrs[2].buffer = 0;
    attrs[2].offset = 24;
    attrs[2].type = rhi::ElementType::FLOAT4;  // color
    attrs[3].buffer = 0;
    attrs[3].offset = 40;
    attrs[3].type = rhi::ElementType::UINT;    // featureId

    auto vbih = driver.createVertexBufferInfo(1, 4, attrs);
    auto vbh = driver.createVertexBuffer(vertexCount, vbih);
    driver.setVertexBufferObject(vbh, 0, vbo);
    mesh->setSharedVertexResources(vbo, vbih, vbh);

    // Create + back the IBO. createIndexBuffer only does glGenBuffers (no data
    // store); updateIndexBuffer is the API that glBufferData's the
    // IndexBufferHandle the draw binds as GL_ELEMENT_ARRAY_BUFFER. Uploading to a
    // disconnected BufferObjectHandle (createBufferObject + updateBufferObject,
    // target=VERTEX) leaves the ibh unbacked → glDrawElements reads an empty
    // element buffer and SIGSEGV's (KERN_INVALID_ADDRESS at 0x0) on the 2nd+ draw.
    // 1:1 with PolyfaceGraphic::uploadToGpu + PlanarGridGraphic.
    auto ibh = driver.createIndexBuffer(
        rhi::ElementType::UINT, indexCount, rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor iboDesc(meshData.getIndices().data(),
                                  static_cast<size_t>(indexCount) * sizeof(uint32_t));
    driver.updateIndexBuffer(ibh, std::move(iboDesc), 0);

    // Create render primitive
    auto primitive = driver.createRenderPrimitive(vbh, ibh, rhi::PrimitiveType::TRIANGLES);

    // Create surface geometry with the primitive
    auto surface = std::make_unique<SurfaceGeometry>(
        driver, ibh, indexCount, SurfaceType::Opaque, false, meshData.hasParams());
    surface->setPrimitive(primitive);

    mesh->addSurface(std::move(surface));
    return mesh;
}

// ---------------------------------------------------------------------------
// MeshRenderGeometry::create(driver, Mesh) — PR F bug-fix path.
// Convert an accumulator Mesh → MeshData (flat GPU buffer) then delegate to the MeshData overload,
// reusing its verified VBO/IBO/primitive creation. The returned MeshGraphic IS a Graphic, so wrapping
// it via RenderGraphicAdapter → static_cast<Graphic*> is defined behavior (the non-renderable-bug fix).
// ---------------------------------------------------------------------------
namespace {

// Resolve per-vertex colors (tbgr) from the Mesh's color table + per-vertex color indices.
// 1:1 createMeshArgs color resolution: uniform → all table[0]; non-uniform → table[mesh.colors[i]].
std::vector<uint32_t> resolveVertexColors(Mesh const& mesh, size_t vertexCount)
{
    std::vector<uint32_t> colors;
    colors.reserve(vertexCount);
    auto const& colorMap = mesh.colorMap();
    const int tableLen = colorMap.length();
    if (tableLen == 0) {
        colors.assign(vertexCount, 0xFFFFFFFF);  // fallback (should not happen — colorMap always ≥1)
        return colors;
    }
    const std::vector<uint32_t> table = colorMap.toArray();  // colors by insertion index
    if (tableLen == 1 || mesh.colors().empty()) {
        colors.assign(vertexCount, table.empty() ? 0xFFFFFFFF : table.front());  // uniform
        return colors;
    }
    auto const& idx = mesh.colors();
    for (size_t i = 0; i < vertexCount; ++i) {
        const uint32_t ci = (i < idx.size()) ? idx[i] : 0u;
        colors.push_back(ci < table.size() ? table[ci] : table.front());
    }
    return colors;
}

// Resolve per-vertex feature ids from the Mesh's feature index (uniform/non-uniform/empty).
std::vector<uint32_t> resolveVertexFeatures(Mesh const& mesh, size_t vertexCount)
{
    std::vector<uint32_t> features;
    features.reserve(vertexCount);
    if (!mesh.features()) {
        features.assign(vertexCount, 0u);
        return features;
    }
    dqCommon::FeatureIndex fi;
    mesh.toFeatureIndex(fi);
    using namespace dqCommon;
    if (fi.type == FeatureIndexType::Uniform) {
        features.assign(vertexCount, fi.featureID);
    } else if (fi.type == FeatureIndexType::NonUniform) {
        for (size_t i = 0; i < vertexCount; ++i)
            features.push_back(i < fi.featureIDs.size() ? fi.featureIDs[i] : 0u);
    } else {
        features.assign(vertexCount, 0u);  // Empty
    }
    return features;
}

}  // namespace

std::unique_ptr<MeshGraphic> MeshRenderGeometry::create(
    rhi::Driver& driver, Mesh const& mesh, uint32_t defaultColor)
{
    const size_t vertexCount = mesh.points().length();
    if (vertexCount == 0)
        return nullptr;

    // Build interleaved vertex buffer with FLOAT positions (the recentered Point3d coords → float).
    // Faithful non-quantized path (PrimitiveBuilder decoration graphics): geometry renders at its
    // recentered position; PrimitiveBuilder.finish() applies the inverse center translation to recover
    // world placement. Layout matches the MeshData overload's Vertex (48 bytes/vertex).
    struct Vertex { float pos[3]; float normal[3]; float color[4]; uint32_t featureId; };
    std::vector<Vertex> vertices(vertexCount);
    auto const& pts = mesh.points().points;
    auto const& colors = resolveVertexColors(mesh, vertexCount);
    auto const& features = resolveVertexFeatures(mesh, vertexCount);
    auto const& normals = mesh.normals();
    const bool haveNormals = !normals.empty();

    for (size_t i = 0; i < vertexCount; ++i) {
        Vertex& v = vertices[i];
        v.pos[0] = static_cast<float>(pts[i].x);
        v.pos[1] = static_cast<float>(pts[i].y);
        v.pos[2] = static_cast<float>(pts[i].z);

        // Decode oct (ix|iy<<16, 16+16 per dqCommon::OctEncodedNormal) → unit float3.
        if (haveNormals && i < normals.size()) {
            const uint32_t val = normals[i].value;
            const float scale = 1.0f / 65535.0f;
            float nx = static_cast<float>(val & 0xFFFF) * scale * 2.0f - 1.0f;
            float ny = static_cast<float>((val >> 16) & 0xFFFF) * scale * 2.0f - 1.0f;
            float nz = 1.0f - std::abs(nx) - std::abs(ny);
            if (nz < 0.0f) {
                float tx = (1.0f - std::abs(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
                float ty = (1.0f - std::abs(nx)) * (ny >= 0.0f ? 1.0f : -1.0f);
                nx = tx;
                ny = ty;
            }
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0f) { v.normal[0] = nx / len; v.normal[1] = ny / len; v.normal[2] = nz / len; }
            else { v.normal[0] = 0.0f; v.normal[1] = 0.0f; v.normal[2] = 1.0f; }
        } else {
            v.normal[0] = 0.0f; v.normal[1] = 0.0f; v.normal[2] = 1.0f;
        }

        // Color: tbgr 0xTTBBGGRR (byte0=R). Per ColorDef the high byte is TRANSPARENCY
        // (0=opaque, 255=transparent), so ALPHA = 255 - transparency. The prior code
        // used the transparency byte directly as alpha, INVERTING opacity — a faint
        // fill (transparency 200) rendered at alpha 0.78 (opaque) and an outline
        // (transparency 75) at alpha 0.29 (too faint). Ported from itwinjs VertexTable
        // alpha packing (alpha = 1 - transparency/255). (The MeshData/glTF path stores
        // true RGBA8, so it is unaffected.)
        if (i < colors.size()) {
            uint32_t const cc = colors[i];
            v.color[0] = static_cast<float>((cc >> 0) & 0xFF) / 255.0f;
            v.color[1] = static_cast<float>((cc >> 8) & 0xFF) / 255.0f;
            v.color[2] = static_cast<float>((cc >> 16) & 0xFF) / 255.0f;
            v.color[3] = static_cast<float>(255u - ((cc >> 24) & 0xFF)) / 255.0f;
        } else {
            uint32_t const cc = defaultColor;
            v.color[0] = static_cast<float>((cc >> 0) & 0xFF) / 255.0f;
            v.color[1] = static_cast<float>((cc >> 8) & 0xFF) / 255.0f;
            v.color[2] = static_cast<float>((cc >> 16) & 0xFF) / 255.0f;
            v.color[3] = static_cast<float>((cc >> 24) & 0xFF) / 255.0f;
        }

        v.featureId = (i < features.size()) ? features[i] : 0u;
    }


    // Shared VBO for all geometries of this mesh.
    const uint32_t vc = static_cast<uint32_t>(vertexCount);
    size_t vertexDataSize = vc * sizeof(Vertex);
    auto vbo = driver.createBufferObject(static_cast<uint32_t>(vertexDataSize),
                                         rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor vboDesc(vertices.data(), vertexDataSize);
    driver.updateBufferObject(vbo, std::move(vboDesc), 0);

    rhi::AttributeArray attrs = {};
    attrs[0] = {0, 0, rhi::ElementType::FLOAT3};   // position
    attrs[1] = {0, 12, rhi::ElementType::FLOAT3};  // normal
    attrs[2] = {0, 24, rhi::ElementType::FLOAT4};  // color
    attrs[3] = {0, 40, rhi::ElementType::UINT};    // featureId
    auto vbih = driver.createVertexBufferInfo(1, 4, attrs);
    auto vbh = driver.createVertexBuffer(vc, vbih);
    driver.setVertexBufferObject(vbh, 0, vbo);

    auto result = std::make_unique<MeshGraphic>(driver);
    result->setSharedVertexResources(vbo, vbih, vbh);

    // Helper: create + back an index buffer (uint32) from an index list, mirroring
    // the MeshData overload. createIndexBuffer only does glGenBuffers (no data
    // store); updateIndexBuffer glBufferData's the IndexBufferHandle the draw binds
    // as GL_ELEMENT_ARRAY_BUFFER. Uploading to a disconnected BufferObjectHandle
    // leaves the ibh unbacked → glDrawElements SIGSEGV on the 2nd+ draw.
    // 1:1 with PolyfaceGraphic::uploadToGpu + PlanarGridGraphic.
    auto createIbo = [&driver](uint32_t indexCount, uint32_t const* data) {
        auto ibh = driver.createIndexBuffer(rhi::ElementType::UINT, indexCount, rhi::BufferUsage::STATIC);
        rhi::BufferDescriptor desc(data, static_cast<size_t>(indexCount) * sizeof(uint32_t));
        driver.updateIndexBuffer(ibh, std::move(desc), 0);
        return ibh;
    };

    switch (mesh.type()) {
    case MeshPrimitiveType::Mesh: {
        TriangleList const* triangles = mesh.triangles();
        if (!triangles || triangles->isEmpty())
            break;
        auto const& idx = triangles->indices();
        const uint32_t indexCount = static_cast<uint32_t>(idx.size());
        auto ibh = createIbo(indexCount, idx.data());
        auto primitive = driver.createRenderPrimitive(vbh, ibh, rhi::PrimitiveType::TRIANGLES);
        auto surface = std::make_unique<SurfaceGeometry>(driver, ibh, indexCount, SurfaceType::Opaque, mesh.isPlanar(), false);
        surface->setPrimitive(primitive);
        result->addSurface(std::move(surface));
        break;
    }
    case MeshPrimitiveType::Polyline: {
        // Faithful thick-line path (ported from itwinjs-core Polyline.ts
        // PolylineGeometry.create :142-152 + CachedGeometry.ts PolylineBuffers
        // :1123-1158). For each polyline:
        //   1) gather the line's sequential points from the mesh vertex pool
        //      (PolylineTesselator/VertexTableBuilder take a sequential [0..n-1]
        //      point array — PolylineParams.ts:103-104 / VertexTableBuilder.ts
        //      buildFromPolylines);
        //   2) VertexTableBuilder::buildFromPolylines → transposed-SoA LUT data;
        //   3) VertexLutTexture::create → u_vertLUT sampler;
        //   4) PolylineTesselator::tesselate → corner buffer (24-bit indices +
        //      param byte) consumed by the GPU Polyline technique;
        //   5) upload 3 VBOs as a RenderPrimitiveHandle (TRIANGLES, no IBO) —
        //      the multi-VBO path matches itwinjs PolylineBuffers verbatim.
        MeshPolylineList const* polylines = mesh.polylines();
        if (!polylines) break;
        const float width = static_cast<float>(mesh.displayParams().width());
        // Uniform line color (DisplayParams.lineColor — each addLineString used
        // one setSymbology, so every polyline in this Mesh shares one color).
        // Drives u_color (Polyline.ts:129-131 uniform-color path).
        dqCommon::ColorDef const uniformColor = mesh.displayParams().lineColor();
        auto const& meshPoints = pts;  // mesh.points().points (built at top of fn)

        for (auto const& poly : *polylines) {
            if (poly.indices.size() < 2) continue;

            // (1) Sequential polyline vertex pool — deref poly.indices into the
            // mesh point pool. PolylineTesselator/VertexTableBuilder operate on
            // a sequential [0..n-1] point array (no index list).
            std::vector<dqGeom::Point3d> polyPoints;
            polyPoints.reserve(poly.indices.size());
            for (uint32_t idx : poly.indices) {
                polyPoints.push_back(meshPoints[idx]);
            }
            uint32_t const numPolyVerts = static_cast<uint32_t>(polyPoints.size());

            // (2) VertexTableBuilder → transposed LUT data (6 RGBA8/vertex).
            BuiltVertexTable vt = VertexTableBuilder::buildFromPolylines(
                polyPoints.data(), numPolyVerts, uniformColor);

            // (3) VertexLutTexture (owns GL texture + width/height/numRgba/numVerts).
            VertexLutTexture lut;
            lut.create(driver, vt.data.data(), vt.width, vt.height, vt.numVertices, vt.numRgbaPerVertex);

            // (4) PolylineTesselator → 3 corner arrays (3/3/4 bytes/corner).
            TesselatedPolyline tess = PolylineTesselator::tesselate(
                polyPoints.data(), numPolyVerts, width,
                /*is2d*/ false, /*disjoint*/ false);
            uint32_t const numCorners = tess.numCorners();
            if (numCorners == 0) continue;

            // (5) Corner VBOs — multi-VBO layout faithful to PolylineBuffers
            // (CachedGeometry.ts:1141-1146). 3 BufferObjects + a 4-attribute
            // VertexBufferInfo whose attribute-order indices ARE the technique's
            // a_pos/a_prevIndex/a_nextIndex/a_param locations (0/1/2/3), since
            // OpenGLDriver::bindRenderPrimitive uses the array index i as the
            // attrib location. Strides auto-summed per buffer index
            // (strides[0]=3, strides[1]=3, strides[2]=4) — matching the reference
            // strides 0/0/4/4 (WebGL stride=0 = tightly packed = attr size).
            //   buffer 0 (indices):              a_pos       = UBYTE3 @ offset 0
            //   buffer 1 (prevIndices):          a_prevIndex = UBYTE3 @ offset 0
            //   buffer 2 (nextIndicesAndParams): a_nextIndex = UBYTE3 @ offset 0
            //                                    a_param     = UBYTE  @ offset 3
            auto makeVbo = [&driver](uint8_t const* bytes, uint32_t byteCount) {
                auto boh = driver.createBufferObject(byteCount,
                    rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
                rhi::BufferDescriptor desc(bytes, static_cast<size_t>(byteCount));
                driver.updateBufferObject(boh, std::move(desc), 0);
                return boh;
            };
            uint32_t const posBytes = numCorners * 3u;
            uint32_t const prevBytes = numCorners * 3u;
            uint32_t const nextPropsBytes = numCorners * 4u;
            auto posVbo = makeVbo(tess.indices.data(), posBytes);
            auto prevVbo = makeVbo(tess.prevIndices.data(), prevBytes);
            auto nextPropsVbo = makeVbo(tess.nextIndicesAndParams.data(), nextPropsBytes);

#pragma warning(suppress : 4456) // 内层作用域 attrs 与参考同名（分作用域各自构造），遮蔽外层局部仅 MSVC /W4 提示
            rhi::AttributeArray attrs = {};
            attrs[0].buffer = 0;                              // a_pos       (location 0)
            attrs[0].offset = 0;
            attrs[0].type = rhi::ElementType::UBYTE3;
            attrs[1].buffer = 1;                              // a_prevIndex (location 1)
            attrs[1].offset = 0;
            attrs[1].type = rhi::ElementType::UBYTE3;
            attrs[2].buffer = 2;                              // a_nextIndex (location 2)
            attrs[2].offset = 0;
            attrs[2].type = rhi::ElementType::UBYTE3;
            attrs[3].buffer = 2;                              // a_param     (location 3)
            attrs[3].offset = 3;
            attrs[3].type = rhi::ElementType::UBYTE;
            auto cornerVbih = driver.createVertexBufferInfo(3, 4, attrs);
            auto cornerVbh = driver.createVertexBuffer(numCorners, cornerVbih);
            driver.setVertexBufferObject(cornerVbh, 0, posVbo);
            driver.setVertexBufferObject(cornerVbh, 1, prevVbo);
            driver.setVertexBufferObject(cornerVbh, 2, nextPropsVbo);

            // No element index buffer: glDrawArrays(GL_TRIANGLES) reads the
            // corner VAO directly. createRenderPrimitive takes an empty IBO
            // handle (itwinjs PolylineBuffers has no indices buffer either).
            auto primitive = driver.createRenderPrimitive(
                cornerVbh, rhi::IndexBufferHandle{}, rhi::PrimitiveType::TRIANGLES);

            auto pg = std::make_unique<PolylineGeometry>(
                driver, std::move(lut), primitive,
                cornerVbh, cornerVbih, posVbo, prevVbo, nextPropsVbo,
                numCorners, width, uniformColor);
            result->addPolyline(std::move(pg));
        }
        break;
    }
    case MeshPrimitiveType::Point: {
        std::vector<uint32_t> idx(vc);
        for (uint32_t i = 0; i < vc; ++i) idx[i] = i;
        auto ibh = createIbo(vc, idx.data());
        auto primitive = driver.createRenderPrimitive(vbh, ibh, rhi::PrimitiveType::POINTS);
        const float weight = static_cast<float>(mesh.displayParams().width());
        auto ps = std::make_unique<PointStringGeometry>(driver, ibh, vc, weight);
        ps->setPrimitive(primitive);
        result->addPointString(std::move(ps));
        break;
    }
    }

    return result;
}

END_DQ_RENDER_NAMESPACE
