// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ViewFlags for rendering pipeline
// Ported from: itwinjs-core core/common/src/ViewFlags.ts
//
// Uses dqCommon::ViewFlagsProperties as the mutable view flag struct for
// per-branch rendering state. This avoids duplicating the ViewFlags definition
// while providing direct field access needed by the rendering pipeline.
//
// In itwinjs-core, BranchState stores a ViewFlags object directly.
// In DanQing, BranchState stores ViewFlagsProperties (mutable struct with
// direct field access) for performance.
#pragma once

#include <dqCommon/ViewFlags.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

// NOTE: #include must be BEFORE namespace opening to avoid pulling
// dqCommon declarations into dqRender namespace.

BEGIN_DQ_RENDER_NAMESPACE

// Bring dqCommon types into dqRender namespace for convenience.
using dqCommon::RenderMode;
using dqCommon::ViewFlagsProperties;

// ViewFlags for the rendering pipeline is dqCommon::ViewFlagsProperties.
// It provides direct mutable field access (renderMode, visibleEdges, etc.)
// matching itwinjs-core's ViewFlags property access pattern.
using RenderViewFlags = ViewFlagsProperties;

// Convenience alias — most code just says "ViewFlags".
using ViewFlags = ViewFlagsProperties;

END_DQ_RENDER_NAMESPACE
