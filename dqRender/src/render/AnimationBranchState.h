// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Animation branch states
// Ported from: itwinjs-core core/frontend/src/internal/render/AnimationBranchState.ts
//
// Clip/Transform for a branch that are varied over time (schedule scripts).
// Viewport.renderFrame step 8 evaluates the display style's schedule script at
// the current time point and installs the result on the target; RenderCommands
// consults it while building draw commands (omit branches / push clip volumes).
//
// M-U U-e：参考形全文重写（早期自创桩 AnimationBranchState[transform 矩阵/
// visible/nodeId] 清退——非参考 API；RemainingItemsTest 三例随重写）。
#pragma once

#include "ClipVolume.h"

#include <dqBase/bmap.h>
#include <dqBase/bset.h>
#include <dqCommon/RenderSchedule.h>

#include <cstdint>
#include <optional>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Clip/omit state for a branch that is varied over time.
// Ported from: itwinjs-core AnimationBranchState (AnimationBranchState.ts:15-18)
struct AnimationBranchState {
    // ← AnimationBranchState.clip?: RenderClipVolume
    ClipVolume::Ptr clip;
    // ← AnimationBranchState.omit?: boolean
    bool omit = false;
};

// Format a branch Id from model + node. branchId < 0 → the model itself.
// Ported from: itwinjs-core formatAnimationBranchId (AnimationBranchState.ts:21-26)
inline std::string formatAnimationBranchId(std::string const& modelId, int branchId)
{
    if (branchId < 0)
        return modelId;
    return modelId + "_Node_" + std::to_string(branchId);
}

// Mapping from node/branch IDs to animation branch state.
// Ported from: itwinjs-core AnimationBranchStates (AnimationBranchState.ts:36-41)
struct AnimationBranchStates {
    // Maps node Id to branch state.
    dqBase::bmap<std::string, AnimationBranchState> branchStates;
    // Ids of nodes that apply a transform.
    dqBase::bset<int> transformNodeIds;
};

// Ported from: itwinjs-core addAnimationBranch (AnimationBranchState.ts:28-33)
// §3.4 适配：参考经 IModelApp.renderSystem.createClipVolume（全局系统门面）；
// DanQing 单后端直调 ClipVolume::create（ClipStack.cpp:89 的同一适配先例）。
inline void addAnimationBranch(std::string const& modelId,
                               dqCommon::RenderSchedule::Timeline const& timeline,
                               int branchId, AnimationBranchStates& branches, double time)
{
    auto const clipVector = timeline.getClipVector(time);
    if (!clipVector)
        return;

    ClipVolume::Ptr const clip = ClipVolume::create(*clipVector);
    if (clip.IsValid())
        branches.branchStates[formatAnimationBranchId(modelId, branchId)].clip = clip;
}

// Evaluate a schedule script at the specified time and produce branch states.
// Ported from: itwinjs-core AnimationBranchStates.fromScript (:44-63)
inline std::optional<AnimationBranchStates> animationBranchStatesFromScript(
    dqCommon::RenderSchedule::Script const& script, double time)
{
    if (!script.containsModelClipping && !script.requiresBatching)
        return std::nullopt;

    AnimationBranchStates result;
    for (auto const& model : script.modelTimelines) {
        addAnimationBranch(model.modelId, model, -1, result, time);
        for (auto const& elem : model.elementTimelines) {
            if (elem.getVisibility(time) <= 0.0)
                result.branchStates[formatAnimationBranchId(model.modelId,
                                                            static_cast<int>(elem.batchId))]
                    .omit = true;
            else
                addAnimationBranch(model.modelId, elem, static_cast<int>(elem.batchId), result,
                                   time);
        }
    }

    for (auto const batchId : script.transformBatchIds)
        result.transformNodeIds.insert(static_cast<int>(batchId));

    return result;
}

END_DQ_RENDER_NAMESPACE
