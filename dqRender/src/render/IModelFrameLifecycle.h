// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Frame lifecycle event hooks
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/IModelFrameLifecycle.ts
//
// Provides static callback registration for frame lifecycle events.
// These hooks allow subsystems (tile loading, diagnostics, feature
// overrides) to run at specific points in the render loop without
// coupling to the TargetImpl directly.
//
// Uses plain function pointers since DqEvent cannot be used in a
// header-only interface without adding dependencies.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FrameLifecycle — static frame lifecycle event hooks
// (Ported from: itwinjs-core IModelFrameLifecycle.ts)
// ---------------------------------------------------------------------------
class FrameLifecycle {
public:
    /// Callback invoked before any rendering begins for a frame.
    using BeforeRenderCallback = void (*)();

    /// Callback invoked when opaque geometry is about to be rendered.
    using RenderOpaqueCallback = void (*)();

    /// Callback invoked when the camera frustum changes.
    /// @param left, right, bottom, top, near, far  Frustum planes.
    using CameraFrustumCallback = void (*)(float left, float right, float bottom,
                                           float top, float near, float far);

    /// Callback invoked when the camera view (position + orientation) changes.
    /// @param position  Camera position (3 floats).
    /// @param viewX     View X-axis (right) direction (3 floats).
    /// @param viewY     View Y-axis (up) direction (3 floats).
    /// @param viewZ     View Z-axis (forward) direction (3 floats).
    using CameraViewCallback = void (*)(float const* position, float const* viewX,
                                        float const* viewY, float const* viewZ);

    /// Register the "before render" callback (replaces any previous).
    static void setOnBeforeRender(BeforeRenderCallback cb);

    /// Register the "render opaque" callback.
    static void setOnRenderOpaque(RenderOpaqueCallback cb);

    /// Register the "camera frustum changed" callback.
    static void setOnChangeCameraFrustum(CameraFrustumCallback cb);

    /// Register the "camera view changed" callback.
    static void setOnChangeCameraView(CameraViewCallback cb);

    /// Fire the "before render" event (calls registered callback if set).
    static void fireOnBeforeRender();

    /// Fire the "render opaque" event.
    static void fireOnRenderOpaque();

    /// Fire the "camera frustum changed" event.
    static void fireOnChangeCameraFrustum(float left, float right, float bottom,
                                          float top, float near, float far);

    /// Fire the "camera view changed" event.
    static void fireOnChangeCameraView(float const* position, float const* viewX,
                                       float const* viewY, float const* viewZ);

private:
    FrameLifecycle() = delete;

    static BeforeRenderCallback sOnBeforeRender;
    static RenderOpaqueCallback sOnRenderOpaque;
    static CameraFrustumCallback sOnChangeCameraFrustum;
    static CameraViewCallback sOnChangeCameraView;
};

END_DQ_RENDER_NAMESPACE
