// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — SubCategoryAppearance and SubCategoryOverride implementation
//
// Ported from: itwinjs-core core/common/src/SubCategoryAppearance.ts
//              core/common/src/SubCategoryOverride.ts
#include "dqCommon/SubCategoryAppearance.h"

BEGIN_DQ_COMMON_NAMESPACE

static const SubCategoryAppearance s_subCatDefaults{};
static const SubCategoryOverride s_subCatOverrideDefaults{};

const SubCategoryAppearance& SubCategoryAppearance::defaults() noexcept
{
    return s_subCatDefaults;
}

const SubCategoryOverride& SubCategoryOverride::defaults() noexcept
{
    return s_subCatOverrideDefaults;
}

// Ported from: itwinjs-core SubCategoryOverride.override()
SubCategoryAppearance SubCategoryOverride::override(const SubCategoryAppearance& appearance) const
{
    if (!anyOverridden())
        return appearance;

    SubCategoryAppearance result = appearance;
    if (invisible.has_value())
        result.invisible = invisible.value();
    if (weight.has_value())
        result.weight = weight.value();
    if (priority.has_value())
        result.priority = priority.value();
    if (material.has_value())
        result.materialId = material.value();
    if (transparency.has_value())
        result.transparency = transparency.value();
    if (color.has_value())
        result.color = color.value();
    return result;
}

// Ported from: itwinjs-core SubCategoryOverride.equals()
bool SubCategoryOverride::equals(const SubCategoryOverride& other) const noexcept
{
    if (invisible != other.invisible || weight != other.weight ||
        priority != other.priority || material != other.material || transparency != other.transparency)
        return false;

    if (color.has_value() && other.color.has_value())
        return color->getTbgr() == other.color->getTbgr();
    return !color.has_value() && !other.color.has_value();
}

// Ported from: itwinjs-core SubCategoryOverride.fromJSON()
SubCategoryOverride SubCategoryOverride::fromJSON(std::optional<ColorDef> color,
                                                   std::optional<bool> invisible,
                                                   std::optional<int> weight,
                                                   std::optional<int> priority,
                                                   std::optional<double> transparency)
{
    SubCategoryOverride ovr;
    ovr.color = color;
    ovr.invisible = invisible;
    ovr.weight = weight;
    ovr.priority = priority;
    ovr.transparency = transparency;
    return ovr;
}

END_DQ_COMMON_NAMESPACE
