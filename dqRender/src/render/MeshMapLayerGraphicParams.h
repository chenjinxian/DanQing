// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mesh map layer graphic parameters
// Ported from: itwinjs-core core/frontend/src/internal/render/MeshMapLayerGraphicParams.ts
//
// Parameters linking mesh geometry to map layers.
#pragma once

#include <cstdint>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MeshMapLayerGraphicParams — mesh-to-map-layer linkage
// (Ported from: itwinjs-core MeshMapLayerGraphicParams.ts)
// ---------------------------------------------------------------------------
class MeshMapLayerGraphicParams {
public:
    MeshMapLayerGraphicParams() = default;

    /// Set the map layer ID.
    void setLayerId(uint32_t id) noexcept { m_layerId = id; }

    /// Get the map layer ID.
    uint32_t getLayerId() const noexcept { return m_layerId; }

    /// Set the tile URL.
    void setTileUrl(std::string const& url) { m_tileUrl = url; }

    /// Get the tile URL.
    std::string const& getTileUrl() const noexcept { return m_tileUrl; }

    /// Set the LOD (level of detail).
    void setLod(uint32_t lod) noexcept { m_lod = lod; }

    /// Get the LOD.
    uint32_t getLod() const noexcept { return m_lod; }

    /// Set the tile row.
    void setRow(uint32_t row) noexcept { m_row = row; }

    /// Get the tile row.
    uint32_t getRow() const noexcept { return m_row; }

    /// Set the tile column.
    void setColumn(uint32_t column) noexcept { m_column = column; }

    /// Get the tile column.
    uint32_t getColumn() const noexcept { return m_column; }

private:
    uint32_t m_layerId = 0;
    std::string m_tileUrl;
    uint32_t m_lod = 0;
    uint32_t m_row = 0;
    uint32_t m_column = 0;
};

END_DQ_RENDER_NAMESPACE
