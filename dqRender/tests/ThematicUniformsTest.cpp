// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ThematicUniforms tests
//
// Authored: no reference test exists in itwinjs-core for ThematicUniforms
//（全仓无 ThematicUniforms.test.ts——行为锚定 ThematicUniforms.ts:90-152
//  update 语义 / Target.ts:398-409 计算 getter / :537 changeRenderPlan 序；
//  M-S S-c 重写——本文件旧版 "Ported from .../ThematicUniforms.test.ts"
//  溯源头注系错误登记[该文件不存在]，随重写一并订正）。
#include "render/ThematicUniforms.h"

#include "NullDriver.h"
#include "render/FrustumUniforms.h"
#include "render/RenderSystemImpl.h"
#include "render/TargetImpl.h"
#include "render/TechniqueImpl.h"
#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"

#include <dqGeom/Angle.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

namespace {

// MockDriver（NullDriver.h 既定模式：派生+只覆写关心面）——createTexture 回
// 有效句柄（HandleAllocator 承载）+ 计数，供 ThematicUniforms 快路径
//（`&& _texture` 条件，ThematicUniforms.ts:93）与纹理重建次数断言。
class CountingTextureDriver : public rhi::NullDriver {
public:
    rhi::TextureHandle createTexture(rhi::SamplerType, uint8_t, rhi::TextureFormat,
                                     uint32_t w, uint32_t h, uint32_t,
                                     rhi::TextureUsage) noexcept override
    {
        ++createTextureCalls;
        lastTexWidth = w;
        lastTexHeight = h;
        return m_allocator.allocate<rhi::HwTexture>();
    }

    int createTextureCalls = 0;
    uint32_t lastTexWidth = 0;
    uint32_t lastTexHeight = 0;

private:
    rhi::HandleAllocator m_allocator{1024 * 1024};
};

// TargetImpl 真实栈（NullDriver——ctor 链 GL-free：RenderSystemImpl 仅
// createDefaultRenderTarget 桩 / SceneCompositor 仅渲染态成员）。
struct ThematicTargetFixture {
    CountingTextureDriver* driver;
    std::unique_ptr<RenderSystemImpl> system;
    Techniques techniques;
    std::unique_ptr<TargetImpl> target;

    ThematicTargetFixture()
    {
        auto drv = std::make_unique<CountingTextureDriver>();
        driver = drv.get();
        system = std::make_unique<RenderSystemImpl>(std::move(drv));
        target = std::make_unique<TargetImpl>(*system, techniques, ViewRect(0, 0, 100, 100));
    }
};

// changeRenderPlan(thematic) + uniforms.thematic.update(target)——
// Target.ts:533-537 序的手动驱动（vf.thematicDisplay 位 + thematic 段）。
//（dqRender 面的 ViewFlags=dqCommon::ViewFlagsProperties 别名——直字段。）
void PushThematic(TargetImpl& t, ThematicDisplay const* td, bool vfOn = true)
{
    ViewFlagsProperties vf;
    vf.thematicDisplay = vfOn;
    t.changeRenderPlan(vf, true, nullptr, td);
    t.getUniforms().thematic.update(t);
}

// 视矩阵注入（FrustumUniforms::changeViewMatrix 公共面——FrustumUniforms.h:290）。
void SetViewRotation(TargetImpl& t, dqGeom::Matrix3d const& rot)
{
    t.getUniforms().frustum.changeViewMatrix(dqGeom::Transform::CreateOriginAndMatrix(
        dqGeom::Point3d::From(0, 0, 0), rot));
}

} // namespace

// 谓词面：wantIsoLines/wantSlopeMode/wantHillShadeMode
//（ThematicUniforms.ts:49-61——含 IsoLines 仅 Height 门）。
TEST(ThematicUniformsTest, ModePredicates)
{
    ThematicTargetFixture f;
    auto& thematic = f.target->getUniforms().thematic;

    EXPECT_EQ(thematic.getThematicDisplay(), nullptr);
    EXPECT_FALSE(thematic.wantIsoLines());
    EXPECT_TRUE(thematic.isDisposed());

    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Height;
    td.gradientSettings.mode = ThematicGradientMode::IsoLines;
    PushThematic(*f.target, &td);
    EXPECT_TRUE(thematic.wantIsoLines());
    EXPECT_FALSE(thematic.wantSlopeMode());
    EXPECT_FALSE(thematic.wantHillShadeMode());

    td.gradientSettings.mode = ThematicGradientMode::Smooth;
    td.displayMode = ThematicDisplayMode::Slope;
    PushThematic(*f.target, &td);
    EXPECT_FALSE(thematic.wantIsoLines());
    EXPECT_TRUE(thematic.wantSlopeMode());

    td.displayMode = ThematicDisplayMode::HillShade;
    PushThematic(*f.target, &td);
    EXPECT_TRUE(thematic.wantHillShadeMode());
    EXPECT_FALSE(thematic.wantSlopeMode());
}

// clear 路径（ThematicUniforms.ts:110-113——plan.thematic 缺席→清态+纹理释放）。
TEST(ThematicUniformsTest, UpdateClearsWhenPlanLacksThematic)
{
    ThematicTargetFixture f;
    auto& thematic = f.target->getUniforms().thematic;

    ThematicDisplay td;
    PushThematic(*f.target, &td);
    ASSERT_NE(thematic.getThematicDisplay(), nullptr);
    ASSERT_EQ(f.driver->createTextureCalls, 1);  // 渐变纹理已建

    PushThematic(*f.target, nullptr);
    EXPECT_EQ(thematic.getThematicDisplay(), nullptr);
    EXPECT_TRUE(thematic.isDisposed());  // 纹理已释放
}

// Slope range 度→弧度（ThematicUniforms.ts:115-121）。
TEST(ThematicUniformsTest, SlopeRangeConvertsDegreesToRadians)
{
    ThematicTargetFixture f;
    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Slope;
    td.range = dqGeom::Range1d(0.0, 90.0);
    PushThematic(*f.target, &td);

    UniformHandle h;
    f.target->getUniforms().thematic.bindRange(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], static_cast<float>(dqGeom::Angle::kPi / 2.0));
}

// 轴恒 normalize；Height 不经视矩阵（ThematicUniforms.ts:73-79/125）。
TEST(ThematicUniformsTest, AxisNormalizedAndViewTransformedOnlyForSlope)
{
    ThematicTargetFixture f;
    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Height;
    td.axis = dqGeom::Vector3d::From(0, 0, 2);

    // 视图旋转 90°（绕 X——+Z 世界向入视为 (0,-1,0)）。
    auto rot = dqGeom::Matrix3d::CreateRotationAroundAxis(
        dqGeom::Vector3d::From(1, 0, 0), dqGeom::Angle::kPi / 2.0);
    SetViewRotation(*f.target, rot);
    PushThematic(*f.target, &td);

    UniformHandle h;
    f.target->getUniforms().thematic.bindAxis(h);
    // Height：视矩阵不参与——归一化后的世界轴原样。
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], 1.0f);

    // Slope：轴经视矩阵变换（(0,0,2) → 视 (0,-1,0)，归一化；旋转残差
    // cos(90°)~6e-17 以 1e-6 容差断言）。
    td.displayMode = ThematicDisplayMode::Slope;
    td.gradientSettings.mode = ThematicGradientMode::Smooth;  // Slope 禁用 IsoLines 无涉
    PushThematic(*f.target, &td);
    f.target->getUniforms().thematic.bindAxis(h);
    EXPECT_NEAR(h.getData()[0], 0.0f, 1e-6f);
    EXPECT_NEAR(h.getData()[1], -1.0f, 1e-6f);
    EXPECT_NEAR(h.getData()[2], 0.0f, 1e-6f);
}

// HillShade 太阳向：视矩阵变换 + negate + normalize（ThematicUniforms.ts:81-88）。
TEST(ThematicUniformsTest, HillShadeSunDirectionNegatedAndViewTransformed)
{
    ThematicTargetFixture f;
    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::HillShade;
    td.sunDirection = dqGeom::Vector3d::From(0, 0, 1);

    PushThematic(*f.target, &td);
    UniformHandle h;
    f.target->getUniforms().thematic.bindSunDirection(h);
    // 恒等视：世界 +Z → negate → (0,0,-1)。
    EXPECT_FLOAT_EQ(h.getData()[0], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], -1.0f);

    // 旋视 90°（绕 X）：(0,0,1)→视(0,-1,0)→negate→(0,1,0)。
    auto rot = dqGeom::Matrix3d::CreateRotationAroundAxis(
        dqGeom::Vector3d::From(1, 0, 0), dqGeom::Angle::kPi / 2.0);
    SetViewRotation(*f.target, rot);
    PushThematic(*f.target, &td);
    f.target->getUniforms().thematic.bindSunDirection(h);
    EXPECT_NEAR(h.getData()[0], 0.0f, 1e-6f);
    EXPECT_NEAR(h.getData()[1], 1.0f, 1e-6f);
    EXPECT_NEAR(h.getData()[2], 0.0f, 1e-6f);
}

// 快路径（ThematicUniforms.ts:93-106）：设置等值+纹理在→不重建纹理；
// Slope 轴随视矩阵逐帧刷新（desync 发生但 createTexture 不再调）。
TEST(ThematicUniformsTest, FastPathRefreshesSlopeAxisWithoutTextureRebuild)
{
    ThematicTargetFixture f;
    auto& thematic = f.target->getUniforms().thematic;

    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Slope;
    td.axis = dqGeom::Vector3d::From(0, 0, 1);
    PushThematic(*f.target, &td);
    ASSERT_EQ(f.driver->createTextureCalls, 1);
    const auto key0 = thematic.getSyncKey();

    // 同设置 + 视矩阵变 → 快路径：纹理不重建、syncKey 翻（Slope 臂 desync）、
    // 轴值刷新。
    auto rot = dqGeom::Matrix3d::CreateRotationAroundAxis(
        dqGeom::Vector3d::From(1, 0, 0), dqGeom::Angle::kPi / 2.0);
    SetViewRotation(*f.target, rot);
    PushThematic(*f.target, &td);

    EXPECT_EQ(f.driver->createTextureCalls, 1);
    EXPECT_NE(thematic.getSyncKey(), key0);
    UniformHandle h;
    thematic.bindAxis(h);
    EXPECT_NEAR(h.getData()[1], -1.0f, 1e-6f);

    // Height 模式同设置 → 快路径但无视图相关刷新（无 desync）。
    ThematicDisplay hd;
    hd.displayMode = ThematicDisplayMode::Height;
    hd.axis = dqGeom::Vector3d::From(0, 0, 1);
    PushThematic(*f.target, &hd);
    ASSERT_EQ(f.driver->createTextureCalls, 2);  // 模式变→全重建
    const auto key1 = thematic.getSyncKey();
    PushThematic(*f.target, &hd);  // 同设置
    EXPECT_EQ(f.driver->createTextureCalls, 2);
    EXPECT_EQ(thematic.getSyncKey(), key1);  // Height 快路径无 desync
}

// 渐变纹理形态（ThematicUniforms.ts:149-151 + Gradient.ts:307-359——1×N 列向：
// Smooth→8192 高 / Stepped→stepCount 高）。
TEST(ThematicUniformsTest, GradientTextureIsColumnWithModeDimension)
{
    ThematicTargetFixture f;

    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Height;
    PushThematic(*f.target, &td);
    EXPECT_EQ(f.driver->lastTexWidth, 1u);
    EXPECT_EQ(f.driver->lastTexHeight, 8192u);  // Smooth → kDefaultGradientDimension

    ThematicDisplay sd;
    sd.displayMode = ThematicDisplayMode::Height;
    sd.gradientSettings.mode = ThematicGradientMode::Stepped;
    sd.gradientSettings.stepCount = 4;
    PushThematic(*f.target, &sd);
    EXPECT_EQ(f.driver->lastTexWidth, 1u);
    EXPECT_EQ(f.driver->lastTexHeight, 4u);  // Stepped → stepCount
}

// fragSettings 四槽 + marginColor + displayMode（:130-140）。
TEST(ThematicUniformsTest, FragSettingsAndMarginColorBind)
{
    ThematicTargetFixture f;
    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::Slope;
    td.gradientSettings.mode = ThematicGradientMode::Stepped;
    td.gradientSettings.stepCount = 7;
    td.gradientSettings.marginColor = ColorDef::red;
    td.gradientSettings.transparencyMode = ThematicGradientTransparencyMode::MultiplySurfaceAndGradient;
    td.sensorSettings.distanceCutoff = 12.5;
    PushThematic(*f.target, &td);

    auto const& thematic = f.target->getUniforms().thematic;
    UniformHandle h;
    thematic.bindFragSettings(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);   // Stepped
    EXPECT_FLOAT_EQ(h.getData()[1], 12.5f);  // distanceCutoff
    EXPECT_FLOAT_EQ(h.getData()[2], 7.0f);   // stepCount
    EXPECT_FLOAT_EQ(h.getData()[3], 1.0f);   // multiply gradient alpha

    thematic.bindDisplayMode(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 2.0f);   // Slope

    thematic.bindMarginColor(h);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);   // red
    EXPECT_FLOAT_EQ(h.getData()[1], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[2], 0.0f);
    EXPECT_FLOAT_EQ(h.getData()[3], 1.0f);   // alpha = 1 - transparency 0
}

// Target.wantThematicDisplay 计算 getter（Target.ts:398-400——
// currentViewFlags.thematicDisplay && is3d && uniforms.thematic.thematicDisplay）。
TEST(ThematicUniformsTest, WantThematicDisplayIsComputedFromFlagsAndUniforms)
{
    ThematicTargetFixture f;
    EXPECT_FALSE(f.target->wantThematicDisplay());  // 初始无

    ThematicDisplay td;
    PushThematic(*f.target, &td);
    EXPECT_TRUE(f.target->wantThematicDisplay());

    // vf 位关（thematic 段仍在）→ false（vf 门在 getter 内）。
    PushThematic(*f.target, &td, /*vfOn=*/false);
    EXPECT_FALSE(f.target->wantThematicDisplay());

    // vf 开 + thematic 缺席 → false。
    PushThematic(*f.target, nullptr, true);
    EXPECT_FALSE(f.target->wantThematicDisplay());
}

// Target.wantThematicSensors（Target.ts:406-409——IDW 且 sensors 非空）+
// 全局传感器纹理臂（ThematicUniforms.ts:142-147——CPU 数据面+计数）。
TEST(ThematicUniformsTest, WantThematicSensorsAndGlobalSensorState)
{
    ThematicTargetFixture f;

    ThematicDisplay td;
    td.displayMode = ThematicDisplayMode::InverseDistanceWeightedSensors;
    ThematicDisplaySensor s1;
    s1.position = dqGeom::Point3d::From(1, 2, 3);
    s1.value = 0.5;
    td.sensorSettings.sensors.push_back(s1);
    ThematicDisplaySensor s2;
    s2.position = dqGeom::Point3d::From(4, 5, 6);
    s2.value = 1.0;
    td.sensorSettings.sensors.push_back(s2);

    PushThematic(*f.target, &td);
    EXPECT_TRUE(f.target->wantThematicSensors());
    UniformHandle h;
    f.target->getUniforms().thematic.bindNumSensors(h);
    EXPECT_EQ(h.getIntData()[0], 2);  // 全局臂装载 2 传感器

    // 空传感器 → false（Target.ts:408 sensors.length>0 门）。
    ThematicDisplay empty = td;
    empty.sensorSettings.sensors.clear();
    PushThematic(*f.target, &empty);
    EXPECT_FALSE(f.target->wantThematicSensors());

    // Height 模式（传感器在）→ false（mode 门）。
    ThematicDisplay height = td;
    height.displayMode = ThematicDisplayMode::Height;
    PushThematic(*f.target, &height);
    EXPECT_FALSE(f.target->wantThematicSensors());
}

TEST(ThematicUniformsTest, DefaultGradientDimension)
{
    EXPECT_EQ(kDefaultGradientDimension, 8192);
}
