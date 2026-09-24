// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Auxiliary channel table for animation displacement
// Ported from: itwinjs-core core/frontend/src/common/internal/render/AuxChannelTable.ts
//
// Stores per-vertex animation data (displacement, normal, scalar) in a
// rectangular RGBA8 texture for GPU-side animation interpolation.
#pragma once

#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi { class Driver; }

// ---------------------------------------------------------------------------
// AuxChannel — base class for animation channel data
// Ported from: itwinjs-core AuxChannelTable.ts AuxChannel (line 30-48)
// ---------------------------------------------------------------------------
class AuxChannel {
public:
    AuxChannel(std::string name, std::vector<float> inputs, std::vector<int> indices)
        : m_name(std::move(name))
        , m_inputs(std::move(inputs))
        , m_indices(std::move(indices))
    {}

    std::string const& name() const { return m_name; }
    std::vector<float> const& inputs() const { return m_inputs; }
    std::vector<int> const& indices() const { return m_indices; }

private:
    std::string m_name;
    std::vector<float> m_inputs;
    std::vector<int> m_indices;
};

// ---------------------------------------------------------------------------
// AuxDisplacementChannel — displacement animation channel
// Ported from: itwinjs-core AuxChannelTable.ts AuxDisplacementChannel (line 51-68)
// ---------------------------------------------------------------------------
class AuxDisplacementChannel : public AuxChannel {
public:
    AuxDisplacementChannel(std::string name, std::vector<float> inputs,
                           std::vector<int> indices,
                           float qOrigin[3], float qScale[3])
        : AuxChannel(std::move(name), std::move(inputs), std::move(indices))
    {
        for (int i = 0; i < 3; ++i) {
            m_qOrigin[i] = qOrigin[i];
            m_qScale[i] = qScale[i];
        }
    }

    float const* qOrigin() const { return m_qOrigin; }
    float const* qScale() const { return m_qScale; }

private:
    float m_qOrigin[3] = {0, 0, 0};
    float m_qScale[3] = {1, 1, 1};
};

// ---------------------------------------------------------------------------
// AuxParamChannel — scalar parameter animation channel
// Ported from: itwinjs-core AuxChannelTable.ts AuxParamChannel (line 71-88)
// ---------------------------------------------------------------------------
class AuxParamChannel : public AuxChannel {
public:
    AuxParamChannel(std::string name, std::vector<float> inputs,
                    std::vector<int> indices, float qOrigin, float qScale)
        : AuxChannel(std::move(name), std::move(inputs), std::move(indices))
        , m_qOrigin(qOrigin)
        , m_qScale(qScale)
    {}

    float qOrigin() const { return m_qOrigin; }
    float qScale() const { return m_qScale; }

private:
    float m_qOrigin = 0;
    float m_qScale = 1;
};

// ---------------------------------------------------------------------------
// AuxChannelTable — rectangular RGBA8 texture data for animation
// Ported from: itwinjs-core AuxChannelTable.ts AuxChannelTable (line 118-189)
//
// Packs per-vertex animation data (displacement, normal, scalar channels)
// into a rectangular RGBA8 texture for GPU-side interpolation.
// ---------------------------------------------------------------------------
class AuxChannelTable {
public:
    AuxChannelTable(std::vector<uint8_t> data, uint32_t width, uint32_t height,
                    uint32_t numVertices, uint32_t numBytesPerVertex,
                    std::vector<AuxDisplacementChannel> displacements,
                    std::vector<AuxChannel> normals,
                    std::vector<AuxParamChannel> params);

    /// Create from pre-packed data + channel metadata.
    static std::unique_ptr<AuxChannelTable> create(
        std::vector<uint8_t> data, uint32_t width, uint32_t height,
        uint32_t numVertices, uint32_t numBytesPerVertex,
        std::vector<AuxDisplacementChannel> displacements,
        std::vector<AuxChannel> normals,
        std::vector<AuxParamChannel> params);

    /// Create the GPU RGBA8 texture from packed data.
    rhi::TextureHandle createLutTexture(rhi::Driver& driver) const;

    /// Accessors
    std::vector<uint8_t> const& data() const { return m_data; }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t numVertices() const { return m_numVertices; }
    uint32_t numBytesPerVertex() const { return m_numBytesPerVertex; }

    std::vector<AuxDisplacementChannel> const& displacements() const { return m_displacements; }
    std::vector<AuxChannel> const& normals() const { return m_normals; }
    std::vector<AuxParamChannel> const& params() const { return m_params; }

    bool hasDisplacements() const { return !m_displacements.empty(); }
    bool hasNormals() const { return !m_normals.empty(); }
    bool hasParams() const { return !m_params.empty(); }

private:
    std::vector<uint8_t> m_data;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_numVertices;
    uint32_t m_numBytesPerVertex;
    std::vector<AuxDisplacementChannel> m_displacements;
    std::vector<AuxChannel> m_normals;
    std::vector<AuxParamChannel> m_params;
    rhi::TextureHandle m_texture;
};

END_DQ_RENDER_NAMESPACE
