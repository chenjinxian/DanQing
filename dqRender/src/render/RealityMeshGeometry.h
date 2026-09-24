// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Reality mesh geometry
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RealityMesh.ts
//
// Reality mesh geometry (textured triangle meshes from reality data/photogrammetry).
// Uses indexed geometry with texture coordinates for orthoimagery draping.
//
// Extends IndexedGeometry (not CachedGeometry directly) to match reference hierarchy.
// The reference RealityMeshGeometry extends IndexedGeometry and implements RenderGeometry.
#pragma once

#include "CachedGeometry.h"
#include "dqRender/RenderMemory.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// RealityMeshGeometry — reality mesh geometry
// (Ported from: itwinjs-core RealityMesh.ts, line 93-321)
//
// Textured triangle mesh from reality data (photogrammetry, 3D scanning).
// Supports texture draping, terrain rendering, and layer classification.
// ---------------------------------------------------------------------------
class RealityMeshGeometry : public CachedGeometry {
public:
    RealityMeshGeometry(uint32_t numIndices, uint32_t vertexCount);
    ~RealityMeshGeometry() override;

    RealityMeshGeometry(RealityMeshGeometry const&) = delete;
    RealityMeshGeometry& operator=(RealityMeshGeometry const&) = delete;

    // --- CachedGeometry interface ---
    TechniqueId getTechniqueId() const noexcept override { return TechniqueId::RealityMesh; }

    /// Pass depends on transparency and thematic display.
    /// Ported from: itwinjs-core RealityMesh.ts line 308-313
    Pass getPass() const noexcept override;

    /// Reality meshes render as unlit surfaces.
    /// Ported from: itwinjs-core RealityMesh.ts line 314
    RenderOrder getRenderOrder() const noexcept override { return RenderOrder::UnlitSurface; }

    void draw(rhi::Driver& driver) override;
    void collectStatistics(RenderMemory::Statistics& stats) const override;

    // --- Reality mesh properties (Ported from: itwinjs-core line 97-104) ---
    bool hasTexturesData() const noexcept { return m_hasTextures; }
    bool hasFeatures() const override { return m_hasFeatureId; }
    bool supportsThematicDisplay() const override { return true; }
    float getOverrideColorMix() const noexcept { return 0.5f; }
    bool hasBakedLighting() const override { return true; }

    /// Casting accessor.
    RealityMeshGeometry* asRealityMesh() override { return this; }

    // --- Quantization (for range computation) ---
    void setQuantization(float originX, float originY, float originZ,
                         float scaleX, float scaleY, float scaleZ);
    float const* getQOrigin() const override { return m_qOrigin; }
    float const* getQScale() const override { return m_qScale; }

    /// Compute the bounding range from quantization parameters.
    /// Ported from: itwinjs-core RealityMesh.ts line 214-216
    struct Range3d {
        float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    };
    Range3d getRange() const;

    // --- Configuration ---
    void setPrimitive(rhi::RenderPrimitiveHandle primitive) { m_primitive = primitive; }
    void setDrapeTexture(rhi::TextureHandle texture) { m_drapeTexture = texture; }
    rhi::TextureHandle getDrapeTexture() const noexcept { return m_drapeTexture; }
    void setHasTextures(bool val) noexcept { m_hasTextures = val; }
    void setHasFeatureId(bool val) noexcept { m_hasFeatureId = val; }
    void setBaseIsTransparent(bool val) noexcept { m_baseIsTransparent = val; }
    void setIsTerrain(bool val) noexcept { m_isTerrain = val; }

    void setUvTransform(float scaleU, float scaleV, float offsetU, float offsetV)
    {
        m_uvTransform[0] = scaleU; m_uvTransform[1] = scaleV;
        m_uvTransform[2] = offsetU; m_uvTransform[3] = offsetV;
    }
    float const* getUvTransform() const noexcept { return m_uvTransform; }

    uint32_t getNumIndices() const noexcept { return m_numIndices; }
    uint32_t getVertexCount() const noexcept { return m_vertexCount; }

private:
    rhi::RenderPrimitiveHandle m_primitive;
    rhi::TextureHandle m_drapeTexture;
    uint32_t m_numIndices = 0;
    uint32_t m_vertexCount = 0;
    float m_uvTransform[4] = {1.0f, 1.0f, 0.0f, 0.0f};

    // Ported from: itwinjs-core RealityMesh.ts line 97-113
    bool m_hasTextures = false;
    bool m_hasFeatureId = false;
    bool m_baseIsTransparent = false;
    bool m_isTerrain = false;

    // Quantization parameters for range computation
    float m_qOrigin[3] = {0.0f, 0.0f, 0.0f};
    float m_qScale[3] = {1.0f, 1.0f, 1.0f};
};

END_DQ_RENDER_NAMESPACE
