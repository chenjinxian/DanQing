// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — GeometryParams implementation
//
// Ported from: itwinjs-core core/common/src/GeometryParams.ts
#include "dqCommon/GraphicParams.h"

BEGIN_DQ_COMMON_NAMESPACE

// Ported from: itwinjs-core GeometryParams.isEquivalent() (lines 172-238)
bool GeometryParams::isEquivalent(const GeometryParams& other) const noexcept
{
    if (this == &other)
        return true;

    if (categoryId != other.categoryId)
        return false;
    if (subCategoryId != other.subCategoryId)
        return false;
    if (geometryClass != other.geometryClass)
        return false;

    if (elmPriority != other.elmPriority)
        return false;
    if (elmTransparency != other.elmTransparency)
        return false;
    if (fillTransparency != other.fillTransparency)
        return false;

    // lineColor
    if (lineColor.has_value() != other.lineColor.has_value())
        return false;
    if (lineColor.has_value() && !lineColor->equals(*other.lineColor))
        return false;

    if (weight != other.weight)
        return false;

    // materialId (Id64String identity compare, ref lines 199-203)
    if (materialId.has_value() != other.materialId.has_value())
        return false;
    if (materialId.has_value() && *materialId != *other.materialId)
        return false;

    // styleInfo: not ported (DanQing has no LineStyle.Info yet).

    if (fillDisplay != other.fillDisplay)
        return false;

    // Gradient / backgroundFill / fillColor are only relevant when fillDisplay != Never.
    if (fillDisplay.has_value() && *fillDisplay != FillDisplay::Never) {
        if (gradient.has_value() != other.gradient.has_value())
            return false;
        if (gradient.has_value() && !gradient->equals(*other.gradient))
            return false;

        if (backgroundFill != other.backgroundFill)
            return false;

        if (!backgroundFill.has_value() || *backgroundFill == BackgroundFill::None) {
            if (fillColor.has_value() != other.fillColor.has_value())
                return false;
            if (fillColor.has_value() && !fillColor->equals(*other.fillColor))
                return false;
        }
    }

    // pattern: not ported (DanQing has no AreaPattern.Params yet).

    return true;
}

END_DQ_COMMON_NAMESPACE
