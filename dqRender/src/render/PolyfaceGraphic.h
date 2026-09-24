// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PolyfaceGraphic (GPU geometry from Polyface)
// Ported from: itwinjs-core core/frontend/src/render/Polyface.ts
//
// Converts an IndexedPolyface (dqGeom) into GPU-ready geometry (dqRender).
// Builds interleaved vertex buffer (position + normal + color) and
// triangle index buffer from the polyface's indexed face loops.
//
// This is the dqRender → dqGeom integration point.
#pragma once

#include "CachedGeometry.h"
#include "dqRender/RenderMemory.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <dqGeom/IndexedPolyface.h>

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PolyfaceGraphic — GPU geometry from IndexedPolyface
// ---------------------------------------------------------------------------
class PolyfaceGraphic : public CachedGeometry {
public:
    /// Construct GPU geometry from a polyface mesh.
    /// Triangulates n-gon faces via fan triangulation.
    /// @param driver RHI driver for resource creation.
    /// @param polyface Source mesh data.
    /// @param defaultColor Color to use if polyface has no per-vertex colors.
    /// @param featureId Per-vertex feature id (batch-local feature index) — baked
    ///        into the vertex buffer at build time (the reference bakes
    ///        featureIndex into the vertex table at mesh build,
    ///        MeshBuilder.ts:142-151; setting it after construction has no
    ///        effect on already-built vertices).
    PolyfaceGraphic(rhi::Driver& driver, dqGeom::IndexedPolyface const& polyface,
                    uint32_t defaultColor = 0xFF8080FF, uint32_t featureId = 0);
    ~PolyfaceGraphic() override;

    PolyfaceGraphic(PolyfaceGraphic const&) = delete;
    PolyfaceGraphic& operator=(PolyfaceGraphic const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::Surface; }
    Pass getPass() const noexcept override { return Pass::Opaque; }
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::LitSurface; }
    /// PolyfaceGraphic uploads raw FLOAT3 a_position attributes (no VertexLUT, no
    /// u_qOrigin/u_qScale), so it must select the Unquantized Surface variant.
    /// Ported from: itwinjs-core cachedGeometry.usesQuantizedPositions (DrawCommand.ts:221).
    bool usesQuantizedPositions() const noexcept override { return false; }
    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    /// Get vertex count (for testing).
    uint32_t getVertexCount() const noexcept { return m_vertexCount; }

    /// Get index count (for testing).
    uint32_t getIndexCount() const noexcept { return m_indexCount; }

    /// The feature id baked into the vertices at construction (read-only —
    /// vertex data is frozen by then; pass the id to the ctor instead).
    uint32_t getFeatureId() const noexcept { return m_featureId; }

    /// Surface baseColor texture (s_texture). Owned by this graphic — destroyed
    /// in the dtor. Ported from: itwinjs-core SurfaceGeometry.texture (setTexture path).
    void setTexture(rhi::TextureHandle texture) { m_texture = texture; }
    rhi::TextureHandle getSurfaceTexture() const noexcept override { return m_texture; }

    // Texture lifetime flags. Default false = this graphic owns + disposes the
    // texture (CreateTextureArgs.ownership undefined semantics: "lifetime
    // controlled by the first RenderGraphic with which it associates",
    // CreateTextureArgs.ts:47). Set true for externally-owned textures
    // (GltfDecoration's per-BuildGraphic resolvedTextures cache — the cache
    // owner disposes, mirroring CreateTextureArgs.ownership="external",
    // CreateTextureArgs.ts:50-52). Ported from the same reference contract.
    void setTextureExternal(bool external) noexcept { m_textureExternal = external; }

    /// Normal map texture (s_normalMap). Owned by this graphic — destroyed in
    /// the dtor. Ported from: itwinjs-core MeshData.normalMap (MeshData.ts :61-67
    /// — geometry-sourced from the material's normalMapParams).
    void setNormalMapTexture(rhi::TextureHandle texture) { m_normalMapTexture = texture; }
    rhi::TextureHandle getNormalMapTexture() const noexcept override { return m_normalMapTexture; }

    /// Same ownership flag for the normal map (external cache path).
    void setNormalMapTextureExternal(bool external) noexcept { m_normalMapTextureExternal = external; }

    /// Normal map scale (u_normalMapScale). Ported from: itwinjs-core
    /// Surface.ts addNormal (:541-550) — the caller applies the greenUp
    /// negation (glTF: greenUp=true → -1.0, GltfReader.ts :2587).
    void setNormalMapScale(float scale) noexcept { m_normalMapScale = scale; }
    float getNormalMapScale() const noexcept override { return m_normalMapScale; }

    /// Test accessor: per-vertex UV (buildFromPolyface output).
    void getVertexTexCoord(uint32_t index, float out[2]) const
    {
        out[0] = index < m_vertices.size() ? m_vertices[index].texCoord[0] : 0.0f;
        out[1] = index < m_vertices.size() ? m_vertices[index].texCoord[1] : 0.0f;
    }

private:
    struct Vertex {
        float position[3];
        float normal[3];
        float color[4];
        float texCoord[2];
        uint32_t featureId;
    };

    void buildFromPolyface(dqGeom::IndexedPolyface const& polyface, uint32_t defaultColor);
    void uploadToGpu(rhi::Driver& driver);

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    rhi::VertexBufferInfoHandle m_vbih;
    rhi::VertexBufferHandle m_vbh;
    rhi::IndexBufferHandle m_ibh;
    rhi::BufferObjectHandle m_vbo;
    rhi::RenderPrimitiveHandle m_primitive;

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    uint32_t m_featureId = 0;  // default feature ID
    rhi::TextureHandle m_texture;
    rhi::TextureHandle m_normalMapTexture;
    bool m_textureExternal = false;
    bool m_normalMapTextureExternal = false;
    float m_normalMapScale = 1.0f;  // scale ?? 1.0 (Surface.ts :546-547)

    rhi::Driver& m_driver;
};

END_DQ_RENDER_NAMESPACE
