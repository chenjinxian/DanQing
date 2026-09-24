// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Platform factory (auto-selects platform)
// Authored: platform abstraction layer for Qt/OpenGL, no direct reference equivalent
//
// Automatically selects the correct platform implementation based on
// the build target (macOS, Windows, Linux).
#pragma once

#include "dqRender/rhi/OpenGLPlatform.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

/// Create the platform-specific OpenGL platform instance.
/// Caller owns the returned pointer.
/// - macOS: PlatformCocoaGl
/// - Windows: PlatformWgl
/// - Linux: PlatformGlx (or PlatformEgl for headless)
OpenGLPlatform* createPlatform();

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
