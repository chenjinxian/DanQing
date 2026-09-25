// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PolyfaceGraphic implementation
// Ported from: itwinjs-core core/frontend/src/render/Polyface.ts
//
// Converts IndexedPolyface (dqGeom) → GPU geometry (dqRender).
// Fan triangulation for n-gon faces.
#include "PolyfaceGraphic.h"

#include <dqGeom/PolyfaceQuery.h>
#include "gl/GL.h"
#include "rhi/opengl/GlLoader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

BEGIN_DQ_RENDER_NAMESPACE

PolyfaceGraphic::PolyfaceGraphic(rhi::Driver& driver,
                                  dqGeom::IndexedPolyface const& polyface,
                                  uint32_t defaultColor, uint32_t featureId)
    : m_driver(driver)
    , m_featureId(featureId)
{
    buildFromPolyface(polyface, defaultColor);
    uploadToGpu(driver);
}

PolyfaceGraphic::~PolyfaceGraphic()
{
    if (m_primitive) m_driver.destroyRenderPrimitive(m_primitive);
    if (m_vbo) m_driver.destroyBufferObject(m_vbo);
    if (m_vbh) m_driver.destroyVertexBuffer(m_vbh);
    if (m_ibh) m_driver.destroyIndexBuffer(m_ibh);
    if (m_vbih) m_driver.destroyVertexBufferInfo(m_vbih);
    // External ownership (GltfDecoration resolvedTextures cache): the cache
    // owner disposes — this graphic must NOT double-destroy (use-after-free
    // on the cache's surviving handle).
    if (m_texture && !m_textureExternal) m_driver.destroyTexture(m_texture);
    if (m_normalMapTexture && !m_normalMapTextureExternal)
        m_driver.destroyTexture(m_normalMapTexture);
}

// ---------------------------------------------------------------------------
// Build interleaved vertex + index arrays from polyface data
// ---------------------------------------------------------------------------
void PolyfaceGraphic::buildFromPolyface(dqGeom::IndexedPolyface const& polyface,
                                         uint32_t defaultColor)
{
    auto const& data = polyface.Data();
    bool hasNormals = data.NormalCount() > 0;
    bool hasColors = data.ColorCount() > 0;

    // Default normal (up)
    float defNormal[3] = {0.0f, 0.0f, 1.0f};

    // Unpack default color. The packing convention is (a<<24)|(b<<16)|(g<<8)|r
    // — i.e. little-endian RGBA byte order (GL/itwinjs Color32 convention;
    // every caller packs this way, e.g. RealityTile/GltfDecoration
    // `color = (a << 24) | (b << 16) | (g << 8) | r`). TD-17 (2026-09-23):
    // this unpack previously read (r<<24)|(g<<16)|(b<<8)|a — channel-shifted,
    // which the textured paths masked (texture supplies the color) while
    // untextured solid colors rendered wrong (green→magenta, blue→yellow,
    // alpha-in-low-byte → transparent→black).
    float defR = static_cast<float>((defaultColor >> 0) & 0xFF) / 255.0f;
    float defG = static_cast<float>((defaultColor >> 8) & 0xFF) / 255.0f;
    float defB = static_cast<float>((defaultColor >> 16) & 0xFF) / 255.0f;
    float defA = static_cast<float>((defaultColor >> 24) & 0xFF) / 255.0f;

    // For each facet, fan-triangulate and emit vertices
    for (size_t facet = 0; facet < polyface.FacetCount(); ++facet) {
        size_t i0 = polyface.FacetIndex0(facet);
        size_t i1 = polyface.FacetIndex1(facet);
        size_t numEdge = i1 - i0;
        if (numEdge < 3) continue;

        // Compute facet normal if no per-vertex normals
        float facetNormal[3] = {defNormal[0], defNormal[1], defNormal[2]};
        if (!hasNormals) {
            dqGeom::Vector3d normal;
            if (dqGeom::PolyfaceQuery::ComputeFacetUnitNormal(polyface, facet, normal)) {
                facetNormal[0] = static_cast<float>(normal.x);
                facetNormal[1] = static_cast<float>(normal.y);
                facetNormal[2] = static_cast<float>(normal.z);
            }
        }

        // Fan triangulation: apex = vertex 0, triangles (0, i, i+1)
        uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());

        // Emit all vertices of this facet
        for (size_t i = 0; i < numEdge; ++i) {
            int32_t ptIdx = data.pointIndex[static_cast<int>(i0 + i)];
            if (ptIdx == 0) continue;

            dqGeom::Point3d pt = data.GetPoint(ptIdx);
            Vertex v;
            v.position[0] = static_cast<float>(pt.x);
            v.position[1] = static_cast<float>(pt.y);
            v.position[2] = static_cast<float>(pt.z);

            // normal — normalIndex/colorIndex 与 pointIndex 同长（imodel-native
            // PolyfaceData 语义：per-corner 索引数组等长或为空）。保护条件必须与
            // paramIndex 同式（(i0+i) < 数组长度）——此前用 `i < size`（facet 内
            // 索引与全局长度比较）在 facet 位于索引数组尾部且 numEdge < size 时
            // 越界读（ABeautifulGame 29162 顶点 121440 索引 SEH 0xc0000005）。
            if (hasNormals && (i0 + i) < static_cast<size_t>(data.normalIndex.size())) {
                int32_t nIdx = data.normalIndex[static_cast<int>(i0 + i)];
                if (nIdx > 0) {
                    dqGeom::Vector3d n = data.GetNormal(nIdx);
                    v.normal[0] = static_cast<float>(n.x);
                    v.normal[1] = static_cast<float>(n.y);
                    v.normal[2] = static_cast<float>(n.z);
                } else {
                    v.normal[0] = facetNormal[0];
                    v.normal[1] = facetNormal[1];
                    v.normal[2] = facetNormal[2];
                }
            } else {
                v.normal[0] = facetNormal[0];
                v.normal[1] = facetNormal[1];
                v.normal[2] = facetNormal[2];
            }

            // Color（同 normal 的下标保护修正）
            if (hasColors && (i0 + i) < static_cast<size_t>(data.colorIndex.size())) {
                int32_t cIdx = data.colorIndex[static_cast<int>(i0 + i)];
                if (cIdx > 0) {
                    uint32_t c = data.GetColor(cIdx);
                    // Same (a<<24)|(b<<16)|(g<<8)|r packing convention.
                    v.color[0] = static_cast<float>((c >> 0) & 0xFF) / 255.0f;
                    v.color[1] = static_cast<float>((c >> 8) & 0xFF) / 255.0f;
                    v.color[2] = static_cast<float>((c >> 16) & 0xFF) / 255.0f;
                    v.color[3] = static_cast<float>((c >> 24) & 0xFF) / 255.0f;
                } else {
                    v.color[0] = defR; v.color[1] = defG;
                    v.color[2] = defB; v.color[3] = defA;
                }
            } else {
                v.color[0] = defR; v.color[1] = defG;
                v.color[2] = defB; v.color[3] = defA;
            }

            // UV from PolyfaceData params (Ported from: PolyfaceData.param per-corner)
            bool const hasParams = data.ParamCount() > 0;
            if (hasParams && (i0 + i) < data.paramIndex.size()) {
                int32_t const uvIdx = data.paramIndex[static_cast<int>(i0 + i)];
                if (uvIdx > 0) {
                    dqGeom::Point2d const uv = data.GetParam(uvIdx);
                    v.texCoord[0] = static_cast<float>(uv.x);
                    v.texCoord[1] = static_cast<float>(uv.y);
                } else {
                    v.texCoord[0] = 0.0f;
                    v.texCoord[1] = 0.0f;
                }
            } else {
                v.texCoord[0] = 0.0f;
                v.texCoord[1] = 0.0f;
            }

            // Feature ID
            v.featureId = m_featureId;

            m_vertices.push_back(v);
        }

        // Emit triangle indices (fan: 0, i, i+1)
        uint32_t vertCount = static_cast<uint32_t>(m_vertices.size() - baseIndex);
        for (uint32_t i = 1; i + 1 < vertCount; ++i) {
            m_indices.push_back(baseIndex);
            m_indices.push_back(baseIndex + i);
            m_indices.push_back(baseIndex + i + 1);
        }
    }

    m_vertexCount = static_cast<uint32_t>(m_vertices.size());
    m_indexCount = static_cast<uint32_t>(m_indices.size());

    // TEMP-DIAG（贴图 U 翻转 saga）：dump GPU 侧顶点（位置+UV），env 门控。
    if (getenv("DANQING_UV_TRACE")) {
        for (size_t i = 0; i < m_vertices.size() && i < 12; ++i) {
            printf("[PGV] v%zu pos=(%.2f,%.2f,%.2f) uv=(%.2f,%.2f)\n", i,
                   m_vertices[i].position[0], m_vertices[i].position[1], m_vertices[i].position[2],
                   m_vertices[i].texCoord[0], m_vertices[i].texCoord[1]);
        }
    }
}

// ---------------------------------------------------------------------------
// Upload to GPU via RHI Driver
// ---------------------------------------------------------------------------
void PolyfaceGraphic::uploadToGpu(rhi::Driver& driver)
{
    if (m_vertices.empty() || m_indices.empty()) return;

    // Vertex attributes: position(3f) + normal(3f) + color(4f) + texCoord(2f) + featureId(1ui) = 52 bytes
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
    attrs[3].type = rhi::ElementType::FLOAT2;  // texCoord
    attrs[4].buffer = 0;
    attrs[4].offset = 48;
    attrs[4].type = rhi::ElementType::UINT;    // featureId

    m_vbih = driver.createVertexBufferInfo(1, 5, attrs);
    m_vbh = driver.createVertexBuffer(m_vertexCount, m_vbih);

    // Upload vertex data
    size_t vertexDataSize = m_vertexCount * sizeof(Vertex);
    m_vbo = driver.createBufferObject(
        static_cast<uint32_t>(vertexDataSize),
        rhi::BufferObjectBinding::VERTEX,
        rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor vboData(m_vertices.data(), vertexDataSize);
    driver.updateBufferObject(m_vbo, std::move(vboData), 0);
    driver.setVertexBufferObject(m_vbh, 0, m_vbo);

    // Upload index data — wire the bytes INTO m_ibh's GL buffer (the element
    // buffer the draw binds as GL_ELEMENT_ARRAY_BUFFER). createIndexBuffer only
    // does glGenBuffers (no data store); updateIndexBuffer does glBufferData on
    // it. Previously the bytes went to a disconnected throwaway BufferObjectHandle,
    // leaving m_ibh unbacked — glDrawElements read it and faulted (SIGSEGV at 0).
    m_ibh = driver.createIndexBuffer(
        rhi::ElementType::UINT, m_indexCount, rhi::BufferUsage::STATIC);
    rhi::BufferDescriptor iboData(m_indices.data(), m_indexCount * sizeof(uint32_t));
    driver.updateIndexBuffer(m_ibh, std::move(iboData), 0);

    m_primitive = driver.createRenderPrimitive(m_vbh, m_ibh, rhi::PrimitiveType::TRIANGLES);
}

void PolyfaceGraphic::draw(rhi::Driver& driver)
{
    if (!m_primitive) return;
    driver.bindRenderPrimitive(m_primitive);
    // TEMP-DIAG（U 翻转 saga）：绘制瞬间读回 attrib3 的 GL 指针状态 + 该偏移
    // 处的实际 16 字节（pos+uv 配对的最终对质），env 门控。
    if (getenv("DANQING_DRAW_TRACE")) {
        static int n = 0;
        if (n++ < 6) {
            GLint enabled = 0, size = 0, stride = 0, type = 0;
            void* ptr = nullptr;
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
            dqglGetVertexAttribPointerv(3, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
            GLint boundAB = 0;
            dqglGetIntegerv(GL_ARRAY_BUFFER_BINDING, &boundAB);
            // TEMP-DIAG：attrib 各自真正绑定的 GL buffer（VAO 视角）——若 attrib0/3
            // 不同 buffer，即"读回的字节 ≠ 绘制抓取的字节"实锤。
            GLint bufA0 = 0, bufA3 = 0, bufA2 = 0;
            dqglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufA0);
            dqglGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufA2);
            dqglGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufA3);
            GLint curVao = 0;
            dqglGetIntegerv(GL_VERTEX_ARRAY_BINDING, &curVao);
            printf("[DRAW3] enabled=%d size=%d stride=%d type=0x%x offset=%ld boundAB=%u vao=%u bufA0=%u bufA2=%u bufA3=%u\n",
                   enabled, size, stride, type, static_cast<long>(reinterpret_cast<intptr_t>(ptr)),
                   boundAB, curVao, bufA0, bufA2, bufA3);
        }
    }
    driver.draw2(0, m_indexCount, 1);
}

void PolyfaceGraphic::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core Polyface.ts collectStatistics
    stats.addSurface(
                    static_cast<uint64_t>(m_vertexCount * sizeof(Vertex)));
    stats.addSurface(
                    static_cast<uint64_t>(m_indexCount * sizeof(uint32_t)));
}

END_DQ_RENDER_NAMESPACE
