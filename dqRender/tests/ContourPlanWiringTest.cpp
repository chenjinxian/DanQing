// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour plan/branch wiring tests（M-X X-a2）
//
// 锚定：RenderPlan.ts:62（plan.contours 字段）+ :128（提取门
//   is3d && groups.length>0）+ Target.ts:533（branch.changeRenderPlan 第 4 参
//   plan.contours）+ :269（currentContours=currentBranch.contourLine）+
//   BranchState.ts:54/:84/:93-98（contourLine 载体 + changeRenderPlan 消费）
//   + :123（fromBranch 继承 `branch.contourLine ?? prev.contourLine`）。
//
// Authored: no reference test exists in itwinjs-core for the contour plan/branch
//           wiring（core/frontend 无 Contours/BranchState 单测面——行为锚=
//           上述源码逐行；X-a5 像素锁覆盖端到端面）。
#include <gtest/gtest.h>

#include "NullDriver.h"
#include "NullTargetFixture.h"
#include "render/BranchStack.h"
#include "render/Graphic.h"
#include "render/TargetImpl.h"

#include <dqCommon/ContourDisplay.h>
#include <dqRender/RenderPlan.h>

using namespace dqRender;

namespace {

dqCommon::ContourDisplay makeContours(bool display, double minorInterval)
{
    dqCommon::ContourDisplay d;
    d.displayContours = display;
    dqCommon::ContourGroup g;
    g.contourDef.minorInterval = minorInterval;
    d.groups.push_back(g);
    return d;
}

}  // namespace

// plan 载体：RenderPlan.contours optional 段（RenderPlan.ts:62）+ equals 段
//（thematic/ao 段同形先例——RenderPlan.h 的在场性+值双重比较）。
TEST(ContourPlanWiring, RenderPlanContoursCarrier)
{
    RenderPlan a;
    RenderPlan b;
    EXPECT_FALSE(a.contours.has_value());
    EXPECT_TRUE(a.equals(b));

    // 在场性差异 → 不等。
    b.contours = makeContours(true, 1.0);
    EXPECT_FALSE(a.equals(b));
    EXPECT_FALSE(b.equals(a));

    // 同值 → 等。
    a.contours = makeContours(true, 1.0);
    EXPECT_TRUE(a.equals(b));

    // 值差异 → 不等（minorInterval 段——ContourDisplay.equals 全字段面）。
    b.contours = makeContours(true, 2.0);
    EXPECT_FALSE(a.equals(b));
}

// branch 透传：BranchStack 底层 changeRenderPlan 第 4 参（BranchState.ts:93-98
// 的 `_opts.contourLine = contour`）→ 栈顶读取面（Target.ts:269 的
// currentBranch.contourLine）。
TEST(ContourPlanWiring, BranchStackChangeRenderPlanCarriesContourLine)
{
    BranchStack stack;
    EXPECT_FALSE(stack.getTop().getContourLine().has_value());

    dqRender::ViewFlags vf;
    auto const contours = makeContours(true, 1.0);
    stack.changeRenderPlan(vf, true, nullptr, &contours);
    ASSERT_TRUE(stack.getTop().getContourLine().has_value());
    EXPECT_TRUE(stack.getTop().getContourLine()->equals(contours));

    // 下一轮换 plan 缺席 contours（参考 undefined）→ contourLine 清
    //（BranchState.changeRenderPlan 无条件赋值——`:97 this._opts.contourLine =
    //  contour`，无短路）。
    stack.changeRenderPlan(vf, true);
    EXPECT_FALSE(stack.getTop().getContourLine().has_value());
}

// fromBranch 继承（BranchState.ts:123——`contourLine: branch.contourLine ??
// prev.contourLine`）：branch 无 contourLine → 继承父；有 → 覆写。
TEST(ContourPlanWiring, FromBranchInheritsOrOverridesContourLine)
{
    BranchStack stack;
    dqRender::ViewFlags vf;
    auto const planContours = makeContours(true, 1.0);
    stack.changeRenderPlan(vf, true, nullptr, &planContours);

    // 无 contourLine 的 branch → 继承父栈顶（plan 值）。
    auto plainBranch = std::make_unique<Branch>();
    BranchState inherited = BranchState::fromBranch(stack.getTop(), *plainBranch);
    ASSERT_TRUE(inherited.getContourLine().has_value());
    EXPECT_TRUE(inherited.getContourLine()->equals(planContours))
        << "branch without contourLine inherits the parent's (BranchState.ts:123 ?? prev)";

    // 携 contourLine 的 branch → 覆写。
    auto const branchContours = makeContours(false, 9.0);
    auto carryingBranch = std::make_unique<Branch>();
    carryingBranch->setContourLine(branchContours);
    BranchState overridden = BranchState::fromBranch(stack.getTop(), *carryingBranch);
    ASSERT_TRUE(overridden.getContourLine().has_value());
    EXPECT_TRUE(overridden.getContourLine()->equals(branchContours))
        << "branch contourLine overrides the parent's (Graphic.ts:353/:374)";
}

// changeRenderPlan 调用序（Target.ts:533→:539）：TargetImpl::changeRenderPlan 携
// contours 入栈底 → getCurrentContours（Target.ts:269=栈顶 contourLine）回读 +
// uniforms.contours.update（TargetUniforms.contours 成员——TargetUniforms.ts:134）
// 消费同值。
TEST(ContourPlanWiring, TargetChangeRenderPlanFeedsContoursAndUniforms)
{
    NullTargetFixture fixture;

    // 初始：无 contours（reference Target.currentContours=undefined 形）。
    EXPECT_EQ(nullptr, fixture.target->getCurrentContours());

    dqRender::ViewFlags vf;
    auto const contours = makeContours(true, 3.0);
    fixture.target->changeRenderPlan(vf, true, nullptr,
                                     nullptr, nullptr, &contours);

    // currentContours 自 branch 栈顶读（Target.ts:269 归位——不再经 target 成员）。
    auto const* current = fixture.target->getCurrentContours();
    ASSERT_NE(nullptr, current);
    EXPECT_TRUE(current->equals(contours));

    // TargetUniforms.contours.update 消费 currentContours（Target.ts:539）。
    fixture.target->getUniforms().contours.update(*fixture.target);
    auto const* uniformDisplay = fixture.target->getUniforms().contours.getContourDisplay();
    ASSERT_NE(nullptr, uniformDisplay);
    EXPECT_TRUE(uniformDisplay->equals(contours));

    // 轮廓值变更（同址替换新值）→ 值等比短路不命中、重打包（参考
    // ContourUniforms.update :58-61 的 equals 门——非指针同一性）。
    auto const changed = makeContours(true, 7.5);
    fixture.target->changeRenderPlan(vf, true, nullptr,
                                     nullptr, nullptr, &changed);
    fixture.target->getUniforms().contours.update(*fixture.target);
    ASSERT_NE(nullptr, fixture.target->getUniforms().contours.getContourDisplay());
    EXPECT_NEAR(7.5, fixture.target->getUniforms().contours.getContourDisplay()
                         ->groups[0].contourDef.minorInterval, 1e-12);

    // 同值重写 → update 的 equals 门短路（m_contourDisplay 保持——值语义存管）。
    fixture.target->getUniforms().contours.update(*fixture.target);
    EXPECT_NEAR(7.5, fixture.target->getUniforms().contours.getContourDisplay()
                         ->groups[0].contourDef.minorInterval, 1e-12);
}
