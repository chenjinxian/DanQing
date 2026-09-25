// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — PlanarGrid 经真实合成器的像素可见性测试
//
// Authored: no reference test exists in itwinjs-core for planar-grid rendering
//           (openBlankViewport asserts color sets only); scenario transcribes
//           ViewContext.ts:341-352 drawStandardGrid → PlanarGridGeometry.create
//           (frustum∩plane polygon + gridsPerRef emphasis) + glsl/PlanarGrid.ts.
//
// 背景（systematic-debugging 2026-09-11）：用户实测勾选 Grid 后初始视图不可见、
// 放大多次才部分出现。本测试用已知相位网格（spacing=40, gridsPerRef=10 →
// ref 线每 400 世界单位 = 每 40px）断言规则间隔的线像素在回读中出现。

#include <gtest/gtest.h>

#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/Scene.h"
#include "dqRender/RenderPlan.h"
#include "dqRender/PlanarGridProps.h"

#include "render/OpenGLRenderSystem.h"
#include "render/TechniqueImpl.h"
#include "render/TechniqueRegistry.h"
#include "render/PlanarGridGraphic.h"   // TEMP-DIAG: BuildPlanarGridPolygon CPU 单测
#include "platform/PlatformFactory.h"
#include "rhi/opengl/GlLoader.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/Frustum.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace dqRender;

namespace {

struct GridEnv {
    std::unique_ptr<rhi::OpenGLPlatform> platform;
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

uint8_t const* pixelAt(std::vector<uint8_t> const& px, uint32_t x, uint32_t rowFromTop,
                       uint32_t w, uint32_t h) {
    return &px[(static_cast<size_t>(h - 1 - rowFromTop) * w + x) * 4];
}

}  // namespace

// Authored: Top 正交相机下，spacing=40/gridsPerRef=10 的网格应在回读中呈现
// 每 40px 一条 ref 线（世界 400 单位）。断言线位与线间像素的亮度差。
TEST(PlanarGridRender, TopViewGridLinesVisibleAtRegularInterval)
{
    GridEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL unavailable";

    constexpr uint32_t kW = 160, kH = 120;
    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    // 视锥角点（世界）＝ortho 盒 8 角（Npc 序：0=LBR..）
    //（先于 changeRenderPlan 构造——plan.frustum 是 ef247b8 后投影的唯一
    // 来源（changeRenderPlan→changeFrustum(plan.frustum)，参考 Target.ts:534）；
    // plan 默认 is3d=true+fraction=0+单位盒走透视分支 → l/r/b/t 全 ×0 →
    // frustumProjection 除零 → u_proj=NaN → 0 fragments。正交 fraction=1.0
    //（参考 ViewingSpace.frustFraction 默认，正交恒 1）。）
    dqCommon::Frustum fr;
    fr.initNpc();
    // 直接手写 8 角（Npc 序，Npc.h：0=LBR 1=RBR 2=LTR 3=RTR 4=LBF 5=RBF 6=LTF 7=RTF）
    fr.points[0] = dqGeom::Point3d::From(-800, -600, -1000);  // LBR（rear=远=低 z）
    fr.points[1] = dqGeom::Point3d::From(800, -600, -1000);   // RBR
    fr.points[2] = dqGeom::Point3d::From(-800, 600, -1000);   // LTR
    fr.points[3] = dqGeom::Point3d::From(800, 600, -1000);    // RTR
    fr.points[4] = dqGeom::Point3d::From(-800, -600, 1000);   // LBF（front=近=高 z）
    fr.points[5] = dqGeom::Point3d::From(800, -600, 1000);    // RBF
    fr.points[6] = dqGeom::Point3d::From(-800, 600, 1000);    // LTF
    fr.points[7] = dqGeom::Point3d::From(800, 600, 1000);     // RTF

    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    plan.fraction = 1.0;
    plan.frustum = fr;
    target->changeRenderPlan(plan);

    // Top 正交相机（俯视，对齐 blank 初始视图）：世界 x∈[-800,800]、y∈[-600,600]、
    // z(view)∈[-1000,1000]。世界 z=0 地面在视锥中部。eye = world（rotation=I）。
    float const view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -0.0f, 1};
    float const worldToNdc[16] = {
        1.0f / 800.0f, 0, 0, 0,
        0, 1.0f / 600.0f, 0, 0,
        0, 0, 1.0f / 1000.0f, 0,
        0, 0, 0, 1};
    target->setViewportTransform(view, worldToNdc);

    PlanarGridProps props;
    props.origin = dqGeom::Point3d::From(0, 0, 0);
    props.rMatrix = dqGeom::Matrix3d::CreateIdentity();
    props.spacing = dqGeom::Point2d{40.0, 40.0};
    props.gridsPerRef = 10.0;
    props.color = dqCommon::ColorDef::from(60, 60, 60);   // 暗灰：避免面饱和淹没线差异

    auto* grid = env.system->createPlanarGrid(fr, props);
    ASSERT_NE(grid, nullptr);
    Scene scene;
    scene.foreground = {grid};
    target->changeScene(scene);
    target->drawFrame();

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));

    // 世界 x=0,±400 → NDC x=0,±0.5 → 屏幕 x=80,40,120（ref 线每 40px）。
    // OIT 合成语义（Composite.ts:112-113 over）：线位 alpha 叠加更高 → 合成
    // 权重 (1−ta) 更低 → 黑背景上线比面暗、白背景上线比面亮（DTA 截图同样：
    // 天空背景上网格线呈对比暗色）。断言 |线-面| 差异显著且网格可见。
    auto lum = [&](uint32_t x, uint32_t y) {
        uint8_t const* p = pixelAt(px, x, y, kW, kH);
        return (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
    };
    // 全行扫描：线（ref/minor）与格中间的最小/最大亮度差。手工取单点易踩线位
    //（uv 整数=格中间、.5=线，屏幕↔世界换算易错；此前 x=100 恰在次线上）。
    // y=54（世界 y=−60 → uv=−1.5 → 格中间）：y=60 恰是水平网格线（uv.y=0
    // → grid.y=0 → min 恒 0 → 整行都在线上，行扫描退化为常数）。
    int minLum = 255, maxLum = 0;
    for (uint32_t x = 8; x < kW - 8; ++x) {
        int l = lum(x, 54);
        minLum = std::min(minLum, l);
        maxLum = std::max(maxLum, l);
    }
    printf("[GRIDDIAG] row min=%d max=%d\n", minLum, maxLum);
    // 期望值由参考公式推导（WBOIT 全链：ClearTranslucent (0,0,0,1)/(1,0,0,1) +
    // _translucentRenderState blend (srcRGB=One,srcAlpha=Zero,dstRGB=One,
    // dstAlpha=OneMinusSrcAlpha)（SceneCompositor.ts:1268，签名序 srcRgb,srcAlpha,
    // dstRgb,dstAlpha）+ computeTranslucentColor over 合成（Composite.ts:106-116)）：
    //   黑背景上 col = (1-Π(1-ai))·(60/255)·255 ≈ ai·60：
    //     面（planeTransparency=0.9 → ai=0.1）≈ 6/255 —— 淡但非零；
    //     次线（lineTransparency=0.75 → ai=0.25）≈ 15；ref 线（0.5）≈ 30。
    // （旧断言 min>25 标定在被修复前的错误链上——RGB 替换混合 + accum 清 alpha=0，
    //   见 2026-09-12 OIT 链修复。）
    EXPECT_GE(minLum, 5) << "grid plane must be visible against black bg (ai=0.1 -> ~6/255)";
    // 线/面亮度差（次线 ~15 对面 ~6 → 差 ≥ 8；量化 UV（PlanarGrid.ts:70-77，
    // 参考 1:1）后线位含 1/65535 精度偏移，差值恰为 8 属量化容差内）。
    EXPECT_GE(maxLum - minLum, 8)
        << "grid lines must differ from plane (min=" << minLum << " max=" << maxLum << ")";
}

// TEMP-DIAG（2026-09-14 初始网格间距排查）：真实 blank-connection 比例的间距测量。
// 比例复刻真实视口：2560 物理 px / 2000 units = 1.28 px/unit；视锥 y 按真实
// aspect（1057.8/2000）。spacing=1、gridsPerRef=10 → minor 1.28px（亚像素）、
// ref 12.8px。断言：x 与 y 两方向都出现 ~12.8px 周期的 ref 线（行/列剖面的
// 自相关峰），且周期不偏离 12.8 的 ±50%。真实 app 实测 bug：x 方向无线、
// y 方向周期 ~39px（≈3×）。
TEST(PlanarGridRender, BlankConnectionSpacing1PitchIsReferenceAccurate)
{
    GridEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL unavailable";

    // 512x271 视口 = 真实 2560x1354 的 1/5（比例保真：px/unit 不变）
    constexpr uint32_t kW = 512, kH = 271;
    // 世界范围：x 400 units（512px/1.28）、y 211.56（271/1.28）——中心对齐原点
    double const kHalfX = 200.0;
    double const kHalfY = 211.56 / 2.0;
    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    // 视锥角点先于 changeRenderPlan（plan.frustum = 投影唯一来源，同上例）。
    dqCommon::Frustum fr;
    fr.initNpc();
    fr.points[0] = dqGeom::Point3d::From(-kHalfX, -kHalfY, -1000);
    fr.points[1] = dqGeom::Point3d::From(kHalfX, -kHalfY, -1000);
    fr.points[2] = dqGeom::Point3d::From(-kHalfX, kHalfY, -1000);
    fr.points[3] = dqGeom::Point3d::From(kHalfX, kHalfY, -1000);
    fr.points[4] = dqGeom::Point3d::From(-kHalfX, -kHalfY, 1000);
    fr.points[5] = dqGeom::Point3d::From(kHalfX, -kHalfY, 1000);
    fr.points[6] = dqGeom::Point3d::From(-kHalfX, kHalfY, 1000);
    fr.points[7] = dqGeom::Point3d::From(kHalfX, kHalfY, 1000);

    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    plan.fraction = 1.0;
    plan.frustum = fr;
    target->changeRenderPlan(plan);

    float const view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -0.0f, 1};
    float const worldToNdc[16] = {
        static_cast<float>(1.0 / kHalfX), 0, 0, 0,
        0, static_cast<float>(1.0 / kHalfY), 0, 0,
        0, 0, 0.001f, 0,
        0, 0, 0, 1};
    target->setViewportTransform(view, worldToNdc);

    PlanarGridProps props;
    props.origin = dqGeom::Point3d::From(0, 0, 0);
    props.rMatrix = dqGeom::Matrix3d::CreateIdentity();
    props.spacing = dqGeom::Point2d{1.0, 1.0};
    props.gridsPerRef = 10.0;
    props.color = dqCommon::ColorDef::from(60, 60, 60);

    auto* grid = env.system->createPlanarGrid(fr, props);
    ASSERT_NE(grid, nullptr);
    Scene scene;
    scene.foreground = {grid};
    target->changeScene(scene);
    target->drawFrame();

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));
    auto lum = [&](uint32_t x, uint32_t y) {
        uint8_t const* p = pixelAt(px, x, y, kW, kH);
        return (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
    };

    // 行/列剖面。采样行避开网格线（y=kH/2 的世界 y=0 恰是线位——整行恒定
    // alpha 无 x 向周期；spacing40 测试注释的同类坑）。取 kH/2+7（世界 y≈5.46，
    // 非整数格）；列取 kW/2+3（世界 x≈2.34 非整数）。
    std::vector<int> row, col;
    for (uint32_t x = 4; x < kW - 4; ++x) row.push_back(lum(x, kH / 2 + 7));
    for (uint32_t y = 4; y < kH - 4; ++y) col.push_back(lum(kW / 2 + 3, y));
    auto peakGaps = [](std::vector<int> const& v) {
        std::vector<int> gaps;
        int mean = 0;
        for (int x : v) mean += x;
        mean /= static_cast<int>(v.size());
        int prevPeak = -100;
        for (size_t i = 1; i + 1 < v.size(); ++i) {
            bool const isPeak = v[i] > v[i - 1] && v[i] >= v[i + 1] && (v[i] - mean) > 2;
            if (isPeak) {
                if (prevPeak > 0 && static_cast<int>(i) - prevPeak > 1)
                    gaps.push_back(static_cast<int>(i) - prevPeak);
                prevPeak = static_cast<int>(i);
            }
        }
        return gaps;
    };
    auto rowGaps = peakGaps(row);
    auto colGaps = peakGaps(col);
    printf("[PITCHDIAG] row(x-dir) gaps:");
    for (int g : rowGaps) printf(" %d", g);
    printf(" | col(y-dir) gaps:");
    for (int g : colGaps) printf(" %d", g);
    printf("\n");
    // ref 线周期 = 10 units · (512px/400units) = 12.8px。两方向都应出现 12.8±4
    // 的周期峰（修复前：x 方向零峰、y 方向 24/49 错乱——量化 origin 用了取模值）。
    auto medianGap = [](std::vector<int> const& gaps) {
        if (gaps.empty()) return 0;
        std::vector<int> s(gaps);
        std::sort(s.begin(), s.end());
        return s[s.size() / 2];
    };
    int const rowMed = medianGap(rowGaps);
    int const colMed = medianGap(colGaps);
    EXPECT_GT(rowGaps.size(), 4u) << "x-direction grid lines must be visible";
    EXPECT_GT(colGaps.size(), 4u) << "y-direction grid lines must be visible";
    EXPECT_NEAR(rowMed, 13, 4) << "x-direction ref-line pitch ~12.8px, got " << rowMed;
    EXPECT_NEAR(colMed, 13, 4) << "y-direction ref-line pitch ~12.8px, got " << colMed;
}

// TEMP-DIAG（2026-09-14 间距排查）：CPU 端直接检查 BuildPlanarGridPolygon 的
// 多边形/UV/量化参数（无 GL）。期望：矩形 4 顶点、UV 跨度 = 世界跨度/spacing、
// q∈[0,1] 且 4 角对应 (0/1, 0/1)、qTexCoordParams=(fmod(min,10), extent)。
TEST(PlanarGridRender, BuildPolygonUvSanity)
{
    dqCommon::Frustum fr;
    fr.initNpc();
    double const hx = 200.0, hy = 105.78;
    fr.points[0] = dqGeom::Point3d::From(-hx, -hy, -1000);
    fr.points[1] = dqGeom::Point3d::From(hx, -hy, -1000);
    fr.points[2] = dqGeom::Point3d::From(-hx, hy, -1000);
    fr.points[3] = dqGeom::Point3d::From(hx, hy, -1000);
    fr.points[4] = dqGeom::Point3d::From(-hx, -hy, 1000);
    fr.points[5] = dqGeom::Point3d::From(hx, -hy, 1000);
    fr.points[6] = dqGeom::Point3d::From(-hx, hy, 1000);
    fr.points[7] = dqGeom::Point3d::From(hx, hy, 1000);

    PlanarGridProps props;
    props.origin = dqGeom::Point3d::From(0, 0, 0);
    props.rMatrix = dqGeom::Matrix3d::CreateIdentity();
    props.spacing = dqGeom::Point2d{1.0, 1.0};
    props.gridsPerRef = 10.0;

    auto polygon = BuildPlanarGridPolygon(fr, props);
    ASSERT_TRUE(polygon.has_value());
    printf("[POLYDIAG] n=%zu q=(%.4f,%.4f,%.4f,%.4f)\n",
           polygon->positions.size(),
           polygon->qTexCoordParams[0], polygon->qTexCoordParams[1],
           polygon->qTexCoordParams[2], polygon->qTexCoordParams[3]);
    for (size_t i = 0; i < polygon->positions.size(); ++i) {
        printf("[POLYDIAG] v%zu pos=(%.2f,%.2f) uv=(%.4f,%.4f)\n", i,
               polygon->positions[i].x, polygon->positions[i].y,
               polygon->uvs[i].x, polygon->uvs[i].y);
    }
    ASSERT_EQ(polygon->positions.size(), 4u);
    // UV 跨度：世界 400×211.56 → 格 400×211.56
    double const expectedExtentX = 400.0;
    EXPECT_NEAR(polygon->qTexCoordParams[2], expectedExtentX, 1e-3);
    EXPECT_NEAR(polygon->qTexCoordParams[3], 211.56, 0.01);
    // 量化 UV 应覆盖 [0,1] 的四角（v0=(-200,-105.78)→(0,0)、
    // v1=(+200,-105.78)→(1,0)、v2=(+200,+105.78)→(1,1)、v3=(-200,+105.78)→(0,1)）。
    EXPECT_NEAR(polygon->uvs[0].x, 0.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[0].y, 0.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[1].x, 1.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[1].y, 0.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[2].x, 1.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[2].y, 1.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[3].x, 0.0, 1e-3);
    EXPECT_NEAR(polygon->uvs[3].y, 1.0, 1e-3);
}



