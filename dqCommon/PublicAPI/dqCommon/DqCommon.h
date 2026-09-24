// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Common types module bootstrap
//
// dqCommon corresponds to @itwin/core-common in itwinjs-core.
// Depends on dqBase + dqGeom. Provides shared types used by dqRender and dqApp:
// colors, view flags, frustum, feature tables, display styles, etc.
//
// Ported from: itwinjs-core core/common/src/
#pragma once

#include <dqBase/DqBase.h>
#include <dqGeom/DqGeom.h>

#include "Export.h"

// ---------------------------------------------------------------------------
// dqCommon version
// ---------------------------------------------------------------------------
#define DQ_COMMON_VERSION_MAJOR 0
#define DQ_COMMON_VERSION_MINOR 1
#define DQ_COMMON_VERSION_PATCH 0
#define DQ_COMMON_VERSION_STRING "0.1.0"

// ---------------------------------------------------------------------------
// Namespace macros
// ---------------------------------------------------------------------------
#define BEGIN_DQ_COMMON_NAMESPACE namespace dqCommon {
#define END_DQ_COMMON_NAMESPACE }

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// Global initialization (once per process)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT DqCommonLib {
public:
    static void initialize() noexcept;
    static void shutdown() noexcept;
};

END_DQ_COMMON_NAMESPACE
