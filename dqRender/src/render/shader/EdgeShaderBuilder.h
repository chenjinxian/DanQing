// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Edge shader builder
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Edge.ts
//
// Creates ProgramBuilder instances for edge rendering with three type variants:
// SegmentEdge (visible edges), Silhouette (silhouette edges), IndexedEdge (LUT).
#pragma once

#include "render/ShaderBuilder.h"
#include "render/TechniqueImpl.h"

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// EdgeBuilderType — edge geometry type
// Ported from: itwinjs-core Edge.ts EdgeBuilderType (line 26)
// ---------------------------------------------------------------------------
enum class EdgeBuilderType : uint8_t {
    SegmentEdge,
    Silhouette,
    IndexedEdge,
};

/// Create a ProgramBuilder for edge rendering.
/// Ported from: itwinjs-core Edge.ts createEdgeBuilder() (line 306)
ProgramBuilder createEdgeProgramBuilder(EdgeBuilderType type, FeatureMode featureMode,
                                        PositionType posType);

/// Create a ProgramBuilder for segment edge rendering (convenience).
inline ProgramBuilder createEdgeProgramBuilder(FeatureMode featureMode, PositionType posType)
{
    return createEdgeProgramBuilder(EdgeBuilderType::SegmentEdge, featureMode, posType);
}

END_DQ_RENDER_NAMESPACE
