// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Polyline shader builder
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Polyline.ts
#pragma once

#include "render/ShaderBuilder.h"
#include "render/TechniqueImpl.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

/// Create a ProgramBuilder for polyline rendering (full joint/miter system).
/// Ported from: itwinjs-core Polyline.ts createPolylineBuilder() (line 413)
ProgramBuilder createPolylineProgramBuilder(FeatureMode featureMode, PositionType posType);

/// Create a ProgramBuilder for point string rendering.
/// Ported from: itwinjs-core PointString.ts createPointStringBuilder()
ProgramBuilder createPointStringProgramBuilder(FeatureMode featureMode, PositionType posType);

/// Create a ProgramBuilder for point cloud rendering.
/// Ported from: itwinjs-core PointCloud.ts createPointCloudBuilder()
ProgramBuilder createPointCloudProgramBuilder(FeatureMode featureMode, PositionType posType);

END_DQ_RENDER_NAMESPACE
