// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Windows WGL platform implementation
// Ported from: filament backend/src/opengl/platforms/PlatformWgl.cpp
#ifdef _WIN32

#include "PlatformWgl.h"
#include <cstdio>
#include "rhi/opengl/OpenGLDriver.h"
#include "rhi/opengl/Gl.h" // GL 声明统一来源（Windows: glcorearb.h，见 Gl.h）

#include <windows.h>

// WGL function typedefs
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// TEMP-DIAG（黑方块真机取证）：swapchain 生命周期轨迹（env DANQING_GL_TRACE=1）。
static bool glTrace() { static bool const v = std::getenv("DANQING_GL_TRACE") != nullptr; return v; }

void PlatformWgl::terminate() noexcept
{
    if (m_context && !m_externalContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_context);
        m_context = nullptr;
    }
    if (m_deviceContext) {
        ReleaseDC(m_window, m_deviceContext);
        m_deviceContext = nullptr;
    }
    if (m_window) {
        DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void PlatformWgl::setExternalContext(void* nativeContext)
{
    if (nativeContext) {
        m_context = static_cast<HGLRC>(nativeContext);
        m_externalContext = true;
    }
}

Driver* PlatformWgl::createDriver(void* sharedContext, DriverConfig const& config)
{
    // PFD 初始化无条件先行（filament PlatformWGL::createDriver :83-100）：上下文
    // 承载窗口与所有 swapchain 窗口共用同一 m_pfd——HDC 与 HGLRC 的像素格式
    // 必须匹配（filament createSwapChain :240 注释）。窗口默认帧缓冲只接收最终
    // blit，stencil 不需要（filament 同为 0）。
    m_pfd = {};
    m_pfd.nSize = sizeof(m_pfd);
    m_pfd.nVersion = 1;
    m_pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    m_pfd.iPixelType = PFD_TYPE_RGBA;
    m_pfd.cColorBits = 32;
    m_pfd.cDepthBits = 24;
    m_pfd.cStencilBits = 0;
    m_pfd.iLayerType = PFD_MAIN_PLANE;

    if (!m_context) {
        // 长期隐藏窗口承载 GL context（filament createDriver :104-110 的 dummy
        // "STATIC" 窗口；本实现注册自有类——承载窗口在 context 生命周期内必须
        // 存活，销毁后其 HDC 悬空即访问违例）。
        WNDCLASSA wc = {};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "danqing_wgl_ctx";
        RegisterClassA(&wc);

        m_window = CreateWindowA("danqing_wgl_ctx", "", WS_POPUP, 0, 0, 1, 1,
                                 nullptr, nullptr, wc.hInstance, nullptr);
        if (!m_window)
            return nullptr;
        HDC hdc = GetDC(m_window);

        int format = ChoosePixelFormat(hdc, &m_pfd);
        SetPixelFormat(hdc, format, &m_pfd);

        // Temporary context to get wglCreateContextAttribsARB
        //（filament :115-124）
        HGLRC tempContext = wglCreateContext(hdc);
        wglMakeCurrent(hdc, tempContext);

        auto wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));

        if (wglCreateContextAttribsARB) {
            int attribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                WGL_CONTEXT_MINOR_VERSION_ARB, 1,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            m_context = wglCreateContextAttribsARB(hdc, static_cast<HGLRC>(sharedContext), attribs);
        }

        // Clean up temporary context（承载窗口 m_window 保留）
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempContext);

        if (!m_context) {
            ReleaseDC(m_window, hdc);
            DestroyWindow(m_window);
            m_window = nullptr;
            return nullptr;
        }

        m_deviceContext = hdc;
    }

    // Make the context current
    wglMakeCurrent(m_deviceContext, m_context);

    // Create the OpenGL driver
    auto* driver = new OpenGLDriver(*this, sharedContext, config);
    driver->initializeGL();
    return driver;
}

// Ported from: filament PlatformWGL::createSwapChain(nativeWindow) (:226-245)。
// swapchain 持有自己的 DC；像素格式经同一 m_pfd 选择（与上下文承载窗口匹配）。
void* PlatformWgl::createSwapChain(void* nativeWindow, uint64_t)
{
    if (!nativeWindow || !m_context) return nullptr;

    auto* swapChain = new WglSwapChain();
    swapChain->isHeadless = false;
    swapChain->hWnd = static_cast<HWND>(nativeWindow);
    swapChain->hDc = GetDC(swapChain->hWnd);
    if (glTrace())
        fprintf(stderr, "[WGL] createSwapChain hwnd=%p dc=%p fmt=%d err=%lu\n",
                swapChain->hWnd, swapChain->hDc,
                swapChain->hDc ? GetPixelFormat(swapChain->hDc) : 0, GetLastError());
    if (!swapChain->hDc)
        return swapChain;

    // We have to match pixel formats across the HDC and HGLRC (filament :240-242)
    int pixelFormat = ChoosePixelFormat(swapChain->hDc, &m_pfd);
    SetPixelFormat(swapChain->hDc, pixelFormat, &m_pfd);
    return swapChain;
}

// Ported from: filament PlatformWGL::createSwapChain(width, height) (:247-266)。
// WS_POPUP：其他窗口样式在 readPixels 下出现像素损坏（filament 实验结论 :251-253）。
void* PlatformWgl::createSwapChain(uint32_t width, uint32_t height, uint64_t)
{
    auto* swapChain = new WglSwapChain();
    swapChain->isHeadless = true;

    RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_POPUP, FALSE);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    swapChain->hWnd = CreateWindowA("STATIC", "headless", WS_POPUP, 0, 0,
                                    static_cast<int>(width), static_cast<int>(height),
                                    nullptr, nullptr, nullptr, nullptr);
    swapChain->hDc = GetDC(swapChain->hWnd);
    int pixelFormat = ChoosePixelFormat(swapChain->hDc, &m_pfd);
    SetPixelFormat(swapChain->hDc, pixelFormat, &m_pfd);
    return swapChain;
}

// Ported from: filament PlatformWGL::destroySwapChain (:268-283)。
// 只销毁 headless 自建窗口；应用传入的 HWND 永不 DestroyWindow（此前实现
// 直接 DestroyWindow(swapChain) —— swapChain 曾是应用窗口句柄，析构时会
// 摧毁 Qt 子窗口）。销毁后把上下文重新挂回承载窗口 DC（filament :282）。
void PlatformWgl::destroySwapChain(void* swapChain) noexcept
{
    if (!swapChain) return;
    auto* sc = static_cast<WglSwapChain*>(swapChain);
    if (glTrace())
        fprintf(stderr, "[WGL] destroySwapChain hwnd=%p dc=%p headless=%d IsWindow=%d\n",
                sc->hWnd, sc->hDc, static_cast<int>(sc->isHeadless),
                sc->hWnd ? static_cast<int>(IsWindow(sc->hWnd)) : -1);
    ReleaseDC(sc->hWnd, sc->hDc);
    if (sc->isHeadless)
        DestroyWindow(sc->hWnd);
    delete sc;

    // make this swapChain not current (by making the carrier one current)
    if (m_context)
        wglMakeCurrent(m_deviceContext, m_context);
}

// Ported from: filament PlatformWGL::makeCurrent (:285-302)——经 swapchain 持有的
// DC 挂载；失败时做表面自愈（见内注释）。
bool PlatformWgl::makeCurrent(void* draw, void* /*read*/) noexcept
{
    if (!m_context) return false;

    if (draw) {
        auto* sc = static_cast<WglSwapChain*>(draw);
        if (sc->hWnd) {
            if (sc->hDc && wglMakeCurrent(sc->hDc, m_context))
                return true;

            // 表面失效自愈（Vulkan VK_ERROR_OUT_OF_DATE_KHR 的 WGL 等价）：窗口
            // 系统会在重排/状态跃迁时销毁重建原生窗口（句柄可能回收复用同一地
            // 址）——旧 DC 随旧窗口死亡，wglMakeCurrent 报 ERROR_INVALID_HANDLE。
            // 从句柄重新取 DC（新窗口需重设像素格式）并重试；仍失败则回落到
            // 承载窗口（渲染进离屏 FBO 照常，仅不上屏——同 filament destroySwapChain
            // 后的 rebind 语义）。
            if (glTrace())
                fprintf(stderr,
                        "[WGL] makeCurrent FAIL hwnd=%p dc=%p IsWindow=%d err=%lu -> self-heal\n",
                        sc->hWnd, sc->hDc, static_cast<int>(IsWindow(sc->hWnd)),
                        GetLastError());
            HDC fresh = GetDC(sc->hWnd);
            if (fresh) {
                if (GetPixelFormat(fresh) == 0) {
                    int pixelFormat = ChoosePixelFormat(fresh, &m_pfd);
                    SetPixelFormat(fresh, pixelFormat, &m_pfd);
                }
                if (wglMakeCurrent(fresh, m_context)) {
                    if (glTrace())
                        fprintf(stderr, "[WGL] self-heal OK freshDC=%p fmt=%d\n", fresh,
                                GetPixelFormat(fresh));
                    if (sc->hDc)
                        ReleaseDC(sc->hWnd, sc->hDc);
                    sc->hDc = fresh;
                    return true;
                }
                ReleaseDC(sc->hWnd, fresh);
            }
        }
    }

    wglMakeCurrent(m_deviceContext, m_context);
    return true;
}

// Ported from: filament PlatformWGL::commit (:304-310)。
void PlatformWgl::commit(void* swapChain) noexcept
{
    if (swapChain) {
        auto* sc = static_cast<WglSwapChain*>(swapChain);
        if (sc->hDc) {
            BOOL const ok = SwapBuffers(sc->hDc);
            if (glTrace() && !ok)
                fprintf(stderr, "[WGL] SwapBuffers FAIL hwnd=%p dc=%p IsWindow=%d err=%lu\n",
                        sc->hWnd, sc->hDc, static_cast<int>(IsWindow(sc->hWnd)),
                        GetLastError());
        }
    }
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE

#endif  // _WIN32
