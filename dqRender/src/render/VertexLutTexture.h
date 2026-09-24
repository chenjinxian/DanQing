// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex LUT Texture (quantized vertex data)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/VertexLUT.ts
//
// Packs vertex data (positions, normals, colors, UVs) into a 2D texture
// for efficient GPU memory usage.  The vertex shader reads from this
// texture instead of traditional vertex buffers.
//
// This is critical for large BIM models with millions of vertices.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// VertexLutParams — parameters for LUT texture lookup
// ---------------------------------------------------------------------------
struct VertexLutParams {
    uint32_t texWidth = 0;      // texture width in pixels
    uint32_t texHeight = 0;     // texture height in pixels
    uint32_t numRgbaPerVert = 0; // number of RGBA values per vertex
    uint32_t numVertices = 0;    // total number of vertices
};

// ---------------------------------------------------------------------------
// VertexLutTexture — manages vertex data packed into a 2D texture
// ---------------------------------------------------------------------------
class VertexLutTexture {
public:
    VertexLutTexture() = default;
    ~VertexLutTexture();

    VertexLutTexture(VertexLutTexture const&) = delete;
    VertexLutTexture& operator=(VertexLutTexture const&) = delete;

    // Move-only ownership transfer (PolylineGeometry owns its LUT by value).
    // All members are trivially relocatable (TextureHandle + POD params/float[3]).
    VertexLutTexture(VertexLutTexture&& other) noexcept = default;
    // TODO: move-assign bit-copies m_texture without destroying the lhs texture
    // (no m_driver reference is held, so the dtor cannot release it either).
    // PolylineGeometry moves once at construction so this is unexercised today;
    // add a destroy(m_driver)-on-lhs move-assign if reuse is needed.
    VertexLutTexture& operator=(VertexLutTexture&& other) noexcept = default;

    /// create a LUT texture from packed vertex data.
    /// @param driver RHI driver for texture creation.
    /// @param data Packed RGBA vertex data (4 bytes per component).
    /// @param numVertices Number of vertices.
    /// @param numRgbaPerVert Number of RGBA values per vertex (1-4).
    /// @return true on success.
    /// The texture MUST use the VertexTableBuilder's exact (width, height) — the
    /// builder emits a transposed SoA layout keyed on width%numRgbaPerVert==0 (a
    /// vertex's texels never wrap rows). Recomputing pow-2 dims here corrupts that
    /// layout and the shader samples garbage → 0 fragments. data is exactly
    /// width*height*4 bytes (no overread).
    bool create(rhi::Driver& driver, uint8_t const* data,
                uint32_t width, uint32_t height,
                uint32_t numVertices, uint32_t numRgbaPerVert);

    /// Destroy the LUT texture.
    void destroy(rhi::Driver& driver);

    /// Get the texture handle.
    rhi::TextureHandle getTexture() const noexcept { return m_texture; }

    /// Get the LUT parameters (for uniform upload).
    VertexLutParams const& getParams() const noexcept { return m_params; }

    /// Get the quantization origin (for uniform upload).
    float const* getQOrigin() const noexcept { return m_qOrigin; }

    /// Get the quantization scale (for uniform upload).
    float const* getQScale() const noexcept { return m_qScale; }

    /// Set quantization parameters.
    void setQuantization(float originX, float originY, float originZ,
                         float scaleX, float scaleY, float scaleZ);

    /// Check if the LUT is valid.
    bool isValid() const noexcept { return m_texture != rhi::TextureHandle{}; }

private:
    rhi::TextureHandle m_texture;
    VertexLutParams m_params;
    float m_qOrigin[3] = {0.0f, 0.0f, 0.0f};
    float m_qScale[3] = {1.0f, 1.0f, 1.0f};
};

END_DQ_RENDER_NAMESPACE
