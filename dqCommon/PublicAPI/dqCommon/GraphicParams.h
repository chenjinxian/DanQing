// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Graphic params and geometry params
//
// Ported from: itwinjs-core core/common/src/GraphicParams.ts
//              core/common/src/GeometryParams.ts
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "FillFlags.h"
#include "GeometryClass.h"
#include "Gradient.h"
#include "LinePixels.h"
#include "RenderMaterial.h"
#include "DqCommon.h"

#include <dqBase/RefCounted.h>
#include <dqBase/DqId.h>

#include <cstdint>
#include <memory>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Whether a closed region should be filled in wireframe.
// Ported from: itwinjs-core GeometryParams.ts
enum class FillDisplay : uint8_t {
    Never = 0,
    ByView = 1,
    Always = 2,
    Blanking = 3,
};

// How a view's background color affects the interior of a closed region.
// Ported from: itwinjs-core GeometryParams.ts
enum class BackgroundFill : uint8_t {
    None = 0,
    Solid = 1,
    Outline = 2,
};

// The "cooked" material and symbology for a RenderGraphic.
// Ported from: itwinjs-core GraphicParams.ts
class DQ_COMMON_EXPORT GraphicParams {
public:
    FillFlags fillFlags = FillFlags::None;
    LinePixels linePixels = LinePixels::Solid;
    int rasterWidth = 1;
    ColorDef lineColor = ColorDef::black;
    ColorDef fillColor = ColorDef::black;
    // Material applied to surfaces. (ref line 52)
    dqBase::RefPtr<RenderMaterial> material;
    // Gradient fill applied to surfaces. (ref line 54)
    std::optional<GradientSymb> gradient;

    // Set line transparency (0=opaque, 255=transparent).
    // Ported from: itwinjs-core GraphicParams.setLineTransparency()
    void setLineTransparency(int transparency) { lineColor = lineColor.withTransparency(transparency); }

    // Set fill transparency (0=opaque, 255=transparent).
    // Ported from: itwinjs-core GraphicParams.setFillTransparency()
    void setFillTransparency(int transparency) { fillColor = fillColor.withTransparency(transparency); }

    // clone. (ref lines 62-74) — copies material pointer + gradient value.
    // Ported from: itwinjs-core GraphicParams.clone()
    GraphicParams clone() const { return *this; }

    // Create from commonly-used properties.
    // Ported from: itwinjs-core GraphicParams.fromSymbology()
    static GraphicParams fromSymbology(const ColorDef& lineColor, const ColorDef& fillColor,
                                        int lineWidth, LinePixels pixels = LinePixels::Solid)
    {
        GraphicParams params;
        params.lineColor = lineColor;
        params.fillColor = fillColor;
        params.rasterWidth = lineWidth;
        params.linePixels = pixels;
        return params;
    }

    // Create with blanking fill.
    // Ported from: itwinjs-core GraphicParams.fromBlankingFill()
    static GraphicParams fromBlankingFill(const ColorDef& fillColor)
    {
        GraphicParams params;
        params.fillColor = fillColor;
        params.fillFlags = FillFlags::Blanking;
        return params;
    }
};

// Describes display properties of graphics not inherited from SubCategoryAppearance.
// Ported from: itwinjs-core GeometryParams.ts
class DQ_COMMON_EXPORT GeometryParams {
public:
    dqBase::DqId categoryId;
    dqBase::DqId subCategoryId;

    std::optional<dqBase::DqId> materialId;
    std::optional<int> elmPriority;
    std::optional<int> weight;
    std::optional<ColorDef> lineColor;
    std::optional<ColorDef> fillColor;
    std::optional<BackgroundFill> backgroundFill;
    std::optional<FillDisplay> fillDisplay;
    std::optional<double> elmTransparency;
    std::optional<double> fillTransparency;
    // Optional geometry classification. Default Primary. (ref line 112)
    std::optional<GeometryClass> geometryClass;
    // Optional gradient fill settings. (ref line 120) — already present as `gradient`.
    std::optional<GradientSymb> gradient;

    // Constructor.
    // Ported from: itwinjs-core GeometryParams constructor (lines 131-134)
    // @note If a valid subCategoryId is not supplied, the default subcategory for the category is
    //       used (ref: IModel.getDefaultSubCategoryId).
    GeometryParams(const dqBase::DqId& catId, const dqBase::DqId& subCatId = dqBase::DqId{})
        : categoryId(catId)
    {
        subCategoryId = subCatId.isValid() ? subCatId : getDefaultSubCategoryId(catId);
    }

    // clone. (ref lines 136-152) — deep copy of optional gradient/style/pattern.
    // Ported from: itwinjs-core GeometryParams.clone()
    GeometryParams clone() const { return *this; }

    // Clear overrides while preserving category/subcategory. (ref lines 155-169)
    // Ported from: itwinjs-core GeometryParams.resetAppearance()
    void resetAppearance() noexcept
    {
        materialId.reset();
        elmPriority.reset();
        weight.reset();
        lineColor.reset();
        fillColor.reset();
        backgroundFill.reset();
        fillDisplay.reset();
        elmTransparency.reset();
        fillTransparency.reset();
        geometryClass.reset();
        gradient.reset();
    }

    // Compare for equivalence. (ref lines 172-238)
    // Ported from: itwinjs-core GeometryParams.isEquivalent()
    bool isEquivalent(const GeometryParams& other) const noexcept;

    // Change categoryId, reset subcategoryId to the category's default subcategory, and
    // optionally clear appearance overrides. (ref lines 241-246)
    // Ported from: itwinjs-core GeometryParams.setCategoryId()
    void setCategoryId(const dqBase::DqId& newCategoryId, bool clearAppearanceOverrides = true)
    {
        categoryId = newCategoryId;
        subCategoryId = getDefaultSubCategoryId(newCategoryId);
        if (clearAppearanceOverrides)
            resetAppearance();
    }

    // Change subcategoryId and optionally clear appearance overrides. (ref lines 249-253)
    // Ported from: itwinjs-core GeometryParams.setSubCategoryId()
    void setSubCategoryId(const dqBase::DqId& newSubCategoryId, bool clearAppearanceOverrides = true)
    {
        subCategoryId = newSubCategoryId;
        if (clearAppearanceOverrides)
            resetAppearance();
    }

private:
    // Returns the default subcategory Id for a category: localId + 1 in the same briefcase.
    // Ported from: itwinjs-core IModel.getDefaultSubCategoryId (line 636-638)
    static dqBase::DqId getDefaultSubCategoryId(const dqBase::DqId& categoryId) noexcept
    {
        if (!categoryId.isValid())
            return dqBase::DqId{};
        const uint32_t local = dqBase::Id64::GetLocalId(categoryId);
        const uint32_t briefcase = dqBase::Id64::GetBriefcaseId(categoryId);
        return dqBase::Id64::FromLocalAndBriefcaseIds(local + 1, briefcase);
    }
};

END_DQ_COMMON_NAMESPACE
