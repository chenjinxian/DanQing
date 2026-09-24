// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Concrete OpenGL RenderTarget implementation
// Ported from: filament filament/src/RenderTarget.h
//
// Bridges the public RenderTarget interface to the internal TargetImpl.
#pragma once

#include "dqRender/RenderTarget.h"

#include "TargetImpl.h"

#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class OpenGLRenderSystem;

// Concrete RenderTarget using OpenGL.
// Bridges public RenderTarget interface → internal TargetImpl.
class OpenGLRenderTarget : public RenderTarget {
public:
    OpenGLRenderTarget(OpenGLRenderSystem& system, std::unique_ptr<TargetImpl> impl);
    ~OpenGLRenderTarget() override = default;

    // RenderTarget interface
    RenderSystem& renderSystem() override;
    ViewRect viewRect() const override;

    // Debug control (RenderTargetDebugControl family) — routed to TargetImpl.
    RenderCommandCount getRenderCommands() const override;
    bool displayNormalMaps() const override;
    void setDisplayNormalMaps(bool on) override;
    void setViewRect(const ViewRect& rect) override;
    bool updateViewRect() override;
    void changeScene(Scene& scene) override;
    void changeDecorations(Decorations& decorations) override;
    void changeRenderPlan(RenderPlan const& plan) override;
    void drawFrame(float elapsedMs) override;
    // ← Target.ts:552（drawFrame 无条件 drawOverlayDecorations）——GL 帧内栅格化
    // 2D canvas 装饰（APPROVED DEVIATION，spec §2.4；默认实现为 no-op）。
    void drawCanvasDecorations(std::vector<CanvasDecoration> const& canvasDecs) override;
    void setLightSettings(float const* sunDir, float sunIntensity,
                           float const* ambientColor) override;
    bool readPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                    std::vector<uint8_t>& pixels) override;
    bool readPickData(int32_t x, int32_t y, uint32_t width, uint32_t height,
                      uint32_t* outIds, uint32_t outCount) override;
    // depthAndOrder 附件读回（pickDepthPoint 用）。
    bool readPickDepth(int32_t x, int32_t y, uint32_t width, uint32_t height,
                       float* outFractions, uint32_t outCount) override;
    void setHiliteSet(uint32_t const* elementIds, size_t count) override;
    void setFlashed(uint32_t elementId, float intensity) override;
    void setHiliteColor(float r, float g, float b) override;
    void setViewportTransform(float const* mv16, float const* mvp16) override;
    void setDevicePixelRatio(float ratio) override;

    // Access internal implementation
    TargetImpl& getImpl() noexcept { return *m_impl; }

private:
    OpenGLRenderSystem& m_system;
    std::unique_ptr<TargetImpl> m_impl;
    Scene* m_scene = nullptr;           // Not owned — stored for drawFrame()
    Decorations* m_decorations = nullptr;  // Not owned — stored for drawFrame()
    bool m_hasViewportTransform = false;
};

END_DQ_RENDER_NAMESPACE
