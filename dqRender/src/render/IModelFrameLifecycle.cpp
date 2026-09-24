// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Frame lifecycle event hooks implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IModelFrameLifecycle.ts
#include "IModelFrameLifecycle.h"

BEGIN_DQ_RENDER_NAMESPACE

// Static storage for registered callbacks.
FrameLifecycle::BeforeRenderCallback FrameLifecycle::sOnBeforeRender = nullptr;
FrameLifecycle::RenderOpaqueCallback FrameLifecycle::sOnRenderOpaque = nullptr;
FrameLifecycle::CameraFrustumCallback FrameLifecycle::sOnChangeCameraFrustum = nullptr;
FrameLifecycle::CameraViewCallback FrameLifecycle::sOnChangeCameraView = nullptr;

// ---------------------------------------------------------------------------
// Setters
// (Ported from: itwinjs-core IModelFrameLifecycle.ts)
// ---------------------------------------------------------------------------
void FrameLifecycle::setOnBeforeRender(BeforeRenderCallback cb)
{
    sOnBeforeRender = cb;
}

void FrameLifecycle::setOnRenderOpaque(RenderOpaqueCallback cb)
{
    sOnRenderOpaque = cb;
}

void FrameLifecycle::setOnChangeCameraFrustum(CameraFrustumCallback cb)
{
    sOnChangeCameraFrustum = cb;
}

void FrameLifecycle::setOnChangeCameraView(CameraViewCallback cb)
{
    sOnChangeCameraView = cb;
}

// ---------------------------------------------------------------------------
// Fire methods
// (Ported from: itwinjs-core IModelFrameLifecycle.ts)
// ---------------------------------------------------------------------------
void FrameLifecycle::fireOnBeforeRender()
{
    if (sOnBeforeRender) sOnBeforeRender();
}

void FrameLifecycle::fireOnRenderOpaque()
{
    if (sOnRenderOpaque) sOnRenderOpaque();
}

void FrameLifecycle::fireOnChangeCameraFrustum(float left, float right, float bottom,
                                                float top, float near, float far)
{
    if (sOnChangeCameraFrustum) sOnChangeCameraFrustum(left, right, bottom, top, near, far);
}

void FrameLifecycle::fireOnChangeCameraView(float const* position, float const* viewX,
                                             float const* viewY, float const* viewZ)
{
    if (sOnChangeCameraView) sOnChangeCameraView(position, viewX, viewY, viewZ);
}

END_DQ_RENDER_NAMESPACE
