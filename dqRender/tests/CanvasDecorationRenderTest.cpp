// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — CanvasDecoration GL 栅格化像素测试（2D overlay 的 GL 后端）
//
// Authored: no reference test exists in itwinjs-core for GL-rasterized canvas
//           decorations (reference uses an HTML 2D canvas); scenario from
//           Target.ts:1413-1424 (per-entry save → position?translate →
//           drawDecoration → restore) + ViewTool.ts:3720-3729 (crosshair body:
//           floor+0.5 pixel-center alignment, contrast color, lineWidth 1,
//           full-screen horizontal + vertical lines).
//
// 驱动真实 RenderTarget::drawCanvasDecorations GL 路径（RenderPipeline/DTA 与
// RenderSmokeTest 同一离屏 harness）：drawFrame 后调 drawCanvasDecorations，
// readPixels 回读断言横/竖线像素为描边色、线外像素保持背景色。

#include <gtest/gtest.h>

#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/Scene.h"
#include "dqRender/RenderPlan.h"
#include "dqRender/CanvasDecoration.h"

#include "render/OpenGLRenderSystem.h"  // 内部头（测试可访问，dqRenderTest 有 src include）
#include "render/TechniqueImpl.h"       // Techniques（空集即可渲染背景 clear）
#include "platform/PlatformFactory.h"   // createPlatform（平台无关工厂）
#include "rhi/opengl/GlLoader.h"        // Windows: zogl::init 运行时符号装载

#include <dqCommon/ColorDef.h>
#include <dqGeom/Point2d.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace dqRender;

namespace {

// 离屏渲染环境（同 RenderSmokeTest 的 OffscreenRenderEnv：平台 → GL driver →
// RenderSystem + 空 Techniques——背景 clear 与 Canvas2d 线均不需要 Technique）。
// Ported from: itwinjs-core openBlankViewport.ts（BlankViewport.create 最小化）
struct Canvas2dRenderEnv {
    std::unique_ptr<rhi::OpenGLPlatform> platform;
    std::unique_ptr<OpenGLRenderSystem> system;
    rhi::Driver* driver = nullptr;  // 所有权在 system，此处仅引用

    bool init() {
#if defined(_WIN32)
        // bluegl 机制：先解析全部 GL 符号（失败=无 GPU/驱动，测试环境不可用）
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
        system->setTechniques(new Techniques());
        return true;
    }
};

::testing::AssertionResult pixelIsColor(const uint8_t* px,
                                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (px[0] != r || px[1] != g || px[2] != b || px[3] != a)
        return ::testing::AssertionFailure()
               << "pixel (" << int(px[0]) << "," << int(px[1]) << ","
               << int(px[2]) << "," << int(px[3]) << ")"
               << " != expected (" << int(r) << "," << int(g) << "," << int(b) << "," << int(a) << ")";
    return ::testing::AssertionSuccess();
}

constexpr uint32_t kW = 100;
constexpr uint32_t kH = 100;

// readPixels 输出是 GL 坐标（原点左下，逐行向上）；装饰坐标原点左上 y 向下。
// 装饰行 rowFromTop → GL 行 kH-1-rowFromTop。
uint8_t const* pixelAt(std::vector<uint8_t> const& px, uint32_t x, uint32_t rowFromTop) {
    size_t const glRow = kH - 1 - rowFromTop;
    return px.data() + (glRow * kW + x) * 4;
}

}  // namespace

// 十字线经真实 GLCanvasContext 路径栅格化进 GL 帧：黑背景上全屏白色横+竖线，
// 断言中心行/列像素为描边色、线外像素保持背景色。
// Authored: no reference test exists in itwinjs-core for GL-rasterized canvas
//           decorations (reference uses an HTML 2D canvas); scenario from
//           Target.ts:1413-1424 + ViewTool.ts:3720-3729.
TEST(CanvasDecorationRender, CrossHairLinesRasterizeInGlFrame) {
    Canvas2dRenderEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    Scene scene;   // 空场景（blank connection：无几何）
    target->changeScene(scene);
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;  // 黑背景（byte0=r…byte3=a，0xFF000000=黑不透明）
    target->changeRenderPlan(plan);
    target->drawFrame();

    // 十字线体（ViewTool.ts:3720-3729 的形状）：像素中心对齐（floor+0.5 → .5）、
    // 对比色白、lineWidth 1、全屏横竖线。无 position（参考十字线 { drawDecoration }）。
    dqRender::CanvasDecoration cross;
    cross.drawDecoration = [](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.setStrokeStyle(dqCommon::ColorDef::white);
        ctx.setLineWidth(1.0);
        ctx.moveTo(0.0, 50.5);                 // 横线（viewRect.left → right）
        ctx.lineTo(100.0, 50.5);
        ctx.moveTo(50.5, 0.0);                 // 竖线（viewRect.top → bottom）
        ctx.lineTo(50.5, 100.0);
        ctx.stroke();
    };
    std::vector<dqRender::CanvasDecoration> decs;
    decs.push_back(std::move(cross));

    // ← Target.ts:552：drawFrame 内无条件 drawOverlayDecorations；DanQing 在
    //   drawFrame 后、present 前调用（present 在 dqApp Viewport 侧）。
    target->drawCanvasDecorations(decs);

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));
    ASSERT_EQ(px.size(), size_t(kW) * kH * 4);

    // 横线 y=50.5 → 顶起第 50 行像素中心。
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 20, 50), 255, 255, 255, 255)) << "horizontal line";
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 80, 50), 255, 255, 255, 255)) << "horizontal line";
    // 竖线 x=50.5 → 第 50 列像素中心。
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 50, 20), 255, 255, 255, 255)) << "vertical line";
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 50, 80), 255, 255, 255, 255)) << "vertical line";
    // 线外像素保持黑背景。
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 20, 20), 0, 0, 0, 255)) << "off-line pixel";
}

// position 平移（Target.ts:1417-1418：overlay.position → ctx.translate(x, y)）：
// 装饰体画局部坐标 (0,0.5)→(20,0.5) 的短线，position=(10,10) 平移后应落在
// 顶起第 10 行、x∈[10,30) 区间；平移区间外（x=5 同行）保持背景色。
// Authored: no reference test exists in itwinjs-core for GL-rasterized canvas
//           decorations; scenario from Target.ts:1415-1423 (position translate).
TEST(CanvasDecorationRender, PositionTranslateShiftsStrokedPath) {
    Canvas2dRenderEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    Scene scene;
    target->changeScene(scene);
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    target->changeRenderPlan(plan);
    target->drawFrame();

    dqRender::CanvasDecoration dec;
    dec.position = dqGeom::Point2d{10.0, 10.0};   // ← CanvasDecoration.ts:28-31 (position?)
    dec.drawDecoration = [](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.setStrokeStyle(dqCommon::ColorDef::white);
        ctx.setLineWidth(1.0);
        ctx.moveTo(0.0, 0.5);
        ctx.lineTo(20.0, 0.5);
        ctx.stroke();
    };
    std::vector<dqRender::CanvasDecoration> decs;
    decs.push_back(std::move(dec));
    target->drawCanvasDecorations(decs);

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));

    // 平移后：y = 10 + 0.5 = 10.5 → 顶起第 10 行；x = 10..30。
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 15, 10), 255, 255, 255, 255)) << "translated line";
    // 未平移处（局部原点附近 x=5 同行 y=0.5→第 0 行；第 10 行 x=5 在线段左端外）。
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 5, 10), 0, 0, 0, 255)) << "before translate origin";
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 15, 0), 0, 0, 0, 255)) << "untranslated row";
}

// 空调用守卫：空列表 + 无描边体的条目都不改帧内容（GL 帧每帧整体重绘，参考
// _2dCanvas.needsClear 的 clearRect 无 GL 等价物——场景 pass 的清屏即清除）。
// Authored: no reference test exists in itwinjs-core for GL-rasterized canvas
//           decorations; scenario from Target.ts:1413-1424 (canvasDecs 缺失 → 无绘制).
TEST(CanvasDecorationRender, EmptyAndStrokelessDecorationsLeaveFrameUntouched) {
    Canvas2dRenderEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    Scene scene;
    target->changeScene(scene);
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    target->changeRenderPlan(plan);
    target->drawFrame();

    // 空列表。
    std::vector<dqRender::CanvasDecoration> empty;
    target->drawCanvasDecorations(empty);

    // 无 stroke 的条目（beginPath/moveTo/lineTo 但从不 stroke——HTML canvas 语义：
    // 未 stroke 的路径不产生像素）。
    dqRender::CanvasDecoration noStroke;
    noStroke.drawDecoration = [](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.moveTo(0.0, 0.0);
        ctx.lineTo(100.0, 100.0);
    };
    std::vector<dqRender::CanvasDecoration> decs;
    decs.push_back(std::move(noStroke));
    target->drawCanvasDecorations(decs);

    std::vector<uint8_t> px;
    ASSERT_TRUE(target->readPixels(0, 0, kW, kH, px));
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 50, 50), 0, 0, 0, 255)) << "frame must stay background";
    EXPECT_TRUE(pixelIsColor(pixelAt(px, 0, 0), 0, 0, 0, 255));
}

// GLCanvasContext::flush 必须 GL 状态中立：进入时力置 overlay 态（depth test off /
// depthMask=false / blend on + premultiplied func）后，必须恢复进入前的物理状态。
// 否则 RenderState 跟踪器与物理态 desync（空 WorldOverlay/ViewOverlay 列表时跟踪器
// 停在非 overlay 态——如 ViewFlags 隐藏 ACS triad），下一帧 opaque pass 会在深度测试
// 物理关闭下绘制且不自愈。测试：先建立一个已知非 overlay 态，走真实
// drawCanvasDecorations 路径，断言四组状态值回到调用前。
// Authored: no reference test exists in itwinjs-core for GL-rasterized canvas
//           decorations (the reference rasterizes to an HTML 2D canvas outside
//           GL state); drawing environment Target.ts:1408-1439.
TEST(CanvasDecorationRender, FlushRestoresPreExistingGlState) {
    Canvas2dRenderEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    auto target = env.system->createTarget(nullptr, kW, kH);
    ASSERT_NE(target, nullptr);

    Scene scene;
    target->changeScene(scene);
    RenderPlan plan;
    plan.backgroundColor = 0xFF000000;
    target->changeRenderPlan(plan);
    target->drawFrame();

    // 已知非 overlay 态（opaque pass 形态）：depth test on、depthMask=true、
    // blend off、func(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)。
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean const wantDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean wantDepthMask = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &wantDepthMask);
    GLboolean const wantBlend = glIsEnabled(GL_BLEND);
    GLint wantBlendSrcRgb = 0, wantBlendDstRgb = 0, wantBlendSrcAlpha = 0, wantBlendDstAlpha = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &wantBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &wantBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &wantBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &wantBlendDstAlpha);
    ASSERT_EQ(wantDepthTest, GL_TRUE);   // 前提：确为非 overlay 态
    ASSERT_EQ(wantDepthMask, GL_TRUE);
    ASSERT_EQ(wantBlend, GL_FALSE);

    dqRender::CanvasDecoration line;
    line.drawDecoration = [](dqRender::CanvasContext& ctx) {
        ctx.beginPath();
        ctx.setStrokeStyle(dqCommon::ColorDef::white);
        ctx.setLineWidth(1.0);
        ctx.moveTo(0.0, 50.5);
        ctx.lineTo(100.0, 50.5);
        ctx.stroke();
    };
    std::vector<dqRender::CanvasDecoration> decs;
    decs.push_back(std::move(line));
    target->drawCanvasDecorations(decs);   // ← 真实 flush 路径（Target.ts:552 同位）

    GLboolean gotDepthMask = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &gotDepthMask);
    GLint gotBlendSrcRgb = 0, gotBlendDstRgb = 0, gotBlendSrcAlpha = 0, gotBlendDstAlpha = 0;
    glGetIntegerv(GL_BLEND_SRC_RGB, &gotBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &gotBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &gotBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &gotBlendDstAlpha);

    EXPECT_EQ(glIsEnabled(GL_DEPTH_TEST), wantDepthTest) << "depth test must be restored";
    EXPECT_EQ(gotDepthMask, wantDepthMask) << "depth write mask must be restored";
    EXPECT_EQ(glIsEnabled(GL_BLEND), wantBlend) << "blend enable must be restored";
    EXPECT_EQ(gotBlendSrcRgb, wantBlendSrcRgb) << "blend src RGB must be restored";
    EXPECT_EQ(gotBlendDstRgb, wantBlendDstRgb) << "blend dst RGB must be restored";
    EXPECT_EQ(gotBlendSrcAlpha, wantBlendSrcAlpha) << "blend src alpha must be restored";
    EXPECT_EQ(gotBlendDstAlpha, wantBlendDstAlpha) << "blend dst alpha must be restored";
}
