// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — macOS OpenGL platform
// Ported from: filament backend/src/opengl/platforms/PlatformCocoaGL.mm
//
// Creates NSOpenGLContext, manages swap chains (NSView-based), and
// presents frames via [context flushBuffer].
#define GL_SILENCE_DEPRECATION
#import "dqRender/rhi/OpenGLPlatform.h"

#import <OpenGL/OpenGL.h>
#import <Cocoa/Cocoa.h>

#include "rhi/opengl/OpenGLDriver.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// ---------------------------------------------------------------------------
// PlatformCocoaGl — macOS NSOpenGL platform
// ---------------------------------------------------------------------------
class PlatformCocoaGl final : public OpenGLPlatform {
public:
    PlatformCocoaGl() = default;
    ~PlatformCocoaGl() override { terminate(); }

    void setExternalContext(void* nativeContext) override
    {
        // Accept an existing NSOpenGLContext (e.g., from Qt's QOpenGLContext)
        if (nativeContext) {
            m_context = (NSOpenGLContext*)nativeContext;
            m_externalContext = true;
        }
    }

    Driver* createDriver(void* sharedContext, DriverConfig const& config) override
    {
        // Create GL context if not provided and not using external context
        if (!m_context) {
            NSOpenGLPixelFormatAttribute attrs[] = {
                // DanQing's shaders declare `#version 410 core` (e.g.
                // CompositingShaderBuilders.h, SkyShaderBuilders.h, the Surface variant
                // compiler output), which requires a GL 4.1 Core context. macOS caps a
                // 3.2 Core context at GLSL 1.50, so a 3.2 context compiles the sources
                // but fails to LINK them ("Compiled vertex/fragment shader was corrupt")
                // — leaving the app with no valid programs (blank viewport). Use 4.1 Core
                // to match the shaders (same profile as the headless CGL test harness).
                NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
                NSOpenGLPFADoubleBuffer,
                NSOpenGLPFADepthSize, 24,
                NSOpenGLPFAStencilSize, 8,
                NSOpenGLPFAColorSize, 32,
                NSOpenGLPFASampleBuffers, 1,
                NSOpenGLPFASamples, 4,
                0
            };
            NSOpenGLPixelFormat* fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
            m_context = [[NSOpenGLContext alloc] initWithFormat:fmt
                                                 shareContext:(NSOpenGLContext*)sharedContext];
        }

        [m_context makeCurrentContext];

        auto* driver = new OpenGLDriver(*this, sharedContext, config);
        driver->initializeGL();
        return driver;
    }

    void terminate() noexcept override
    {
        if (m_context && !m_externalContext) {
            [NSOpenGLContext clearCurrentContext];
            m_context = nil;
        }
        // If using external context (Qt), don't release it — Qt owns it
    }

    void* createSwapChain(void* nativeWindow, uint64_t) override
    {
        if (!nativeWindow || !m_context) return nullptr;

        NSView* view = (NSView*)nativeWindow;
        [m_context setView:view];
        [m_context update];
        return nativeWindow;
    }

    void* createSwapChain(uint32_t, uint32_t, uint64_t) override
    {
        // Headless: create a hidden window + view
        NSWindow* window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, 1, 1)
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [window orderOut:nil];
        NSView* view = [[window contentView] retain];
        [m_context setView:view];
        [m_context update];
        return view;
    }

    void destroySwapChain(void* swapChain) noexcept override
    {
        // Nothing to do for Phase 0
        (void)swapChain;
    }

    bool makeCurrent(void* draw, void* read) noexcept override
    {
        (void)read;
        if (m_context) {
            [m_context makeCurrentContext];
            if (draw) {
                [m_context setView:(NSView*)draw];
                [m_context update];
            }
            return true;
        }
        return false;
    }

    void commit(void*) noexcept override
    {
        if (m_context) {
            [m_context flushBuffer];
        }
    }

private:
    NSOpenGLContext* m_context = nil;
    bool m_externalContext = false;
};

// Factory function for macOS
OpenGLPlatform* createPlatformCocoaGl()
{
    return new PlatformCocoaGl();
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
