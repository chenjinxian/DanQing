// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — VisibleFeature and visible-feature query options (type surface).
//
// Ported from: itwinjs-core core/frontend/src/render/VisibleFeature.ts
//
// Type-only port: VisibleFeature struct + the two query-option structs +
// the source enum. The query callback type is faithfully aliased but the
// IModelConnection dependency is opaque (not yet ported) — forward-declared.
#pragma once

#include "Export.h"

#include <dqBase/DqId.h>
#include <dqCommon/GeometryClass.h>

#include <cstdint>
#include <functional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Forward declaration — IModelConnection lives in core/frontend; not yet
// ported into DanQing. Faithful opaque handle (§6: no half-impl).
// TODO: IModelConnection not yet ported; replace handle type when it lands.
class IModelConnection;

// Forward declaration — ViewRect lives in core/frontend/common; not yet
// ported. Faithful placeholder.
// TODO: ViewRect not yet ported.
struct ViewRect;

// Represents a Feature determined to be visible within a Viewport.
// Ported from: itwinjs-core VisibleFeature (public interface)
struct DQ_RENDER_EXPORT VisibleFeature {
    // The Id of the Element associated with the feature. May be invalid or transient.
    dqBase::DqId elementId{};

    // The Id of the SubCategory associated with the feature. May be invalid or transient.
    dqBase::DqId subCategoryId{};

    // The class of geometry associated with the feature.
    dqCommon::GeometryClass geometryClass = dqCommon::GeometryClass::Primary;

    // The Id of the GeometricModel associated with the feature. May be invalid or transient.
    dqBase::DqId modelId{};

    // The iModel associated with the feature. In some cases this may differ from
    // the Viewport's iModel. Opaque handle — not yet ported (TODO above).
    IModelConnection* iModel = nullptr;
};

// Source discriminator for QueryVisibleFeaturesOptions.
// Ported from: itwinjs-core QueryVisibleFeaturesOptions union discriminator.
enum class VisibleFeatureSource : uint8_t {
    Screen = 0,  // query by reading pixels rendered by a Viewport
    Tiles = 1,   // query by inspecting Tiles selected for display
};

// Options for visible-feature queries.
// Base for QueryScreenFeaturesOptions / QueryTileFeaturesOptions.
// Ported from: itwinjs-core QueryScreenFeaturesOptions / QueryTileFeaturesOptions
struct DQ_RENDER_EXPORT QueryVisibleFeaturesOptions {
    VisibleFeatureSource source = VisibleFeatureSource::Screen;

    // If true, non-locatable features are considered visible.
    bool includeNonLocatable = false;
};

// Screen-source options: source == Screen, optional sub-region rect.
// Ported from: itwinjs-core QueryScreenFeaturesOptions
struct DQ_RENDER_EXPORT QueryScreenFeaturesOptions : public QueryVisibleFeaturesOptions {
    QueryScreenFeaturesOptions() noexcept { source = VisibleFeatureSource::Screen; }

    // If specified, a sub-region of the Viewport to which to constrain the query.
    // TODO: ViewRect not yet ported; opaque pointer placeholder.
    const ViewRect* rect = nullptr;
};

// Tile-source options: source == Tiles.
// Ported from: itwinjs-core QueryTileFeaturesOptions
struct DQ_RENDER_EXPORT QueryTileFeaturesOptions : public QueryVisibleFeaturesOptions {
    QueryTileFeaturesOptions() noexcept { source = VisibleFeatureSource::Tiles; }
};

// A function supplied to Viewport.queryVisibleFeatures to process results.
// Ported from: itwinjs-core QueryVisibleFeaturesCallback
// DanQing note: itwinjs passes an Iterable<VisibleFeature>; C++ uses a vector ref.
using QueryVisibleFeaturesCallback = std::function<void(const std::vector<VisibleFeature>&)>;

END_DQ_RENDER_NAMESPACE
