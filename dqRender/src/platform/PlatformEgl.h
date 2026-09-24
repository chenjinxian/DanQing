// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — EGL platform (headless/CI)
// Ported from: filament backend/src/opengl/platforms/PlatformEgl.cpp
//
// EGL-based OpenGL context management for headless rendering.
// Works on Linux, Android, and other EGL-capable platforms.
#pragma once

#include "dqRender/rhi/OpenGLPlatform.h"

#if !defined(__APPLE__) && !defined(_WIN32)
// EGL is available on Linux/Android
#define DQ_RENDER_HAS_EGL 1
#endif

#ifdef DQ_RENDER_HAS_EGL

#include <EGL/egl.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// PlatformEgl — EGL headless context management
// ---------------------------------------------------------------------------
class PlatformEgl final : public OpenGLPlatform {
public:
    PlatformEgl() = default;
    ~PlatformEgl() override { terminate(); }

    void terminate() noexcept override;

    void* createSwapChain(void* nativeWindow, uint64_t flags) override;
    void* createSwapChain(uint32_t width, uint32_t height, uint64_t flags) override;
    void destroySwapChain(void* swapChain) noexcept override;
    bool makeCurrent(void* draw, void* read) noexcept override;
    void commit(void* swapChain) noexcept override;

    Driver* createDriver(void* sharedContext, DriverConfig const& config) override;

private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLConfig m_config = nullptr;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // DQ_RENDER_HAS_EGL
