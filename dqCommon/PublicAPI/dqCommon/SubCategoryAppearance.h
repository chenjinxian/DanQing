// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — SubCategory appearance and override
//
// Ported from: itwinjs-core core/common/src/SubCategoryAppearance.ts
//              core/common/src/SubCategoryOverride.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "DqCommon.h"

#include <dqBase/DqId.h>

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Parameters defining how geometry on a SubCategory appears.
// Ported from: itwinjs-core SubCategoryAppearance
class DQ_COMMON_EXPORT SubCategoryAppearance {
public:
    ColorDef color = ColorDef::black;
    int weight = 0;
    int priority = 0;
    double transparency = 0.0;
    bool invisible = false;
    bool dontPlot = false;
    bool dontSnap = false;
    bool dontLocate = false;
    dqBase::DqId styleId;
    dqBase::DqId materialId;

    // Fill color (defaults to line color).
    ColorDef getFillColor() const noexcept { return m_fillColor.has_value() ? m_fillColor.value() : color; }
    // Fill transparency (defaults to line transparency).
    double getFillTransparency() const noexcept { return m_fillTransparency.has_value() ? m_fillTransparency.value() : transparency; }

    // Set fill color override.
    void setFillColor(const ColorDef& c) noexcept { m_fillColor = c; }
    void setFillTransparency(double t) noexcept { m_fillTransparency = t; }

    // Default constructor.
    // Ported from: itwinjs-core SubCategoryAppearance constructor (no args)
    SubCategoryAppearance() noexcept = default;

    // Equality.
    // Ported from: itwinjs-core SubCategoryAppearance.equals()
    bool equals(const SubCategoryAppearance& other) const noexcept
    {
        return invisible == other.invisible &&
               dontPlot == other.dontPlot &&
               dontSnap == other.dontSnap &&
               dontLocate == other.dontLocate &&
               color.equals(other.color) &&
               weight == other.weight &&
               priority == other.priority &&
               styleId == other.styleId &&
               materialId == other.materialId &&
               transparency == other.transparency &&
               getFillColor().equals(other.getFillColor()) &&
               getFillTransparency() == other.getFillTransparency();
    }

    // clone.
    SubCategoryAppearance clone() const { return *this; }

    // Static defaults.
    static const SubCategoryAppearance& defaults() noexcept;

private:
    std::optional<ColorDef> m_fillColor;
    std::optional<double> m_fillTransparency;
};

// Overrides selected aspects of a SubCategoryAppearance.
// Ported from: itwinjs-core core/common/src/SubCategoryOverride.ts
class DQ_COMMON_EXPORT SubCategoryOverride {
public:
    std::optional<ColorDef> color;
    std::optional<bool> invisible;
    std::optional<int> weight;
    std::optional<int> priority;
    std::optional<dqBase::DqId> material;
    std::optional<double> transparency;

    // Whether any aspect is overridden.
    // Ported from: itwinjs-core SubCategoryOverride.anyOverridden
    bool anyOverridden() const noexcept
    {
        return invisible.has_value() || color.has_value() || weight.has_value() ||
               priority.has_value() || material.has_value() || transparency.has_value();
    }

    // Apply this override to a SubCategoryAppearance.
    // Ported from: itwinjs-core SubCategoryOverride.override()
    SubCategoryAppearance override(const SubCategoryAppearance& appearance) const;

    // Equality.
    // Ported from: itwinjs-core SubCategoryOverride.equals()
    bool equals(const SubCategoryOverride& other) const noexcept;

    // Create from optional props.
    static SubCategoryOverride fromJSON(std::optional<ColorDef> color = std::nullopt,
                                        std::optional<bool> invisible = std::nullopt,
                                        std::optional<int> weight = std::nullopt,
                                        std::optional<int> priority = std::nullopt,
                                        std::optional<double> transparency = std::nullopt);

    // Static defaults (overrides nothing).
    static const SubCategoryOverride& defaults() noexcept;
};

END_DQ_COMMON_NAMESPACE
