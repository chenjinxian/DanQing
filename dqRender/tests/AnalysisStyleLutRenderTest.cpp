// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AnalysisStyle LUT 网格离屏渲染复现锁（M-W W-a6 E2E 零内容回修）
//
// Authored: no reference test exists in itwinjs-core for analysis-style animation
//   pixel output（参考的消费面在 display-test-app 交互会话——AnalysisStyleExample
//   无离线回放对应物；渲染像素回归授权 §5(g)）。
//   复现配方（= 真窗 E2E AnalysisStyleE2E.FlatWavesDisplaceUpwardWithFractionAndRecolor
//   的引擎侧同构面，去 Qt/真窗）：auxData 网格（Vector 位移 + Scalar 通道、2 帧）
//   → PrimitiveBuilder::finish 的 auxChannels LUT 路由（createLutMeshGraphic）
//   → Scene.foreground → 离屏 target 的 changeRenderPlan（analysisStyle/fraction
//   载体——OpenGLRenderTarget 桥 → TargetImpl::setAnalysisStyle → analysisTexture）
//   → drawFrame → readPixels 内容断言。
//   证据链钉：M-W E2E 真窗首跑 flatStats.count=0 的根因 = compositor 的
//   s_texture analysisTexture 臂嵌套在 surfTex 非空分支内（Surface.ts:596-609
//   的 useTexture 门内 hasAnimation&&analysisTexture 优先臂——无自有纹理的
//   动画网格在参考侧也绑渐变纹理）→ HasTexture=1（scalar 门已置位）而 unit 0
//   无绑定 → 黑网格 on 黑背景 → count=0。本锁在修复前 RED、修复后 GREEN。

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

#include <dqCommon/AnalysisStyle.h>
#include <dqCommon/ColorDef.h>

#include <dqGeom/AuxData.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Range3d.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <vector>

using namespace dqRender;

namespace {

// 离屏渲染环境（RenderSmokeTest OffscreenRenderEnv 同构最小面——真 WGL driver +
// 全量默认 techniques；Windows 可跑）。
struct AsOffscreenEnv {
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

// 黑背景正交顶视 plan（RenderSmokeTest blackBackgroundPlan 同构——灯光隔离：
// ambient 1.0 黑色 → litColor==rgb 精确；frustum 盒 ±1/depth 10 顶视）。
RenderPlan makeAsPlan() {
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    plan.lights.solar.intensity = 0.0;
    plan.lights.portraitIntensity = 0.0;
    plan.lights.specularIntensity = 0.0;
    plan.lights.hemisphere.intensity = 0.0;
    plan.lights.ambient.intensity = 1.0;
    plan.lights.ambient.color = dqCommon::RgbColor{0, 0, 0};
    plan.is3d = true;
    plan.fraction = 1.0;
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomRear) = dqGeom::Point3d::From(-1, -1, -5);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomRear) = dqGeom::Point3d::From(1, -1, -5);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopRear) = dqGeom::Point3d::From(-1, 1, -5);
    plan.frustum.getCorner(dqCommon::Npc::RightTopRear) = dqGeom::Point3d::From(1, 1, -5);
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomFront) = dqGeom::Point3d::From(-1, -1, 5);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomFront) = dqGeom::Point3d::From(1, -1, 5);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopFront) = dqGeom::Point3d::From(-1, 1, 5);
    plan.frustum.getCorner(dqCommon::Npc::RightTopFront) = dqGeom::Point3d::From(1, 1, 5);
    return plan;
}

// 非背景（非黑）像素计数。
long countNonBlack(std::vector<uint8_t> const& f) {
    long n = 0;
    for (size_t i = 0; i + 3 < f.size(); i += 4) {
        if (f[i] > 24 || f[i + 1] > 24 || f[i + 2] > 24)
            ++n;
    }
    return n;
}

// TEMP-DIAG：帧的唯一色统计（区分"零片元"与"黑片元"——前者全帧纯背景色，
// 后者背景色+(0,0,0,255) 族）。env 门控（DANQING_AS_TRACE——§13.1 族同型）。
void dumpUniqueColors(char const* tag, std::vector<uint8_t> const& f) {
    if (!std::getenv("DANQING_AS_TRACE"))
        return;
    std::vector<std::pair<uint32_t, long>> colors;
    for (size_t i = 0; i + 3 < f.size(); i += 4) {
        uint32_t const key = (static_cast<uint32_t>(f[i]) << 24)
            | (static_cast<uint32_t>(f[i + 1]) << 16)
            | (static_cast<uint32_t>(f[i + 2]) << 8) | f[i + 3];
        bool found = false;
        for (auto& c : colors) {
            if (c.first == key) { ++c.second; found = true; break; }
        }
        if (!found)
            colors.emplace_back(key, 1);
    }
    std::fprintf(stderr, "[ASDIAG] %s uniqueColors=%zu:", tag, colors.size());
    size_t const n = std::min(colors.size(), static_cast<size_t>(12));
    for (size_t i = 0; i < n; ++i)
        std::fprintf(stderr, " #%06x(a=%02x)x%ld", colors[i].first >> 8,
                     colors[i].first & 0xFF, colors[i].second);
    std::fprintf(stderr, "\n");
}

long frameDiffCount(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b) {
    if (a.size() != b.size())
        return -1;
    long n = 0;
    for (size_t i = 0; i + 3 < a.size(); i += 4) {
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2])
            ++n;
    }
    return n;
}

// auxData 网格生成：nDim×nDim 点阵（z=0 平面，[-0.8,0.8]²）+ 位移通道（Vector，
// 2 帧：零场 / +Z 隆起）+ scalar 通道（2 帧：零场 / x+0.8 横向渐变）。
// 数据形态镜像 AnalysisStyleExample.ts createFlatMeshWithWaves（:85-177）的
// 通道结构（位移 Vector + 高度 scalar 同 inputName），规模裁剪至测试域。
dqBase::RefPtr<dqGeom::IndexedPolyface> buildAuxQuadGrid(int nDim) {
    auto options = dqGeom::StrokeOptions::CreateForFacets();
    options.shouldTriangulate = true;
    auto builder = dqGeom::PolyfaceBuilder::create(options);
    double const step = 1.6 / (nDim - 1);
    for (int iRow = 0; iRow < nDim - 1; ++iRow) {
        for (int iCol = 0; iCol < nDim - 1; ++iCol) {
            double const x0 = -0.8 + iRow * step, x1 = x0 + step;
            double const y0 = -0.8 + iCol * step, y1 = y0 + step;
            dqGeom::Point3d const quad[4] = {
                dqGeom::Point3d::From(x0, y0, 0.0),
                dqGeom::Point3d::From(x1, y0, 0.0),
                dqGeom::Point3d::From(x1, y1, 0.0),
                dqGeom::Point3d::From(x0, y1, 0.0),
            };
            builder->AddQuadFacet(quad);
        }
    }
    auto polyface = builder->ClaimPolyface();

    std::vector<double> zeroScalar, zeroDisp, bumpDisp, rampScalar;
    for (auto const& point : polyface->Data().points) {
        zeroScalar.push_back(0.0);
        zeroDisp.push_back(0.0);
        zeroDisp.push_back(0.0);
        zeroDisp.push_back(0.0);
        bumpDisp.push_back(0.0);
        bumpDisp.push_back(0.0);
        bumpDisp.push_back(0.3);  // 帧 1：全域 +Z 隆起 0.3（视域 z∈[-5,5] 内）
        rampScalar.push_back(point.x + 0.8);  // 帧 1：x 向 [0,1.6] 渐变
    }

    std::vector<dqGeom::AuxChannel> channels;
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroDisp),
                                            dqGeom::AuxChannelData(1.0, bumpDisp)},
        dqGeom::AuxChannelDataType::Vector, std::string("disp"), std::string("T"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroScalar),
                                            dqGeom::AuxChannelData(1.0, rampScalar)},
        dqGeom::AuxChannelDataType::Scalar, std::string("scalar"), std::string("T"));
    polyface->Data().auxData =
        dqGeom::PolyfaceAuxData(std::move(channels), polyface->Data().pointIndex);
    return polyface;
}

dqCommon::AnalysisStyle makeAsStyle() {
    dqCommon::AnalysisStyle style;
    dqCommon::AnalysisStyleDisplacement disp;
    disp.channelName = "disp";
    disp.scale = 1.0;
    style.displacement = disp;
    dqCommon::AnalysisStyleThematic scalar;
    scalar.channelName = "scalar";
    scalar.range = dqGeom::Range1d::CreateXX(0.0, 1.6);
    style.scalar = scalar;
    return style;
}

}  // namespace

// Authored（§5(g)）：E2E 同规模复现——100×100 点阵（58,806 mesh 角点）×
// 九通道 140B/顶点（AnalysisStyleExample.ts createFlatMeshWithWaves :85-177 的
// 通道结构原样：Static×3 + Animated×3[3 帧] + Linear×3[10 帧]）。真窗 E2E 首跑的
// 堆腐蚀（0xc0000374，Techniques 拆卸期检出）疑与规模相关——本锁在离屏环境以
// 同规模数据驱动同一条 LUT 生产/绘制链，CRT 调试堆在释放点检出腐蚀即复现。
TEST(AnalysisStyleLutRenderTest, FlatWavesScaleMeshRendersWithoutHeapDamage) {
    AsOffscreenEnv env;
    if (!env.init())
        GTEST_SKIP() << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, 100, 100);
    ASSERT_NE(target, nullptr);

    // 视口变换臂（同上——m_hasViewportTransform 门）。
    {
        float const view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5.0f, 1};
        float const worldToNdc[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -0.2f, 0, 0, 0, 0.0f, 1};
        target->setViewportTransform(view, worldToNdc);
    }

    // 100×100 点阵（E2E 同规模）。
    int const nDim = 100;
    auto options = dqGeom::StrokeOptions::CreateForFacets();
    options.shouldTriangulate = true;
    auto pfBuilder = dqGeom::PolyfaceBuilder::create(options);
    for (int iRow = 0; iRow < nDim - 1; ++iRow) {
        for (int iCol = 0; iCol < nDim - 1; ++iCol) {
            dqGeom::Point3d const quad[4] = {
                dqGeom::Point3d::From(iRow * 1.0, iCol * 1.0, 0.0),
                dqGeom::Point3d::From((iRow + 1) * 1.0, iCol * 1.0, 0.0),
                dqGeom::Point3d::From((iRow + 1) * 1.0, (iCol + 1) * 1.0, 0.0),
                dqGeom::Point3d::From(iRow * 1.0, (iCol + 1) * 1.0, 0.0),
            };
            pfBuilder->AddQuadFacet(quad);
        }
    }
    auto polyface = pfBuilder->ClaimPolyface();
    ASSERT_TRUE(polyface.IsValid());

    // 九通道（:112-173 的结构原样——径向 3 静态 + 径向 3 动画[3 帧] +
    // 线性 3[10 帧]；值域镜像参考生成式，规模参数同 E2E）。
    size_t const nPts = polyface->Data().points.size();
    std::vector<double> zeroScalar(nPts, 0.0);
    std::vector<double> zeroDisp(nPts * 3, 0.0);
    std::vector<double> radialHeight(nPts), radialSlope(nPts), radialDisp(nPts * 3);
    double const radius = nDim * 1.0 / 2.0;
    double const cx = radius, cy = radius;
    double const maxHeight = radius / 4.0;
    for (size_t i = 0; i < nPts; ++i) {
        auto const& p = polyface->Data().points[i];
        double const dx = p.x - cx, dy = p.y - cy;
        double const angle = 2.0 * 3.14159265358979323846 * std::sqrt(dx * dx + dy * dy) / radius;
        double const height = maxHeight * std::sin(angle);
        radialHeight[i] = height;
        radialSlope[i] = std::fabs(std::cos(angle));
        radialDisp[i * 3 + 2] = height;
    }

    std::vector<dqGeom::AuxChannel> channels;
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, radialDisp)},
        dqGeom::AuxChannelDataType::Vector, std::string("Static Radial Displacement"),
        std::string("Radial: Static"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(1.0, radialHeight)},
        dqGeom::AuxChannelDataType::Distance, std::string("Static Radial Height"),
        std::string("Radial: Static"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(1.0, radialSlope)},
        dqGeom::AuxChannelDataType::Scalar, std::string("Static Radial Slope"),
        std::string("Radial: Static"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroDisp),
                                            dqGeom::AuxChannelData(1.0, radialDisp),
                                            dqGeom::AuxChannelData(2.0, zeroDisp)},
        dqGeom::AuxChannelDataType::Vector, std::string("Animated Radial Displacement"),
        std::string("Radial: Time"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroScalar),
                                            dqGeom::AuxChannelData(1.0, radialHeight),
                                            dqGeom::AuxChannelData(2.0, zeroScalar)},
        dqGeom::AuxChannelDataType::Distance, std::string("Animated Radial Height"),
        std::string("Radial: Time"));
    channels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroScalar),
                                            dqGeom::AuxChannelData(1.0, radialSlope),
                                            dqGeom::AuxChannelData(2.0, zeroScalar)},
        dqGeom::AuxChannelDataType::Scalar, std::string("Animated Radial Slope"),
        std::string("Radial: Time"));

    std::vector<dqGeom::AuxChannelData> linDisp, linHeight, linSlope;
    double const waveHeight = radius / 20.0;
    double const waveLength = radius / 2.0;
    for (int f = 0; f < 10; ++f) {
        double const waveCenter = waveLength * (static_cast<double>(f) / 9.0);
        std::vector<double> h(nPts), s(nPts), d(nPts * 3, 0.0);
        for (size_t i = 0; i < nPts; ++i) {
            auto const& p = polyface->Data().points[i];
            double const theta = 2.0 * 3.14159265358979323846 * (p.x - waveCenter) / waveLength;
            double const height = waveHeight * std::sin(theta);
            h[i] = height;
            s[i] = std::fabs(std::cos(theta));
            d[i * 3 + 2] = height;
        }
        linDisp.emplace_back(static_cast<double>(f), std::move(d));
        linHeight.emplace_back(static_cast<double>(f), std::move(h));
        linSlope.emplace_back(static_cast<double>(f), std::move(s));
    }
    channels.emplace_back(std::move(linDisp), dqGeom::AuxChannelDataType::Vector,
                          std::string("Linear Displacement"), std::string("Linear: Time"));
    channels.emplace_back(std::move(linHeight), dqGeom::AuxChannelDataType::Distance,
                          std::string("Linear Height"), std::string("Linear: Time"));
    channels.emplace_back(std::move(linSlope), dqGeom::AuxChannelDataType::Scalar,
                          std::string("Linear Slope"), std::string("Linear: Time"));

    polyface->Data().auxData =
        dqGeom::PolyfaceAuxData(std::move(channels), polyface->Data().pointIndex);

    dqCommon::AnalysisStyle style;
    dqCommon::AnalysisStyleDisplacement disp;
    disp.channelName = "Animated Radial Displacement";
    disp.scale = 1.0;
    style.displacement = disp;
    dqCommon::AnalysisStyleThematic scalar;
    scalar.channelName = "Animated Radial Height";
    scalar.range = dqGeom::Range1d::CreateXX(-maxHeight, maxHeight);
    style.scalar = scalar;

    GraphicBuilderOptions opts;
    opts.computeChordTolerance = [] { return 0.01; };
    opts.analysisStyle = style;
    auto builder = env.system->createGraphicBuilder(opts);
    ASSERT_NE(builder, nullptr);
    auto const darkSlateBlue = dqCommon::ColorDef::from(0x2F, 0x4F, 0x4F);
    builder->setSymbology(darkSlateBlue, darkSlateBlue, 1);
    builder->addPolyface(*polyface, false);
    auto* graphic = builder->finish();
    ASSERT_NE(graphic, nullptr);

    Scene scene;
    scene.foreground.push_back(graphic);
    target->changeScene(scene);

    // 网格域 [0,99]² → 视域适配顶视（±60 盒，含位移幅度）。
    RenderPlan plan = makeAsPlan();
    plan.analysisStyle = style;
    plan.analysisFraction = 0.5f;
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomRear) = dqGeom::Point3d::From(-10, -10, -60);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomRear) = dqGeom::Point3d::From(110, -10, -60);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopRear) = dqGeom::Point3d::From(-10, 110, -60);
    plan.frustum.getCorner(dqCommon::Npc::RightTopRear) = dqGeom::Point3d::From(110, 110, -60);
    plan.frustum.getCorner(dqCommon::Npc::LeftBottomFront) = dqGeom::Point3d::From(-10, -10, 60);
    plan.frustum.getCorner(dqCommon::Npc::RightBottomFront) = dqGeom::Point3d::From(110, -10, 60);
    plan.frustum.getCorner(dqCommon::Npc::LeftTopFront) = dqGeom::Point3d::From(-10, 110, 60);
    plan.frustum.getCorner(dqCommon::Npc::RightTopFront) = dqGeom::Point3d::From(110, 110, 60);
    target->changeRenderPlan(plan);
    target->drawFrame();

    std::vector<uint8_t> frame;
    ASSERT_TRUE(target->readPixels(0, 0, 100, 100, frame));
    EXPECT_GT(countNonBlack(frame), 3000L)
        << "E2E-scale animated LUT mesh must be on screen";
    // 析构期的 CRT 调试堆校验 = 堆腐蚀检出面（无显式断言——进程存活即证）。
}

// Authored（§5(f)——参考无离线变体编译锁）：animated+unquantized-LUT Surface 变体
// 在真 Windows GL 驱动上编译+链接（M-L 12.9 静默跳过族——变体编译失败=整图元不画
// 的直接判别面；GlslCompileHarness 为 macOS-only，本锁走真 driver）。
TEST(AnalysisStyleLutRenderTest, AnimatedUnquantizedLutVariantCompilesOnRealDriver) {
    AsOffscreenEnv env;
    if (!env.init())
        GTEST_SKIP() << "offscreen GL environment unavailable";

    auto techs = createDefaultTechniques(*env.driver);
    auto* technique = techs->getTechnique(TechniqueId::Surface);
    ASSERT_NE(technique, nullptr);

    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;
    flags.isLutUnquantized = true;
    flags.isAnimated = true;
    auto* shader = technique->getShader(flags);
    ASSERT_NE(shader, nullptr);
    EXPECT_EQ(shader->ensureCompiled(*env.driver), CompileStatus::Success)
        << "animated unquantized-LUT Surface variant must compile+link";
}

// Authored（§5(g)——见文件头复现配方）：auxData 网格 LUT 路由 + animated 变体 +
// scalar 渐变纹理（analysisTexture）的离屏像素锁。fraction=0 平帧与 fraction=1
// 隆起+重着色帧都必须有内容，且两帧显著不同（scalar 重着色 + 位移）。
TEST(AnalysisStyleLutRenderTest, AuxChannelsLutMeshRendersAndRecolors) {
    AsOffscreenEnv env;
    if (!env.init())
        GTEST_SKIP() << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, 100, 100);
    ASSERT_NE(target, nullptr);

    // 视口变换臂（OpenGLRenderTarget::drawFrame 的 m_hasViewportTransform 门——
    // 离屏 harness 契约，RenderSmokeTest makeTopViewTarget 同型；实际投影在
    // changeRenderPlan→changeFrustum 后由 FrustumUniforms 绘制期组装）。
    {
        float const view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5.0f, 1};
        float const worldToNdc[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -0.2f, 0, 0, 0, 0.0f, 1};
        target->setViewportTransform(view, worldToNdc);
    }

    auto polyface = buildAuxQuadGrid(4);  // 4×4 点阵 → 9 quad → 18 tri → 54 角点
    ASSERT_TRUE(polyface.IsValid());
    ASSERT_TRUE(polyface->Data().auxData.has_value());

    auto const style = makeAsStyle();

    GraphicBuilderOptions opts;  // GraphicType::Scene 默认
    opts.computeChordTolerance = [] { return 0.01; };
    opts.analysisStyle = style;
    auto builder = env.system->createGraphicBuilder(opts);
    ASSERT_NE(builder, nullptr);
    auto const darkSlateBlue = dqCommon::ColorDef::from(0x2F, 0x4F, 0x4F);
    builder->setSymbology(darkSlateBlue, darkSlateBlue, 1);
    builder->addPolyface(*polyface, false);
    auto* graphic = builder->finish();
    ASSERT_NE(graphic, nullptr);

    Scene scene;
    scene.foreground.push_back(graphic);
    target->changeScene(scene);

    // fraction=0：平帧（位移帧 0 = 零场）——内容必须在屏（RED 锚：修复前黑网格）。
    RenderPlan plan0 = makeAsPlan();
    plan0.analysisStyle = style;
    plan0.analysisFraction = 0.0f;
    target->changeRenderPlan(plan0);
    target->drawFrame();
    std::vector<uint8_t> frameFlat;
    ASSERT_TRUE(target->readPixels(0, 0, 100, 100, frameFlat));
    dumpUniqueColors("flat", frameFlat);
    long const flatCount = countNonBlack(frameFlat);
    EXPECT_GT(flatCount, 3000L) << "flat (fraction=0) animated LUT mesh must be on screen";

    // fraction=1：隆起 + scalar 渐变着色帧——内容在屏且与平帧显著不同。
    RenderPlan plan1 = makeAsPlan();
    plan1.analysisStyle = style;
    plan1.analysisFraction = 1.0f;
    target->changeRenderPlan(plan1);
    target->drawFrame();
    std::vector<uint8_t> frameWave;
    ASSERT_TRUE(target->readPixels(0, 0, 100, 100, frameWave));
    dumpUniqueColors("wave", frameWave);
    long const waveCount = countNonBlack(frameWave);
    EXPECT_GT(waveCount, 3000L) << "displaced (fraction=1) mesh must stay on screen";

    long const diff = frameDiffCount(frameFlat, frameWave);
    EXPECT_GT(diff, 1000L) << "fraction 0->1 must recolor (scalar gradient) / displace";
}
