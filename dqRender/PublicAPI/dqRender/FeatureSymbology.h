// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature symbology public types
//
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts (FeatureAppearance, lines 137+)
// Public API for per-feature visual overrides (color, transparency, weight, visibility).
// Internal implementation lives in src/render/FeatureSymbology.h.
#pragma once

#include "Export.h"

#include <dqCommon/GeometryClass.h>

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// GeometryClass is the authoritative dqCommon::GeometryClass (Primary/Construction/
// Dimension/Pattern, Ported from GeometryParams.ts). Re-exported here so dqRender
// feature-override code can refer to it unqualified, matching itwinjs-core where
// FeatureSymbology.ts uses [GeometryClass]($common).
// (Previously this header defined a divergent 10-member GeometryClass {Solid,Surface,
//  Wire,Edge,Text,Particle,RenderTimeline,...} that conflicted with dqCommon's and caused
//  a type mismatch with feature.geometryClass — removed per §0/§7.)
using dqCommon::GeometryClass;

// ---------------------------------------------------------------------------
// FeatureAppearance — immutable per-feature visual override
// (Ported from: itwinjs-core FeatureSymbology.ts FeatureAppearance)
//
// Represents the visual override for a single feature (element, subcategory,
// or model). All fields use sentinel values to indicate "not overridden"
// (e.g. rgb = 0 means no color override). Use extendAppearance to merge
// two appearances with override-wins semantics.
// ---------------------------------------------------------------------------
struct DQ_RENDER_EXPORT FeatureAppearance {
    uint32_t rgb = 0;
    uint32_t lineRgb = 0;
    uint8_t weight = 0;
    float transparency = 0.0f;
    float lineTransparency = 0.0f;
    uint16_t linePixels = 0;
    bool ignoresMaterial = false;
    bool nonLocatable = false;
    bool emphasized = false;

    /// Default appearance (nothing overridden).
    static FeatureAppearance defaults() noexcept { return FeatureAppearance{}; }

    /// Create appearance with only rgb overridden.
    static FeatureAppearance fromRgb(uint32_t color) noexcept;

    /// Create appearance with only transparency overridden.
    static FeatureAppearance fromTransparency(float t) noexcept;

    /// True if this appearance makes the feature fully transparent.
    /// Ported from: itwinjs-core FeatureSymbology.ts FeatureAppearance.isFullyTransparent
    /// Both surface and line transparency must be >= 1.0.  If lineTransparency is
    /// zero (unset), it falls back to the surface transparency.
    bool isFullyTransparent() const noexcept {
        float surf = transparency;
        float line = lineTransparency > 0.0f ? lineTransparency : transparency;
        return surf >= 1.0f && line >= 1.0f;
    }

    /// True if any field differs from the default appearance.
    bool anyOverridden() const noexcept;

    /// Equality comparison (all fields must match).
    bool equals(FeatureAppearance const& o) const noexcept;

    /// Merge this appearance on top of a base appearance.
    /// Non-zero / non-default fields in *this* override the base.
    FeatureAppearance extendAppearance(FeatureAppearance const& base) const noexcept;

    /// Create a copy of this appearance.
    FeatureAppearance clone() const noexcept { return *this; }
};

// Forward declarations — full types in internal headers.
class FeatureOverridesBase;
class FeatureSymbologyOverrides;

END_DQ_RENDER_NAMESPACE
