// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Visible tile features implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/VisibleTileFeatures.ts
#include "VisibleTileFeatures.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Construction
// (Ported from: itwinjs-core VisibleTileFeatures.ts constructor)
// ---------------------------------------------------------------------------
VisibleTileFeatures::VisibleTileFeatures(std::vector<FeatureBatchData> const& batches,
                                         QueryTileFeaturesOptions const& options)
    : m_batches(batches), m_options(options)
{
}

// ---------------------------------------------------------------------------
// isFeatureVisible — check visibility against batch overrides
// (Ported from: itwinjs-core VisibleTileFeatures.ts isFeatureVisible)
// ---------------------------------------------------------------------------
bool VisibleTileFeatures::isFeatureVisible(FeatureEntry const& feature,
                                           FeatureBatchData const& batch) const
{
    if (!batch.overrides) {
        return true;
    }

    FeatureGpuAppearance const app = batch.overrides->getAppearance(feature.elementId);
    if (app.alpha == 0) {
        return false;
    }

    if (!m_options.includeNonLocatable && app.nonLocatable) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// forEach — iterate visible features with callback
// (Ported from: itwinjs-core VisibleTileFeatures.ts iterator/commandIterator)
// ---------------------------------------------------------------------------
void VisibleTileFeatures::forEach(Callback callback) const
{
    for (auto const& batch : m_batches) {
        // Skip entirely hidden batches.
        if (batch.allHidden) {
            continue;
        }

        bool const hasOverrides = batch.overrides != nullptr && batch.overrides->anyOverridden();

        for (auto const& feature : batch.features) {
            // If no overrides exist for this batch, all features are visible.
            // If overrides exist, check each feature individually.
            if (!hasOverrides || isFeatureVisible(feature, batch)) {
                // 公开 API 的 VisibleFeature（DqId/GeometryClass 字段，参考
                // VisibleFeature.ts）——FeatureEntry 的 uint32/uint8 简化表示在此转换。
                VisibleFeature vf;
                vf.elementId = dqBase::DqId{feature.elementId};
                vf.subCategoryId = dqBase::DqId{feature.subCategoryId};
                vf.geometryClass = static_cast<dqCommon::GeometryClass>(feature.geometryClass);
                callback(vf);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// collect — gather all visible features into a vector
// ---------------------------------------------------------------------------
std::vector<VisibleFeature> VisibleTileFeatures::collect() const
{
    std::vector<VisibleFeature> result;
    forEach([&result](VisibleFeature const& feature) {
        result.push_back(feature);
    });
    return result;
}

END_DQ_RENDER_NAMESPACE
