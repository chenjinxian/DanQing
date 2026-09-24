// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Windows WGL platform
// Ported from: filament backend/src/opengl/platforms/PlatformWgl.cpp
//
// Windows OpenGL context management via WGL.
#pragma once

#include "dqRender/rhi/OpenGLPlatform.h"

#ifdef _WIN32
#include <windows.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// WGLSwapChain — Ported from: filament backend/src/opengl/platforms/PlatformWGL.cpp
// (:69-73 struct WGLSwapChain)。swapchain 持有自己的窗口 DC（创建时 GetDC 一次、
// makeCurrent/commit 复用、销毁时 ReleaseDC）——filament 的生命周期契约。
struct WglSwapChain {
    HDC hDc = nullptr;
    HWND hWnd = nullptr;
    bool isHeadless = false;
};

// ---------------------------------------------------------------------------
// PlatformWgl — Windows WGL context management
// ---------------------------------------------------------------------------
class PlatformWgl final : public OpenGLPlatform {
public:
    PlatformWgl() = default;
    ~PlatformWgl() override { terminate(); }

    void terminate() noexcept override;

    void* createSwapChain(void* nativeWindow, uint64_t flags) override;
    void* createSwapChain(uint32_t width, uint32_t height, uint64_t flags) override;
    void destroySwapChain(void* swapChain) noexcept override;
    bool makeCurrent(void* draw, void* read) noexcept override;
    void commit(void* swapChain) noexcept override;

    Driver* createDriver(void* sharedContext, DriverConfig const& config) override;

    void setExternalContext(void* nativeContext) override;

private:
    HGLRC m_context = nullptr;
    HDC m_deviceContext = nullptr;
    HWND m_window = nullptr;  // 承载 GL context 的长期隐藏窗口（context 生命周期内必须存活；filament createDriver 的 dummy 窗口）
    bool m_externalContext = false;
    PIXELFORMATDESCRIPTOR m_pfd = {};  // 上下文承载窗口与各 swapchain 窗口共用（格式必须匹配，filament mPfd）
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // _WIN32
