// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — FrameStats (timing statistics for a single rendered frame).
//
// Ported from: itwinjs-core core/frontend/src/render/FrameStats.ts
//
// Type-only port: a plain aggregate of CPU-time-in-ms fields matching the
// itwinjs-core interface field-by-field (§5). The fields frameId, totalFrameTime,
// and totalSceneTime are mandatory; the rest may be zero on frames that skip
// the corresponding operation.
#pragma once

#include "Export.h"

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Describes timing statistics for a single rendered frame.
// Ported from: itwinjs-core FrameStats (alpha interface)
struct DQ_RENDER_EXPORT FrameStats {
    // A unique number identifying the frame to which these statistics belong.
    uint64_t frameId = 0;

    // CPU time (ms) spent setting up the scene. Does NOT include totalFrameTime.
    double totalSceneTime = 0.0;

    // CPU time (ms) spent on animations during scene setup (subset of totalSceneTime).
    double animationTime = 0.0;

    // CPU time (ms) spent setting up the view during scene setup (subset of totalSceneTime).
    double setupViewTime = 0.0;

    // CPU time (ms) spent creating/changing the invalid scene (subset of totalSceneTime).
    double createChangeSceneTime = 0.0;

    // CPU time (ms) spent validating the render plan (subset of totalSceneTime).
    double validateRenderPlanTime = 0.0;

    // CPU time (ms) spent adding/changing decorations (subset of totalSceneTime).
    double decorationsTime = 0.0;

    // CPU time (ms) spent executing target.onBeforeRender (subset of totalSceneTime).
    double onBeforeRenderTime = 0.0;

    // CPU time (ms) spent rendering the frame. Does NOT include totalSceneTime.
    double totalFrameTime = 0.0;

    // CPU time (ms) spent rendering opaque geometry (subset of totalFrameTime).
    double opaqueTime = 0.0;

    // CPU time (ms) spent in IModelFrameLifecycle.onRenderOpaque (subset of totalFrameTime).
    double onRenderOpaqueTime = 0.0;

    // CPU time (ms) spent rendering translucent geometry (subset of totalFrameTime).
    double translucentTime = 0.0;

    // CPU time (ms) spent rendering overlays (subset of totalFrameTime).
    double overlaysTime = 0.0;

    // CPU time (ms) spent rendering the solar shadow map (subset of totalFrameTime).
    double shadowsTime = 0.0;

    // CPU time (ms) spent rendering planar and volume classifiers (subset of totalFrameTime).
    double classifiersTime = 0.0;

    // CPU time (ms) spent applying screenspace effects (subset of totalFrameTime).
    double screenspaceEffectsTime = 0.0;

    // CPU time (ms) spent rendering background geometry (subset of totalFrameTime).
    double backgroundTime = 0.0;
};

END_DQ_RENDER_NAMESPACE
