// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature symbology overrides implementation
// Ported from: itwinjs-core core/common/src/FeatureSymbology.ts (lines 524-952)
#include "dqCommon/FeatureOverrides.h"

BEGIN_DQ_COMMON_NAMESPACE

using namespace dqBase;

// Static weight-1 appearance for when line weights are disabled.
// Ported from: itwinjs-core FeatureOverrides._weight1Appearance = FeatureAppearance.fromJSON({weight: 1})
const FeatureAppearance FeatureOverrides::s_weight1Appearance = []() {
    FeatureAppearanceProps props;
    props.weight = 1.0;
    return FeatureAppearance::fromJSON(&props);
}();

// ---------------------------------------------------------------------------
// Subcategory visibility
// ---------------------------------------------------------------------------

bool FeatureOverrides::isSubCategoryVisible(uint32_t subcatLo, uint32_t subcatHi) const
{
    const uint64_t key = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
    return m_visibleSubCategories.count(key) > 0;
}

bool FeatureOverrides::isSubCategoryVisibleInModel(uint32_t subcatLo, uint32_t subcatHi,
                                                    uint32_t modelLo, uint32_t modelHi) const
{
    if (ignoreSubCategory)
        return true;

    bool vis = isSubCategoryVisible(subcatLo, subcatHi);
    const uint64_t modelKey = (static_cast<uint64_t>(modelHi) << 32) | modelLo;
    auto modelIt = m_modelSubCategoryOverrides.find(modelKey);
    if (modelIt != m_modelSubCategoryOverrides.end()) {
        const uint64_t subcatKey = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
        if (modelIt->second.count(subcatKey) > 0)
            vis = !vis;
    }
    return vis;
}

void FeatureOverrides::addModelSubCategoryOverride(uint64_t modelId, uint64_t subcategoryId)
{
    m_modelSubCategoryOverrides[modelId].insert(subcategoryId);
}

void FeatureOverrides::setVisibleSubCategory(const DqId& id)
{
    m_visibleSubCategories.insert(idToKey(id));
}

// ---------------------------------------------------------------------------
// Never-drawn / Always-drawn
// ---------------------------------------------------------------------------

void FeatureOverrides::setNeverDrawn(const DqId& id)
{
    m_neverDrawn.insert(idToKey(id));
}

void FeatureOverrides::setAlwaysDrawn(const DqId& id)
{
    m_alwaysDrawn.insert(idToKey(id));
}

void FeatureOverrides::setAnimationNodeNeverDrawn(uint32_t id)
{
    neverDrawnAnimationNodes.insert(id);
}

void FeatureOverrides::setNeverDrawnSet(const std::vector<DqId>& ids)
{
    m_neverDrawn.clear();
    for (const auto& id : ids)
        m_neverDrawn.insert(idToKey(id));
}

void FeatureOverrides::setAlwaysDrawnSet(const std::vector<DqId>& ids, bool exclusive, bool ignoreSubCat)
{
    m_alwaysDrawn.clear();
    for (const auto& id : ids)
        m_alwaysDrawn.insert(idToKey(id));
    isAlwaysDrawnExclusive = exclusive;
    alwaysDrawnIgnoresSubCategory = ignoreSubCat;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool FeatureOverrides::isNeverDrawn(uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const
{
    const uint64_t key = (static_cast<uint64_t>(elemHi) << 32) | elemLo;
    if (m_neverDrawn.count(key) > 0)
        return true;
    return neverDrawnAnimationNodes.count(animationNodeId) > 0;
}

bool FeatureOverrides::isAlwaysDrawn(uint32_t elemLo, uint32_t elemHi) const
{
    const uint64_t key = (static_cast<uint64_t>(elemHi) << 32) | elemLo;
    return m_alwaysDrawn.count(key) > 0;
}

const FeatureAppearance* FeatureOverrides::getModelOverrides(uint32_t modelLo, uint32_t modelHi) const
{
    const uint64_t key = (static_cast<uint64_t>(modelHi) << 32) | modelLo;
    auto it = m_modelOverrides.find(key);
    return it != m_modelOverrides.end() ? &it->second : nullptr;
}

const FeatureAppearance* FeatureOverrides::getElementAnimationOverrides(
    uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const
{
    if (animationNodeOverrides.empty())
        return nullptr;

    auto it = animationNodeOverrides.find(animationNodeId);
    if (it == animationNodeOverrides.end())
        return nullptr;

    // NB: An animation node Id of zero means "not animated". Some providers like EmphasizeElements
    // may supply an appearance override for unanimated nodes. That should be preserved.
    // (ref lines 637-641)
    if (animationNodeId == 0 || m_ignoreAnimationCallbacks.empty())
        return &it->second;

    // If any registered callback returns true for this element, suppress the script's overrides.
    IgnoreAnimationOverridesArgs args{};
    args.elementId.lower = elemLo;
    args.elementId.upper = elemHi;
    args.animationNodeId = animationNodeId;
    for (const auto& cb : m_ignoreAnimationCallbacks) {
        if (cb(args))
            return nullptr;
    }
    return &it->second;
}

const FeatureAppearance* FeatureOverrides::getElementOverrides(
    uint32_t elemLo, uint32_t elemHi, uint32_t animationNodeId) const
{
    const uint64_t key = (static_cast<uint64_t>(elemHi) << 32) | elemLo;
    auto elemIt = m_elementOverrides.find(key);
    const FeatureAppearance* elemApp = (elemIt != m_elementOverrides.end()) ? &elemIt->second : nullptr;
    const FeatureAppearance* nodeApp = getElementAnimationOverrides(elemLo, elemHi, animationNodeId);

    if (elemApp) {
        // Combine: animation extends element (thread-local scratch)
        if (nodeApp) {
            thread_local FeatureAppearance s_scratch;
            s_scratch = nodeApp->extendAppearance(*elemApp);
            return &s_scratch;
        }
        return elemApp;
    }
    return nodeApp;
}

const FeatureAppearance* FeatureOverrides::getSubCategoryOverrides(uint32_t subcatLo, uint32_t subcatHi) const
{
    const uint64_t key = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
    auto it = m_subCategoryOverrides.find(key);
    return it != m_subCategoryOverrides.end() ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Classifier appearance
// ---------------------------------------------------------------------------

std::optional<FeatureAppearance> FeatureOverrides::getClassifierAppearance(
    uint32_t elemLo, uint32_t elemHi,
    uint32_t subcatLo, uint32_t subcatHi,
    uint32_t modelLo, uint32_t modelHi,
    uint32_t animationNodeId) const
{
    FeatureAppearance app = FeatureAppearance::defaults();
    const FeatureAppearance* modelApp = getModelOverrides(modelLo, modelHi);
    if (modelApp)
        app = modelApp->extendAppearance(app);

    const FeatureAppearance* elemApp = getElementOverrides(elemLo, elemHi, animationNodeId);
    if (elemApp)
        app = modelApp ? elemApp->extendAppearance(app) : *elemApp;

    if (!ignoreSubCategory) {
        const uint64_t subcatKey = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
        if (subcatKey != 0) {
            const FeatureAppearance* subCat = getSubCategoryOverrides(subcatLo, subcatHi);
            if (subCat)
                app = subCat->extendAppearance(app);
        }
    }

    if (!elemApp && !modelApp)
        app = m_defaultOverrides.extendAppearance(app);

    return app;
}

// ---------------------------------------------------------------------------
// Core appearance resolution (9-parameter cascade)
// Ported from: itwinjs-core FeatureOverrides.getAppearance() (lines 700-770)
// ---------------------------------------------------------------------------

std::optional<FeatureAppearance> FeatureOverrides::getAppearance(
    uint32_t elemLo, uint32_t elemHi,
    uint32_t subcatLo, uint32_t subcatHi,
    GeometryClass geomClass,
    uint32_t modelLo, uint32_t modelHi,
    BatchType type, uint32_t animationNodeId) const
{
    // Classifiers use a separate resolution path
    if (type == BatchType::VolumeClassifier || type == BatchType::PlanarClassifier)
        return getClassifierAppearance(elemLo, elemHi, subcatLo, subcatHi, modelLo, modelHi, animationNodeId);

    // Step 1: Line-weight base
    FeatureAppearance app = m_lineWeights ? FeatureAppearance::defaults() : s_weight1Appearance;

    // Step 2: Model overrides
    const FeatureAppearance* modelApp = getModelOverrides(modelLo, modelHi);
    if (modelApp)
        app = modelApp->extendAppearance(app);

    // Step 3-5: Element visibility and overrides
    const FeatureAppearance* elemApp = nullptr;
    bool alwaysDrawn = false;
    const uint64_t elemKey = (static_cast<uint64_t>(elemHi) << 32) | elemLo;

    if (elemKey != 0) {
        // Never-drawn check
        if (isNeverDrawn(elemLo, elemHi, animationNodeId))
            return std::nullopt;

        // Always-drawn exclusive check
        alwaysDrawn = isAlwaysDrawn(elemLo, elemHi);
        if (!alwaysDrawn && isAlwaysDrawnExclusive)
            return std::nullopt;

        // Element overrides
        elemApp = getElementOverrides(elemLo, elemHi, animationNodeId);
        if (elemApp)
            app = modelApp ? elemApp->extendAppearance(app) : *elemApp;
    }

    // Step 6: Subcategory visibility and overrides
    const FeatureAppearance* subCatApp = nullptr;
    if (!ignoreSubCategory) {
        const uint64_t subcatKey = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
        if (subcatKey != 0) {
            // Visibility check
            if ((!alwaysDrawn || !alwaysDrawnIgnoresSubCategory) &&
                !isSubCategoryVisibleInModel(subcatLo, subcatHi, modelLo, modelHi))
                return std::nullopt;

            subCatApp = getSubCategoryOverrides(subcatLo, subcatHi);
            if (subCatApp)
                app = subCatApp->extendAppearance(app);
        }
    }

    // Step 7: Default overrides (only if no explicit overrides registered)
    if (!elemApp && !modelApp && !subCatApp)
        app = m_defaultOverrides.extendAppearance(app);

    // Step 8: Geometry class visibility + full transparency
    bool visible = alwaysDrawn || isClassVisible(geomClass);
    if (visible && app.isFullyTransparent())
        visible = false;

    return visible ? std::make_optional(app) : std::nullopt;
}

std::optional<FeatureAppearance> FeatureOverrides::getFeatureAppearance(
    const Feature& feature, const DqId& modelId,
    BatchType type, uint32_t animationNodeId) const
{
    return getAppearance(
        Id64::GetLowerUint32(feature.elementId), Id64::GetUpperUint32(feature.elementId),
        Id64::GetLowerUint32(feature.subCategoryId), Id64::GetUpperUint32(feature.subCategoryId),
        feature.geometryClass,
        Id64::GetLowerUint32(modelId), Id64::GetUpperUint32(modelId),
        type, animationNodeId);
}

// ---------------------------------------------------------------------------
// Geometry class visibility
// Ported from: itwinjs-core FeatureOverrides.isClassVisible()
// ---------------------------------------------------------------------------

bool FeatureOverrides::isClassVisible(GeometryClass geomClass) const
{
    switch (geomClass) {
        case GeometryClass::Construction: return m_constructions;
        case GeometryClass::Dimension: return m_dimensions;
        case GeometryClass::Pattern: return m_patterns;
        default: return true;
    }
}

// ---------------------------------------------------------------------------
// override registration
// Ported from: itwinjs-core FeatureOverrides.override()
// ---------------------------------------------------------------------------

FeatureAppearance FeatureOverrides::resolveConflict(const FeatureAppearance& existing,
                                                     const FeatureAppearance& incoming,
                                                     OverrideConflict conflict)
{
    switch (conflict) {
        case OverrideConflict::Replace: return incoming;
        case OverrideConflict::Skip: return existing;
        case OverrideConflict::Extend: return incoming.extendAppearance(existing);
        case OverrideConflict::Subsume:
        default:
            // The TS default: existing.extendAppearance(new) — existing's aspects are overwritten
            // by new where new overrides them. (ref lines 809-810)
            return existing.extendAppearance(incoming);
    }
}

void FeatureOverrides::overrideModel(const DqId& modelId, const FeatureAppearance& appearance,
                                      OverrideConflict conflict)
{
    const uint64_t key = idToKey(modelId);
    auto it = m_modelOverrides.find(key);
    if (it != m_modelOverrides.end()) {
        m_modelOverrides[key] = resolveConflict(it->second, appearance, conflict);
    } else {
        m_modelOverrides[key] = appearance;
    }
}

void FeatureOverrides::overrideElement(const DqId& elementId, const FeatureAppearance& appearance,
                                        OverrideConflict conflict)
{
    const uint64_t key = idToKey(elementId);
    // Skip if element is never-drawn
    if (m_neverDrawn.count(key) > 0)
        return;

    auto it = m_elementOverrides.find(key);
    if (it != m_elementOverrides.end()) {
        m_elementOverrides[key] = resolveConflict(it->second, appearance, conflict);
    } else {
        m_elementOverrides[key] = appearance;
    }
}

void FeatureOverrides::overrideSubCategory(const DqId& subCategoryId, const FeatureAppearance& appearance,
                                            OverrideConflict conflict)
{
    const uint64_t key = idToKey(subCategoryId);
    auto it = m_subCategoryOverrides.find(key);
    if (it != m_subCategoryOverrides.end()) {
        m_subCategoryOverrides[key] = resolveConflict(it->second, appearance, conflict);
    } else {
        m_subCategoryOverrides[key] = appearance;
    }
}

void FeatureOverrides::overrideAnimationNode(uint32_t id, const FeatureAppearance& appearance)
{
    animationNodeOverrides[id] = appearance;
}

void FeatureOverrides::setDefaultOverrides(const FeatureAppearance& appearance, bool replaceExisting)
{
    if (replaceExisting || !appearance.overridesSymbology())
        m_defaultOverrides = appearance;
}

// ---------------------------------------------------------------------------
// Subcategory priorities
// Ported from: itwinjs-core FeatureOverrides.getSubCategoryPriority()
// ---------------------------------------------------------------------------

uint32_t FeatureOverrides::getSubCategoryPriority(uint32_t subcatLo, uint32_t subcatHi) const
{
    const uint64_t key = (static_cast<uint64_t>(subcatHi) << 32) | subcatLo;
    auto it = m_subCategoryPriorities.find(key);
    return it != m_subCategoryPriorities.end() ? it->second : 0;
}

// ---------------------------------------------------------------------------
// Visibility queries
// Ported from: itwinjs-core FeatureOverrides.isFeatureVisible()
// ---------------------------------------------------------------------------

bool FeatureOverrides::isFeatureVisible(const Feature& feature) const
{
    const bool isValidElemId = feature.elementId.isValid();

    if (isValidElemId) {
        const uint32_t lo = Id64::GetLowerUint32(feature.elementId);
        const uint32_t hi = Id64::GetUpperUint32(feature.elementId);
        if (isNeverDrawn(lo, hi, 0))
            return false;

        const bool alwaysDrawn = isAlwaysDrawn(lo, hi);
        if (alwaysDrawn || isAlwaysDrawnExclusive)
            return alwaysDrawn;
    }

    // Subcategory visibility (ignoring per-model overrides since no model specified)
    {
        const uint32_t subLo = Id64::GetLowerUint32(feature.subCategoryId);
        const uint32_t subHi = Id64::GetUpperUint32(feature.subCategoryId);
        if (!isSubCategoryVisible(subLo, subHi))
            return false;
    }

    return isClassVisible(feature.geometryClass);
}

void FeatureOverrides::addInvisibleElementOverridesToNeverDrawn()
{
    for (const auto& [key, _] : m_elementOverrides) {
        const uint32_t lo = static_cast<uint32_t>(key & 0xFFFFFFFF);
        const uint32_t hi = static_cast<uint32_t>(key >> 32);
        const FeatureAppearance* app = getElementOverrides(lo, hi, 0);
        if (app && app->isFullyTransparent())
            m_neverDrawn.insert(key);
    }
}

// ---------------------------------------------------------------------------
// Backward-compatible simple API
// ---------------------------------------------------------------------------

void FeatureOverrides::setFeatureAppearance(uint64_t featureId, const FeatureAppearance& appearance)
{
    if (appearance.matchesDefaults()) {
        m_elementOverrides.erase(featureId);
    } else {
        m_elementOverrides[featureId] = appearance;
    }
}

const FeatureAppearance* FeatureOverrides::getFeatureAppearance(uint64_t featureId) const
{
    auto it = m_elementOverrides.find(featureId);
    return it != m_elementOverrides.end() ? &it->second : nullptr;
}

bool FeatureOverrides::hasOverride(uint64_t featureId) const
{
    return m_elementOverrides.count(featureId) > 0;
}

void FeatureOverrides::removeFeatureAppearance(uint64_t featureId)
{
    m_elementOverrides.erase(featureId);
}

void FeatureOverrides::clear()
{
    m_neverDrawn.clear();
    m_alwaysDrawn.clear();
    m_defaultOverrides = FeatureAppearance::defaults();
    m_modelOverrides.clear();
    m_elementOverrides.clear();
    m_subCategoryOverrides.clear();
    m_visibleSubCategories.clear();
    m_subCategoryPriorities.clear();
    m_modelSubCategoryOverrides.clear();
    animationNodeOverrides.clear();
    neverDrawnAnimationNodes.clear();
    isAlwaysDrawnExclusive = false;
    alwaysDrawnIgnoresSubCategory = true;
    ignoreSubCategory = false;
    m_constructions = false;
    m_dimensions = false;
    m_patterns = false;
    m_lineWeights = true;
    m_ignoreAnimationCallbacks.clear();
}

void FeatureOverrides::mergeFrom(const FeatureOverrides& other)
{
    for (const auto& [k, v] : other.m_modelOverrides)
        m_modelOverrides[k] = v;
    for (const auto& [k, v] : other.m_elementOverrides)
        m_elementOverrides[k] = v;
    for (const auto& [k, v] : other.m_subCategoryOverrides)
        m_subCategoryOverrides[k] = v;
    for (const auto& k : other.m_neverDrawn)
        m_neverDrawn.insert(k);
    for (const auto& k : other.m_alwaysDrawn)
        m_alwaysDrawn.insert(k);
    for (const auto& k : other.m_visibleSubCategories)
        m_visibleSubCategories.insert(k);
}

// ---------------------------------------------------------------------------
// FeatureAppearanceProvider implementations
// ---------------------------------------------------------------------------

namespace {

class SupplementProvider : public FeatureAppearanceProvider {
public:
    using SupplementFn = std::function<FeatureAppearance(const FeatureAppearance&)>;
    explicit SupplementProvider(SupplementFn fn) : m_fn(std::move(fn)) {}

    std::optional<FeatureAppearance> getFeatureAppearance(
        const FeatureAppearanceSource& source,
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        GeometryClass geomClass,
        uint32_t modelLo, uint32_t modelHi,
        BatchType type, uint32_t animationNodeId) const override
    {
        auto app = source.getAppearance(elemLo, elemHi, subcatLo, subcatHi, geomClass,
                                        modelLo, modelHi, type, animationNodeId);
        return app ? std::make_optional(m_fn(*app)) : app;
    }

private:
    SupplementFn m_fn;
};

class ChainedProvider : public FeatureAppearanceProvider {
public:
    ChainedProvider(std::unique_ptr<FeatureAppearanceProvider> first,
                    std::unique_ptr<FeatureAppearanceProvider> second)
        : m_first(std::move(first)), m_second(std::move(second)) {}

    std::optional<FeatureAppearance> getFeatureAppearance(
        const FeatureAppearanceSource& source,
        uint32_t elemLo, uint32_t elemHi,
        uint32_t subcatLo, uint32_t subcatHi,
        GeometryClass geomClass,
        uint32_t modelLo, uint32_t modelHi,
        BatchType type, uint32_t animationNodeId) const override
    {
        // Wrap source with first provider, then apply second
        class WrappedSource : public FeatureAppearanceSource {
        public:
            WrappedSource(const FeatureAppearanceSource& inner, const FeatureAppearanceProvider& provider)
                : m_inner(inner), m_provider(provider) {}

            std::optional<FeatureAppearance> getAppearance(
                uint32_t elemLo, uint32_t elemHi,
                uint32_t subcatLo, uint32_t subcatHi,
                GeometryClass geomClass,
                uint32_t modelLo, uint32_t modelHi,
                BatchType type, uint32_t animationNodeId) const override
            {
                return m_provider.getFeatureAppearance(m_inner, elemLo, elemHi, subcatLo, subcatHi,
                                                       geomClass, modelLo, modelHi, type, animationNodeId);
            }
        private:
            const FeatureAppearanceSource& m_inner;
            const FeatureAppearanceProvider& m_provider;
        };

        WrappedSource wrapped(source, *m_first);
        return m_second->getFeatureAppearance(wrapped, elemLo, elemHi, subcatLo, subcatHi,
                                              geomClass, modelLo, modelHi, type, animationNodeId);
    }

private:
    std::unique_ptr<FeatureAppearanceProvider> m_first;
    std::unique_ptr<FeatureAppearanceProvider> m_second;
};

}  // namespace

std::unique_ptr<FeatureAppearanceProvider> FeatureAppearanceProvider::supplement(
    std::function<FeatureAppearance(const FeatureAppearance&)> supplementFn)
{
    return std::make_unique<SupplementProvider>(std::move(supplementFn));
}

std::unique_ptr<FeatureAppearanceProvider> FeatureAppearanceProvider::chain(
    std::unique_ptr<FeatureAppearanceProvider> first,
    std::unique_ptr<FeatureAppearanceProvider> second)
{
    return std::make_unique<ChainedProvider>(std::move(first), std::move(second));
}

END_DQ_COMMON_NAMESPACE
