// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Animation branch state
// Ported from: itwinjs-core core/frontend/src/internal/render/AnimationBranchState.ts
//
// State management for animated branches (schedule scripts, etc.).
// Evaluates schedule scripts at a given time to produce branch states
// for clip planes, visibility, and transforms.
#pragma once

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

// ---------------------------------------------------------------------------
// AnimationBranchState — animation state for a branch
// (Ported from: itwinjs-core AnimationBranchState.ts)
//
// Contains clip/omit state for an animated branch. Used by the rendering
// pipeline to determine which branches to render and how to clip them.
// ---------------------------------------------------------------------------
class AnimationBranchState {
public:
    AnimationBranchState() = default;

    /// Set the animation transform (4x4 matrix).
    void setTransform(float const* matrix4x4)
    {
        for (int i = 0; i < 16; ++i) m_transform[i] = matrix4x4[i];
        m_hasTransform = true;
    }

    /// Get the animation transform.
    float const* getTransform() const noexcept { return m_transform; }

    /// Check if this state has an animation transform.
    bool hasTransform() const noexcept { return m_hasTransform; }

    /// Set the animation node ID.
    void setNodeId(uint32_t id) noexcept { m_nodeId = id; }

    /// Get the animation node ID.
    uint32_t getNodeId() const noexcept { return m_nodeId; }

    /// Set whether this branch is visible.
    void setVisible(bool visible) noexcept { m_visible = visible; }

    /// Check if this branch is visible.
    bool isVisible() const noexcept { return m_visible; }

    /// Set whether to omit this branch entirely.
    void setOmit(bool omit) noexcept { m_omit = omit; }

    /// Check if this branch should be omitted.
    bool isOmitted() const noexcept { return m_omit; }

private:
    float m_transform[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    uint32_t m_nodeId = 0;
    bool m_hasTransform = false;
    bool m_visible = true;
    bool m_omit = false;
};

// ---------------------------------------------------------------------------
// formatAnimationBranchId — format a branch ID from model + node
// Ported from: itwinjs-core AnimationBranchState.ts formatAnimationBranchId()
// ---------------------------------------------------------------------------
inline std::string formatAnimationBranchId(std::string const& modelId, int branchId)
{
    if (branchId < 0)
        return modelId;
    return modelId + "_Node_" + std::to_string(branchId);
}

// ---------------------------------------------------------------------------
// AnimationBranchStates — mapping from node IDs to branch states
// Ported from: itwinjs-core AnimationBranchState.ts AnimationBranchStates
// ---------------------------------------------------------------------------
struct AnimationBranchStates {
    /// Maps branch ID string to branch state.
    dqBase::bmap<std::string, AnimationBranchState> branchStates;

    /// IDs of nodes that apply a transform.
    dqBase::bset<int> transformNodeIds;
};

// ---------------------------------------------------------------------------
// AnimationBranchStates::fromScript — create branch states from a schedule script
// Ported from: itwinjs-core AnimationBranchState.ts AnimationBranchStates.fromScript()
//
// Evaluates a schedule script at the given time and produces branch states
// for clip planes, visibility, and transforms.
// ---------------------------------------------------------------------------
inline std::optional<AnimationBranchStates> animationBranchStatesFromScript(
    dqCommon::RenderSchedule::Script const& script, double time)
{
    // M-U U-a 最小适配：dqCommon RenderSchedule 全量移植（RenderSchedule.ts 1:1）
    // 后 containsX 为字段、elementTimelines 为解析后对象——本文件系早期自创
    // 桩（"For now, use the first entry (simplified)"），U-e 将按
    // AnimationBranchState.ts 全文重写（getClipVector(time) 消费 + fromScript
    // 1:1）；此处仅保编译兼容。
    if (!script.containsModelClipping && !script.containsTransform)
        return std::nullopt;

    AnimationBranchStates result;

    for (auto const& model : script.modelTimelines) {
        for (auto const& elem : model.elementTimelines) {
            if (elem.getVisibility(time) <= 0.0) {
                auto branchId = formatAnimationBranchId(model.modelId, static_cast<int>(elem.batchId));
                AnimationBranchState state;
                state.setOmit(true);
                result.branchStates[branchId] = state;
            }

            if (elem.containsTransform())
                result.transformNodeIds.insert(static_cast<int>(elem.batchId));
        }
    }

    return result;
}

END_DQ_RENDER_NAMESPACE
