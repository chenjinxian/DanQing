// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Feature symbology override implementation
// Ported from: itwinjs-core core/frontend/src/render/FeatureSymbology.ts
#include "FeatureSymbology.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FeatureAppearance
// ---------------------------------------------------------------------------

FeatureAppearance FeatureAppearance::fromRgb(uint32_t color) noexcept
{
    FeatureAppearance a;
    a.rgb = color;
    return a;
}

FeatureAppearance FeatureAppearance::fromTransparency(float t) noexcept
{
    FeatureAppearance a;
    a.transparency = t;
    return a;
}

bool FeatureAppearance::anyOverridden() const noexcept
{
    return rgb != 0 || lineRgb != 0 || weight != 0 || transparency != 0.0f ||
           lineTransparency != 0.0f || linePixels != 0 || ignoresMaterial ||
           nonLocatable || emphasized;
}

bool FeatureAppearance::equals(FeatureAppearance const& o) const noexcept
{
    return rgb == o.rgb && lineRgb == o.lineRgb && weight == o.weight &&
           transparency == o.transparency && lineTransparency == o.lineTransparency &&
           linePixels == o.linePixels && ignoresMaterial == o.ignoresMaterial &&
           nonLocatable == o.nonLocatable && emphasized == o.emphasized;
}

FeatureAppearance FeatureAppearance::extendAppearance(FeatureAppearance const& base) const noexcept
{
    FeatureAppearance result = base;

    // override-wins: non-zero fields in *this* replace base values.
    if (rgb != 0)
        result.rgb = rgb;
    if (lineRgb != 0)
        result.lineRgb = lineRgb;
    if (weight != 0)
        result.weight = weight;
    if (transparency != 0.0f)
        result.transparency = transparency;
    if (lineTransparency != 0.0f)
        result.lineTransparency = lineTransparency;
    if (linePixels != 0)
        result.linePixels = linePixels;
    if (ignoresMaterial)
        result.ignoresMaterial = true;
    if (nonLocatable)
        result.nonLocatable = true;
    if (emphasized)
        result.emphasized = true;

    return result;
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — subcategory visibility
// ---------------------------------------------------------------------------

void FeatureOverridesBase::setVisibleSubCategory(uint32_t subcatId)
{
    m_visibleSubCategories.insert(subcatId);
}

void FeatureOverridesBase::clearVisibleSubCategory(uint32_t subcatId)
{
    m_visibleSubCategories.erase(subcatId);
}

bool FeatureOverridesBase::isSubCategoryVisible(uint32_t subcatId) const
{
    return m_visibleSubCategories.count(subcatId) > 0;
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — neverDrawn / alwaysDrawn
// ---------------------------------------------------------------------------

void FeatureOverridesBase::setNeverDrawn(uint32_t elemId)
{
    m_neverDrawn.insert(elemId);
}

void FeatureOverridesBase::clearNeverDrawn(uint32_t elemId)
{
    m_neverDrawn.erase(elemId);
}

void FeatureOverridesBase::setAlwaysDrawn(uint32_t elemId)
{
    m_alwaysDrawn.insert(elemId);
}

void FeatureOverridesBase::clearAlwaysDrawn(uint32_t elemId)
{
    m_alwaysDrawn.erase(elemId);
}

void FeatureOverridesBase::setNeverDrawnSet(std::unordered_set<uint32_t> const& ids)
{
    m_neverDrawn = ids;
}

void FeatureOverridesBase::setAlwaysDrawnSet(std::unordered_set<uint32_t> const& ids)
{
    m_alwaysDrawn = ids;
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — default overrides
// ---------------------------------------------------------------------------

void FeatureOverridesBase::setDefaultOverrides(FeatureAppearance const& appearance)
{
    m_defaultOverrides = appearance;
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — per-feature override setters
// ---------------------------------------------------------------------------

void FeatureOverridesBase::override(uint32_t elemId, FeatureAppearance const& appearance)
{
    m_elementOverrides[elemId] = appearance;
}

void FeatureOverridesBase::overrideSubCategory(uint32_t subcatId, FeatureAppearance const& appearance)
{
    m_subCategoryOverrides[subcatId] = appearance;
}

void FeatureOverridesBase::overrideModel(uint32_t modelId, FeatureAppearance const& appearance)
{
    m_modelOverrides[modelId] = appearance;
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — query
// ---------------------------------------------------------------------------

bool FeatureOverridesBase::isClassVisible(GeometryClass geomClass) const
{
    switch (geomClass) {
    case GeometryClass::Construction:
        return m_constructions;
    case GeometryClass::Dimension:
        return m_dimensions;
    case GeometryClass::Pattern:
        return m_patterns;
    default:
        return true;
    }
}

bool FeatureOverridesBase::isFeatureVisible(uint32_t elemId, uint32_t subcatId,
                                        GeometryClass geomClass) const
{
    // Ported from: itwinjs-core FeatureSymbology.ts getAppearance (lines 694-738)
    // Check neverDrawn first — absolute priority.
    if (isInNeverDrawn(elemId))
        return false;

    // Check alwaysDrawn / exclusive mode.
    bool alwaysDrawn = isInAlwaysDrawn(elemId);
    if (!alwaysDrawn && m_isAlwaysDrawnExclusive)
        return false;

    // Subcategory visibility (unless alwaysDrawn overrides it, or ignoreSubCategory).
    if (subcatId != 0 && !m_ignoreSubCategory) {
        if ((!alwaysDrawn || !m_alwaysDrawnIgnoresSubCategory) && !isSubCategoryVisible(subcatId))
            return false;
    }

    // Geometry class visibility — always-drawn bypasses class checks.
    return alwaysDrawn || isClassVisible(geomClass);
}

FeatureAppearance FeatureOverridesBase::getAppearance(uint32_t elemId, uint32_t subcatId,
                                                  GeometryClass geomClass,
                                                  uint32_t modelId) const
{
    // Ported from: itwinjs-core FeatureSymbology.ts FeatureOverrides.getAppearance (lines 694-738)
    //
    // Precedence resolution (matching itwinjs-core exactly):
    //   1. neverDrawn → return empty
    //   2. alwaysDrawn (with exclusive mode)
    //   3. Base appearance: weight-1 override if !lineWeights, else defaults
    //   4. Model overrides (extend base)
    //   5. Element overrides (extend base if model exists, else standalone)
    //   6. Subcategory visibility + overrides
    //   7. Default overrides (only if no explicit overrides registered)
    //   8. Visibility: alwaysDrawn || isClassVisible(geomClass)
    //   9. isFullyTransparent → return empty

    // (1) neverDrawn → invisible.
    if (isInNeverDrawn(elemId))
        return FeatureAppearance{};

    // (2) alwaysDrawn / exclusive mode.
    bool alwaysDrawn = isInAlwaysDrawn(elemId);
    if (!alwaysDrawn && m_isAlwaysDrawnExclusive)
        return FeatureAppearance{};

    // (3) Start with base appearance: when line weights are disabled,
    //     all features get weight=1.  Otherwise start from defaults.
    static FeatureAppearance const s_weight1 = [] {
        FeatureAppearance a;
        a.weight = 1;
        return a;
    }();
    FeatureAppearance result = m_lineWeights ? FeatureAppearance::defaults() : s_weight1;

    // (4) Model overrides — extend base if present.
    FeatureAppearance const* modelApp = nullptr;
    {
        auto it = m_modelOverrides.find(modelId);
        if (it != m_modelOverrides.end()) {
            modelApp = &it->second;
            result = modelApp->extendAppearance(result);
        }
    }

    // (5) Element overrides — if modelApp exists, extend combined; else standalone.
    FeatureAppearance const* elemApp = nullptr;
    {
        auto it = m_elementOverrides.find(elemId);
        if (it != m_elementOverrides.end()) {
            elemApp = &it->second;
            result = (modelApp != nullptr) ? elemApp->extendAppearance(result) : *elemApp;
        }
    }

    // (6) Subcategory visibility + overrides.
    FeatureAppearance const* subCatApp = nullptr;
    if (!m_ignoreSubCategory && subcatId != 0) {
        if ((!alwaysDrawn || !m_alwaysDrawnIgnoresSubCategory) && !isSubCategoryVisible(subcatId))
            return FeatureAppearance{};

        auto it = m_subCategoryOverrides.find(subcatId);
        if (it != m_subCategoryOverrides.end()) {
            subCatApp = &it->second;
            result = subCatApp->extendAppearance(result);
        }
    }

    // (7) Default overrides — only if *no* explicit override was registered
    //     (doesn't matter if the registered override doesn't actually change anything).
    if (elemApp == nullptr && modelApp == nullptr && subCatApp == nullptr)
        result = m_defaultOverrides.extendAppearance(result);

    // (8) Visibility: alwaysDrawn bypasses geometry class checks.
    bool visible = alwaysDrawn || isClassVisible(geomClass);

    // (9) Fully transparent → don't bother rendering.
    if (visible && result.isFullyTransparent())
        visible = false;

    return visible ? result : FeatureAppearance{};
}

// ---------------------------------------------------------------------------
// FeatureOverridesBase — private helpers
// ---------------------------------------------------------------------------

bool FeatureOverridesBase::isInNeverDrawn(uint32_t elemId) const
{
    return m_neverDrawn.count(elemId) > 0;
}

bool FeatureOverridesBase::isInAlwaysDrawn(uint32_t elemId) const
{
    return m_alwaysDrawn.count(elemId) > 0;
}

// ---------------------------------------------------------------------------
// FeatureSymbologyOverrides
// ---------------------------------------------------------------------------

void FeatureSymbologyOverrides::initFromView(std::vector<uint32_t> const& categoryIds,
                                              std::vector<uint32_t> const& subcategoryIds,
                                              std::unordered_set<uint32_t> const& excludedElementIds,
                                              bool constructions, bool dimensions,
                                              bool patterns, bool lineWeights,
                                              std::unordered_map<uint32_t, FeatureAppearance> const& subcategoryOverrides,
                                              std::unordered_map<uint32_t, FeatureAppearance> const& modelOverrides)
{
    // Ported from: itwinjs-core FeatureSymbology.ts _initFromView (lines 119-174)
    //              + _initSubCategoryOverrides (lines 176-204)

    // (1) Excluded elements from displayStyle.settings.excludedElementIds → neverDrawn.
    //     itwinjs: for (const excluded of view.displayStyle.settings.excludedElementIds)
    //                this.setNeverDrawn(excluded);
    for (auto id : excludedElementIds) {
        m_neverDrawn.insert(id);
    }

    // (2) Apply geometry class visibility from view flags.
    //     itwinjs: this._constructions = constructions; ...
    m_constructions = constructions;
    m_dimensions = dimensions;
    m_patterns = patterns;
    m_lineWeights = lineWeights;

    // (3) Populate visible subcategories from the view definition.
    //     itwinjs iterates categorySelector.categories, resolves subcategories,
    //     checks visibility, and adds to _visibleSubCategories.
    //     Here we receive the already-resolved visible IDs directly.
    for (auto id : subcategoryIds) {
        m_visibleSubCategories.insert(id);
    }
    for (auto id : categoryIds) {
        m_visibleSubCategories.insert(id);
    }

    // (4) Subcategory appearance overrides (from SubCategoryOverride entries).
    //     itwinjs: _initSubCategoryOverrides iterates _visibleSubCategories,
    //     calls view.getSubCategoryOverride(), converts to FeatureAppearance,
    //     and stores in _subCategoryOverrides.
    for (auto const& [subcatId, appearance] : subcategoryOverrides) {
        if (appearance.anyOverridden()) {
            m_subCategoryOverrides[subcatId] = appearance;
        }
    }

    // (5) Model appearance overrides from displayStyle.settings.modelAppearanceOverrides.
    //     itwinjs: style.settings.modelAppearanceOverrides.forEach(
    //                (appearance, modelId) => this.override({ modelId, appearance, onConflict: "skip" }));
    for (auto const& [modelId, appearance] : modelOverrides) {
        overrideModel(modelId, appearance);
    }
}

void FeatureSymbologyOverrides::initFromViewport(std::vector<uint32_t> const& neverDrawn,
                                                  std::vector<uint32_t> const& alwaysDrawn)
{
    m_neverDrawn.clear();
    m_alwaysDrawn.clear();

    for (auto id : neverDrawn) {
        m_neverDrawn.insert(id);
    }
    for (auto id : alwaysDrawn) {
        m_alwaysDrawn.insert(id);
    }
}

END_DQ_RENDER_NAMESPACE
