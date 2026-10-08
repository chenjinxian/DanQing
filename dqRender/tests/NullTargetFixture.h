// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TargetImpl test fixture (NullDriver-backed)
//
// Authored: DanQing test infrastructure——无参考对应（itwinjs 的 Target 是
//           运行期 GL 对象；本夹具以 NullDriver 构造真实 TargetImpl 栈供
//           GL-free 单测消费）。
//
// ctor 链 GL-free（M-S S-c 实证）：RenderSystemImpl 仅 createDefaultRenderTarget
// 桩（NullDriver 回空句柄）；TargetImpl 的 SceneCompositor 仅持渲染态成员。
// 用途：getPass(target)（CachedGeometry.ts:84 签名）等 target 消费面的单测。
#pragma once

#include "NullDriver.h"
#include "render/RenderSystemImpl.h"
#include "render/TargetImpl.h"
#include "render/TechniqueImpl.h"

#include <memory>

BEGIN_DQ_RENDER_NAMESPACE

struct NullTargetFixture {
    std::unique_ptr<RenderSystemImpl> system;
    Techniques techniques;
    std::unique_ptr<TargetImpl> target;

    NullTargetFixture()
    {
        system = std::make_unique<RenderSystemImpl>(std::make_unique<rhi::NullDriver>());
        target = std::make_unique<TargetImpl>(*system, techniques, ViewRect(0, 0, 100, 100));
    }
};

END_DQ_RENDER_NAMESPACE
