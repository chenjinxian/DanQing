// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 渲染独立验证（BlankConnection 冒烟）
// Ported from: itwinjs-core core/frontend/src/test/openBlankViewport.ts
//              testBlankViewport（官方渲染回归测试范式：离屏视口 + 像素回读断言）
//
// 设计（分析文档 docs/渲染独立验证-BlankConnection-分析.md §6）：
//   不依赖仓外数据层/SQLite，直接驱动 dqRender 离屏闭环
//   RenderSystem.createTarget(100x100 FBO) → changeScene/changeRenderPlan
//   → drawFrame → readPixels → 颜色断言。
// 验收顺序的第一步：背景色（ClearOnly 无需任何 Technique）。

#include <gtest/gtest.h>

#include "dqRender/RenderSystem.h"
#include "dqRender/RenderTarget.h"
#include "dqRender/Scene.h"
#include "dqRender/RenderPlan.h"

#include "render/OpenGLRenderSystem.h"  // 内部头（测试可访问，dqRenderTest 有 src include）
#include "render/TechniqueImpl.h"       // Techniques
#include "render/TechniqueRegistry.h"   // createDefaultTechniques（与 RenderPipeline 同注册路径）
#include "platform/PlatformFactory.h"   // createPlatform（平台无关工厂）
#include "rhi/opengl/GlLoader.h"        // Windows: zogl::init 运行时符号装载
#include "dqRender/PlanarGridProps.h"   // PlanarGridProps（程序网格设置）
#include "render/PolyfaceGraphic.h"     // PolyfaceGraphic（hilite 用例的 feature 载体）
#include "render/Graphic.h"             // Primitive（CachedGeometry → Graphic 包装）
#include "render/Batch.h"               // Batch（hilite 命令路径的载体）

#include <dqCommon/ColorDef.h>
#include <dqCommon/Frustum.h>
#include <dqGeom/Angle.h>
#include <dqGeom/AngleSweep.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/Box.h>
#include <dqGeom/Cone.h>
#include <dqGeom/CurvePrimitive.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Loop.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Path.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Sphere.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>

using namespace dqRender;

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace {

// 离屏渲染冒烟环境：平台 → GL driver → RenderSystem（空 Techniques——背景
// clear 不需要任何 Technique，对齐 openBlankViewport 的最小确定性设置）
// Ported from: itwinjs-core openBlankViewport.ts（BlankViewport.create 最小化）
struct OffscreenRenderEnv {
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
        // createTarget 的 techniques 前置：空集即可渲染背景（ClearOnly）
        system->setTechniques(new Techniques());
        return true;
    }
};

// RGBA8888 像素比较（readPixels 输出格式）
::testing::AssertionResult pixelIsColor(const std::vector<uint8_t>& px, size_t offset,
                                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (px.size() < offset + 4)
        return ::testing::AssertionFailure() << "pixel buffer too small: " << px.size();
    if (px[offset] != r || px[offset + 1] != g || px[offset + 2] != b || px[offset + 3] != a)
        return ::testing::AssertionFailure()
               << "pixel (" << int(px[offset]) << "," << int(px[offset + 1]) << ","
               << int(px[offset + 2]) << "," << int(px[offset + 3]) << ")"
               << " != expected (" << int(r) << "," << int(g) << "," << int(b) << "," << int(a) << ")";
    return ::testing::AssertionSuccess();
}

// ---------------------------------------------------------------------------
// dump PNG 调试钩子 —— 像素断言失败时把全帧 RGBA 写成 PNG 便于人工查看画面。
// Authored: no reference test exists in itwinjs-core for PNG frame dump
// (openBlankViewport.ts 只做 readUniqueColors/expectUniqueColors 颜色集合断言，
//  人工查看画面靠浏览器 devtools；C++ 离屏环境无此便利，故自建)。
// 零依赖：手写 PNG（IHDR + 存储块 deflate IDAT + IEND，CRC32/Adler32 自算）。
// ---------------------------------------------------------------------------
uint32_t pngCrc32(uint8_t const* data, size_t len) {
    static uint32_t table[256];
    static bool tableReady = false;
    if (!tableReady) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        tableReady = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void pngAppendChunk(std::vector<uint8_t>& out, char const type[5], uint8_t const* payload,
                    size_t len) {
    auto pushBe32 = [&out](uint32_t v) {
        out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(v & 0xFF));
    };
    pushBe32(static_cast<uint32_t>(len));
    std::vector<uint8_t> crcInput(type, type + 4);
    crcInput.insert(crcInput.end(), payload, payload + len);
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload, payload + len);
    pushBe32(pngCrc32(crcInput.data(), crcInput.size()));
}

// 写 RGBA 帧 → PNG 文件。返回 false = 文件写入失败。
bool dumpFramePng(char const* path, uint32_t width, uint32_t height,
                  uint8_t const* rgbaTopDown) {
    std::vector<uint8_t> png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    // IHDR: w/h (BE), bitdepth=8, colortype=6 (RGBA), 其余 0
    uint8_t ihdr[13] = {};
    ihdr[0] = static_cast<uint8_t>((width >> 24) & 0xFF);
    ihdr[1] = static_cast<uint8_t>((width >> 16) & 0xFF);
    ihdr[2] = static_cast<uint8_t>((width >> 8) & 0xFF);
    ihdr[3] = static_cast<uint8_t>(width & 0xFF);
    ihdr[4] = static_cast<uint8_t>((height >> 24) & 0xFF);
    ihdr[5] = static_cast<uint8_t>((height >> 16) & 0xFF);
    ihdr[6] = static_cast<uint8_t>((height >> 8) & 0xFF);
    ihdr[7] = static_cast<uint8_t>(height & 0xFF);
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 6;   // color type RGBA
    pngAppendChunk(png, "IHDR", ihdr, 13);

    // 扫描线：每行前置 filter byte 0（None）
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (1 + static_cast<size_t>(width) * 4));
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        uint8_t const* row = rgbaTopDown + static_cast<size_t>(y) * width * 4;
        raw.insert(raw.end(), row, row + static_cast<size_t>(width) * 4);
    }

    // zlib 流：0x78 0x01 + 存储块（每块 ≤65535B）+ Adler32
    std::vector<uint8_t> idat = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t const block = std::min<size_t>(raw.size() - pos, 65535);
        bool const finalBlock = (pos + block == raw.size());
        idat.push_back(finalBlock ? 1 : 0);
        uint16_t len = static_cast<uint16_t>(block);
        idat.push_back(static_cast<uint8_t>(len & 0xFF));
        idat.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        idat.push_back(static_cast<uint8_t>(~len & 0xFF));
        idat.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
        idat.insert(idat.end(), raw.begin() + static_cast<ptrdiff_t>(pos),
                    raw.begin() + static_cast<ptrdiff_t>(pos + block));
        pos += block;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    uint32_t adler = (b << 16) | a;
    idat.push_back(static_cast<uint8_t>((adler >> 24) & 0xFF));
    idat.push_back(static_cast<uint8_t>((adler >> 16) & 0xFF));
    idat.push_back(static_cast<uint8_t>((adler >> 8) & 0xFF));
    idat.push_back(static_cast<uint8_t>(adler & 0xFF));
    pngAppendChunk(png, "IDAT", idat.data(), idat.size());

    uint8_t const noPayload = 0;
    pngAppendChunk(png, "IEND", &noPayload, 0);

    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(png.data(), 1, png.size(), f) == png.size();
    fclose(f);
    return ok;
}

// readPixels 输出是 GL 坐标（原点左下）——dump 前翻转到自上而下，方便人工比对。
bool dumpFramePngFlipped(char const* path, std::vector<uint8_t> const& pixelsBottomUp,
                         uint32_t width, uint32_t height) {
    if (pixelsBottomUp.size() < static_cast<size_t>(width) * height * 4)
        return false;
    std::vector<uint8_t> topDown(pixelsBottomUp.size());
    for (uint32_t y = 0; y < height; ++y) {
        memcpy(topDown.data() + static_cast<size_t>(y) * width * 4,
               pixelsBottomUp.data() + static_cast<size_t>(height - 1 - y) * width * 4,
               static_cast<size_t>(width) * 4);
    }
    return dumpFramePng(path, width, height, topDown.data());
}

// ---------------------------------------------------------------------------
// 像素断言公共帮手（全用例共用）
// Ported from: itwinjs-core openBlankViewport.ts readUniqueColors/expectUniqueColors
//（全图唯一色集合 + 期望色出现断言；hasColorNear 为光照/合成色偏差留 40 容差）
// ---------------------------------------------------------------------------
struct FrameColors {
    std::vector<uint8_t> px;
    std::set<uint32_t> colors;
};

FrameColors readUniqueColors(RenderTarget& target) {
    FrameColors fc;
    target.readPixels(0, 0, 100, 100, fc.px);
    for (size_t i = 0; i + 3 < fc.px.size(); i += 4)
        fc.colors.insert((uint32_t(fc.px[i]) << 24) | (uint32_t(fc.px[i + 1]) << 16)
                         | (uint32_t(fc.px[i + 2]) << 8) | uint32_t(fc.px[i + 3]));
    return fc;
}

bool hasColorNear(std::set<uint32_t> const& colors, uint8_t r, uint8_t g, uint8_t b,
                  int tol = 40) {
    for (uint32_t c : colors) {
        int cr = int((c >> 24) & 0xFF), cg = int((c >> 16) & 0xFF), cb = int((c >> 8) & 0xFF);
        if (std::abs(cr - r) <= tol && std::abs(cg - g) <= tol && std::abs(cb - b) <= tol)
            return true;
    }
    return false;
}

// expectUniqueColors 的容差版：任一期望色缺失时打印颜色集 + dump PNG（调试钩子）。
void expectAllColorsPresent(char const* dumpName, FrameColors const& fc,
                            std::initializer_list<std::array<uint8_t, 3>> expected) {
    std::vector<bool> ok;
    for (auto const& c : expected)
        ok.push_back(hasColorNear(fc.colors, c[0], c[1], c[2]));
    if (std::any_of(ok.begin(), ok.end(), [](bool v) { return !v; })) {
        fprintf(stderr, "DBG colors(%zu):", fc.colors.size());
        for (auto c : fc.colors)
            fprintf(stderr, " %06X", (c >> 8) & 0xFFFFFF);
        fprintf(stderr, "\n");
        fflush(stderr);
        std::string path = std::string("RenderSmokeTest_") + dumpName + ".png";
        if (dumpFramePngFlipped(path.c_str(), fc.px, 100, 100))
            fprintf(stderr, "DBG frame dumped to %s\n", path.c_str());
    }
    size_t i = 0;
    for (auto const& c : expected) {
        EXPECT_TRUE(ok[i]) << "expected color (" << int(c[0]) << "," << int(c[1]) << ","
                           << int(c[2]) << ") not rendered";
        ++i;
    }
}

// 每图元族用例的公共前置：GL env + 全 technique 集 + 100x100 target + 正交俯视
// 相机（(0,0,5) 看向原点，对齐 SpatialViewState.createBlank 的标准俯视形态）。
// 失败返回 nullptr（GL 不可用——调用方 ASSERT_NE 并 skip）。
//
// setViewportTransform(mv, mvp) 契约：mvp = worldToNdc = proj·view（完整世界→
// 裁剪矩阵，near/far 从中提取）。ortho(n=0,f=10)·T(0,0,-5)：
//   z' = -0.2·(z-5) - 1 = -0.2·z  → 场景 z∈[-1,1] 映射 NDC z'∈[-0.2,0.2]，
//   场景完整落在视锥内（near 在相机处 z=5，far 在 z=-5）。
std::unique_ptr<RenderTarget> makeTopViewTarget(OffscreenRenderEnv& env) {
    if (!env.init())
        return nullptr;
    env.system->setTechniques(createDefaultTechniques(*env.driver).release());
    auto target = env.system->createTarget(nullptr, 100, 100);
    if (!target)
        return nullptr;
    float const view[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5.0f, 1};
    float const worldToNdc[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -0.2f, 0, 0, 0, 0.0f, 1};
    target->setViewportTransform(view, worldToNdc);
    return target;
}

// GraphicBuilderOptions：computeChordTolerance 闭包必须设置（finish() 前置）
GraphicBuilderOptions makeSceneBuilderOptions() {
    GraphicBuilderOptions options;  // GraphicType::Scene
    options.computeChordTolerance = [] { return 0.01; };
    return options;
}

// 黑背景 RenderPlan（衬托图元色）+ 单位环境光。
// 灯光隔离：ambient 1.0（颜色黑 → applyLighting 的 ambientColor=rgb）、方向光全 0
// → litColor == rgb（精确），图元色像素断言不受参考默认光照（~0.7× 调光）影响。
// surface u_surfaceFlags 上传修通后（GltfTexturePixelTest 链路），FillFlags::Lit
// 图元在参考光照下会调光 ~0.7×，超出 hasColorNear 容差 40 —— 无此隔离则像素
// 断言锁的是"无光照"的旧行为。
// Authored: no reference test exists in itwinjs-core for offscreen color smoke
//（display-test-app 无颜色断言测试；灯光值为 itwinjs LightSettings 语义）。
//
// plan.frustum/fraction 携带 makeTopViewTarget 相机的等价视域（ef247b8 后
// 投影唯一来源 = changeRenderPlan→changeFrustum(plan.frustum)，参考
// Target.ts:534；plan 默认 is3d=true+fraction=0+单位盒会走透视分支 →
// l/r/b/t 全 ×fraction(0)=0 → frustumProjection 除零 → u_proj=NaN → 图元
// 0 fragments——2026-09-23 排查的 13 项存量失败根因）。
// 相机语义（makeTopViewTarget 注释）：eye(0,0,5) 朝 −Z、ortho(n=0,f=10)、
// 世界 x/y∈[−1,1]、可见 z∈[−5,5] → 正交盒 8 角点 + fraction=1.0（正交
// 完成态，参考 ViewingSpace.frustFraction 默认 1.0）→ ortho 分支的
// ortho(±1,±1,0,10) 与 kTopViewWorldToNdc 的 x/y 投影一致。
// Authored（frustum 部分）: 参考离屏测试走 Viewport/ViewingSpace 全链构造
// plan（RenderPlan.ts:103-111 从 viewingSpace 取）；DanQing 离屏测试无该层，
// 按同语义手填。
RenderPlan blackBackgroundPlan() {
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

// 单个彩色 LineString（区间角点摆放），经 changeDecorations 按 GraphicType 入桶。
// finish() 返回裸 RenderGraphic*（dqRender 所有 graphic 树为裸指针 + Owner 包装；
// 测试场景/装饰桶在测试存活期持有引用，gtest 不强制查泄漏）。
// Ported from: itwinjs-core CesiumDecorator.createLineStringDecorations
//              (EmptyExample.ts:90-115, 三色 WorldDecoration/WorldOverlay 线串)
dqRender::RenderGraphic* finishColoredLine(OffscreenRenderEnv& env,
    GraphicType type, dqGeom::Point3d a, dqGeom::Point3d b,
    uint8_t r, uint8_t g, uint8_t bch) {
    GraphicBuilderOptions options;
    options.type = type;
    options.computeChordTolerance = [] { return 0.01; };
    auto builder = env.system->createGraphicBuilder(options);
    if (!builder)
        return nullptr;
    using dqCommon::ColorDef;
    builder->setSymbology(ColorDef::from(r, g, bch), ColorDef::from(r, g, bch), 3);
    dqGeom::Point3d const pts[2] = {a, b};
    builder->addLineString(pts, 2);
    return builder->finish();
}

} // namespace

// Ported from: itwinjs-core core/frontend/src/test/openBlankViewport.ts
//              testBlankViewport（渲染一帧并回读断言）
TEST(RenderSmokeTest, BlankViewportRendersPlanBackgroundColor) {
    OffscreenRenderEnv env;
    ASSERT_TRUE(env.init()) << "offscreen GL environment unavailable";

    // 100x100 离屏 target（openBlankViewport 的固定尺寸，保证确定性）
    auto target = env.system->createTarget(nullptr, 100, 100);
    ASSERT_NE(target, nullptr);

    // 空 Scene（blank connection：无几何）
    Scene scene;
    target->changeScene(scene);

    // RenderPlan：红色背景（0xAABBGGRR?  RenderPlan 注释 RGBA —— 以测试断言实测格式）
    RenderPlan plan;
    plan.backgroundColor = 0xFF0000FF;  // 红色（按 RGBA 注释）
    target->changeRenderPlan(plan);

    target->drawFrame();

    // 回读中心像素
    std::vector<uint8_t> pixels;
    ASSERT_TRUE(target->readPixels(50, 50, 1, 1, pixels));

    // 期望背景色（红色，任一字节序变体都应命中红色分量主导）
    // openBlankViewport.ts expectUniqueColors 的最小化对应物
    EXPECT_TRUE(pixelIsColor(pixels, 0, 0xFF, 0x00, 0x00, 0xFF));
}

// dump PNG 帮手自验证：2x2 RGBA 写 PNG，回读校验签名/IHDR/zlib 头与像素数据。
// Authored: no reference test exists in itwinjs-core for PNG frame dump
// (openBlankViewport.ts 只有 readUniqueColors；PNG 帮手为 C++ 离屏环境自建)
TEST(RenderSmokeTest, DumpPngHelperWritesValidPng) {
    uint8_t const frame[16] = {
        0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,   // top row: red, green
        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,   // bottom row: blue, yellow
    };
    ASSERT_TRUE(dumpFramePng("RenderSmokeTest_DumpPngHelper.png", 2, 2, frame));

    FILE* f = fopen("RenderSmokeTest_DumpPngHelper.png", "rb");
    ASSERT_NE(f, nullptr);
    std::vector<uint8_t> png;
    uint8_t buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        png.insert(png.end(), buf, buf + n);
    fclose(f);
    remove("RenderSmokeTest_DumpPngHelper.png");

    // PNG 签名
    uint8_t const sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    ASSERT_GE(png.size(), 61u);  // 8(sig)+25(IHDR)+~34(IDAT)+12(IEND) 下限
    EXPECT_EQ(0, memcmp(png.data(), sig, 8));

    // IHDR：长度 13、类型、width=2、height=2、bitdepth=8、colortype=6(RGBA)
    ASSERT_GE(png.size(), 8u + 4u + 4u + 13u);
    EXPECT_EQ(png[8], 0); EXPECT_EQ(png[9], 0); EXPECT_EQ(png[10], 0); EXPECT_EQ(png[11], 13);
    EXPECT_EQ(0, memcmp(&png[12], "IHDR", 4));
    EXPECT_EQ(0u, png[16]); EXPECT_EQ(0u, png[17]); EXPECT_EQ(0u, png[18]); EXPECT_EQ(2u, png[19]);
    EXPECT_EQ(0u, png[20]); EXPECT_EQ(0u, png[21]); EXPECT_EQ(0u, png[22]); EXPECT_EQ(2u, png[23]);
    EXPECT_EQ(8u, png[24]);   // bit depth
    EXPECT_EQ(6u, png[25]);   // color type RGBA

    // IDAT zlib 头（0x78 0x01）+ 首存储块含 filter byte 0
    ASSERT_GE(png.size(), 8u + 25u + 4u + 4u + 2u + 5u);
    EXPECT_EQ(0x78, png[8 + 25 + 4 + 4 + 0]);
    EXPECT_EQ(0x01, png[8 + 25 + 4 + 4 + 1]);
    EXPECT_EQ(1, png[8 + 25 + 4 + 4 + 2]);          // BFINAL=1, BTYPE=00（存储块）
    EXPECT_EQ(0, png[8 + 25 + 4 + 4 + 2 + 5]);      // 首行 filter byte = None

    // 尾部 IEND
    ASSERT_GE(png.size(), 12u);
    EXPECT_EQ(0, memcmp(&png[png.size() - 8], "IEND", 4));
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts CesiumDecorator
//              （图元清单的最小子集）+ openBlankViewport.ts readUniqueColors
// 三个图元（Box=Surface 技术、LineString=Polyline 技术、Shape=三角面）各自不同
// 颜色，正交俯视后断言画面中同时出现三种图元色。
TEST(RenderSmokeTest, DecoratorPrimitivesRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    // 三图元（分象限摆放避免遮挡耦合）：蓝 Box 居中、绿 LineString 左下、红 Shape 右上
    auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
    ASSERT_NE(builder, nullptr);

    using dqCommon::ColorDef;
    builder->setSymbology(ColorDef::from(0, 0, 255), ColorDef::from(0, 0, 255), 0);
    dqGeom::Point3d boxCorners[2] = {
        dqGeom::Point3d(-0.25, -0.25, -0.25),
        dqGeom::Point3d(0.25, 0.25, 0.25),
    };
    auto box = dqGeom::Box::CreateRange(dqGeom::Range3d(boxCorners[0], boxCorners[1]), true);
    builder->addSolidPrimitive(*box);

    builder->setSymbology(ColorDef::from(0, 255, 0), ColorDef::from(0, 255, 0), 0);
    dqGeom::Point3d linePts[2] = {
        dqGeom::Point3d(-0.85, -0.85, 0.0),
        dqGeom::Point3d(-0.35, -0.35, 0.0),
    };
    builder->addLineString(linePts, 2);

    builder->setSymbology(ColorDef::from(255, 0, 0), ColorDef::from(255, 0, 0), 0);
    dqGeom::Point3d shapePts[3] = {
        dqGeom::Point3d(0.35, 0.35, 0.0),
        dqGeom::Point3d(0.85, 0.35, 0.0),
        dqGeom::Point3d(0.6, 0.8, 0.0),
    };
    builder->addShape(shapePts, 3);

    Scene scene;
    scene.foreground.push_back(builder->finish());
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("DecoratorPrimitives", readUniqueColors(*target),
                           {{0, 0, 255}, {0, 255, 0}, {255, 0, 0}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:69-88
//              CesiumDecorator.createPointDecorations + openBlankViewport.ts readUniqueColors
// 三个蓝色 PointString（各自 builder / setSymbology w1），断言点精灵颜色出现。
TEST(RenderSmokeTest, PointStringRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    dqGeom::Point3d const pts[3][1] = {
        {dqGeom::Point3d(-0.5, 0.0, 0.1)},
        {dqGeom::Point3d(0.0, 0.5, 0.2)},
        {dqGeom::Point3d(0.5, -0.5, 0.3)},
    };
    Scene scene;
    for (auto const& p : pts) {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(0, 0, 255), ColorDef::from(0, 0, 255), 1);
        builder->addPointString(p, 1);
        scene.foreground.push_back(builder->finish());
    }
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("PointString", readUniqueColors(*target), {{0, 0, 255}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:200-254
//              CesiumDecorator.createArcDecorations + openBlankViewport.ts readUniqueColors
// 三段圆弧：黄半圆（open）、青色整椭圆（filled）、粉色 3/4 弧，断言三色各自出现。
TEST(RenderSmokeTest, ArcPrimitivesRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    // 参考 Arc3d.createScaledXYColumns(center, identity, rx, ry, sweep) → FromVectors 等价
    struct ArcDef {
        dqGeom::Point3d center;
        double rx, ry;
        dqGeom::AngleSweep sweep;
        bool isEllipse, filled;
        uint8_t r, g, b;
    };
    ArcDef const arcDefs[3] = {
        {dqGeom::Point3d(-0.5, -0.5, 0.35), 0.4, 0.4,
         dqGeom::AngleSweep::FromStartEndRadians(0.0, dqGeom::Angle::kPi),
         false, false, 255, 255, 0},
        {dqGeom::Point3d(0.5, 0.5, 0.4), 0.3, 0.5,
         dqGeom::AngleSweep::FromStartEndRadians(0.0, 2.0 * dqGeom::Angle::kPi),
         true, true, 0, 255, 255},
        {dqGeom::Point3d(0.0, -0.6, 0.45), 0.6, 0.3,
         dqGeom::AngleSweep::FromStartEndRadians(dqGeom::Angle::kPi / 4.0, 1.5 * dqGeom::Angle::kPi),
         false, false, 255, 100, 100},
    };

    Scene scene;
    for (auto const& a : arcDefs) {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(a.r, a.g, a.b), ColorDef::from(a.r, a.g, a.b), 2);
        auto arc = dqGeom::Arc3d::FromVectors(
            a.center, dqGeom::Vector3d::From(a.rx, 0, 0), dqGeom::Vector3d::From(0, a.ry, 0),
            a.sweep);
        ASSERT_TRUE(arc);
        builder->addArc(*arc, a.isEllipse, a.filled);
        scene.foreground.push_back(builder->finish());
    }
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("ArcPrimitives", readUniqueColors(*target),
                           {{255, 255, 0}, {0, 255, 255}, {255, 100, 100}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:259-331
//              CesiumDecorator.createPathDecorations + openBlankViewport.ts readUniqueColors
// 复合 Path（两段直线 + 90° 圆弧过渡 + 直线收尾），断言路径色出现。
TEST(RenderSmokeTest, PathPrimitiveRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    dqGeom::Point3d const P0(-0.8, -0.8, 0.5);
    dqGeom::Point3d const P1(-0.2, -0.8, 0.5);
    dqGeom::Point3d const P2(-0.2, -0.2, 0.5);
    double const R = 0.2;
    dqGeom::Point3d const arcMid(P2.x + R / std::sqrt(2.0), P2.y + R / std::sqrt(2.0), 0.5);
    dqGeom::Point3d const arcEnd(P2.x + R, P2.y, 0.5);
    auto arc = dqGeom::Arc3d::FromVectors(
        dqGeom::Point3d(P2.x, P2.y, 0.5), dqGeom::Vector3d::From(R, 0, 0),
        dqGeom::Vector3d::From(0, R, 0),
        dqGeom::AngleSweep::FromStartEndRadians(0.0, dqGeom::Angle::kPi / 2.0));
    // 参考 createCircularStartMiddleEnd(P2, arcMid, arcEnd)：以 P2 为起点经 arcMid 到
    // arcEnd 的 1/4 圆——FromVectors(center, R·x̂, R·ŷ, 0..π/2) 即 P2→arcEnd 经 arcMid。
    (void)arcMid;
    ASSERT_TRUE(arc);
    dqGeom::Point3d const P3(arcEnd.x, arcEnd.y + 0.4, 0.5);

    std::vector<dqGeom::CurvePrimitivePtr> curves = {
        dqGeom::LineString3d::create({P0, P1}),
        dqGeom::LineString3d::create({P1, P2}),
        arc,
        dqGeom::LineString3d::create({arcEnd, P3}),
    };
    auto path = dqGeom::Path::Create(curves);
    ASSERT_TRUE(path);

    auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
    ASSERT_NE(builder, nullptr);
    builder->setSymbology(ColorDef::from(255, 100, 200), ColorDef::from(255, 100, 200), 3);
    builder->addPath(*path);

    Scene scene;
    scene.foreground.push_back(builder->finish());
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("PathPrimitive", readUniqueColors(*target), {{255, 100, 200}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:334-353
//              CesiumDecorator.createLoopDecorations + openBlankViewport.ts readUniqueColors
// Loop.create(LineString3d.create(trianglePoints)) 三角形区域，断言洋红色出现。
TEST(RenderSmokeTest, LoopPrimitiveRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    auto loop = dqGeom::Loop::Create(
        {dqGeom::LineString3d::create({
            dqGeom::Point3d(-0.6, -0.6, 0.7),
            dqGeom::Point3d(0.6, -0.6, 0.7),
            dqGeom::Point3d(0.0, 0.6, 0.7),
            dqGeom::Point3d(-0.6, -0.6, 0.7),
        })});
    ASSERT_TRUE(loop);

    auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
    ASSERT_NE(builder, nullptr);
    builder->setSymbology(ColorDef::from(255, 0, 255), ColorDef::from(255, 0, 255), 2);
    builder->addLoop(*loop);

    Scene scene;
    scene.foreground.push_back(builder->finish());
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("LoopPrimitive", readUniqueColors(*target), {{255, 0, 255}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:356-397
//              CesiumDecorator.createPolyfaceDecorations (+createPyramidPolyface 372-412,
//              +createBoxPolyface 414-444) + openBlankViewport.ts readUniqueColors
// PolyfaceBuilder 手工构面：金字塔（AddQuadFacet 底 + 4×AddTriangleFacet 侧面）与
// 长方体（6×AddQuadFacet），addPolyface(filled=true)，断言两轮廓色出现。
TEST(RenderSmokeTest, PolyfacePrimitivesRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    // --- 金字塔（参考 createPyramidPolyface：底面方形 + 尖顶朝 +X，旋转侧放）---
    dqGeom::Point3d const pc(-0.4, 0.3, 0.0);
    double const baseSize = 0.5, halfSize = baseSize / 2.0, height = baseSize * 0.8;
    dqGeom::Point3d const base1(pc.x, pc.y - halfSize, pc.z - halfSize);
    dqGeom::Point3d const base2(pc.x, pc.y + halfSize, pc.z - halfSize);
    dqGeom::Point3d const base3(pc.x, pc.y + halfSize, pc.z + halfSize);
    dqGeom::Point3d const base4(pc.x, pc.y - halfSize, pc.z + halfSize);
    dqGeom::Point3d const apex(pc.x + height, pc.y, pc.z);
    auto pyramid = dqGeom::PolyfaceBuilder::create();
    {
        dqGeom::Point3d const quad[4] = {base1, base2, base3, base4};
        pyramid->AddQuadFacet(quad);
        dqGeom::Point3d const t1[3] = {base1, apex, base2};
        dqGeom::Point3d const t2[3] = {base2, apex, base3};
        dqGeom::Point3d const t3[3] = {base3, apex, base4};
        dqGeom::Point3d const t4[3] = {base4, apex, base1};
        pyramid->AddTriangleFacet(t1);
        pyramid->AddTriangleFacet(t2);
        pyramid->AddTriangleFacet(t3);
        pyramid->AddTriangleFacet(t4);
    }

    // --- 长方体（参考 createBoxPolyface：6 面各 AddQuadFacet，外法线绕序）---
    dqGeom::Point3d const bc(0.5, 0.3, 0.0);
    double const hw = 0.2, hd = 0.2, hh = 0.25;
    dqGeom::Point3d const b1(bc.x - hw, bc.y - hd, bc.z - hh);
    dqGeom::Point3d const b2(bc.x + hw, bc.y - hd, bc.z - hh);
    dqGeom::Point3d const b3(bc.x + hw, bc.y + hd, bc.z - hh);
    dqGeom::Point3d const b4(bc.x - hw, bc.y + hd, bc.z - hh);
    dqGeom::Point3d const t1(bc.x - hw, bc.y - hd, bc.z + hh);
    dqGeom::Point3d const t2(bc.x + hw, bc.y - hd, bc.z + hh);
    dqGeom::Point3d const t3(bc.x + hw, bc.y + hd, bc.z + hh);
    dqGeom::Point3d const t4(bc.x - hw, bc.y + hd, bc.z + hh);
    auto box = dqGeom::PolyfaceBuilder::create();
    {
        dqGeom::Point3d const q1[4] = {b4, b3, b2, b1};  // bottom
        dqGeom::Point3d const q2[4] = {t1, t2, t3, t4};  // top
        dqGeom::Point3d const q3[4] = {b1, b2, t2, t1};  // front
        dqGeom::Point3d const q4[4] = {b2, b3, t3, t2};  // right
        dqGeom::Point3d const q5[4] = {b3, b4, t4, t3};  // back
        dqGeom::Point3d const q6[4] = {b4, b1, t1, t4};  // left
        box->AddQuadFacet(q1);
        box->AddQuadFacet(q2);
        box->AddQuadFacet(q3);
        box->AddQuadFacet(q4);
        box->AddQuadFacet(q5);
        box->AddQuadFacet(q6);
    }

    Scene scene;
    {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        // 参考：橙轮廓 + 半透明填充（255,165,0 / 255,165,0,128）
        builder->setSymbology(ColorDef::from(255, 165, 0), ColorDef::from(255, 165, 0), 2);
        builder->addPolyface(*pyramid->ClaimPolyface(), true);
        scene.foreground.push_back(builder->finish());
    }
    {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        // 参考：青轮廓 + 半透明填充（100,255,255 / 100,255,255,100）
        builder->setSymbology(ColorDef::from(100, 255, 255), ColorDef::from(100, 255, 255), 2);
        builder->addPolyface(*box->ClaimPolyface(), true);
        scene.foreground.push_back(builder->finish());
    }
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("PolyfacePrimitives", readUniqueColors(*target),
                           {{255, 165, 0}, {100, 255, 255}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts:447-497
//              CesiumDecorator.createSolidPrimitiveDecorations（Sphere/Cone 段；
//              Box 段由 DecoratorPrimitivesRenderTheirColors 覆盖）
//              + openBlankViewport.ts readUniqueColors
// Sphere.createCenterRadius 与 Cone（createAxisPoints 等价：底/顶圆心 + 半径 a/b、
// capped），断言两轮廓色出现。
TEST(RenderSmokeTest, SolidPrimitiveSphereAndConeRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    auto sphere = dqGeom::Sphere::CreateCenterRadius(
        dqGeom::Point3d(-0.4, 0.0, 0.0), 0.3,
        dqGeom::Sphere::FullLatitudeSweep(), /*capped=*/true);
    ASSERT_TRUE(sphere);
    auto cone = dqGeom::Cone::CreateBaseAndTarget(
        dqGeom::Point3d(0.5, 0.0, -0.25), dqGeom::Point3d(0.5, 0.0, 0.25),
        dqGeom::Vector3d::UnitX(), dqGeom::Vector3d::UnitY(),
        /*radiusA=*/0.2, /*radiusB=*/0.0, /*capped=*/true);
    ASSERT_TRUE(cone);

    Scene scene;
    {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        // 参考：蓝轮廓 + 半透明填充（100,100,255 / 100,100,255,120）
        builder->setSymbology(ColorDef::from(100, 100, 255), ColorDef::from(100, 100, 255), 2);
        builder->addSolidPrimitive(*sphere);
        scene.foreground.push_back(builder->finish());
    }
    {
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        // 参考：绿轮廓 + 半透明填充（100,255,100 / 100,255,100,100）
        builder->setSymbology(ColorDef::from(100, 255, 100), ColorDef::from(100, 255, 100), 2);
        builder->addSolidPrimitive(*cone);
        scene.foreground.push_back(builder->finish());
    }
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("SolidSphereCone", readUniqueColors(*target),
                           {{100, 100, 255}, {100, 255, 100}});
}

// Ported from: itwinjs-core test-apps/display-test-app EmptyExample.ts
//              CesiumDecorator 各 createXxxDecorations 的 2d 变体段
//              （addPointString2d:71-77 / addLineString2d:107-115 / addShape2d:154-162 /
//               addArc2d:246-253）+ openBlankViewport.ts readUniqueColors
// 四个 2d 变体（zDepth 提升到 z 平面后走 3d 虚函数），断言各自颜色出现。
TEST(RenderSmokeTest, TwoDeePrimitiveVariantsRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    Scene scene;
    {  // 金色 2d 点串（参考 overlayPoints + addPointString2d w2）
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(255, 215, 0), ColorDef::from(255, 215, 0), 2);
        dqGeom::Point2d const pts[1] = {dqGeom::Point2d(-0.7, 0.7)};
        builder->addPointString2d(pts, 1, 0.06);
        scene.foreground.push_back(builder->finish());
    }
    {  // 青色 2d 线串矩形（参考 overlayLinePoints + addLineString2d w3）
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(0, 200, 255), ColorDef::from(0, 200, 255), 3);
        dqGeom::Point2d const pts[5] = {
            dqGeom::Point2d(0.3, -0.9), dqGeom::Point2d(0.9, -0.9),
            dqGeom::Point2d(0.9, -0.3), dqGeom::Point2d(0.3, -0.3),
            dqGeom::Point2d(0.3, -0.9),
        };
        builder->addLineString2d(pts, 5, 0.08);
        scene.foreground.push_back(builder->finish());
    }
    {  // 紫红 2d 实心矩形（参考 overlayShapePoints + addShape2d w3）
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(186, 85, 211), ColorDef::from(186, 85, 211), 3);
        dqGeom::Point2d const pts[5] = {
            dqGeom::Point2d(0.3, 0.3), dqGeom::Point2d(0.9, 0.3),
            dqGeom::Point2d(0.9, 0.9), dqGeom::Point2d(0.3, 0.9),
            dqGeom::Point2d(0.3, 0.3),
        };
        builder->addShape2d(pts, 5, 0.09);
        scene.foreground.push_back(builder->finish());
    }
    {  // 绿松石 2d 实心椭圆（参考 overlayFullEllipse + addArc2d isEllipse/filled）
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        ASSERT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(64, 224, 208), ColorDef::from(64, 224, 208), 2);
        auto ellipse = dqGeom::Arc3d::FromVectors(
            dqGeom::Point3d(-0.5, -0.5, 0.0), dqGeom::Vector3d::From(0.25, 0, 0),
            dqGeom::Vector3d::From(0, 0.4, 0), dqGeom::AngleSweep::FullCircle());
        ASSERT_TRUE(ellipse);
        builder->addArc2d(*ellipse, /*isEllipse=*/true, /*filled=*/true, 0.12);
        scene.foreground.push_back(builder->finish());
    }
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("TwoDeeVariants", readUniqueColors(*target),
                           {{255, 215, 0}, {0, 200, 255}, {186, 85, 211}, {64, 224, 208}});
}

// ---------------------------------------------------------------------------
// GraphicType 渲染路径验证（每型一个像素断言）
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core EmptyExample.ts CesiumDecorator + Decorations.ts
//              DecorateContext.addDecoration(GraphicType.WorldDecoration, g)
// GraphicType::WorldDecoration 经 changeDecorations 入 world 桶 → addWorldDecorations
// 包 WorldDecorations branch → opaque 渲染。一条红线横穿画面断言其颜色。
TEST(RenderSmokeTest, WorldDecorationRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    Decorations decorations;
    auto line = finishColoredLine(env, GraphicType::WorldDecoration,
                                  dqGeom::Point3d(-0.8, -0.6, 0.0),
                                  dqGeom::Point3d(0.8, -0.6, 0.0),
                                  255, 0, 0);
    ASSERT_TRUE(line);
    decorations.add(GraphicType::WorldDecoration, line);
    target->changeDecorations(decorations);

    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("WorldDecoration", readUniqueColors(*target), {{255, 0, 0}});
}

// Ported from: itwinjs-core EmptyExample.ts CesiumDecorator + Decorations.ts
//              DecorateContext.addDecoration(GraphicType.WorldOverlay, g)
// GraphicType::WorldOverlay 经 changeDecorations 入 worldOverlay 桶 →
// RenderPass::WorldOverlay（overlay 混合 pass，世界单位）。一条绿线断言其颜色。
TEST(RenderSmokeTest, WorldOverlayRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    Decorations decorations;
    auto line = finishColoredLine(env, GraphicType::WorldOverlay,
                                  dqGeom::Point3d(-0.8, 0.0, 0.0),
                                  dqGeom::Point3d(0.8, 0.0, 0.0),
                                  0, 255, 0);
    ASSERT_TRUE(line);
    decorations.add(GraphicType::WorldOverlay, line);
    target->changeDecorations(decorations);

    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("WorldOverlay", readUniqueColors(*target), {{0, 255, 0}});
}

// Ported from: itwinjs-core EmptyExample.ts CesiumDecorator + Decorations.ts
//              DecorateContext.addDecoration(GraphicType.ViewOverlay, g)
// GraphicType::ViewOverlay 经 changeDecorations 入 viewOverlay 桶 →
// RenderPass::ViewOverlay（overlay 混合 pass，视图单位）。一条蓝线断言其颜色。
TEST(RenderSmokeTest, ViewOverlayRendersItsColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    Decorations decorations;
    auto line = finishColoredLine(env, GraphicType::ViewOverlay,
                                  dqGeom::Point3d(-0.8, 0.6, 0.0),
                                  dqGeom::Point3d(0.8, 0.6, 0.0),
                                  0, 0, 255);
    ASSERT_TRUE(line);
    decorations.add(GraphicType::ViewOverlay, line);
    target->changeDecorations(decorations);

    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("ViewOverlay", readUniqueColors(*target), {{0, 0, 255}});
}

// Scene.foreground 已是 DecoratorPrimitivesRenderTheirColors 覆盖的 Scene 类型
// 路径；此处显式对照：同一 builder（GraphicType::Scene）直接入 Scene.foreground
// 与经 changeDecorations 入 normal 桶，两路径都应渲出同一颜色。
// Ported from: itwinjs-core openBlankViewport.ts readUniqueColors
TEST(RenderSmokeTest, SceneGraphicRendersViaBothSceneAndDecorationPaths) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    // 路径 1：Scene.foreground（GraphicType::Scene 的标准归属）
    Scene scene;
    scene.foreground.push_back(finishColoredLine(env, GraphicType::Scene,
                                                 dqGeom::Point3d(-0.8, -0.8, 0.0),
                                                 dqGeom::Point3d(0.8, -0.8, 0.0),
                                                 255, 255, 0));
    target->changeScene(scene);

    // 路径 2：Decorations.normal（GraphicType::Scene 装饰桶，与 scene 同绘）
    Decorations decorations;
    auto line = finishColoredLine(env, GraphicType::Scene,
                                  dqGeom::Point3d(-0.8, 0.8, 0.0),
                                  dqGeom::Point3d(0.8, 0.8, 0.0),
                                  255, 255, 0);
    ASSERT_TRUE(line);
    decorations.add(GraphicType::Scene, line);
    target->changeDecorations(decorations);

    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("SceneBothPaths", readUniqueColors(*target), {{255, 255, 0}});
}

// Ported from: itwinjs-core ViewContext.drawStandardGrid (ViewContext.ts:348) —
//              createPlanarGrid(vp.getFrustum(), props) + PlanarGrid.ts 程序网格
// dqApp 的 viewFlags.grid 门控在 Viewport.cpp:1141（drawStandardGrid 调用点）；
// dqRender 冒烟层绕过 viewFlags，直接验证 viewFlags.grid=ON 时 target 应渲染的
// 图形对象（createPlanarGrid(frustum, props) → Primitive → Scene.foreground）。
// 网格面在 z=0，间距 0.2（100px 视口内约 5 条主线）——断言近白的网格线色出现。
TEST(RenderSmokeTest, PlanarGridRendersGridLines) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    // 视锥 = 俯视视口的世界体积（±1 平面, z∈[-5,5]）——等价 vp.getFrustum()
    dqGeom::Range3d volume;
    volume.low = dqGeom::Point3d(-1.0, -1.0, -5.0);
    volume.high = dqGeom::Point3d(1.0, 1.0, 5.0);
    auto frustum = dqCommon::Frustum::fromRange(volume);

    PlanarGridProps props;
    props.origin = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    props.rMatrix = dqGeom::Matrix3d::CreateIdentity();
    props.spacing = dqGeom::Point2d::From(0.2, 0.2);
    props.gridsPerRef = 5.0;
    props.color = dqCommon::ColorDef::from(200, 200, 200);  // 浅灰网格线

    auto* grid = env.system->createPlanarGrid(frustum, props);
    ASSERT_NE(grid, nullptr) << "createPlanarGrid failed";

    Scene scene;
    scene.foreground.push_back(grid);
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    // OIT 语义（2026-09-12 网格修复后）：网格走 translucent+OIT 合成
    //（PlanarGrid.ts:39 + Composite.ts:106-116），黑背景上输出 = 半透明面+
    // 线叠加的连续灰阶，不再是旧 OpaquePlanar 直混的精确 (200,200,200)。
    // 断言网格可见：帧内存在亮度 >25 的非背景像素。
    {
        auto fc = readUniqueColors(*target);
        bool anyVisible = false;
        for (uint32_t c : fc.colors) {
            uint8_t r = static_cast<uint8_t>(c >> 24), g = static_cast<uint8_t>(c >> 16),
                     b = static_cast<uint8_t>(c >> 8);
            int l = (int(r) * 299 + int(g) * 587 + int(b) * 114) / 1000;
            if (l > 25) { anyVisible = true; break; }
        }
        EXPECT_TRUE(anyVisible) << "planar grid must render (OIT) against black background";
    }
}

// Ported from: itwinjs-core AuxCoordSys.ts:181-299 (AuxCoordSystemState.display /
//              addAxis X=red Y=green Z=blue) + AccuDraw.ts:2287-2297 (viewFlags.acsTriad
//              门控) + dqApp AcsTriadDecorator（WorldOverlay 桶）
// ACS triad 的 viewFlags 门控与 DecorateContext 接线在 dqApp（Decorate 按
// viewFlags.acsTriad 画原点三轴）；冒烟层验证该装饰器的语义等价物：WorldOverlay
// 桶中自原点发出的三条着色轴线（X 红 / Y 绿 / Z 蓝）全部渲出。
TEST(RenderSmokeTest, AcsTriadAxesRenderTheirColors) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    Decorations decorations;
    auto const o = dqGeom::Point3d(0.0, 0.0, 0.0);
    auto xAxis = finishColoredLine(env, GraphicType::WorldOverlay,
                                   o, dqGeom::Point3d(0.6, 0.0, 0.0), 255, 0, 0);
    auto yAxis = finishColoredLine(env, GraphicType::WorldOverlay,
                                   o, dqGeom::Point3d(0.0, 0.6, 0.0), 0, 255, 0);
    // Z 轴沿视线方向时俯视投影退化为点（itwinjs 中 ACS triad 的 Z 分量本就
    // 投影为原点处的圆点/短线）；冒烟断言用微倾斜的 Z 轴（非零 xy 分量）验证
    // 「Z 色（蓝）确实画出来了」，与 ACS 红/绿轴同桶同 pass。
    auto zAxis = finishColoredLine(env, GraphicType::WorldOverlay,
                                   o, dqGeom::Point3d(0.2, 0.2, 0.6), 0, 0, 255);
    ASSERT_TRUE(xAxis && yAxis && zAxis);
    decorations.add(GraphicType::WorldOverlay, xAxis);
    decorations.add(GraphicType::WorldOverlay, yAxis);
    decorations.add(GraphicType::WorldOverlay, zAxis);
    target->changeDecorations(decorations);

    target->changeRenderPlan(blackBackgroundPlan());
    target->drawFrame();

    expectAllColorsPresent("AcsTriad", readUniqueColors(*target),
                           {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}});
}

// Ported from: itwinjs-core Target.setHiliteSet() (Target.ts) + hilite pass
//              (SceneCompositorImpl renderHilite/Composite) + FeatureSymbology
//              applyFeatureSymbology (kOvrBit_Hilited → mix(baseColor, hiliteColor, 0.5))
// 选择/hilite 链：PolyfaceGraphic::setFeatureId 标记 feature →
// target->setHilitedFeature(id)（LUT 置位）+ setHiliteColor → drawFrame 的
// hilite pass 把 hilited 像素混向 hiliteColor。红 Box + 蓝 hilite → 像素变紫。
//
// GraphicBuilder 的 pickable 链（activatePickableId → PackedFeatureTable）尚未接线
//（PrimitiveBuilder.h:59 待激活）；此用例直接驱动 PolyfaceGraphic 的 setFeatureId —
// 与 RenderPipeline::drawForPick 同一形态。
TEST(RenderSmokeTest, HilitePassShiftsSelectedFeatureColor) {
    OffscreenRenderEnv env;
    auto target = makeTopViewTarget(env);
    ASSERT_NE(target, nullptr) << "offscreen GL environment unavailable";

    // 红 Box polyface（PolyfaceBuilder 手工构面，unit Box），featureId=42。
    dqGeom::Point3d const c(0.0, 0.0, 0.0);
    double const h = 0.3;
    dqGeom::Point3d const b1(c.x - h, c.y - h, c.z - h), b2(c.x + h, c.y - h, c.z - h);
    dqGeom::Point3d const b3(c.x + h, c.y + h, c.z - h), b4(c.x - h, c.y + h, c.z - h);
    dqGeom::Point3d const t1(c.x - h, c.y - h, c.z + h), t2(c.x + h, c.y - h, c.z + h);
    dqGeom::Point3d const t3(c.x + h, c.y + h, c.z + h), t4(c.x - h, c.y + h, c.z + h);
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
    // 红填充：PolyfaceGraphic defaultColor 是 RGBA 字节序（[24..31]=R … [0..7]=A，
    // PolyfaceGraphic.cpp:47-50）——红不透明 = 0xFF0000FF。
    auto* geom = new PolyfaceGraphic(*env.driver, *box->ClaimPolyface(),
                                     /*defaultColor=*/0xFF0000FF);
    // featureId = 该 feature 在 batch LUT 中的**索引**（0..numFeatures-1）——override
    // shader 以 (v_featureId+0.5)*u_featureOverrideWidth 读 LUT texel（3 texel/feature
    // 行主序），不是 elementId。表内唯一 feature（elementId 42）的索引是 0
    // （构造默认 featureId=0，无需再设——顶点在构造期烘入）。
    auto* prim = new Primitive(geom);

    // hilite 是 Batch 专属路径（RenderCommands::addBatch 在
    // batch.hasFeatureOverrides() && lut.anyHilited() 时发 hilite 命令）——
    // PrimitiveBuilder 的 pickable 链（activatePickableId → PackedFeatureTable →
    // Batch）尚未接线（PrimitiveBuilder.h:59），此用例手工组 Batch 验证 hilite pass：
    //   FeatureTable(1 feature id=42) → Batch(1, table) → Primitive 子节点。
    // Ported from: itwinjs-core RenderCommands.addBatch hilite branch (line 692-696).
    auto featureTable = std::make_unique<dqCommon::FeatureTable>(1);
    featureTable->insert(dqCommon::Feature(dqBase::DqId(42), dqBase::DqId(),
                                           dqCommon::GeometryClass::Primary));
    auto batch = std::make_unique<Batch>(1, std::move(featureTable));
    // batch LUT 是 addBatch 检查的 override 载体（hilite 命令的触发条件
    // hasFeatureOverrides && anyHilited）；target 的 hiliteColor 单独供给
    // u_hiliteColor。先建 LUT 再置 hilite 位——参考 setHiliteSet 的等价物。
    batch->getOrCreateFeatureOverrideLUT().setFeatureHilited(0, true);
    batch->setChild(std::unique_ptr<Graphic>(prim));

    Scene scene;
    scene.foreground.push_back(batch.release());
    target->changeScene(scene);
    target->changeRenderPlan(blackBackgroundPlan());

    // hilite：经参考路径 setHiliteSet（elementId 42）——drawFrame 的 PushBatch
    // 惰性更新 batch LUT（FeatureOverrides.update → updateHilite 等价）；不再
    // 手工置位（version 失配会被惰性更新覆盖）。hiliteColor 蓝色供 override
    // shader 的 Hilited mix（参考默认比 0.25，Hilite.ts:53——断言容差覆盖）。
    target->setHiliteColor(0.0f, 0.0f, 1.0f);
    uint32_t const hilitedIds[1] = {42u};
    target->setHiliteSet(hilitedIds, 1);
    target->drawFrame();

    // 红 Box (255,0,0) × 蓝 hilite mix 0.25 → (191,0,64)。整个 Box 都被 hilited
    // （唯一 feature）→ 无残红；主帧颜色全部被 override shader 的 Hilited mix
    // 替换。断言：混合色出现、原始红消失。
    auto const fc = readUniqueColors(*target);
    bool const hasRed = hasColorNear(fc.colors, 255, 0, 0);
    bool const hasPurple = hasColorNear(fc.colors, 191, 0, 64, 30);
    if (!hasPurple)
        dumpFramePngFlipped("RenderSmokeTest_Hilite.png", fc.px, 100, 100);
    EXPECT_FALSE(hasRed) << "hilited feature should no longer render its base color";
    EXPECT_TRUE(hasPurple) << "hilite pass should mix selected feature toward hiliteColor";
}

// Ported from: itwinjs-core openEmptyExample.ts:11-21
//              (setStandardRotation(StandardViewId.Iso) + turnCameraOn +
//               zoomToVolume(projectExtents.expandInPlace(120000)))
// 相机/方向对象素的影响：同一居中 Box 在正交俯视 vs ISO 透视下的 footprint 区域
// 必须显著不同（ISO 旋转斜切画面，正视只占中心方形区域）。
//
// 两帧像素断言：帧1 正交俯视（makeTopViewTarget 既有相机）；帧2 ISO 旋转 +
// 透视（turnCameraOn）+ zoomToVolume 等价的 view/proj 矩阵。ISO 旋转 =
// 绕 X 轴 -35.264°(atan(1/√2)) 后绕 Y 轴 -45°，相机在视线方向上后退使
// Box 完整入画（zoomToVolume 的缩放等价）；45° fovy 透视（turnCameraOn）。
TEST(RenderSmokeTest, IsoPerspectiveCameraChangesFootprint) {
    OffscreenRenderEnv envOrtho;
    auto targetOrtho = makeTopViewTarget(envOrtho);
    ASSERT_NE(targetOrtho, nullptr) << "offscreen GL environment unavailable";

    using dqCommon::ColorDef;
    auto makeBoxScene = [](OffscreenRenderEnv& env) {
        Scene scene;
        auto builder = env.system->createGraphicBuilder(makeSceneBuilderOptions());
        EXPECT_NE(builder, nullptr);
        builder->setSymbology(ColorDef::from(0, 0, 255), ColorDef::from(0, 0, 255), 0);
        dqGeom::Point3d boxCorners[2] = {
            dqGeom::Point3d(-0.25, -0.25, -0.25),
            dqGeom::Point3d(0.25, 0.25, 0.25),
        };
        auto box = dqGeom::Box::CreateRange(dqGeom::Range3d(boxCorners[0], boxCorners[1]), true);
        builder->addSolidPrimitive(*box);
        scene.foreground.push_back(builder->finish());
        return scene;
    };

    // --- 帧 1：正交俯视 ---
    Scene scene = makeBoxScene(envOrtho);
    targetOrtho->changeScene(scene);
    targetOrtho->changeRenderPlan(blackBackgroundPlan());
    targetOrtho->drawFrame();
    auto const fcOrtho = readUniqueColors(*targetOrtho);
    EXPECT_TRUE(hasColorNear(fcOrtho.colors, 0, 0, 255)) << "ortho: blue box absent";
    // 俯视下 Box  footprint：中心 50x50 方形区域（±0.25 世界 → NDC ±0.25 → 像素 25..75）
    auto countBlue = [](FrameColors const& fc) {
        size_t n = 0;
        for (size_t i = 0; i + 3 < fc.px.size(); i += 4)
            if (fc.px[i + 2] > 120 && fc.px[i + 2] > fc.px[i] + 40 &&
                fc.px[i + 2] > fc.px[i + 1] + 40)
                ++n;
        return n;
    };
    size_t const orthoBlue = countBlue(fcOrtho);

    // --- 帧 2：ISO 旋转 + 透视（新 target，换相机）---
    OffscreenRenderEnv envIso;
    ASSERT_TRUE(envIso.init()) << "offscreen GL environment unavailable";
    envIso.system->setTechniques(createDefaultTechniques(*envIso.driver).release());
    auto targetIso = envIso.system->createTarget(nullptr, 100, 100);
    ASSERT_NE(targetIso, nullptr);

    // lookAt(eye=(2.5,2.5,2.5), target=origin, up=+Z)：右手相机系，相机看向 -z。
    //   zc = normalize(eye - target)（从目标指向相机）
    //   xc = normalize(up × zc)；yc = zc × xc
    //   view = [xc yc zc -eye]^T 平移：v' = Rᵀ·(p - eye)，R 行 = xc/yc/zc
    // （zoomToVolume 的缩放等价于后退距离选择——Box ±0.25 完整入画）
    double const eye[3] = {2.5, 2.5, 2.5};
    double const target[3] = {0.0, 0.0, 0.0};
    double const up[3] = {0.0, 0.0, 1.0};
    auto sub = [](double const* a, double const* b) {
        return std::array<double, 3>{a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    };
    auto cross = [](double const* a, std::array<double, 3> const& b) {
        return std::array<double, 3>{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
    };
    auto norm = [](std::array<double, 3>& v) {
        double const l = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        v[0] /= l; v[1] /= l; v[2] /= l;
    };
    auto zc = sub(eye, target); norm(zc);
    auto xc = cross(up, zc);    norm(xc);
    auto yc = [&]() {
        return std::array<double, 3>{zc[1]*xc[2]-zc[2]*xc[1], zc[2]*xc[0]-zc[0]*xc[2], zc[0]*xc[1]-zc[1]*xc[0]};
    }();
    // view 列主序：行 = (xc,tx),(yc,ty),(zc,tz)，t = -basis·eye
    double const tx = -(xc[0]*eye[0] + xc[1]*eye[1] + xc[2]*eye[2]);
    double const ty = -(yc[0]*eye[0] + yc[1]*eye[1] + yc[2]*eye[2]);
    double const tz = -(zc[0]*eye[0] + zc[1]*eye[1] + zc[2]*eye[2]);
    float const view[16] = {
        static_cast<float>(xc[0]), static_cast<float>(yc[0]), static_cast<float>(zc[0]), 0.f,
        static_cast<float>(xc[1]), static_cast<float>(yc[1]), static_cast<float>(zc[1]), 0.f,
        static_cast<float>(xc[2]), static_cast<float>(yc[2]), static_cast<float>(zc[2]), 0.f,
        static_cast<float>(tx),    static_cast<float>(ty),    static_cast<float>(tz),    1.f,
    };
    // 45° fovy 透视（turnCameraOn），aspect=1，n=0.1，f=20：
    //   [f_/a, 0, 0, 0; 0, f_, 0, 0; 0, 0, (f+n)/(n-f), -1; 0, 0, 2fn/(n-f), 0]
    double const fovY = dqGeom::Angle::kPi / 4.0;
    double const f_ = 1.0 / std::tan(fovY / 2.0);
    double const zn = 0.1, zf = 20.0;
    float const proj[16] = {
        static_cast<float>(f_), 0.f, 0.f, 0.f,
        0.f, static_cast<float>(f_), 0.f, 0.f,
        0.f, 0.f, static_cast<float>((zf + zn) / (zn - zf)), -1.f,
        0.f, 0.f, static_cast<float>(2.0 * zf * zn / (zn - zf)), 0.f,
    };
    // worldToNdc = proj · view（列主序乘法）
    float mvp[16] = {};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k)
                s += proj[k * 4 + row] * view[col * 4 + k];
            mvp[col * 4 + row] = s;
        }
    targetIso->setViewportTransform(view, mvp);

    // ef247b8：透视帧的 plan 需要 ISO 相机的视域 + 参考式 fraction
    //（frontFraction/backFraction = zn/zf，ViewState.ts:730-732），覆盖
    // blackBackgroundPlan 的 Top 正交盒。参考几何（computeWorldToNpc
    // ViewState.ts:735-748）：Frustum 角点是 **back 尺寸的平行六面体**——
    // near/far 两面同宽（xExtent/yExtent 取 back 面全宽），透视由 fraction
    // 编码（changeFrustum 从 zVec=farLL→nearLL 外推 cameraPosition，
    // FrustumUniforms.ts:138-140）；不是物理锥台（物理台体的近面收缩会让
    // zVec 带横向分量、外推点偏移 → 投影错位）。
    RenderPlan planIso = blackBackgroundPlan();
    planIso.fraction = zn / zf;
    double const tanHalf = std::tan(fovY / 2.0);
    // near 面 = 物理锥台收拢（hw=dist·tanHalf）——与参考 computeWorldToNpc
    // 公式闭合等价（focus=zf 简化下 zExtent.xy 的仿射收拢恰使 near 半宽
    // = zn·tanHalf，且 cameraPosition 外推（FrustumUniforms.ts:138-140
    // farLL+zVec·1/(1−fraction)）精确还原真 eye）。
    auto isoCorner = [&](dqCommon::Npc npc, double dist, double sx, double sy) {
        double const hw = dist * tanHalf;
        planIso.frustum.getCorner(npc) = dqGeom::Point3d::From(
            eye[0] - dist * zc[0] + sx * hw * xc[0] + sy * hw * yc[0],
            eye[1] - dist * zc[1] + sx * hw * xc[1] + sy * hw * yc[1],
            eye[2] - dist * zc[2] + sx * hw * xc[2] + sy * hw * yc[2]);
    };
    // Npc 标准排布（同 Top 正交盒：Left=sx−、Right=sx+）——viewX=+xc、
    // viewY=+yc、viewZ=X×Y=+zc（朝 eye），参考相机系手性；标反会使 viewZ
    // 反向 → z_view 符号翻转 → 透视 w<0 → 盒被裁（w 反手性排查实录）。
    isoCorner(dqCommon::Npc::LeftBottomFront,  zn, -1, -1);
    isoCorner(dqCommon::Npc::RightBottomFront, zn, +1, -1);
    isoCorner(dqCommon::Npc::LeftTopFront,     zn, -1, +1);
    isoCorner(dqCommon::Npc::RightTopFront,    zn, +1, +1);
    isoCorner(dqCommon::Npc::LeftBottomRear,   zf, -1, -1);
    isoCorner(dqCommon::Npc::RightBottomRear,  zf, +1, -1);
    isoCorner(dqCommon::Npc::LeftTopRear,      zf, -1, +1);
    isoCorner(dqCommon::Npc::RightTopRear,     zf, +1, +1);

    Scene sceneIso = makeBoxScene(envIso);
    targetIso->changeScene(sceneIso);
    targetIso->changeRenderPlan(planIso);
    targetIso->drawFrame();
    auto const fcIso = readUniqueColors(*targetIso);
    EXPECT_TRUE(hasColorNear(fcIso.colors, 0, 0, 255)) << "iso: blue box absent";
    size_t const isoBlue = countBlue(fcIso);

    // ISO 旋转斜切画面 → footprint 与正交俯视显著不同（面积/形状变化）。
    // 正交俯视 Box 占满 50x50=2500 像素（投影 footprint 方形）；ISO 透视旋转后
    // footprint 是斜六边形，面积更小且不再是轴对齐方形。
    EXPECT_NE(isoBlue, orthoBlue)
        << "ISO+camera footprint should differ from orthographic top view";
    // 两侧都应有可观像素（非全零 / 非全屏）
    EXPECT_GT(isoBlue, 100u) << "iso: too few blue pixels (box clipped?)";
    EXPECT_LT(isoBlue, 9000u) << "iso: blue fills nearly the whole frame (zoom too far in?)";
    if (isoBlue == 0u || isoBlue >= 9000u)
        dumpFramePngFlipped("RenderSmokeTest_IsoPerspective.png", fcIso.px, 100, 100);
}
