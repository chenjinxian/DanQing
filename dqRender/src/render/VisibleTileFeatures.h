// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Visible tile features iterator
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/VisibleTileFeatures.ts
//
// Iterates over features visible in tiles selected for display by inspecting
// the batch data and feature overrides.  This is the primary mechanism for
// querying which BIM elements are currently visible on screen.
#pragma once

#include "FeatureOverrides.h"
// 参考实现（VisibleTileFeatures.ts:11）从公开 API import 类型：
//   import { QueryTileFeaturesOptions, VisibleFeature } from "../../../render/VisibleFeature";
// 内部实现不得重定义同名类型——曾在此（与 QueryVisibleFeatures.h）各自定义
// VisibleFeature/QueryVisibleFeaturesOptions/QueryTileFeaturesOptions 的另一布局，
// 与 PublicAPI/dqRender/VisibleFeature.h 构成 ODR 违规：同名内联 ctor COMDAT 被
// 链接器任选其一，20 字节布局的 ctor 写穿 2 字节栈对象 → MSVC Debug RTC
// "stack around the variable 'tiles' was corrupted"（dqRenderTest 崩溃弹窗）。
#include "dqRender/VisibleFeature.h"

#include <cstdint>
#include <functional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// FeatureEntry — a single feature within a batch
// (Ported from: itwinjs-core VisibleFeature.ts / PackedFeature)
// ---------------------------------------------------------------------------
struct FeatureEntry {
    uint32_t elementId = 0;
    uint32_t subCategoryId = 0;
    uint8_t geometryClass = 0;
};

// ---------------------------------------------------------------------------
// FeatureBatchData — a batch of features with associated overrides
// (Simplified representation of a render batch for feature queries)
// ---------------------------------------------------------------------------
struct FeatureBatchData {
    /// Features in this batch.
    std::vector<FeatureEntry> features;

    /// Per-batch feature overrides (may be nullptr if no overrides applied).
    FeatureOverrides const* overrides = nullptr;

    /// True if the entire batch is hidden by symbology.
    bool allHidden = false;
};

// ---------------------------------------------------------------------------
// VisibleTileFeatures — iterates visible features from batch data
// (Ported from: itwinjs-core VisibleTileFeatures.ts)
//
// Simplified version: takes a vector of FeatureBatchData rather than the
// full RenderCommands + Target infrastructure.
// ---------------------------------------------------------------------------
class VisibleTileFeatures {
public:
    using Callback = std::function<void(VisibleFeature const&)>;

    /// Construct with batch data and query options.
    /// @param batches The batch data to iterate over.
    /// @param options Query options controlling visibility filtering.
    explicit VisibleTileFeatures(std::vector<FeatureBatchData> const& batches,
                                 QueryTileFeaturesOptions const& options);

    /// Iterate all visible features, calling the callback for each.
    void forEach(Callback callback) const;

    /// Collect all visible features into a vector.
    std::vector<VisibleFeature> collect() const;

private:
    /// Check if a feature is visible given the batch overrides.
    bool isFeatureVisible(FeatureEntry const& feature,
                          FeatureBatchData const& batch) const;

    std::vector<FeatureBatchData> const& m_batches;
    QueryTileFeaturesOptions m_options;
};

END_DQ_RENDER_NAMESPACE
