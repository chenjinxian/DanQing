// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Conditional diagnostic utilities
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Diagnostics.ts
//
// Integration point: itwinjs checked framebuffer status via
// System.instance.context.checkFramebufferStatus().  In dqRender this would
// go through Driver, but is not yet wired up.
#include "Diagnostics.h"

BEGIN_DQ_RENDER_NAMESPACE

bool Debug::sPrintEnabled = false;
bool Debug::sEvaluateEnabled = false;

void Debug::print(char const* message)
{
    if (sPrintEnabled && nullptr != message)
        std::fprintf(stderr, "%s\n", message);
}

bool Debug::isValidFrameBuffer()
{
    // itwinjs: GL.FrameBuffer.Status.Complete === this.checkFrameBufferStatus()
    // In dqRender this would query the Driver for the current FBO status.
    // Until that is wired up, default to true (assume complete).
    return true;
}

END_DQ_RENDER_NAMESPACE
