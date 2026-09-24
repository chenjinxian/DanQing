// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature overrides (per-element color/weight)
// Ported from: itwinjs-core core/frontend/src/render/FeatureSymbology.ts
//
// Manages per-feature color, weight, and transparency overrides.
// Stored as a packed RGBA texture where feature ID → override color.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FeatureGpuAppearance — lightweight per-feature override for GPU packing
// (Ported from: itwinjs-core FeatureSymbology.ts FeatureAppearance)
//
// Distinct from the full FeatureAppearance in FeatureSymbology.h — this is a
// minimal type used for packed RGBA texture upload to the GPU. It intentionally
// has fewer fields (alpha instead of transparency, no line overrides).
// ---------------------------------------------------------------------------
struct FeatureGpuAppearance {
    uint32_t rgb = 0xFFFFFF;       // RGB color (0xRRGGBB)
    uint8_t alpha = 0xFF;          // Alpha (0-255)
    uint8_t weight = 0;            // Line weight (0 = unchanged)
    bool emphasized = false;       // Hilite/emphasize flag
    bool nonLocatable = false;     // Non-locatable (excluded from queries unless opted-in)

    static FeatureGpuAppearance Default() noexcept { return FeatureGpuAppearance{}; }  // NOLINT(readability-identifier-naming) `default` is a C++ keyword; FeatureGpuAppearance is a DanQing struct (§3.4 keyword deviation)

    bool operator==(FeatureGpuAppearance const& o) const noexcept
    {
        return rgb == o.rgb && alpha == o.alpha && weight == o.weight && emphasized == o.emphasized
            && nonLocatable == o.nonLocatable;
    }
    bool operator!=(FeatureGpuAppearance const& o) const noexcept { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// FeatureOverrides — manages per-feature visual overrides
// (Ported from: itwinjs-core FeatureOverrides.ts)
// ---------------------------------------------------------------------------
class FeatureOverrides {
public:
    FeatureOverrides() = default;
    ~FeatureOverrides() = default;

    /// Set the appearance for a specific feature ID.
    void setAppearance(uint32_t featureId, FeatureGpuAppearance const& appearance);

    /// Remove the override for a specific feature ID.
    void removeAppearance(uint32_t featureId);

    /// clear all overrides.
    void clear();

    /// Check if any overrides exist.
    bool isEmpty() const noexcept { return m_overrides.empty(); }

    /// Get the override for a feature ID, or Default if none.
    FeatureGpuAppearance getAppearance(uint32_t featureId) const;

    /// Build the packed RGBA texture data for GPU upload.
    /// Returns a vector of RGBA bytes (4 bytes per feature).
    std::vector<uint8_t> buildTextureData() const;

    /// Get the number of overridden features.
    size_t getOverrideCount() const noexcept { return m_overrides.size(); }

    /// Get the maximum feature ID (for texture sizing).
    uint32_t getMaxFeatureId() const noexcept { return m_maxFeatureId; }

    /// True if every feature is hidden (no override entries or all entries hidden).
    bool allHidden() const noexcept { return m_allHidden; }

    /// True if at least one feature has a non-default override.
    bool anyOverridden() const noexcept { return m_anyOverridden; }

    /// Set the hidden state (used during override computation).
    void setAllHidden(bool hidden) noexcept { m_allHidden = hidden; }

    /// Set the any-overridden state (used during override computation).
    void setAnyOverridden(bool any) noexcept { m_anyOverridden = any; }

private:
    std::unordered_map<uint32_t, FeatureGpuAppearance> m_overrides;
    uint32_t m_maxFeatureId = 0;
    bool m_allHidden = false;
    bool m_anyOverridden = false;
};

END_DQ_RENDER_NAMESPACE
