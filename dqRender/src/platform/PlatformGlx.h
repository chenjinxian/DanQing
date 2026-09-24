// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Linux GLX platform
// Ported from: filament backend/src/opengl/platforms/PlatformGlx.cpp
//
// Linux OpenGL context management via GLX.
#pragma once

#include "dqRender/rhi/OpenGLPlatform.h"

#ifdef __linux__

#include <X11/Xlib.h>
#include <GL/glx.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// PlatformGlx — Linux GLX context management
// ---------------------------------------------------------------------------
class PlatformGlx final : public OpenGLPlatform {
public:
    PlatformGlx() = default;
    ~PlatformGlx() override { terminate(); }

    void terminate() noexcept override;

    void* createSwapChain(void* nativeWindow, uint64_t flags) override;
    void* createSwapChain(uint32_t width, uint32_t height, uint64_t flags) override;
    void destroySwapChain(void* swapChain) noexcept override;
    bool makeCurrent(void* draw, void* read) noexcept override;
    void commit(void* swapChain) noexcept override;

    Driver* createDriver(void* sharedContext, DriverConfig const& config) override;

    void setExternalContext(void* nativeContext) override;

private:
    GLXContext m_context = nullptr;
    Display* m_display = nullptr;
    bool m_externalContext = false;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // __linux__
