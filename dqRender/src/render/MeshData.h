// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Mesh data (raw mesh from tiles)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/MeshData.ts
//
// Raw mesh data: vertex positions, normals, params, indices.
// Loaded from tiles and converted to GPU geometry.
#pragma once

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// MeshData — raw mesh data from tiles
// (Ported from: itwinjs-core MeshData.ts)
// ---------------------------------------------------------------------------
class MeshData {
public:
    MeshData() = default;
    ~MeshData() = default;

    /// Set vertex positions (quantized uint16, 3 per vertex).
    void setPositions(std::vector<uint16_t> data) { m_positions = std::move(data); }

    /// Set vertex normals (oct-encoded uint16, 2 per vertex).
    void setNormals(std::vector<uint16_t> data) { m_normals = std::move(data); }

    /// Set vertex params (quantized uint16, 2 per vertex).
    void setParams(std::vector<uint16_t> data) { m_params = std::move(data); }

    /// Set vertex colors (RGBA8, 1 uint32 per vertex).
    void setColors(std::vector<uint32_t> data) { m_colors = std::move(data); }

    /// Set feature indices (uint32, 1 per vertex).
    void setFeatureIndices(std::vector<uint32_t> data) { m_featureIndices = std::move(data); }

    /// Set triangle indices (uint32).
    void setIndices(std::vector<uint32_t> data) { m_indices = std::move(data); }

    /// Get vertex positions.
    std::vector<uint16_t> const& getPositions() const noexcept { return m_positions; }

    /// Get vertex normals.
    std::vector<uint16_t> const& getNormals() const noexcept { return m_normals; }

    /// Get vertex params.
    std::vector<uint16_t> const& getParams() const noexcept { return m_params; }

    /// Get vertex colors.
    std::vector<uint32_t> const& getColors() const noexcept { return m_colors; }

    /// Get feature indices.
    std::vector<uint32_t> const& getFeatureIndices() const noexcept { return m_featureIndices; }

    /// Get triangle indices.
    std::vector<uint32_t> const& getIndices() const noexcept { return m_indices; }

    /// Get vertex count.
    uint32_t getVertexCount() const noexcept
    {
        return m_positions.empty() ? 0 : static_cast<uint32_t>(m_positions.size() / 3);
    }

    /// Get index count.
    uint32_t getIndexCount() const noexcept { return static_cast<uint32_t>(m_indices.size()); }

    /// Check if mesh has normals.
    bool hasNormals() const noexcept { return !m_normals.empty(); }

    /// Check if mesh has colors.
    bool hasColors() const noexcept { return !m_colors.empty(); }

    /// Check if mesh has feature indices.
    bool hasFeatureIndices() const noexcept { return !m_featureIndices.empty(); }

    /// Check if mesh has params (texture coordinates).
    bool hasParams() const noexcept { return !m_params.empty(); }

    /// Clear all data.
    void clear()
    {
        m_positions.clear();
        m_normals.clear();
        m_params.clear();
        m_colors.clear();
        m_featureIndices.clear();
        m_indices.clear();
    }

private:
    std::vector<uint16_t> m_positions;   // quantized xyz, 3 per vertex
    std::vector<uint16_t> m_normals;     // oct-encoded, 2 per vertex
    std::vector<uint16_t> m_params;      // quantized uv, 2 per vertex
    std::vector<uint32_t> m_colors;      // RGBA8, 1 per vertex
    std::vector<uint32_t> m_featureIndices;  // 1 per vertex
    std::vector<uint32_t> m_indices;     // triangle indices
};

END_DQ_RENDER_NAMESPACE
