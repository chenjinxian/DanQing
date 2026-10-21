// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contour display E2E pixel locks（M-X X-a5）
//
// 合成场景：侧视正交（相机 (0,-5,0) 朝 +Y、up=+Z——世界 z 映屏幕行）下
// 单特征 Batch 包裹的盒（前表面 y=-0.3 朝相机）——等高线=水平色带：
//   minorInterval=0.25 → 线位于 z=0,±.25,±.5,±.75（coord=v_height/interval
//   整数值——Contours.ts:71-76 的 `abs(fract(coord-0.5)-0.5)` 零点在 coord
//   整数）→ 屏幕行（GL 原点在左下：row=(z+1)*50）=50, 37.5/62.5, 25/75,
//   12.5/87.5；
//   majorIntervalCount=2 → 主次分流（Contours.ts:62——
//   `fract((|coord|+0.5)/2) < 0.5`：|coord|=0/2→主[红]、1/3→副[绿]）→
//   红带行 25/50/75、绿带行 12.5/37.5/62.5/87.5。
// 判据（§5(g)/§11.11——含量值与 WHERE[行带位置+主副色序]断言）：
//   1. 关断基线：全面纯蓝、零红零绿（参考默认 displayContours=false）。
//   2. 开启：红带出现在行 25/50/75、绿带在行 37.5/62.5（+12.5/87.5），带间
//      行（44=两线中点）保持蓝（showGeometry=true 混合臂 Contours.ts:96-98）。
//   3. showGeometry=false：带间 discard（:91-95）→ 带间行落背景黑，线带仍色。
//   4. 关断可逆：恢复基线逐位。
//
// Authored: no reference test exists in itwinjs-core for offscreen contour
//           rendering（§5(g)——DTA 演示面=面板手调；行为锚=Contours.ts/
//           ContourUniforms.ts 逐行 + 本文件头推导）。
#include <gtest/gtest.h>

#include "platform/PlatformFactory.h"   // createPlatform（平台无关工厂）
#include "dqRender/rhi/DriverEnums.h"
#include "rhi/opengl/GlLoader.h"        // Windows: dqgl::init 运行时符号装载
#include "render/Batch.h"
#include "render/Graphic.h"
#include "render/OpenGLRenderSystem.h"
#include "render/PolyfaceGraphic.h"
#include "dqRender/RenderTarget.h"
#include "render/TechniqueImpl.h"       // Techniques
#include "render/TechniqueRegistry.h"   // createDefaultTechniques

#include <dqBase/DqId.h>
#include <dqCommon/ContourDisplay.h>
#include <dqCommon/FeatureTable.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace dqRender;

namespace {

// 离屏 GL 环境（RenderSmokeTest.cpp OffscreenRenderEnv 同形——bluegl 符号解析
// 失败=无 GPU，测试整体跳过[既有族语义]）。
struct ContourOffscreenEnv {
    std::unique_ptr<rhi::Platform> platform;
    std::unique_ptr<OpenGLRenderSystem> system;
    rhi::Driver* driver = nullptr;

    bool init() {
#if defined(_WIN32)
        if (!dqgl::init())
            return false;
#endif
        platform.reset(rhi::createPlatform());
        if (!platform)
            return false;
        rhi::DriverConfig config;
        driver = platform->createDriver(nullptr, config);
        if (!driver)
            return false;
        system = std::make_unique<OpenGLRenderSystem>(std::unique_ptr<rhi::Driver>(driver));
        system->setTechniques(createDefaultTechniques(*driver).release());
        return true;
    }
};

// 侧视 target：相机 (0,-5,0) 朝 +Y、up=+Z，ortho 视域 x∈[-1,1]/z∈[-1,1]/
// 深 [0,10]（y∈[-5,5]）。setViewportTransform + plan.frustum 双通道一致
//（投影唯一来源=changeRenderPlan→changeFrustum[plan.frustum]——RenderSmokeTest
//  blackBackgroundPlan 注；worldToNdc 供 canvas/装饰面）。
std::unique_ptr<RenderTarget> makeSideViewTarget(ContourOffscreenEnv& env) {
    if (!env.init())
        return nullptr;
    auto target = env.system->createTarget(nullptr, 100, 100);
    if (!target)
        return nullptr;
    // 视矩阵（世界→视）：x_v=x_w，y_v=z_w，z_v=-(y_w+5)（列主序）。
    float const view[16] = {1, 0, 0, 0,
                            0, 0, -1, 0,
                            0, 1, 0, 0,
                            0, 0, -5.0f, 1};
    // worldToNdc：x'=x，y'=z，z'=0.2*y（深=-z_v=y+5→z_ndc=2*深/10-1=0.2y）。
    float const worldToNdc[16] = {1, 0, 0, 0,
                                  0, 0, 0.2f, 0,
                                  0, 1, 0, 0,
                                  0, 0, 0, 1};
    target->setViewportTransform(view, worldToNdc);
    return target;
}

// 侧视黑背景 plan（灯光隔离同 RenderSmokeTest——litColor==rgb 精确）。
RenderPlan sideViewPlan() {
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;  // 不透明黑
    plan.lights.solar.intensity = 0.0;
    plan.lights.portraitIntensity = 0.0;
    plan.lights.specularIntensity = 0.0;
    plan.lights.hemisphere.intensity = 0.0;
    plan.lights.ambient.intensity = 1.0;
    plan.lights.ambient.color = dqCommon::RgbColor{0, 0, 0};
    plan.is3d = true;
    plan.fraction = 1.0;
    // frustum 8 角（世界系）：左/右=x∓1（视 x）、下/上=z∓1（视 y=世界 z）、
    // 前（近）y=-5 / 后（远）y=+5。
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomRear)    = dqGeom::Point3d::From(-1,  5, -1);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomRear)   = dqGeom::Point3d::From( 1,  5, -1);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopRear)       = dqGeom::Point3d::From(-1,  5,  1);
    plan.frustum.getCorner(dqCommon::Npc::RightTopRear)      = dqGeom::Point3d::From( 1,  5,  1);
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomFront)   = dqGeom::Point3d::From(-1, -5, -1);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomFront)  = dqGeom::Point3d::From( 1, -5, -1);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopFront)      = dqGeom::Point3d::From(-1, -5,  1);
    plan.frustum.getCorner(dqCommon::Npc::RightTopFront)     = dqGeom::Point3d::From( 1, -5,  1);
    return plan;
}

// 等高线设置：单默认组（空 subCategories——Contours.ts:85-86 的 defaultNdx
// 切换面）+ 主线红/副线绿 + minorInterval=0.25 + majorIntervalCount=2。
dqCommon::ContourDisplay makeContours(bool display, bool showGeometry) {
    dqCommon::ContourDisplay d;
    d.displayContours = display;
    dqCommon::ContourGroup g;  // 默认组
    g.contourDef.minorInterval = 0.25;
    g.contourDef.majorIntervalCount = 2;
    g.contourDef.showGeometry = showGeometry;
    g.contourDef.majorStyle.color = dqCommon::RgbColor(255, 0, 0);  // 主线红
    g.contourDef.minorStyle.color = dqCommon::RgbColor(0, 255, 0);  // 副线绿
    d.groups.push_back(g);
    return d;
}

// 单特征 Batch 包裹的蓝盒（x∈[-0.6,0.6]，y∈[-0.3,0.3]，z∈[-0.9,0.9]——
// 前表面 y=-0.3 朝相机；屏幕 x∈[20,80]、行∈[5,95]）。
std::unique_ptr<Batch> makeBoxBatch(ContourOffscreenEnv& env) {
    dqGeom::Point3d const b1(-0.6, -0.3, -0.9), b2(0.6, -0.3, -0.9),
                          b3(0.6, 0.3, -0.9), b4(-0.6, 0.3, -0.9);
    dqGeom::Point3d const t1(-0.6, -0.3, 0.9), t2(0.6, -0.3, 0.9),
                          t3(0.6, 0.3, 0.9), t4(-0.6, 0.3, 0.9);
    auto box = dqGeom::PolyfaceBuilder::create();
    {
        dqGeom::Point3d const q1[4] = {b4, b3, b2, b1};
        dqGeom::Point3d const q2[4] = {t1, t2, t3, t4};
        dqGeom::Point3d const q3[4] = {b1, b2, t2, t1};
        dqGeom::Point3d const q4[4] = {b2, b3, t3, t2};
        dqGeom::Point3d const q5[4] = {b3, b4, t4, t3};
        dqGeom::Point3d const q6[4] = {b4, b1, t1, t4};
        box->AddQuadFacet(q1); box->AddQuadFacet(q2); box->AddQuadFacet(q3);
        box->AddQuadFacet(q4); box->AddQuadFacet(q5); box->AddQuadFacet(q6);
    }
    // 蓝填充（PolyfaceGraphic defaultColor 打包=(a<<24)|(b<<16)|(g<<8)|r
    // [PolyfaceGraphic.cpp:57-64——TD-17 修正后的字节序] → 不透明蓝 =
    // 0xFFFF0000；红=0xFF0000FF[RenderSmokeTest 钉值]）。
    auto* geom = new PolyfaceGraphic(*env.driver, *box->ClaimPolyface(),
                                     /*defaultColor=*/0xFFFF0000u);
    auto* prim = new Primitive(geom);

    // FeatureTable(1)：特征 elementId=42 / subCategoryId=0x10（默认组空
    // subCategories 兜住一切[defaultNdx 切换]——组映射非本锁变量面）。
    auto featureTable = std::make_unique<dqCommon::FeatureTable>(1);
    featureTable->insert(dqCommon::Feature(dqBase::DqId(42), dqBase::DqId(0x10),
                                           dqCommon::GeometryClass::Primary));
    auto batch = std::make_unique<Batch>(1, std::move(featureTable));
    batch->setChild(std::unique_ptr<Graphic>(prim));
    return batch;
}

// 读列（屏幕 x=50——盒中央）的全行 RGB（GL 原点在左下：row 0=底）。
std::vector<std::array<uint8_t, 3>> readColumn(RenderTarget& target, uint32_t x) {
    std::vector<uint8_t> px(100 * 4);
    bool const ok = target.readPixels(x, 0, 1, 100, px);
    std::vector<std::array<uint8_t, 3>> col(100, {0, 0, 0});
    if (!ok)
        return col;
    for (uint32_t row = 0; row < 100; ++row)
        col[row] = {px[row * 4 + 0], px[row * 4 + 1], px[row * 4 + 2]};
    return col;
}

bool colorNear(std::array<uint8_t, 3> const& c, uint8_t r, uint8_t g, uint8_t b,
               uint8_t tol = 40) {
    return std::abs(int(c[0]) - int(r)) <= tol
        && std::abs(int(c[1]) - int(g)) <= tol
        && std::abs(int(c[2]) - int(b)) <= tol;
}

// 行带判据：bandRows 中至少一行在近色（抗 ±1px AA 抖动——线心 .5 行）。
void expectBandHits(std::vector<std::array<uint8_t, 3>> const& col,
                    std::vector<uint32_t> const& bandRows,
                    uint8_t r, uint8_t g, uint8_t b, char const* what) {
    bool hit = false;
    for (uint32_t row : bandRows) {
        for (uint32_t dr = 0; dr <= 2; ++dr) {  // 线心 ±1 行窗
            if (row + dr < 100 && row >= dr && colorNear(col[row + dr], r, g, b)) { hit = true; break; }
            if (row >= dr && colorNear(col[row - dr], r, g, b)) { hit = true; break; }
        }
        if (hit) break;
    }
    EXPECT_TRUE(hit) << what << " — no colored line pixel in the expected rows";
}

}  // namespace

// 开关 + 线带 WHERE + showGeometry discard 臂 + 可逆（四段一锁——场景/相机
// 不变量共享；段序=基线→开启→discard 臂→关断恢复）。
TEST(ContourDisplayE2ETest, SideViewRendersContourBandsWhereExpected) {
    ContourOffscreenEnv env;
    auto target = makeSideViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    Scene scene;
    scene.foreground.push_back(makeBoxBatch(env).release());
    target->changeScene(scene);

    // ── 段 1：基线（plan 无 contours——RenderPlan.ts:128 的组空门等价面）──
    target->changeRenderPlan(sideViewPlan());
    target->drawFrame();
    auto const baseline = readColumn(*target, 50);
    ASSERT_TRUE(colorNear(baseline[50], 0, 0, 255)) << "baseline: box face blue at center";
    bool anyRed = false, anyGreen = false;
    for (auto const& c : baseline) {
        anyRed = anyRed || colorNear(c, 255, 0, 0);
        anyGreen = anyGreen || colorNear(c, 0, 255, 0);
    }
    EXPECT_FALSE(anyRed) << "baseline: no contour red";
    EXPECT_FALSE(anyGreen) << "baseline: no contour green";

    // ── 段 2：开启（displayContours=true, showGeometry=true）──
    auto planOn = sideViewPlan();
    planOn.contours = makeContours(true, true);
    target->changeRenderPlan(planOn);
    target->drawFrame();
    auto const on = readColumn(*target, 50);

    // 主线红带 WHERE：z=0,±0.5 → 行 50, 25, 75。
    expectBandHits(on, {50}, 255, 0, 0, "major contour band at row 50 (z=0)");
    expectBandHits(on, {25}, 255, 0, 0, "major contour band at row 25 (z=-0.5)");
    expectBandHits(on, {75}, 255, 0, 0, "major contour band at row 75 (z=+0.5)");
    // 副线绿带 WHERE：z=±0.25 → 行 37.5/62.5。
    expectBandHits(on, {37, 38}, 0, 255, 0, "minor contour band near row 37-38 (z=-0.25)");
    expectBandHits(on, {62, 63}, 0, 255, 0, "minor contour band near row 62-63 (z=+0.25)");
    // 带间行保持蓝（showGeometry=true 的 mix 臂——Contours.ts:97-98；
    // 行 44：z=-0.12 → coord=-0.48，fract(-0.98)=0.02 → line≈0.03/0.0667≈0.45
    // <lineRadius？——线宽扩窗内仍可能混色；取更远的行 46：z=-0.08 coord=-0.32
    // fract(-0.82)=0.18 → line=2.7 ≥ lineRadius → 不混[主线宽半径 1.5]）。
    EXPECT_TRUE(colorNear(on[46], 0, 0, 255, 60))
        << "between-line row keeps base color (showGeometry=true)";
    EXPECT_TRUE(colorNear(on[90], 0, 0, 255, 60))
        << "row 90 (z=+0.8, midway between z=0.75/1.0 lines) keeps base color";

    // ── 段 3：showGeometry=false——带间 discard 臂（Contours.ts:91-95）──
    auto planNoGeom = sideViewPlan();
    planNoGeom.contours = makeContours(true, false);
    target->changeRenderPlan(planNoGeom);
    target->drawFrame();
    auto const noGeom = readColumn(*target, 50);
    // 带间 discard → 背景黑（不透明黑 plan 背景）。
    EXPECT_TRUE(colorNear(noGeom[46], 0, 0, 0, 30))
        << "showGeometry=false: between-line row discards to background";
    EXPECT_TRUE(colorNear(noGeom[90], 0, 0, 0, 30))
        << "showGeometry=false: row 90 discards to background";

    // 线带仍着色（discard 只弃非线片元）。
    expectBandHits(noGeom, {50}, 255, 0, 0, "discard arm keeps the major line at row 50");
    expectBandHits(noGeom, {37, 38}, 0, 255, 0, "discard arm keeps the minor line near row 37-38");

    // ── 段 4：关断可逆（displayContours=false——wantContourLines 门落
    // [BatchUniforms.ts:110-113]，u_contourLUTWidth=0 早退[Contours.ts:17-18]）──
    auto planOff = sideViewPlan();
    planOff.contours = makeContours(false, true);
    target->changeRenderPlan(planOff);
    target->drawFrame();
    auto const off = readColumn(*target, 50);
    EXPECT_TRUE(colorNear(off[50], 0, 0, 255)) << "toggle-off restores base color at center";
    bool offRed = false, offGreen = false;
    for (auto const& c : off) {
        offRed = offRed || colorNear(c, 255, 0, 0);
        offGreen = offGreen || colorNear(c, 0, 255, 0);
    }
    EXPECT_FALSE(offRed) << "toggle-off: no contour red remains";
    EXPECT_FALSE(offGreen) << "toggle-off: no contour green remains";
}
