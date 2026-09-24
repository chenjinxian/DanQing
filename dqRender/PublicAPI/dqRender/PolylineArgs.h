// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — arguments for creating indexed polylines.
//
// Ported from: itwinjs-core core/frontend/src/render/PolylineArgs.ts
//
// Dedicated header matching the itwinjs-core filename 1:1. The args struct +
// supporting types (PolylineFlags enum, PolylineIndices alias) were previously
// inlined in MeshArgs.h; they remain there (they share Q-point / ColorIndex /
// FeatureIndex machinery with MeshArgs) and this header re-exports them for
// the 1:1 file mapping. No logic is invented (§6).
#pragma once

#include "Export.h"
#include "MeshArgs.h"  // PolylineArgs, PolylineFlags, PolylineIndices, OctEncodedNormal

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Re-export aliases for documentation discoverability — the canonical
// declarations live in MeshArgs.h.
// Ported from: itwinjs-core PolylineArgs interface
using PolylineArgsReExport = PolylineArgs;

END_DQ_RENDER_NAMESPACE
