// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Background map drape
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/BackgroundMapDrape.ts
//
// Renders map tiles (orthoimagery, elevation) onto an off-screen FBO for
// draping over terrain.  The drape texture is then sampled by the terrain
// fragment shader to composite the map imagery.
#pragma once

#include "TextureDrape.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class RenderGraphic;

// ---------------------------------------------------------------------------
// BackgroundMapDrape — off-screen FBO for map tile draping
// (Ported from: itwinjs-core BackgroundMapDrape.ts)
// ---------------------------------------------------------------------------
class BackgroundMapDrape : public TextureDrape {
public:
    BackgroundMapDrape() = default;
    ~BackgroundMapDrape() = default;

    BackgroundMapDrape(BackgroundMapDrape const&) = delete;
    BackgroundMapDrape& operator=(BackgroundMapDrape const&) = delete;

    /// create a new BackgroundMapDrape.
    static std::unique_ptr<BackgroundMapDrape> create();

    /// add a graphic to be rendered into the drape texture.
    void addGraphic(RenderGraphic* graphic);

    /// Collect graphics for rendering (called during scene preparation).
    void collectGraphics(/* SceneContext context */);

    /// Draw all collected graphics into the FBO.
    void draw(rhi::Driver& driver);

    /// Check if this drape has been disposed.
    bool isDisposed() const noexcept { return m_disposed; }

    /// Dispose the drape, releasing GPU resources.
    void dispose(rhi::Driver& driver);

    /// Get the projection matrix used for the drape rendering.
    float const* getProjectionMatrix() const noexcept { return m_projectionMatrix; }

    /// Set the projection matrix.
    void setProjectionMatrix(float const* matrix);

    /// Get the drape texture width.
    uint32_t getWidth() const noexcept { return m_width; }

    /// Get the drape texture height.
    uint32_t getHeight() const noexcept { return m_height; }

private:
    /// Set up the FBO with the given dimensions.
    bool createFbo(rhi::Driver& driver, uint32_t width, uint32_t height);

    std::vector<RenderGraphic*> m_graphics;
    rhi::RenderTargetHandle m_renderTarget;
    rhi::TextureHandle m_colorTexture;
    float m_projectionMatrix[16];
    uint32_t m_width = 512;
    uint32_t m_height = 512;
    bool m_disposed = false;
};

END_DQ_RENDER_NAMESPACE
