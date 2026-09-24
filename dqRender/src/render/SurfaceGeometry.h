// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/SurfaceGeometry.ts
//
// Concrete surface geometry for rendering solid surfaces.
// The primary geometry type for BIM elements.
#pragma once

#include "MeshGeometry.h"
#include "ViewFlags.h"
#include "dqRender/RenderMemory.h"
#include "VertexLutTexture.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqCommon/ColorDef.h>

#include <algorithm>  // std::clamp (PolylineGeometry::getLineWeight)

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceGeometry — concrete surface geometry
// (Ported from: itwinjs-core SurfaceGeometry.ts)
// ---------------------------------------------------------------------------
class SurfaceGeometry : public MeshGeometry {
public:
    // driver + ibh own this geometry's per-geometry GL resources (ibh is released
    // in the dtor; the render primitive is set via setPrimitive after construction).
    SurfaceGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                    uint32_t numIndices, SurfaceType surfaceType, bool isPlanar, bool hasTextures);
    ~SurfaceGeometry() override;

    // --- Casting accessors (Ported from: itwinjs-core CachedGeometry.ts asSurface/asMesh) ---
    SurfaceGeometry* asSurface() override { return this; }
    MeshGeometry* asMesh() override { return this; }

    SurfaceGeometry(SurfaceGeometry const&) = delete;
    SurfaceGeometry& operator=(SurfaceGeometry const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Surface; }
    Pass getPass() const noexcept override;
    RenderOrder getRenderOrder() const noexcept override;
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    /// Check if this surface is lit.
    bool isLit() const noexcept { return getSurfaceType() != SurfaceType::Unknown; }

    /// Set the render primitive (VAO).
    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

    /// Set the number of indices.
    void setNumIndices(uint32_t count) { m_numIndices = count; }

    /// Derive the 12 SurfaceBitIndex flags from geometry state.
    /// Ported from: itwinjs-core MeshGeometry.computeSurfaceFlags()
    /// Returns a pointer to a thread-local 12-int array (matches itwinjs's
    /// module-level surfaceFlagArray Int32Array pattern).
    /// Step 1 fills: HasTexture/ApplyLighting/HasNormals/IgnoreMaterial/
    /// TransparencyThreshold/HasColorAndNormal/HasMaterialAtlas.
    /// Step 2+ will add: BackgroundFill/OverrideRgb/HasNormalMap/ConstantLod*.
    /// Surface texture (s_texture). Ported from: itwinjs SurfaceGeometry.texture
    void setTexture(rhi::TextureHandle texture) { m_texture = texture; }
    rhi::TextureHandle getTexture() const noexcept { return m_texture; }
    rhi::TextureHandle getSurfaceTexture() const noexcept override { return m_texture; }

    /// Ported from: itwinjs-core SurfaceGeometry.computeSurfaceFlags(params, flags)
    /// — the target-derived gates (displayNormalMaps, currentViewFlags) are
    /// passed explicitly instead of via ShaderProgramParams.target (C++ tests
    /// cannot construct a real TargetImpl). Gates consumed per wantNormalMaps
    /// (SurfaceGeometry.ts :416-428): displayNormalMaps (Target.ts :158,
    /// default true) + renderMode == SmoothShade + viewFlags.textures.
    /// ViewFlags here is the pipeline alias (= dqCommon::ViewFlagsProperties,
    /// field access — ViewFlags.h :36).
    static int const* computeSurfaceFlags(CachedGeometry const& geom,
                                          bool displayNormalMaps,
                                          ViewFlags const& viewFlags);

private:
    rhi::Driver& m_driver;
    rhi::IndexBufferHandle m_ibh;
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::TextureHandle m_texture;
    uint32_t m_numIndices = 0;
};

// ---------------------------------------------------------------------------
// EdgeGeometry — concrete edge geometry
// (Ported from: itwinjs-core EdgeGeometry.ts)
// ---------------------------------------------------------------------------
class EdgeGeometry : public MeshGeometry {
public:
    EdgeGeometry(uint32_t numIndices);
    ~EdgeGeometry() override;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Edge; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Edge; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }
    void setNumIndices(uint32_t count) { m_numIndices = count; }

private:
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_numIndices = 0;
};

// ---------------------------------------------------------------------------
// PolylineGeometry — concrete polyline geometry
// (Ported from: itwinjs-core webgl/Polyline.ts PolylineGeometry)
//
// Faithful thick-line geometry. Owns:
//   - a VertexLutTexture (the per-vertex LUT — positions/transposed SoA layout
//     baked by VertexTableBuilder::buildFromPolylines);
//   - a corner RenderPrimitiveHandle (TRIANGLES, NO element index buffer) built
//     by MeshGraphic from PolylineTesselator's 3 corner arrays (PolylineBuffers,
//     CachedGeometry.ts:1141-1146);
//   - numCorners (triangle-list vertex count — the itwinjs `numIndices` field,
//     Polyline.ts:39);
//   - the line symbology (lineWidth + uniform color).
//
// Draw is glDrawArrays(GL_TRIANGLES, 0, numCorners) (Polyline.ts:133-140 —
// `gl.drawArrays(GL.PrimitiveType.Triangles, 0, this.numIndices)`). The technique
// is TechniqueId::Polyline (Polyline.ts:114) and the VAO must match
// PolylineVariantCompiler's attribute map (a_pos/a_prevIndex/a_nextIndex/a_param
// at locations 0/1/2/3) — flipped in the SAME commit as the VAO change because
// the Surface shader's a_position (float3) is incompatible with the corner
// buffer's a_pos (24-bit LUT index).
// ---------------------------------------------------------------------------
class PolylineGeometry : public CachedGeometry {
public:
    // `driver` owns the GL resources (all destroyed in dtor). Faithful 1:1 with
    // itwinjs Polyline.ts PolylineGeometry constructor + [Symbol.dispose]:
    //   - `lut`                   = VertexLUT (LUT texture + params);
    //   - `primitive`             = PolylineBuffers.buffers (the VAO);
    //   - `cornerVbh`             = VertexBufferHandle that binds the 3 BOs;
    //   - `cornerVbih`            = the 4-attribute VertexBufferInfoHandle;
    //   - `posVbo/prevVbo/nextPropsVbo` = PolylineBuffers.indices/prevIndices/
    //                                     nextIndicesAndParams (CachedGeometry.ts:1148-1150);
    //   - `numCorners`            = numIndices (triangle-list vertex count);
    //   - `lineWidth`             = lineWeight (Polyline.ts:50);
    //   - `color`                 = uniform line color (DisplayParams.lineColor).
    // NullDriver/Mock-based tests pass nullid handles for the GL resources they
    // don't exercise; destroy({}) is a no-op so the dtor is uniform.
    PolylineGeometry(rhi::Driver& driver, VertexLutTexture lut,
                     rhi::RenderPrimitiveHandle primitive,
                     rhi::VertexBufferHandle cornerVbh,
                     rhi::VertexBufferInfoHandle cornerVbih,
                     rhi::BufferObjectHandle posVbo,
                     rhi::BufferObjectHandle prevVbo,
                     rhi::BufferObjectHandle nextPropsVbo,
                     uint32_t numCorners, float lineWidth, dqCommon::ColorDef color);
    ~PolylineGeometry() override;

    PolylineGeometry(PolylineGeometry const&) = delete;
    PolylineGeometry& operator=(PolylineGeometry const&) = delete;

    // --- CachedGeometry interface ---
    // VertexTableBuilder bakes unquantized float positions into the LUT (no
    // QParams3d) → the Polyline technique's Unquantized variant is required.
    bool usesQuantizedPositions() const noexcept override { return false; }
    // Ported from: itwinjs-core Polyline.ts:114 (techniqueId === TechniqueId.Polyline).
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Polyline; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // Per-draw uniform sources (consumed by SceneCompositorImpl Polyline branch).
    // Ported from: itwinjs-core Polyline.ts:123-125 _getLineWeight → this.lineWeight;
    // clamp 1..31 per CachedGeometry.ts:140-150 (prevents 0/inf weight breaking
    // the miter/displacement math). Mirrors PointStringGeometry::getLineWeight
    // (SurfaceGeometry.h:195).
    float getLineWeight() const noexcept override {
        return std::clamp(m_lineWidth, 1.0f, 31.0f);
    }
    // Ported from: itwinjs-core Polyline.ts:129-131 getColor → lut.colorInfo.
    // For the uniform-color path the LUT carries no colorInfo, so the geometry
    // holds the single DisplayParams.lineColor for the dispatch's u_color upload.
    dqCommon::ColorDef getColor() const noexcept { return m_color; }
    // LUT access (u_vertLUT/u_vertParams source for the dispatch).
    VertexLutTexture const& getLut() const noexcept { return m_lut; }

private:
    rhi::Driver& m_driver;
    VertexLutTexture m_lut;
    rhi::RenderPrimitiveHandle m_primitive;
    // Corner buffer GL resources (PolylineBuffers). Owned here because RHI
    // primitives only own the VAO, not the underlying VBOs/VBIH (matches itwinjs
    // PolylineBuffers.[Symbol.dispose] disposing indices/prevIndices/nextIndicesAndParams).
    rhi::VertexBufferHandle m_cornerVbh;
    rhi::VertexBufferInfoHandle m_cornerVbih;
    rhi::BufferObjectHandle m_posVbo;
    rhi::BufferObjectHandle m_prevVbo;
    rhi::BufferObjectHandle m_nextPropsVbo;
    uint32_t m_numCorners = 0;
    float m_lineWidth = 1.0f;
    dqCommon::ColorDef m_color;
};

// ---------------------------------------------------------------------------
// PointCloudGeometry — concrete point cloud geometry
// (Ported from: itwinjs-core PointCloud.ts)
// ---------------------------------------------------------------------------
class PointCloudGeometry : public CachedGeometry {
public:
    PointCloudGeometry(uint32_t vertexCount, float voxelSize);
    ~PointCloudGeometry() override;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::PointCloud; }
    Pass getPass() const noexcept override { return Pass::PointClouds; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    uint32_t getVertexCount() const noexcept { return m_vertexCount; }
    float getVoxelSize() const noexcept { return m_voxelSize; }

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

private:
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_vertexCount = 0;
    float m_voxelSize = 0.0f;
};

// ---------------------------------------------------------------------------
// PointStringGeometry — concrete point string geometry
// (Ported from: itwinjs-core PointString.ts)
// ---------------------------------------------------------------------------
class PointStringGeometry : public CachedGeometry {
public:
    // driver + ibh own this geometry's per-geometry GL resources (ibh is released
    // in the dtor; the render primitive is set via setPrimitive after construction).
    PointStringGeometry(rhi::Driver& driver, rhi::IndexBufferHandle ibh,
                        uint32_t vertexCount, float weight);
    ~PointStringGeometry() override;

    // --- CachedGeometry interface ---
    // MeshRenderGeometry::create uploads raw FLOAT3 positions (no VertexLUT) → must
    // select the Unquantized variant or the triad's point string renders zero-coverage.
    bool usesQuantizedPositions() const noexcept override { return false; }
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::PointString; }
    Pass getPass() const noexcept override { return Pass::OpaqueLinear; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::Linear; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    float getWeight() const noexcept { return m_weight; }

    // Ported from: itwinjs-core PointString.ts:62 (_getLineWeight → this.weight).
    // Drives gl_PointSize in the PointString shader (PolylineShaderBuilder.cpp:367).
    float getLineWeight() const noexcept override { return m_weight; }

    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }

private:
    rhi::Driver& m_driver;
    rhi::IndexBufferHandle m_ibh;
    rhi::RenderPrimitiveHandle m_primitive;
    uint32_t m_vertexCount = 0;
    float m_weight = 1.0f;
};

END_DQ_RENDER_NAMESPACE
