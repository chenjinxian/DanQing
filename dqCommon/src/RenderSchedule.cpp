// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Render schedule script（out-of-line 实现）
// Ported from: itwinjs-core core/common/src/RenderSchedule.ts
//
// addSymbologyOverrides 族：schedule 的 FeatureOverrides 消费腿——
//   Timeline.ts（DTA Animation 下拉）→ Viewport step 8/9 →
//   FeatureSymbology.initFromView → Script.addSymbologyOverrides →
//   FeatureOverrides.overrideModel / overrideAnimationNode /
//   setAnimationNodeNeverDrawn（dqCommon FeatureOverrides 既有面，I10 接线）。
#include "dqCommon/RenderSchedule.h"

#include "dqCommon/FeatureOverrides.h"

BEGIN_DQ_COMMON_NAMESPACE
namespace RenderSchedule {

// FeatureOverrides contribution at the specified time (batchId path).
// Ported from: itwinjs-core RenderSchedule.ElementTimeline.addSymbologyOverrides (:844-856)
void ElementTimeline::addSymbologyOverrides(FeatureOverrides& overrides, double time) const
{
    // 参考 assert(0 !== this.batchId)——DanQing 核心禁断言即异常（§9），
    // batchId==0 时参考行为系未定义；release 形态按原逻辑继续（getVisibility
    // 对 batchId 0 无特殊语义，调用方 Script 数据面保证 batchId 非 0）。
    double const vis = getVisibility(time);
    if (vis <= 0.0) {
        overrides.setAnimationNodeNeverDrawn(batchId);
        return;
    }

    auto const appearance = getFeatureAppearance(vis, time);
    if (appearance)
        overrides.overrideAnimationNode(batchId, *appearance);
}

// FeatureOverrides contribution at the specified time (model-level + element).
// Ported from: itwinjs-core RenderSchedule.ModelTimeline.addSymbologyOverrides (:974-981)
void ModelTimeline::addSymbologyOverrides(FeatureOverrides& overrides, double time) const
{
    auto const appearance = getFeatureAppearance(getVisibility(time), time);
    if (appearance)
        overrides.overrideModel(dqBase::DqId::FromString(modelId), *appearance);

    for (auto const& timeline : elementTimelines)
        timeline.addSymbologyOverrides(overrides, time);
}

// FeatureOverrides contribution of every model timeline at the specified time.
// Ported from: itwinjs-core RenderSchedule.Script.addSymbologyOverrides (:1157-1160)
void Script::addSymbologyOverrides(FeatureOverrides& overrides, double time) const
{
    for (auto const& timeline : modelTimelines)
        timeline.addSymbologyOverrides(overrides, time);
}

}  // namespace RenderSchedule
END_DQ_COMMON_NAMESPACE
