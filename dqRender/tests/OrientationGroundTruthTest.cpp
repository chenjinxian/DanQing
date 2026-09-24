// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 方向真值测试（非对称视口 + Front 相机的世界轴落点断言）
//
// Authored: no reference test exists in itwinjs-core asserting world-axis screen
//           placement per standard view (openBlankViewport only asserts color
//           sets); camera/matrix conventions transcribed from ViewingSpace
//           (buildAffineViewMatrix column layout) + NdcFromNpc (y-up NDC).
//
// 背景（systematic-debugging 2026-09-11）：8 向标准视图与 DTA 视觉不一致。
// CPU 侧已数值验证全部正确（StandardViewTool 旋转矩阵、worldToNdc、
// WorldToView、视锥角点）；既有像素测试用 100×100 对称断言，对全局 y 翻转
// 天然失明。本测试用 160×120 非对称视口 + Front 正交相机：
//   红线 = 世界 +X 轴段 → 应为水平线、贯穿垂直中部（screen y≈60）
//   蓝线 = 世界 +Z 轴段 → 应为垂直线、靠近顶部（rowFromTop≈10）
// 任何 NDC-y 翻转（蓝线落底）、矩阵转置（红线变竖）都会使断言失败。

#include <gtest/gtest.h>

#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/Scene.h"
#include "dqRender/RenderPlan.h"
#include "dqRender/GraphicBuilder.h"

#include "render/OpenGLRenderSystem.h"
#include "render/TechniqueImpl.h"
#include "render/TechniqueRegistry.h"
#include "platform/PlatformFactory.h"
#include "rhi/opengl/GlLoader.h"

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point3d.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace dqRender;

#if defined(_WIN32)
extern "C" BOOL WINAPI zoglInitShim();
#endif

namespace {

struct OrientationEnv {
    std::unique_ptr<rhi::OpenGLPlatform> platform;
    std::unique_ptr<OpenGLRenderSystem> system;
    rhi::Driver* driver = nullptr;

    bool init() {
#if defined(_WIN32)
        if (!zogl::init())
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
        return true;   // setTechniques 为 void（RenderSystem.h:43）；空手返回即认为就绪
    }
};

// readPixels 为 GL 坐标（原点左下、逐行向上）；rowFromTop 语义同
// CanvasDecorationRenderTest::pixelAt。
uint8_t const* pixelAt(std::vector<uint8_t> const& px, uint32_t x, uint32_t rowFromTop,
                       uint32_t w, uint32_t h) {
    return &px[(static_cast<size_t>(h - 1 - rowFromTop) * w + x) * 4];
}

bool isColorish(uint8_t const* p, uint8_t r, uint8_t g, uint8_t b, int tol = 60) {
    return std::abs(int(p[0]) - r) <= tol && std::abs(int(p[1]) - g) <= tol &&
           std::abs(int(p[2]) - b) <= tol;
}

}  // namespace

// Authored: 方向真值——Front 正交相机下世界轴的屏幕落点（见文件头）。
TEST(OrientationGt, FrontViewWorldAxesLandCorrectly)
{
    OrientationEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL unavailable";

    constexpr uint32_t kW = 160, kH = 120;   // 非对称：任何 y 翻转必暴露
    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    // plan.frustum = Front 视图盒（ef247b8 后投影唯一来源 =
    // changeRenderPlan→changeFrustum(plan.frustum)，参考 Target.ts:534；plan
    // 默认 is3d=true+fraction=0+单位盒走透视分支 → l/r/b/t 全 ×0 → 除零 NaN →
    // 0 fragments）。eye 系盒 (±800,±600,±1000) 经 Rᵀ 转世界：w=(v0,−v2,v1)——
    // Front 面(v2=+1000)→w y=−1000、Rear→+1000；正交 fraction=1.0（参考
    // ViewingSpace.frustFraction 正交恒 1）。ortho 分支算得 ortho(±800,±600,0,
    // 2000)，x/y 投影与 mvp 的 diag(1/800,1/600) 一致；viewY=+Z_世界（世界 +Z
    // → NDC y 正 → 屏幕顶，蓝线断言的方向依据）。
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    plan.fraction = 1.0;
    plan.frustum.points[0] = dqGeom::Point3d::From(-800, 1000, -600);  // LBR
    plan.frustum.points[1] = dqGeom::Point3d::From(800, 1000, -600);   // RBR
    plan.frustum.points[2] = dqGeom::Point3d::From(-800, 1000, 600);   // LTR
    plan.frustum.points[3] = dqGeom::Point3d::From(800, 1000, 600);    // RTR
    plan.frustum.points[4] = dqGeom::Point3d::From(-800, -1000, -600); // LBF
    plan.frustum.points[5] = dqGeom::Point3d::From(800, -1000, -600);  // RBF
    plan.frustum.points[6] = dqGeom::Point3d::From(-800, -1000, 600);  // LTF
    plan.frustum.points[7] = dqGeom::Point3d::From(800, -1000, 600);   // RTF
    target->changeRenderPlan(plan);

    // Front 旋转（StandardView.ts:41）：rows (1,0,0),(0,0,1),(0,-1,0)。
    // 视体：eye x∈[-800,800]、eye y∈[-600,600]（对称世界盒，视心=世界原点）。
    // mv（列主序）：列 j = R 的列 j（ViewingSpace::buildAffineViewMatrix 布局）：
    //   col0=(1,0,0,0) col1=(0,0,-1,0) col2=(0,1,0,0) col3=(0,0,0,1)
    float const mv[16] = {
        1, 0,  0, 0,
        0, 0, -1, 0,
        0, 1,  0, 0,
        0, 0,  0, 1,
    };
    // mvp = P·R（行主序推导后转列主序）：P=diag(1/800,1/600,1/1000)。
    // 行主序 mvp = [1/800,0,0,0; 0,0,1/600,0; 0,-1/1000,0,0; 0,0,0,1]
    // → 列主序（out[col*4+row]）：
    float const mvp[16] = {
        1.0f / 800.0f, 0.0f,           0.0f,            0.0f,   // col0
        0.0f,          0.0f,          -1.0f / 1000.0f,  0.0f,   // col1
        0.0f,          1.0f / 600.0f,  0.0f,            0.0f,   // col2
        0.0f,          0.0f,           0.0f,            1.0f,   // col3
    };
    target->setViewportTransform(mv, mvp);

    // 红线：世界 X 轴段 (-500,0,0)→(500,0,0)。eye x=±500 → NDC x=±0.625 →
    // 屏幕 x∈[30,130]，y=中部。蓝线：世界 Z 轴段 (0,0,-500)→(0,0,500)。
    // eye y=±500 → NDC y=±0.833 → 顶部/底部 10px 内。
    auto makeLine = [&env](dqGeom::Point3d a, dqGeom::Point3d b,
                           uint8_t r, uint8_t g, uint8_t bch) -> RenderGraphic* {
        GraphicBuilderOptions options;
        options.computeChordTolerance = [] { return 0.01; };
        auto builder = env.system->createGraphicBuilder(options);
        if (!builder) return nullptr;
        builder->setSymbology(dqCommon::ColorDef::from(r, g, bch),
                              dqCommon::ColorDef::from(r, g, bch), 3);
        dqGeom::Point3d const pts[2] = {a, b};
        builder->addLineString(pts, 2);
        return builder->finish();
    };
    auto* red = makeLine(dqGeom::Point3d::From(-500, 0, 0), dqGeom::Point3d::From(500, 0, 0),
                         255, 0, 0);
    auto* blue = makeLine(dqGeom::Point3d::From(0, 0, -500), dqGeom::Point3d::From(0, 0, 500),
                          0, 90, 255);
    ASSERT_NE(red, nullptr);
    ASSERT_NE(blue, nullptr);
    Scene scene;
    scene.foreground = {red, blue};
    target->changeScene(scene);

    target->drawFrame();

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));

    // 红线（世界 X）：水平、垂直中部。x=110 处 y≈60±5 必须红；y=20/100 不红。
    EXPECT_TRUE(isColorish(pixelAt(px, 110, 60, kW, kH), 255, 0, 0)) << "world +X must cross screen right-half at vertical center";
    EXPECT_FALSE(isColorish(pixelAt(px, 110, 20, kW, kH), 255, 0, 0)) << "world X must NOT be vertical";
    EXPECT_FALSE(isColorish(pixelAt(px, 110, 100, kW, kH), 255, 0, 0)) << "world X must NOT be vertical";

    // 蓝线（世界 Z）：垂直、x=80±5；顶端（rowFromTop≈10±6）必须蓝——
    // 全局 y 翻转会让蓝落到底部（rowFromTop≈110），此断言即翻。
    EXPECT_TRUE(isColorish(pixelAt(px, 80, 10, kW, kH), 0, 90, 255)) << "world +Z must land near screen TOP (NDC y-up)";
    EXPECT_TRUE(isColorish(pixelAt(px, 80, 15, kW, kH), 0, 90, 255)) << "world +Z near top";
    EXPECT_FALSE(isColorish(pixelAt(px, 80, 110, kW, kH), 0, 90, 255)) << "world +Z must NOT be at bottom (flip detector)";
}
