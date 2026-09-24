// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mesh geometry base class
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/MeshGeometry.ts
//
// Abstract base class for all mesh-type geometry (surfaces, edges, polylines
// in meshes). Extends LUTGeometry with mesh-specific properties.
#pragma once

#include "CachedGeometry.h"
#include "VertexLutTexture.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// SurfaceType — surface fill type
// ---------------------------------------------------------------------------
enum class SurfaceType : uint8_t {
    Unknown = 0,
    Opaque = 1,
    Translucent = 2,
    Planar = 3,
};

// ---------------------------------------------------------------------------
// FillFlags — surface fill flags
// ---------------------------------------------------------------------------
enum class FillFlags : uint8_t {
    None = 0,
    Lit = 1,
    Unlit = 2,
    AlwaysTranslucent = 4,
};

// ---------------------------------------------------------------------------
// MeshGeometry — abstract base for mesh geometry
// (Ported from: itwinjs-core MeshGeometry.ts)
// ---------------------------------------------------------------------------
class MeshGeometry : public CachedGeometry {
public:
    ~MeshGeometry() override = default;

    // --- Mesh properties ---
    SurfaceType getSurfaceType() const noexcept { return m_surfaceType; }
    FillFlags getFillFlags() const noexcept { return m_fillFlags; }
    bool isPlanar() const noexcept { return m_isPlanar; }
    bool hasTextures() const noexcept { return m_hasTextures; }
    bool hasBakedLighting() const noexcept override { return m_hasBakedLighting; }
    uint32_t getNumIndices() const noexcept { return m_numIndices; }

    // --- Edge properties ---
    float getEdgeWidth() const noexcept { return m_edgeWidth; }
    uint32_t getEdgeLineCode() const noexcept { return m_edgeLineCode; }

    // --- LUT access ---
    VertexLutTexture const* getLut() const noexcept { return m_lut; }
    void setLut(VertexLutTexture const* lut) noexcept { m_lut = lut; }

    // --- CachedGeometry overrides ---
    // MeshRenderGeometry::create uploads raw FLOAT3 a_position attributes (no
    // VertexLUT, no u_qOrigin/u_qScale), so these must select the Unquantized
    // Surface variant — otherwise the Quantized shader reads the FLOAT3 bytes as
    // uint16 and produces degenerate triangles (zero-coverage → invisible, e.g.
    // the ACS triad). 1:1 with PolyfaceGraphic::usesQuantizedPositions.
    bool usesQuantizedPositions() const noexcept override { return false; }

    float const* getQOrigin() const override
    {
        return m_lut ? m_lut->getQOrigin() : nullptr;
    }
    float const* getQScale() const override
    {
        return m_lut ? m_lut->getQScale() : nullptr;
    }

protected:
    MeshGeometry(uint32_t numIndices, SurfaceType surfaceType, FillFlags fillFlags,
                 bool isPlanar, bool hasTextures, bool hasBakedLighting)
        : m_numIndices(numIndices)
        , m_surfaceType(surfaceType)
        , m_fillFlags(fillFlags)
        , m_isPlanar(isPlanar)
        , m_hasTextures(hasTextures)
        , m_hasBakedLighting(hasBakedLighting)
    {
    }

private:
    uint32_t m_numIndices = 0;
    SurfaceType m_surfaceType = SurfaceType::Unknown;
    FillFlags m_fillFlags = FillFlags::None;
    bool m_isPlanar = false;
    bool m_hasTextures = false;
    bool m_hasBakedLighting = false;
    float m_edgeWidth = 1.0f;
    uint32_t m_edgeLineCode = 0;
    VertexLutTexture const* m_lut = nullptr;
};

END_DQ_RENDER_NAMESPACE
