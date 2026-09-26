// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Indexed edge geometry for silhouette / feature edges
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IndexedEdgeGeometry.ts
#include "IndexedEdgeGeometry.h"

#include "dqRender/rhi/Driver.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EdgeLUT
// ---------------------------------------------------------------------------

// Ported from: IndexedEdgeGeometry.ts EdgeLUT.create (:46-49)——
// TextureHandle.createForData(width, height, data)；创建失败（null 纹理）→
// undefined 的 C++ 形态 = 无效 EdgeLUT（无半成品自持）。
EdgeLUT EdgeLUT::create(rhi::Driver& driver, uint8_t const* data,
                        uint32_t width, uint32_t height,
                        uint32_t numSegments, uint32_t silhouettePadding)
{
    EdgeLUT lut;
    if (!data || width == 0 || height == 0)
        return lut;
    lut.m_texture = driver.createTexture(
        rhi::SamplerType::SAMPLER_2D, 1, rhi::TextureFormat::RGBA8,
        width, height, 1, rhi::TextureUsage::DEFAULT);
    if (!lut.m_texture)
        return lut;
    uint32_t const texBytes = width * height * 4u;
    rhi::PixelBufferDescriptor pbd(data, texBytes, 0, 0, 0, 0, 0, 0, width, height, 0);
    driver.setTextureData(lut.m_texture, 0, 0, 0, 0, width, height, 0, std::move(pbd));
    lut.m_width = width;
    lut.m_height = height;
    lut.m_numSegments = numSegments;
    lut.m_silhouettePadding = silhouettePadding;
    lut.m_bytesUsed = texBytes;  // :51-53 bytesUsed = texture.bytesUsed（RGBA8）
    return lut;
}

// :42-44 [Symbol.dispose] → dispose(texture)。§12.9：同步失效全部缓存成员。
void EdgeLUT::dispose(rhi::Driver& driver)
{
    if (m_texture)
        driver.destroyTexture(m_texture);
    m_texture = rhi::TextureHandle{};
    m_width = 0;
    m_height = 0;
    m_numSegments = 0;
    m_silhouettePadding = 0;
    m_bytesUsed = 0;
}

// ---------------------------------------------------------------------------
// IndexedEdgeGeometry
// ---------------------------------------------------------------------------

// Ported from: IndexedEdgeGeometry.ts ctor (:74-86) + create (:98-102)。
// BO/VAO 由创建点（ImdlGraphics.cpp）组装后注入；本几何按参考 _draw 语义
// bindRenderPrimitive + drawArrays。
IndexedEdgeGeometry::IndexedEdgeGeometry(rhi::Driver& driver, EdgeLUT edgeLut,
                                         rhi::BufferObjectHandle indices,
                                         uint32_t numIndices,
                                         VertexLutTexture const& vertexLut,
                                         float edgeWeight, uint32_t edgeLineCode,
                                         ColorInfo colorInfo, bool isPlanar)
    : MeshGeometry(numIndices, SurfaceType::Unknown, FillFlags::None, isPlanar, false, false)
    , m_driver(&driver)
    , m_edgeLut(std::move(edgeLut))
    , m_indices(indices)
    , m_colorInfo(std::move(colorInfo))
{
    setLut(&vertexLut);
    setEdgeWidth(edgeWeight);
    setEdgeLineCode(edgeLineCode);
}

// 释放 GL 资源（参考 :88-96 [Symbol.dispose]：dispose(_buffers) + dispose(_indices)
// + dispose(edgeLut)）。DanQing RHI：VAO/primitive + 顶点缓冲 + 索引 BO 于此释放，
// 边 LUT 纹理经 EdgeLUT::dispose。destroy({}) 对 nullid 为空调用（无 if 守卫，
// NullDriver 测试形态统一——PolylineGeometry::~PolylineGeometry 先例）。
IndexedEdgeGeometry::~IndexedEdgeGeometry()
{
    if (m_driver == nullptr)
        return;
    m_edgeLut.dispose(*m_driver);
    m_driver->destroyRenderPrimitive(m_primitive);
    m_driver->destroyVertexBuffer(m_vbh);
    m_driver->destroyVertexBufferInfo(m_vbih);
    m_driver->destroyBufferObject(m_indices);
}

// Ported from: IndexedEdgeGeometry.ts _draw (:109-114)——buffers.bind() +
// System.instance.drawArrays(GL.PrimitiveType.Triangles, 0, numIndices)。
// DanQing RHI 等价：bindRenderPrimitive + drawArrays（无 element index buffer；
// SurfaceGeometry::draw / EdgeGeometry::draw 同款）。
void IndexedEdgeGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.drawArrays(0, getNumIndices(), 0);
    }
}

// Ported from: IndexedEdgeGeometry.ts collectStatistics (:104-107)——
// stats.addIndexedEdges(this._indices.bytesUsed) + stats.addEdgeTable(this.edgeLut.bytesUsed)。
// indices.bytesUsed = 3B/顶点（UBYTE3 流——:80 UnsignedByte×3）。
void IndexedEdgeGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    stats.addIndexedEdges(static_cast<uint64_t>(getNumIndices()) * 3u);
    stats.addEdgeTable(static_cast<uint64_t>(m_edgeLut.getBytesUsed()));
}

END_DQ_RENDER_NAMESPACE
