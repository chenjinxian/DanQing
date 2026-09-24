// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Element mesh options
// Ported from: itwinjs-core core/common/src/ElementMesh.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// Options for element mesh generation.
// Ported from: itwinjs-core ElementMeshOptions
struct ElementMeshOptions {
    std::optional<double> chordTolerance;
    std::optional<double> angleTolerance;
    std::optional<double> minBRepFeatureSize;
};

// Request props for element mesh generation.
// Ported from: itwinjs-core ElementMeshRequestProps
struct ElementMeshRequestProps : ElementMeshOptions {
    std::string source;  // Element ID
};

END_DQ_COMMON_NAMESPACE
