// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL Swapchain implementation
// Ported from: filament filament/src/backend/opengl/OpenGLDriver.cpp (SwapChain)
//              PlatformCocoaGl.mm (NSOpenGLContext + NSView binding)
//
// Wraps the RHI Driver's SwapChain into a clean acquire/present lifecycle.
// On macOS: acquire() → [m_context makeCurrentContext] + [m_context setView:]
//           present() → [m_context flushBuffer]
#include "OpenGLSwapchain.h"
#include "dqRender/rhi/Driver.h"

BEGIN_DQ_RENDER_NAMESPACE

OpenGLSwapchain::OpenGLSwapchain(rhi::Driver& driver, void* nativeWindow,
                                 uint32_t width, uint32_t height)
    : m_driver(driver)
    , m_nativeWindow(nativeWindow)
    , m_width(width)
    , m_height(height)
{
    if (!nativeWindow) return;

    // Create the RHI SwapChain from the native window handle.
    // On macOS: binds NSView to NSOpenGLContext via [context setView:].
    m_swapChain = m_driver.createSwapChain(nativeWindow, 0);

    // Create the default render target (FBO 0).
    // This represents the swapchain's surface for rendering.
    m_defaultTarget = m_driver.createDefaultRenderTarget();
}

OpenGLSwapchain::~OpenGLSwapchain()
{
    if (m_defaultTarget) {
        m_driver.destroyRenderTarget(m_defaultTarget);
        m_defaultTarget = {};
    }
    if (m_swapChain) {
        m_driver.destroySwapChain(m_swapChain);
        m_swapChain = {};
    }
}

bool OpenGLSwapchain::isValid() const noexcept
{
    return static_cast<bool>(m_swapChain) && static_cast<bool>(m_defaultTarget) && m_nativeWindow != nullptr;
}

void OpenGLSwapchain::resize(uint32_t width, uint32_t height)
{
    // 表面 extent 未变时只记账（视口由每个 RenderPass 设置）。
    if (width == m_width && height == m_height)
        return;
    m_width = width;
    m_height = height;

    // Windows/DWM：GL 子窗口的表面 extent 变化（最大化/还原/拖拽 resize）会使
    // WGL 呈现关联失效——SwapBuffers 继续返回成功，但内容不再到达窗口的 DWM
    // 表面，屏幕停留在按新尺寸缩放的旧帧（用户所报"黑色大方块"；GL 子窗口任
    // 何后续尺寸变化都会令驱动重新关联——实测 ±2px 即自愈，见 2026-09-14 取
    // 证）。故 resize 即表面过期：销毁并按当前句柄重建平台交换链——Vulkan 的
    // "swapchain extent changed ⇒ VK_ERROR_OUT_OF_DATE_KHR ⇒ 重建"在 WGL 的
    // 对应物。重建只涉及 GetDC/像素格式（窗口已设格式时 SetPixelFormat 失败为
    // 良性——格式一致），无 GL 资源销毁，成本可忽略。
    if (m_swapChain && m_nativeWindow) {
        if (std::getenv("DANQING_GL_TRACE"))
            fprintf(stderr, "[SC] resize %ux%u -> recreate surface\n", width, height);
        m_driver.destroySwapChain(m_swapChain);
        m_swapChain = m_driver.createSwapChain(m_nativeWindow, 0);
    }
}

// rebind — recreate the platform swapchain on a (new) native window handle.
// 窗口系统会在重排/状态跃迁时销毁重建原生子窗口（Qt 对 WA_NativeWindow 子窗口
// 的 MDI 重排/最大化行为）——旧表面句柄死亡后 present 静默停止上屏（屏幕冻结
// 旧帧）。窗口生命周期归应用层；本方法只做 GL 侧表面重建（无窗口工具箱依赖）。
//
// 不得做句柄相等短路：Windows 会立即回收复用 HWND 地址（实测 maximize 跃迁中
// 旧窗口销毁后新窗口拿到同值句柄）。WinIdChange 本身即"旧表面已死"的语义
// （对应 Vulkan 的 surface-out-of-date——无条件重建），句柄相等不说明表面存活。
void OpenGLSwapchain::rebind(void* nativeWindow)
{
    if (!nativeWindow)
        return;
    if (std::getenv("DANQING_GL_TRACE"))
        fprintf(stderr, "[SC] rebind old=%p new=%p\n", m_nativeWindow, nativeWindow);
    if (m_swapChain)
        m_driver.destroySwapChain(m_swapChain);
    m_nativeWindow = nativeWindow;
    m_swapChain = m_driver.createSwapChain(nativeWindow, 0);
}

rhi::RenderTargetHandle OpenGLSwapchain::acquire()
{
    if (!isValid()) return {};

    // Make the GL context current and bind to the native window surface.
    // On macOS: [m_context makeCurrentContext] + [m_context setView:nsView]
    m_driver.makeCurrent(m_swapChain, m_swapChain);

    return m_defaultTarget;
}

void OpenGLSwapchain::present()
{
    if (!isValid()) return;

    // Swap buffers to display the rendered frame.
    // On macOS: [m_context flushBuffer]
    m_driver.commit(m_swapChain);
}

rhi::RenderTargetHandle OpenGLSwapchain::getRenderTarget() const noexcept
{
    return m_defaultTarget;
}

END_DQ_RENDER_NAMESPACE
