// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature symbology overrides
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts (lines 524-952)
//
// Manages per-feature appearance overrides for rendering.
// Used in Viewport.renderFrame() Step 9 to compute feature symbology.
#pragma once

#include "Export.h"
#include "FeatureSymbology.h"
#include "GeometryClass.h"
#include "FeatureTable.h"
#include "DqCommon.h"

#include <dqBase/DqId.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declarations
class FeatureOverrides;

// ---------------------------------------------------------------------------
// FeatureAppearanceSource — abstract interface for appearance lookup
// Ported from: itwinjs-core FeatureAppearanceSource (line 396)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT FeatureAppearanceSource {
public:
    virtual ~FeatureAppearanceSource() = default;

    // Look up appearance for a feature identified by uint32 pairs.
    // Returns nullopt if the feature is not visible.
    // Ported from: itwinjs-core FeatureAppearanceSource.getAppearance()
    virtual std::optional<FeatureAppearance> getAppearance(
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        GeometryClass geomClass,
        uint32_t modelLo, uint32_t modelHi,
        BatchType type, uint32_t animationNodeId) const = 0;
};

// ---------------------------------------------------------------------------
// OverrideConflict — how to handle existing overrides
// Ported from: itwinjs-core OverrideFeatureAppearanceOptions.onConflict (line 436)
// ---------------------------------------------------------------------------
enum class OverrideConflict : uint8_t {
    // The TS DEFAULT. Merge such that any aspect overridden by the existing appearance will be
    // overwritten if also overridden by the new appearance. Computes existing.extendAppearance(new).
    Subsume,
    // Apply the new appearance's aspects only where the existing does not already override.
    // Computes new.extendAppearance(existing).
    Extend,
    // Discard the existing appearance, use the new one.
    Replace,
    // Keep the existing appearance, ignore the new one.
    Skip,
};

// Arguments supplied to an ignoreAnimationOverrides callback.
// Ported from: itwinjs-core IgnoreAnimationOverridesArgs (lines 478-485)
struct IgnoreAnimationOverridesArgs {
    // The element Id under consideration, as a uint32 pair.
    struct {
        uint32_t lower = 0;
        uint32_t upper = 0;
    } elementId;
    // The animation node Id (RenderSchedule.ElementTimeline.batchId).
    uint32_t animationNodeId = 0;
};

// A function registered via ignoreAnimationOverrides(); returns true to suppress schedule-script
// color/transparency overrides for the described element.
// Ported from: itwinjs-core ignoreAnimationOverrides (line 492)
using IgnoreAnimationOverridesFn = std::function<bool(const IgnoreAnimationOverridesArgs&)>;

// ---------------------------------------------------------------------------
// FeatureOverrides — per-feature appearance override collection
// Ported from: itwinjs-core FeatureOverrides (lines 524-885)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT FeatureOverrides : public FeatureAppearanceSource {
public:
    FeatureOverrides() = default;

    // ── Getters ───────────────────────────────────────────────────
    const FeatureAppearance& getDefaultOverrides() const noexcept { return m_defaultOverrides; }
    bool getLineWeights() const noexcept { return m_lineWeights; }

    // Public access to the drawn sets (ref: neverDrawn/alwaysDrawn getters, lines 597-602).
    // Ported from: itwinjs-core FeatureOverrides.neverDrawn / alwaysDrawn
    const std::unordered_set<uint64_t>& neverDrawn() const noexcept { return m_neverDrawn; }
    const std::unordered_set<uint64_t>& alwaysDrawn() const noexcept { return m_alwaysDrawn; }

    // Register an ignoreAnimationOverrides callback. Any number may be registered; if any one
    // returns true for a given element, the schedule-script overrides are not applied.
    // Ported from: itwinjs-core FeatureOverrides.ignoreAnimationOverrides() (lines 585-587)
    void ignoreAnimationOverrides(IgnoreAnimationOverridesFn cb)
    {
        m_ignoreAnimationCallbacks.push_back(std::move(cb));
    }

    // ── Public flags ──────────────────────────────────────────────
    bool isAlwaysDrawnExclusive = false;
    bool alwaysDrawnIgnoresSubCategory = true;
    bool ignoreSubCategory = false;

    // ── Animation node overrides ──────────────────────────────────
    std::unordered_set<uint32_t> neverDrawnAnimationNodes;
    std::unordered_map<uint32_t, FeatureAppearance> animationNodeOverrides;

    // ── Geometry class visibility ─────────────────────────────────
    bool getConstructions() const noexcept { return m_constructions; }
    bool getDimensions() const noexcept { return m_dimensions; }
    bool getPatterns() const noexcept { return m_patterns; }
    void setConstructions(bool v) noexcept { m_constructions = v; }
    void setDimensions(bool v) noexcept { m_dimensions = v; }
    void setPatterns(bool v) noexcept { m_patterns = v; }
    void setLineWeights(bool v) noexcept { m_lineWeights = v; }

    // ── Subcategory visibility ────────────────────────────────────

    // True if subcategory is in the visible set.
    // Ported from: itwinjs-core FeatureOverrides.isSubCategoryVisible()
    bool isSubCategoryVisible(uint32_t subcatLo, uint32_t subcatHi) const;

    // True if subcategory is visible in the context of a specific model.
    // Ported from: itwinjs-core FeatureOverrides.isSubCategoryVisibleInModel()
    bool isSubCategoryVisibleInModel(uint32_t subcatLo, uint32_t subcatHi,
                                     uint32_t modelLo, uint32_t modelHi) const;

    // Add a per-model subcategory visibility inversion.
    // Ported from: itwinjs-core FeatureOverrides._modelSubCategoryOverrides population
    void addModelSubCategoryOverride(uint64_t modelId, uint64_t subcategoryId);

    // Clear all per-model subcategory overrides.
    void clearModelSubCategoryOverrides() { m_modelSubCategoryOverrides.clear(); }

    // add a subcategory to the visible set.
    // Ported from: itwinjs-core FeatureOverrides.setVisibleSubCategory()
    void setVisibleSubCategory(const dqBase::DqId& id);

    // ── Never-drawn / Always-drawn ────────────────────────────────

    // Ported from: itwinjs-core FeatureOverrides.setNeverDrawn()
    void setNeverDrawn(const dqBase::DqId& id);

    // Ported from: itwinjs-core FeatureOverrides.setAlwaysDrawn()
    void setAlwaysDrawn(const dqBase::DqId& id);

    // Ported from: itwinjs-core FeatureOverrides.setAnimationNodeNeverDrawn()
    void setAnimationNodeNeverDrawn(uint32_t id);

    // Ported from: itwinjs-core FeatureOverrides.setNeverDrawnSet()
    void setNeverDrawnSet(const std::vector<dqBase::DqId>& ids);

    // Ported from: itwinjs-core FeatureOverrides.setAlwaysDrawnSet()
    void setAlwaysDrawnSet(const std::vector<dqBase::DqId>& ids, bool exclusive, bool ignoreSubCategory = true);

    // ── Core appearance resolution ────────────────────────────────

    // Returns a feature's appearance, or nullopt if not visible.
    // Ported from: itwinjs-core FeatureOverrides.getAppearance()
    std::optional<FeatureAppearance> getAppearance(
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        GeometryClass geomClass,
        uint32_t modelLo, uint32_t modelHi,
        BatchType type, uint32_t animationNodeId) const override;

    // Convenience: look up by Feature + modelId.
    // Ported from: itwinjs-core FeatureOverrides.getFeatureAppearance()
    std::optional<FeatureAppearance> getFeatureAppearance(
        const Feature& feature, const dqBase::DqId& modelId,
        BatchType type = BatchType::Primary, uint32_t animationNodeId = 0) const;

    // ── Geometry class visibility ─────────────────────────────────

    // Ported from: itwinjs-core FeatureOverrides.isClassVisible()
    bool isClassVisible(GeometryClass geomClass) const;

    // ── override registration ─────────────────────────────────────

    // Register an override for a model, element, or subcategory.
    // Ported from: itwinjs-core FeatureOverrides.override()
    // @note The default conflict mode is Subsume (the TS default, ref line 436).
    void overrideModel(const dqBase::DqId& modelId, const FeatureAppearance& appearance,
                       OverrideConflict conflict = OverrideConflict::Subsume);
    void overrideElement(const dqBase::DqId& elementId, const FeatureAppearance& appearance,
                         OverrideConflict conflict = OverrideConflict::Subsume);
    void overrideSubCategory(const dqBase::DqId& subCategoryId, const FeatureAppearance& appearance,
                             OverrideConflict conflict = OverrideConflict::Subsume);

    // Ported from: itwinjs-core FeatureOverrides.overrideAnimationNode()
    void overrideAnimationNode(uint32_t id, const FeatureAppearance& appearance);

    // Ported from: itwinjs-core FeatureOverrides.setDefaultOverrides()
    void setDefaultOverrides(const FeatureAppearance& appearance, bool replaceExisting = true);

    // ── Subcategory priorities ────────────────────────────────────

    // Ported from: itwinjs-core FeatureOverrides.getSubCategoryPriority()
    uint32_t getSubCategoryPriority(uint32_t subcatLo, uint32_t subcatHi) const;

    // ── Visibility queries ────────────────────────────────────────

    // Ported from: itwinjs-core FeatureOverrides.isFeatureVisible()
    bool isFeatureVisible(const Feature& feature) const;

    // Id64String-based accessors (ref lines 857-864).
    // Ported from: itwinjs-core FeatureOverrides.isSubCategoryIdVisible(Id)
    bool isSubCategoryIdVisible(const dqBase::DqId& id) const
    {
        return isSubCategoryVisible(dqBase::Id64::GetLowerUint32(id), dqBase::Id64::GetUpperUint32(id));
    }
    // Ported from: itwinjs-core FeatureOverrides.isModelIdVisible — not in ref; aligned helper.
    bool isModelIdVisible(const dqBase::DqId& id) const { (void)id; return true; }
    // Ported from: itwinjs-core FeatureOverrides.getModelOverridesById(Id)
    const FeatureAppearance* getModelOverridesById(const dqBase::DqId& id) const
    {
        return getModelOverrides(dqBase::Id64::GetLowerUint32(id), dqBase::Id64::GetUpperUint32(id));
    }
    // Ported from: itwinjs-core FeatureOverrides.getElementOverridesById(Id)
    const FeatureAppearance* getElementOverridesById(const dqBase::DqId& id) const
    {
        return getElementOverrides(dqBase::Id64::GetLowerUint32(id), dqBase::Id64::GetUpperUint32(id), 0);
    }
    // Ported from: itwinjs-core FeatureOverrides.getSubCategoryOverridesById(Id)
    const FeatureAppearance* getSubCategoryOverridesById(const dqBase::DqId& id) const
    {
        return getSubCategoryOverrides(dqBase::Id64::GetLowerUint32(id), dqBase::Id64::GetUpperUint32(id));
    }

    // Ported from: itwinjs-core FeatureOverrides.addInvisibleElementOverridesToNeverDrawn()
    void addInvisibleElementOverridesToNeverDrawn();

    // ── Backward-compatible simple API ────────────────────────────
    // (kept for existing consumers — Viewport::m_featureOverrides)

    void setFeatureAppearance(uint64_t featureId, const FeatureAppearance& appearance);
    const FeatureAppearance* getFeatureAppearance(uint64_t featureId) const;
    bool hasOverride(uint64_t featureId) const;
    void removeFeatureAppearance(uint64_t featureId);
    void clear();
    size_t getCount() const { return m_elementOverrides.size(); }
    bool isEmpty() const { return m_elementOverrides.empty(); }
    void mergeFrom(const FeatureOverrides& other);

private:
    // ── Private helpers ───────────────────────────────────────────
    bool isNeverDrawn(uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const;
    bool isAlwaysDrawn(uint32_t elemLo, uint32_t elemHi) const;

    const FeatureAppearance* getModelOverrides(uint32_t modelLo, uint32_t modelHi) const;
    const FeatureAppearance* getElementOverrides(uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const;
    const FeatureAppearance* getElementAnimationOverrides(uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const;
    const FeatureAppearance* getSubCategoryOverrides(uint32_t subcatLo, uint32_t subcatHi) const;

    std::optional<FeatureAppearance> getClassifierAppearance(
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        uint32_t modelLo, uint32_t modelHi,
        uint32_t animationNodeId) const;

    // Helper: DqId → uint64_t for map keys
    static uint64_t idToKey(const dqBase::DqId& id) { return id.GetValue(); }

    // Helper: apply conflict resolution
    static FeatureAppearance resolveConflict(const FeatureAppearance& existing,
                                             const FeatureAppearance& incoming,
                                             OverrideConflict conflict);

    // ── Private fields ────────────────────────────────────────────
    std::unordered_set<uint64_t> m_neverDrawn;
    std::unordered_set<uint64_t> m_alwaysDrawn;
    FeatureAppearance m_defaultOverrides;

    std::unordered_map<uint64_t, FeatureAppearance> m_modelOverrides;
    std::unordered_map<uint64_t, FeatureAppearance> m_elementOverrides;
    std::unordered_map<uint64_t, FeatureAppearance> m_subCategoryOverrides;
    std::unordered_set<uint64_t> m_visibleSubCategories;
    std::unordered_map<uint64_t, uint32_t> m_subCategoryPriorities;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> m_modelSubCategoryOverrides;

    bool m_constructions = false;
    bool m_dimensions = false;
    bool m_patterns = false;
    bool m_lineWeights = true;

    // Registered ignoreAnimationOverrides callbacks (ref line 526).
    std::vector<IgnoreAnimationOverridesFn> m_ignoreAnimationCallbacks;

    static const FeatureAppearance s_weight1Appearance;
};

// ---------------------------------------------------------------------------
// FeatureAppearanceProvider — interface for chained appearance providers
// Ported from: itwinjs-core FeatureAppearanceProvider (line 887)
// ---------------------------------------------------------------------------
class DQ_COMMON_EXPORT FeatureAppearanceProvider {
public:
    virtual ~FeatureAppearanceProvider() = default;

    virtual std::optional<FeatureAppearance> getFeatureAppearance(
        const FeatureAppearanceSource& source,
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        GeometryClass geomClass,
        uint32_t modelLo, uint32_t modelHi,
        BatchType type, uint32_t animationNodeId) const = 0;

    // Create a provider that supplements visible features' appearances.
    // Ported from: itwinjs-core FeatureAppearanceProvider.supplement()
    static std::unique_ptr<FeatureAppearanceProvider> supplement(
        std::function<FeatureAppearance(const FeatureAppearance&)> supplementFn);

    // chain two providers: first applied before second.
    // Ported from: itwinjs-core FeatureAppearanceProvider.chain()
    static std::unique_ptr<FeatureAppearanceProvider> chain(
        std::unique_ptr<FeatureAppearanceProvider> first,
        std::unique_ptr<FeatureAppearanceProvider> second);
};

// ---------------------------------------------------------------------------
// FeatureOverrideProvider — backward-compatible interface
// (dqApp ViewManager uses this; keep until migrated to FeatureAppearanceProvider)
// Ported from: itwinjs-core FeatureOverrideProvider
// ---------------------------------------------------------------------------
class FeatureOverrideProvider {
public:
    virtual ~FeatureOverrideProvider() = default;
    virtual void addFeatureOverrides(FeatureOverrides& overrides, void* context) = 0;
};

END_DQ_COMMON_NAMESPACE
