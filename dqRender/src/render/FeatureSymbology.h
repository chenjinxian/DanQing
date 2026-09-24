// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BIM feature symbology override system
// Ported from: itwinjs-core core/frontend/src/render/FeatureSymbology.ts
//
// Per-element, per-subcategory, and per-model visual overrides for BIM display.
// Controls visibility, color, transparency, line weight, and line style.
// Public types (FeatureAppearance, GeometryClass) live in PublicAPI/dqRender/FeatureSymbology.h.
#pragma once

#include "dqRender/FeatureSymbology.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FeatureOverridesBase — manages per-feature visual overrides
// (Ported from: itwinjs-core FeatureSymbology.ts FeatureOverrides)
//
// Central registry for BIM symbology overrides. Looks up appearances by
// element ID, subcategory ID, and model ID with a well-defined precedence:
//   model > element (+ animation) > subcategory > default
//
// Also manages visibility sets (neverDrawn / alwaysDrawn) and geometry-class
// visibility flags (constructions, dimensions, patterns).
// ---------------------------------------------------------------------------
class FeatureOverridesBase {
public:
    FeatureOverridesBase() = default;
    virtual ~FeatureOverridesBase() = default;

    // -- Subcategory visibility ------------------------------------------------

    /// add a subcategory to the visible set (overrides default hidden state).
    void setVisibleSubCategory(uint32_t subcatId);

    /// Remove a subcategory from the visible set.
    void clearVisibleSubCategory(uint32_t subcatId);

    /// Check if a subcategory is in the visible set.
    bool isSubCategoryVisible(uint32_t subcatId) const;

    // -- neverDrawn / alwaysDrawn sets -----------------------------------------

    /// Mark an element as never drawn.
    void setNeverDrawn(uint32_t elemId);

    /// Unmark an element as never drawn.
    void clearNeverDrawn(uint32_t elemId);

    /// Mark an element as always drawn.
    void setAlwaysDrawn(uint32_t elemId);

    /// Unmark an element as always drawn.
    void clearAlwaysDrawn(uint32_t elemId);

    /// Replace the entire never-drawn set.
    void setNeverDrawnSet(std::unordered_set<uint32_t> const& ids);

    /// Replace the entire always-drawn set.
    void setAlwaysDrawnSet(std::unordered_set<uint32_t> const& ids);

    /// True if alwaysDrawn set should be exclusive (only those elements drawn).
    bool isAlwaysDrawnExclusive() const noexcept { return m_isAlwaysDrawnExclusive; }

    /// Set the exclusive mode for alwaysDrawn.
    void setAlwaysDrawnExclusive(bool exclusive) noexcept { m_isAlwaysDrawnExclusive = exclusive; }

    // -- Default overrides -----------------------------------------------------

    /// Set the default appearance applied when no specific override exists.
    void setDefaultOverrides(FeatureAppearance const& appearance);

    /// Get the current default appearance.
    FeatureAppearance const& getDefaultOverrides() const noexcept { return m_defaultOverrides; }

    // -- Per-feature overrides -------------------------------------------------

    /// override appearance for a specific element.
    void override(uint32_t elemId, FeatureAppearance const& appearance);

    /// override appearance for a specific subcategory.
    void overrideSubCategory(uint32_t subcatId, FeatureAppearance const& appearance);

    /// override appearance for an entire model.
    void overrideModel(uint32_t modelId, FeatureAppearance const& appearance);

    // -- Query -----------------------------------------------------------------

    /// Get the resolved appearance for a feature, or empty FeatureAppearance
    /// (anyOverridden() == false) if the feature is not visible.
    /// Ported from: itwinjs-core FeatureSymbology.ts FeatureOverrides.getAppearance
    /// Precedence: model > element > subcategory > default.
    FeatureAppearance getAppearance(uint32_t elemId, uint32_t subcatId,
                                    GeometryClass geomClass, uint32_t modelId) const;

    /// Check if geometry of the given class is visible (based on view flags).
    bool isClassVisible(GeometryClass geomClass) const;

    /// Check if a feature is visible given its subcategory and geometry class.
    bool isFeatureVisible(uint32_t elemId, uint32_t subcatId,
                          GeometryClass geomClass) const;

    // -- Configuration flags (matching itwinjs-core FeatureOverrides) --------

    /// If true, always-drawn elements are drawn even if their subcategories
    /// are not visible.  Default: true.
    bool isAlwaysDrawnIgnoresSubCategory() const noexcept { return m_alwaysDrawnIgnoresSubCategory; }
    void setAlwaysDrawnIgnoresSubCategory(bool v) noexcept { m_alwaysDrawnIgnoresSubCategory = v; }

    /// If true, all subcategories are considered visible (bypasses per-subcat
    /// checks).  Default: false.
    bool isIgnoreSubCategory() const noexcept { return m_ignoreSubCategory; }
    void setIgnoreSubCategory(bool v) noexcept { m_ignoreSubCategory = v; }

    // -- Accessors for subclass use -------------------------------------------

    bool getConstructions() const noexcept { return m_constructions; }
    bool getDimensions() const noexcept { return m_dimensions; }
    bool getPatterns() const noexcept { return m_patterns; }
    bool getLineWeights() const noexcept { return m_lineWeights; }

protected:
    // -- Geometry class visibility flags ---------------------------------------
    // Ported from: itwinjs-core FeatureSymbology.ts _constructions/_dimensions/_patterns
    // These default to false per itwinjs-core — construction/dimension/pattern
    // geometry is hidden unless the view flags explicitly enable it.
    bool m_constructions = false;
    bool m_dimensions = false;
    bool m_patterns = false;
    bool m_lineWeights = true;
    bool m_isAlwaysDrawnExclusive = false;
    // Ported from: itwinjs-core FeatureSymbology.ts alwaysDrawnIgnoresSubCategory
    bool m_alwaysDrawnIgnoresSubCategory = true;
    // Ported from: itwinjs-core FeatureSymbology.ts ignoreSubCategory
    bool m_ignoreSubCategory = false;

    // -- Per-key override maps -------------------------------------------------
    std::unordered_map<uint32_t, FeatureAppearance> m_elementOverrides;
    std::unordered_map<uint32_t, FeatureAppearance> m_subCategoryOverrides;
    std::unordered_map<uint32_t, FeatureAppearance> m_modelOverrides;

    // -- Visibility sets -------------------------------------------------------
    std::unordered_set<uint32_t> m_visibleSubCategories;
    std::unordered_set<uint32_t> m_neverDrawn;
    std::unordered_set<uint32_t> m_alwaysDrawn;

    // -- Default appearance ----------------------------------------------------
    FeatureAppearance m_defaultOverrides;

private:
    // -- Internal helpers ------------------------------------------------------
    bool isInNeverDrawn(uint32_t elemId) const;
    bool isInAlwaysDrawn(uint32_t elemId) const;
};

// ---------------------------------------------------------------------------
// FeatureSymbologyOverrides — view-aware feature overrides
// (Ported from: itwinjs-core FeatureSymbology.ts FeatureSymbologyOverrides)
//
// Extended FeatureOverrides that can be initialized from view definition
// categories, subcategories, and view flags. This is the entry point for
// setting up symbology overrides from a BIM view definition.
// ---------------------------------------------------------------------------
class FeatureSymbologyOverrides : public FeatureOverridesBase {
public:
    FeatureSymbologyOverrides() = default;

    /// Initialize from view category/subcategory definitions and style overrides.
    // Ported from: itwinjs-core FeatureSymbology.ts _initFromView + _initSubCategoryOverrides
    ///
    /// @param categoryIds Visible category IDs from the view.
    /// @param subcategoryIds Visible subcategory IDs from the view.
    /// @param excludedElementIds Element IDs excluded by the display style (added to neverDrawn).
    /// @param constructions Show construction geometry.
    /// @param dimensions Show dimension geometry.
    /// @param patterns Show pattern geometry.
    /// @param lineWeights Show line weights.
    /// @param subcategoryOverrides Per-subcategory appearance overrides from SubCategoryOverride entries.
    /// @param modelOverrides Per-model appearance overrides from displayStyle.settings.modelAppearanceOverrides.
    void initFromView(std::vector<uint32_t> const& categoryIds,
                      std::vector<uint32_t> const& subcategoryIds,
                      std::unordered_set<uint32_t> const& excludedElementIds,
                      bool constructions, bool dimensions,
                      bool patterns, bool lineWeights,
                      std::unordered_map<uint32_t, FeatureAppearance> const& subcategoryOverrides = {},
                      std::unordered_map<uint32_t, FeatureAppearance> const& modelOverrides = {});

    /// Initialize from never-drawn and always-drawn element sets.
    /// @param neverDrawn Elements that should never be drawn.
    /// @param alwaysDrawn Elements that should always be drawn.
    void initFromViewport(std::vector<uint32_t> const& neverDrawn,
                          std::vector<uint32_t> const& alwaysDrawn);
};

END_DQ_RENDER_NAMESPACE
