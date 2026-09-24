// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface/Edge/Polyline/PointCloud/PointString geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/
#include "SurfaceGeometry.h"
#include "dqRender/RenderMemory.h"

#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceGeometry
// ---------------------------------------------------------------------------
SurfaceGeometry::SurfaceGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                                   uint32_t numIndices, SurfaceType surfaceType,
                                   bool isPlanar, bool hasTextures)
    : MeshGeometry(numIndices, surfaceType, FillFlags::Lit, isPlanar, hasTextures, true)
    , m_driver(driver)
    , m_ibh(ibh)
    , m_numIndices(numIndices)
{
}

// Release this geometry's GL resources. Safe unconditionally:
// HandleAllocator::deallocate (HandleAllocator.h:81) and handle_cast early-return
// on a nullid handle, so destroy({}) is a no-op on every Driver. Unguarded
// (no `if (m_ibh)`) so the release path is uniform and unit-testable with a
// NullDriver-based mock (whose createXxx return {} ).
// Mirrors PolyfaceGraphic::~PolyfaceGraphic (PolyfaceGraphic.cpp:24-30).
SurfaceGeometry::~SurfaceGeometry()
{
    m_driver.destroyIndexBuffer(m_ibh);
    m_driver.destroyRenderPrimitive(m_primitive);
}

// Ported from: itwinjs-core MeshGeometry.computeSurfaceFlags()
// Derives the 12 SurfaceBitIndex flags from geometry state. Step 1 fills the
// flags derivable from CachedGeometry/MeshGeometry base interfaces; Step 2
// adds HasNormalMap (wantNormalMaps, SurfaceGeometry.ts :416-428);
// BackgroundFill/OverrideRgb/ConstantLod* remain for their feature passes.
int const* SurfaceGeometry::computeSurfaceFlags(CachedGeometry const& geom,
                                                bool displayNormalMaps,
                                                ViewFlags const& viewFlags)
{
    using GL::SurfaceBitIndex;
    thread_local int s_flags[static_cast<size_t>(SurfaceBitIndex::Count)] = {};
    std::memset(s_flags, 0, sizeof(s_flags));

    // Surface-type / texture / fill come from MeshGeometry (via asMesh()).
    MeshGeometry const* mesh = const_cast<CachedGeometry&>(geom).asMesh();
    bool hasTexture = false;
    bool applyLighting = false;
    bool translucent = false;
    if (mesh != nullptr) {
        hasTexture = mesh->hasTextures();
        SurfaceType const st = mesh->getSurfaceType();
        uint8_t const ffBits = static_cast<uint8_t>(mesh->getFillFlags());
        applyLighting = (st != SurfaceType::Unknown) ||
                        ((ffBits & static_cast<uint8_t>(FillFlags::Lit)) != 0);
        translucent = (st == SurfaceType::Translucent) ||
                      ((ffBits & static_cast<uint8_t>(FillFlags::AlwaysTranslucent)) != 0);
    } else {
        // PolyfaceGraphic path: texture presence via getSurfaceTexture()
        // (Surface.ts addTexture routing — spec §4.2 decision 1).
        hasTexture = (geom.getSurfaceTexture() != rhi::TextureHandle{});
        applyLighting = (geom.getTechniqueId() == TechniqueId::Surface);
    }

    bool const ignoreMaterial = (geom.getMaterialInfo() == nullptr);
    bool const hasMaterialAtlas = geom.hasMaterialAtlas();

    s_flags[static_cast<size_t>(SurfaceBitIndex::hasTexture)] = hasTexture ? 1 : 0;
    s_flags[static_cast<size_t>(SurfaceBitIndex::ApplyLighting)] = applyLighting ? 1 : 0;
    s_flags[static_cast<size_t>(SurfaceBitIndex::HasNormals)] = 1;          // SurfaceGeometry supplies a_normal
    s_flags[static_cast<size_t>(SurfaceBitIndex::IgnoreMaterial)] = ignoreMaterial ? 1 : 0;
    s_flags[static_cast<size_t>(SurfaceBitIndex::TransparencyThreshold)] = translucent ? 1 : 0;
    s_flags[static_cast<size_t>(SurfaceBitIndex::HasColorAndNormal)] = 1;
    s_flags[static_cast<size_t>(SurfaceBitIndex::HasMaterialAtlas)] = hasMaterialAtlas ? 1 : 0;

    // HasNormalMap — Ported from: itwinjs-core SurfaceGeometry.ts :348 +
    // wantNormalMaps (:416-428). hasNormalMap (:130) requires isLit &&
    // isTexturedType && a normal map texture; wantNormalMaps gates on
    // target.displayNormalMaps (Target.ts :158, default true), renderMode ==
    // SmoothShade, and viewFlags.textures. The isTexturedType term maps to
    // applyLighting's technique check on the PolyfaceGraphic path (lit surface).
    bool const useNormalMap =
        applyLighting &&
        (geom.getNormalMapTexture() != rhi::TextureHandle{}) &&
        displayNormalMaps &&
        viewFlags.renderMode == RenderMode::SmoothShade &&
        viewFlags.textures;
    s_flags[static_cast<size_t>(SurfaceBitIndex::HasNormalMap)] = useNormalMap ? 1 : 0;

    return s_flags;
}

Pass SurfaceGeometry::getPass() const noexcept
{
    if (isPlanar()) return Pass::OpaquePlanar;
    return Pass::Opaque;
}

RenderOrder SurfaceGeometry::getRenderOrder() const noexcept
{
    return RenderOrder::LitSurface;
}

void SurfaceGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.draw2(0, m_numIndices, 0);
    }
}

void SurfaceGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core SurfaceGeometry.ts collectStatistics
    // Approximate vertex + index buffer sizes.
    stats.addSurface( static_cast<uint64_t>(m_numIndices * 12));
    stats.addSurface( static_cast<uint64_t>(m_numIndices * 4));
}

// ---------------------------------------------------------------------------
// EdgeGeometry
// ---------------------------------------------------------------------------
EdgeGeometry::EdgeGeometry(uint32_t numIndices)
    : MeshGeometry(numIndices, SurfaceType::Unknown, FillFlags::None, false, false, false)
    , m_numIndices(numIndices)
{
}

EdgeGeometry::~EdgeGeometry() = default;

void EdgeGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.draw2(0, m_numIndices, 0);
    }
}

void EdgeGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core EdgeGeometry.ts collectStatistics
    stats.addSurface( static_cast<uint64_t>(m_numIndices * 12));
    stats.addSurface( static_cast<uint64_t>(m_numIndices * 4));
}

// ---------------------------------------------------------------------------
// PolylineGeometry
// ---------------------------------------------------------------------------
PolylineGeometry::PolylineGeometry(rhi::Driver& driver, VertexLutTexture lut,
                                     rhi::RenderPrimitiveHandle primitive,
                                     rhi::VertexBufferHandle cornerVbh,
                                     rhi::VertexBufferInfoHandle cornerVbih,
                                     rhi::BufferObjectHandle posVbo,
                                     rhi::BufferObjectHandle prevVbo,
                                     rhi::BufferObjectHandle nextPropsVbo,
                                     uint32_t numCorners, float lineWidth,
                                     dqCommon::ColorDef color)
    : m_driver(driver)
    , m_lut(std::move(lut))
    , m_primitive(primitive)
    , m_cornerVbh(cornerVbh)
    , m_cornerVbih(cornerVbih)
    , m_posVbo(posVbo)
    , m_prevVbo(prevVbo)
    , m_nextPropsVbo(nextPropsVbo)
    , m_numCorners(numCorners)
    , m_lineWidth(lineWidth)
    , m_color(color)
{
}

// Release this geometry's GL resources. Safe unconditionally:
// HandleAllocator::deallocate (HandleAllocator.h:81) and handle_cast early-return
// on a nullid handle, so destroy({}) is a no-op on every Driver. Unguarded
// (no `if (m_xxx)`) so the release path is uniform and unit-testable with a
// NullDriver-based mock (whose createXxx return {} ).
// Mirrors PolylineGeometry [Symbol.dispose] (Polyline.ts:62-67): dispose(lut) +
// dispose(_buffers); PolylineBuffers.[Symbol.dispose] (CachedGeometry.ts:1171-1176):
// dispose(buffers/indices/prevIndices/nextIndicesAndParams). The RHI primitive
// owns only the VAO, so the corner VBOs/VBIH/VBH are released here.
PolylineGeometry::~PolylineGeometry()
{
    m_lut.destroy(m_driver);
    m_driver.destroyRenderPrimitive(m_primitive);
    m_driver.destroyVertexBuffer(m_cornerVbh);
    m_driver.destroyVertexBufferInfo(m_cornerVbih);
    m_driver.destroyBufferObject(m_posVbo);
    m_driver.destroyBufferObject(m_prevVbo);
    m_driver.destroyBufferObject(m_nextPropsVbo);
}

// Ported from: itwinjs-core Polyline.ts _draw (line 133-140). bind+drawArrays
// of GL_TRIANGLES over numIndices corners (the triangle-list expansion of the
// polyline). Uses drawArrays (not draw2) because PolylineBuffers has NO element
// index buffer — glDrawArrays reads the corner VAO directly. Mirrors
// PointStringGeometry::draw (SurfaceGeometry.cpp:225-231).
// The LUT texture is bound separately by the SceneCompositor dispatch
// (u_vertLUT); the geometry only issues the VAO bind + draw.
void PolylineGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.drawArrays(0, m_numCorners, 0);
    }
}

void PolylineGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core Polyline.ts collectStatistics (line 69-72):
    //   this._buffers.collectStatistics(stats, BufferType.Polylines);
    //   stats.addVertexTable(this.lut.bytesUsed);
    // Corner buffer = 3 VBOs (indices + prevIndices + nextIndicesAndParams) at
    // 3/3/4 bytes/corner ≈ 10 bytes/corner; LUT ≈ 24 bytes/vertex.
    stats.addSurface( static_cast<uint64_t>(m_numCorners) * 10u);
    // stats.addVertexTable(this.lut.bytesUsed) — VertexLutTexture is an RGBA8
    // 2D texture (VertexLutTexture.cpp:39-41), so lut.bytesUsed = texWidth *
    // texHeight * 4 (faithful to VertexLUT.ts:53 → texture.bytesUsed). The
    // 2D texture may be padded larger than numVertices*numRgbaPerVert*4
    // (VertexLutTexture.cpp:46-55); the upload covers the whole texture, so
    // count texWidth*texHeight*4, not the vertex-data bytes. The reference
    // tracks this under ConsumerType::VertexTables, but this Statistics model
    // has no vertex-table consumer — track as BufferType::Texture, mirroring
    // IndexedEdgeGeometry::collectStatistics (IndexedEdgeGeometry.cpp:67-68
    // accounts its edge LUT the same way).
    auto const& lp = m_lut.getParams();
    stats.addTexture(
                    static_cast<uint64_t>(lp.texWidth) * lp.texHeight * 4u);
}

// ---------------------------------------------------------------------------
// PointCloudGeometry
// ---------------------------------------------------------------------------
PointCloudGeometry::PointCloudGeometry(uint32_t vertexCount, float voxelSize)
    : m_vertexCount(vertexCount)
    , m_voxelSize(voxelSize)
{
}

PointCloudGeometry::~PointCloudGeometry() = default;

void PointCloudGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.drawArrays(0, m_vertexCount, 0);
    }
}

void PointCloudGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core PointCloud.ts collectStatistics
    stats.addSurface( static_cast<uint64_t>(m_vertexCount * 16));
}

// ---------------------------------------------------------------------------
// PointStringGeometry
// ---------------------------------------------------------------------------
PointStringGeometry::PointStringGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                                          uint32_t vertexCount, float weight)
    : m_driver(driver)
    , m_ibh(ibh)
    , m_vertexCount(vertexCount)
    , m_weight(weight)
{
}

// Release this geometry's GL resources. Safe unconditionally:
// HandleAllocator::deallocate (HandleAllocator.h:81) and handle_cast early-return
// on a nullid handle, so destroy({}) is a no-op on every Driver. Unguarded
// (no `if (m_ibh)`) so the release path is uniform and unit-testable with a
// NullDriver-based mock (whose createXxx return {} ).
// Mirrors PolyfaceGraphic::~PolyfaceGraphic (PolyfaceGraphic.cpp:24-30).
PointStringGeometry::~PointStringGeometry()
{
    m_driver.destroyIndexBuffer(m_ibh);
    m_driver.destroyRenderPrimitive(m_primitive);
}

void PointStringGeometry::draw(rhi::Driver& driver)
{
    if (m_primitive) {
        driver.bindRenderPrimitive(m_primitive);
        driver.drawArrays(0, m_vertexCount, 0);
    }
}

void PointStringGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core PointString.ts collectStatistics
    stats.addSurface( static_cast<uint64_t>(m_vertexCount * 12));
}

END_DQ_RENDER_NAMESPACE
