// ThematicDisplayE2ETest — Thematic Display 引擎链像素锁（M-S S-d 起，
// S-e 扩 IDW + 视空间归位）。
//
// 链（全段本里程碑接线）：ViewSettings/预设 → DisplayStyle3dSettings.thematic
// + viewFlags.thematicDisplay → Viewport::ValidateRenderPlan（RenderPlan.ts:127
// 门）→ Target.changeRenderPlan → ThematicUniforms.update（渐变纹理 1×N 列向
// + NEAREST；传感器 1×N RGBA32F 视空间打包 + 逐帧惰性刷新）→ drawPass
// flags.isThematic（DrawCommand.ts 206-216——**读绘制栈当前 vf**：S-e 修复
// BranchStack 双栈分裂[compositor/target 各持一栈→参考 BranchUniforms._stack
// 单栈归位] + drawFrame 根 push vf 继承化，此前门恒假）→ Surface thematic
// 变体（rawPosition 顶点索引 + 渐变采样片元 + IDW 传感器循环）。
//
// 锁序（声明序非 dump 先行——防跨测试污染；TD-31 取证另见 MinimalSolidBox
// 内的 fetcher 重挂注记）：
//   1. MinimalSolidBoxHeightAndSlopeAreExact——tileset 三板收敛锁（thematic
//      重上色后三板收敛同族、与原色全不同；Slope 臂分流实证）。
//   2. TallBoxGradientSweepIsExact——2×2×10 盒（GraphicType::Scene 挂接——
//      WorldDecoration 对 thematic 豁免是参考刻意语义[Target.ts:236-243/
//      Graphic.ts:484-491]，S-e 取证后改挂）；TD-30 z 压平形态下 Height 段
//      全域低端纯蓝、Slope 段侧面归白端。
//   3. HeightGradientColorsByWorldZAndRestores——joeshouse Height（蓝族主导
//      + 蓝道 stdB 展布 + 关闭恢复精确）。
//   4. SlopeDistinguishesVerticalFromFlat——joeshouse Slope（Custom 双色键，
//      关光照纯渐变域：竖直面 ndx=1 白族 487891 / 水平面 ndx=0 暗族 557104
//      ——S-e 视空间归位后钉值）。
//   5. SensorIdwColorSeparatesNearEachSensor——joeshouse IDW 双传感器
//     （S-e 主件：红族质心 (736,571)[A 端值 1] / 蓝族 (1408,924)[B 端值 0]、
//      质心 2D 分离 786px——视空间 1/dist² 加权场直接证据）。
//
// Authored: no reference test exists in itwinjs-core for thematic pixel output
//          （参考的渲染覆盖在 DTA 交互式会话，无离线回放对应物；渲染像素回归
//           授权 §5(g)：复现配方 = joeshouse dump 打开链 saved 视图 +
//           overrideDisplayStyle；证据链 = 渐变键字节锁（GradientThematicTest）
//           + uniform 值锁（ThematicDisplayShaderTest）+ readPixels WHERE
//           断言）。dump 资产只读（§11.11）；钉值 = 首绿实测。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include "FileTileFetcher.h"  // dqApp/src（测试隔离重挂——TD-31，见 MinimalSolidBox 注）
#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/StandardView.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqRender/tile/RealityTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvTHM {
    QtEnvTHM()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvTHM s_qtTHM;

std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinTHM(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

void dumpBmpTHM(std::vector<uint8_t> const& frame, uint32_t w, uint32_t h,
                char const* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    uint32_t const rowBytes = w * 4;
    unsigned char head[54] = {};
    head[0] = 'B';
    head[1] = 'M';
    *reinterpret_cast<uint32_t*>(&head[2]) = 54 + rowBytes * h;
    *reinterpret_cast<uint32_t*>(&head[10]) = 54;
    *reinterpret_cast<uint32_t*>(&head[14]) = 40;
    *reinterpret_cast<uint32_t*>(&head[18]) = w;
    *reinterpret_cast<uint32_t*>(&head[22]) = h;
    *reinterpret_cast<uint16_t*>(&head[26]) = 1;
    *reinterpret_cast<uint16_t*>(&head[28]) = 32;
    fwrite(head, 1, 54, f);
    std::vector<unsigned char> row(rowBytes);
    for (uint32_t y = 0; y < h; ++y) {
        memcpy(row.data(), &frame[static_cast<size_t>(y) * rowBytes], rowBytes);
        // BI_RGB 32bpp = BGRX，内存帧 = RGBA——逐像素换 R/B 落盘（M-J(3) 仪器修正）。
        for (uint32_t x = 0; x < w; ++x)
            std::swap(row[x * 4 + 0], row[x * 4 + 2]);
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
    printf("[THM] frame dumped to %s\n", path);
}

// 主导色分类（JoesHouseColor classifyBand 同族）：红族 = r 显著大于 b；
// 蓝族 = b 显著大于 r。thematic 渐变经光照后通道关系保持。
struct HueStats {
    long count = 0;
    double cx = 0.0, cy = 0.0;
};

HueStats classifyHue(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                     uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1,
                     bool wantRed)
{
    HueStats out;
    long long sx = 0, sy = 0;
    for (uint32_t y = y0; y < y1 && y < h; ++y)
        for (uint32_t x = x0; x < x1 && x < w; ++x) {
            uint8_t const* p = &f[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            bool const hit = wantRed
                ? (r >= 90 && r > b + 40 && r > g)
                : (b >= 90 && b > r + 40 && b > g);
            if (hit) {
                ++out.count;
                sx += x;
                sy += y;
            }
        }
    if (out.count > 0) {
        out.cx = static_cast<double>(sx) / out.count;
        out.cy = static_cast<double>(sy) / out.count;
    }
    return out;
}

long countDifferent(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b)
{
    if (a.size() != b.size())
        return -1;
    long n = 0;
    for (size_t i = 0; i + 2 < a.size(); i += 4) {
        if (std::abs(int(a[i]) - int(b[i])) > 8 ||
            std::abs(int(a[i + 1]) - int(b[i + 1])) > 8 ||
            std::abs(int(a[i + 2]) - int(b[i + 2])) > 8)
            ++n;
    }
    return n;
}

struct ThematicFrame {
    dta::DumpOpenResult opened;
    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
};

// 打开 joeshouse + 泵至静默 + 帧稳定（JoesHouseColor 同款 harness）。
void OpenAndStabilize(Gui::View3DInventor& view, ThematicFrame& out)
{
    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/joeshouse-v1";
    pkg.tileRoots = {kDumpRoot + "/joeshouse-v1", kDumpRoot + "/joeshouse-drill-v1",
                     kDumpRoot + "/joeshouse-drill-v2"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;

    size_t lastLogSize = 0;
    int stable = 0;
    bool quiesced = false;
    for (int i = 0; i < 200 && !quiesced; ++i) {
        view.getUeViewport()->InvalidateController();
        spinTHM(100);
        size_t const logSize = opened->fetcher->requestLog().size();
        if (!opened->fetcher->requestLog().empty()
            && logSize == lastLogSize
            && opened->fetcher->getActiveCount() == 0) {
            if (++stable >= 6)
                quiesced = true;
        } else {
            stable = 0;
        }
        lastLogSize = logSize;
    }
    ASSERT_TRUE(quiesced) << "saved view never quiesced (log="
                          << opened->fetcher->requestLog().size() << ")";
    out.opened = std::move(*opened);
}

// 帧稳定（连续两帧逐像素相等）。
void ReadStableFrame(Gui::View3DInventor& view, ThematicFrame& io)
{
    auto* vp = view.getUeViewport();
    std::vector<uint8_t> prev;
    bool frameStable = false;
    for (int i = 0; i < 40 && !frameStable; ++i) {
        spinTHM(150);
        vp->RenderFrame();
        std::vector<uint8_t> cur;
        uint32_t cw = 0, ch = 0;
        ASSERT_TRUE(vp->ReadFrameForTest(cur, cw, ch));
        io.w = cw;
        io.h = ch;
        if (!prev.empty() && prev == cur)
            frameStable = true;
        prev = std::move(cur);
    }
    ASSERT_TRUE(frameStable) << "frame never stabilized";
    io.frame = std::move(prev);
}

}  // namespace

// ---------------------------------------------------------------------------
// 锁声明序（gtest 声明序=执行序；非 dump 锁先行——跨测试污染族[dump 打开链
// 的 fetcher 替换不回溢，CMake 注册注记同族]）：MinimalSolidBox → TallBox →
// HeightGradientColors（joeshouse）→ SlopeDistinguishes（joeshouse）。
TEST(ThematicDisplayE2E, MinimalSolidBoxHeightAndSlopeAreExact)
{
    std::string const tilesetPath = DANQING_TILE_ASSETS_DIR "/minimal-solid/tileset.json";

    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ThematicDisplayE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    // 测试隔离（TD-31——全二进制模式取证实锤）：dump 族测试把 TileAdmin 全局
    // fetcher 换成 DumpTileFetcher 且不恢复（DumpOpenHelper.cpp:71——app 会话内
    // 多模型共享 dump 根的刻意语义）；本测试的文件 tileset 经
    // RealityTile::loadContent 的 `TileAdmin::instance().getFetcher()` 取数，
    // 若fetcher 滞留 dump 根则 .b3dm 命中 manifest NotFound → 内容永不达（全
    // 二进制下前序 dump 测试后 baseline 全黑实录）。重挂 FileTileFetcher =
    // Application::Startup 的初始面（Application.cpp:80）。
    dqRender::TileAdmin::instance().setFetcher(std::make_unique<dqApp::FileTileFetcher>());

    std::unique_ptr<dqRender::RealityTileTree> tree;  // 先声明（M-Q 同款析构序——view 先析构）

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinTHM(400);

    {
        std::ifstream in(tilesetPath, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot open " << tilesetPath;
        std::vector<uint8_t> jsonBytes((std::istreambuf_iterator<char>(in)),
                                        std::istreambuf_iterator<char>());
        tree = dqRender::RealityTileTree::loadTileset(tilesetPath, jsonBytes.data(), jsonBytes.size());
    }
    ASSERT_NE(tree, nullptr);
    view.getUeViewport()->AddTileTree(tree.get());

    auto* view3d = view.getUeViewport()->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-3, -1.2, -1.2, 3, 1.2, 1.2));
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
    }
    view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    auto* vp = view.getUeViewport();
    auto pumpStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 40; ++i) {
            prev = frame;
            spinTHM(120);
            vp->InvalidateController();
            vp->RenderFrame();
            ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    pumpStable(frame, w, h);
    ASSERT_FALSE(frame.empty());

    // --- Height：axis=+Z，range=[-0.5,0.5]（盒 z 全域——渐变满程）---
    // 判据（资产自适应——三板本色即红/绿/蓝，色族判据会被污染）：三板同高
    // 同向 → thematic 重上色后三板收敛同族色（与原色全不同）。面板中心采样。
    auto panelColor = [&](std::vector<uint8_t> const& fr, int idx) {
        // 三板中心（横排：左/中/右——首绿实测定心 x≈0.29/0.50/0.71w, y≈0.50h）。
        uint32_t const px = uint32_t(w * (0.29 + 0.21 * idx));
        uint32_t const py = uint32_t(h * 0.50);
        uint8_t const* p = &fr[(static_cast<size_t>(py) * w + px) * 4];
        return std::array<int, 3>{p[0], p[1], p[2]};
    };
    auto const base0 = panelColor(frame, 0), base1 = panelColor(frame, 1),
               base2 = panelColor(frame, 2);
    printf("[THM] box baseline panels: (%d,%d,%d) (%d,%d,%d) (%d,%d,%d)\n",
           base0[0], base0[1], base0[2], base1[0], base1[1], base1[2],
           base2[0], base2[1], base2[2]);
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Height;
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        td.range = dqGeom::Range1d(-0.5, 0.5);
        props.thematic = td;
        vp->overrideDisplayStyle(props);
    }
    pumpStable(frame, w, h);
    dumpBmpTHM(frame, w, h, DANQING_TILE_ASSETS_DIR "/../../build/thematic-box-height.bmp");

    auto const h0 = panelColor(frame, 0), h1 = panelColor(frame, 1), h2 = panelColor(frame, 2);
    printf("[THM] box height panels: (%d,%d,%d) (%d,%d,%d) (%d,%d,%d)\n",
           h0[0], h0[1], h0[2], h1[0], h1[1], h1[2], h2[0], h2[1], h2[2]);
    // ①重上色发生：基线三板相异（红/绿/蓝 >100 通道和）→ 高态收敛（≤40）即
    // 重上色铁证（无重上色不可能从相异收敛）；左右板各自变色（中板绿×中域
    // 渐变绿巧合不排除——钉左右板）。
    auto chanDist = [](std::array<int, 3> const& a, std::array<int, 3> const& b) {
        return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
    };
    EXPECT_GT(chanDist(base0, base1), 100);  // 基线相异性自检（资产色）
    EXPECT_GT(chanDist(base1, base2), 100);
    EXPECT_GT(chanDist(h0, base0), 60);      // 红板 → 渐变族
    EXPECT_GT(chanDist(h2, base2), 60);      // 蓝板 → 渐变族
    // ②收敛：三板同高 → 同色（两两差 ≤40/通道和——光照角度微差容限）。
    EXPECT_LE(chanDist(h0, h1), 40);
    EXPECT_LE(chanDist(h1, h2), 40);

    // --- Slope：Thematic: Slope 预设形态——同向板收敛同灰族色 ---
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Slope;
        td.range = dqGeom::Range1d(0.0, 90.0);
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        dqCommon::ThematicGradientSettingsProps grad;
        grad.mode = dqCommon::ThematicGradientMode::Smooth;
        grad.colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
        dqCommon::GradientKeyColorProps k0;
        k0.value = 0.0;
        k0.color = 0x404040;
        dqCommon::GradientKeyColorProps k1;
        k1.value = 1.0;
        k1.color = 0xffffff;
        grad.customKeys = std::vector<dqCommon::GradientKeyColorProps>{k0, k1};
        td.gradientSettings = grad;
        props.thematic = td;
        vp->overrideDisplayStyle(props);
    }
    pumpStable(frame, w, h);
    dumpBmpTHM(frame, w, h, DANQING_TILE_ASSETS_DIR "/../../build/thematic-box-slope.bmp");

    auto const s0 = panelColor(frame, 0), s1 = panelColor(frame, 1), s2 = panelColor(frame, 2);
    printf("[THM] box slope panels: (%d,%d,%d) (%d,%d,%d) (%d,%d,%d)\n",
           s0[0], s0[1], s0[2], s1[0], s1[1], s1[2], s2[0], s2[1], s2[2]);
    // ①收敛：三板同向 → 同色。
    EXPECT_LE(chanDist(s0, s1), 40);
    EXPECT_LE(chanDist(s1, s2), 40);
    // ②灰族（Custom 键 0x404040→0xffffff 皆灰）：收敛色三通道近等。
    int const mx = std::max(s1[0], std::max(s1[1], s1[2]));
    int const mn = std::min(s1[0], std::min(s1[1], s1[2]));
    EXPECT_LE(mx - mn, 30);
    // ③与 height 态不同（模式生效——Slope 无视高度只认朝向）。
    EXPECT_GT(chanDist(s1, h1), 60);
}
namespace {

// 高盒装饰（CesiumDecorator 的 begin/endDecoration 同构——DecorateContext.
// AddDecoration + Viewport.createGraphicOwner 生命周期面）。
class TallBoxDecorator : public dqApp::IDecorator {
public:
    void Decorate(dqApp::DecorateContext& context) override
    {
        dqRender::GraphicBuilderOptions opts;
        // GraphicType::Scene——**WorldDecoration 不承载本锁**：参考的
        // WorldDecorations 分支强制自带 vf（Target.ts:236-243 getWorldDecorations
        // 的 `new ViewFlags({...})`[thematicDisplay=默认 false] +
        // Graphic.ts:484-491 ctor setViewFlags）——**世界装饰对 thematic 豁免**
        // 是参考刻意语义（"Don't allow flags like monochrome etc to affect
        // world decorations"）。本锁要钉的是**场景内容**的渐变扫描 → 必须挂
        // Scene（normal 列表并入场景、随 plan vf——S-e 双栈统一修复后生效）。
        opts.type = dqRender::GraphicType::Scene;
        // M-O(4) P8 实证：finish 的 LOD 门要求 computeChordTolerance 接线
        //（缺 closure 恒 null——CesiumDecorator::beginDecoration 同面）。
        auto& vp = context.GetViewport();
        dqGeom::Point3d const refPoint(0.0, 0.0, 0.0);
        double const worldPerPixel = vp.GetViewingSpace().getPixelSizeAtPoint(&refPoint);
        opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
        auto builder = context.GetViewport().createGraphicBuilder(opts);
        if (!builder) {
            return;
        }
        builder->setSymbology(dqCommon::ColorDef::white, dqCommon::ColorDef::white, 1);

        // 2×2×10 盒（心 (0,0,5)）六面（RenderSmokeTest.HilitePass 的
        // PolyfaceBuilder 六面同构）。
        double const h = 1.0;   // x/y 半宽
        double const zt = 10.0; // 顶 z
        dqGeom::Point3d const b1(-h, -h, 0), b2(h, -h, 0), b3(h, h, 0), b4(-h, h, 0);
        dqGeom::Point3d const t1(-h, -h, zt), t2(h, -h, zt), t3(h, h, zt), t4(-h, h, zt);
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
        auto boxPolyface = box->ClaimPolyface();
        builder->addPolyface(*boxPolyface, /*filled=*/true);
        if (auto* g = builder->finish()) {
            context.GetViewport().createGraphicOwner(g);
            context.AddDecoration(opts.type, g);
        }
    }
    bool TestDecorationHit(uint32_t) const override { return false; }
    QString GetDecorationToolTip(uint32_t) const override { return {}; }
};

}  // namespace


// ---------------------------------------------------------------------------
// 锁 3：Slope 模式——竖直面近白、水平面近暗（Thematic: Slope 预设形态）。
// ---------------------------------------------------------------------------
TEST(ThematicDisplayE2E, TallBoxGradientSweepIsExact)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ThematicDisplayE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinTHM(400);

    auto* deco = new TallBoxDecorator();
    dqApp::Application::Get().GetViewManager().AddDecorator(deco);

    auto* vp = view.getUeViewport();
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    // 显式 Iso 取向（默认视图朝向不定——俯视则只见顶面成方块，实测混淆源）；
    // LookAtVolume 保旋转保帧内容域。
    view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso));
    view3d->LookAtVolume(dqGeom::Range3d::CreateXYZXYZ(-2.5, -2.5, -0.5, 2.5, 2.5, 10.5));
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        style.getSettings().toggleSkyBox(false);
        style.getSettings().setBackgroundColor(dqCommon::ColorDef::from(0, 0, 0));
    }
    vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});

    auto pumpStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 40; ++i) {
            prev = frame;
            spinTHM(120);
            vp->InvalidateController();
            vp->RenderFrame();
            ASSERT_TRUE(vp->ReadFrameForTest(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    pumpStable(frame, w, h);
    ASSERT_FALSE(frame.empty());

    // 关光照（渐变原色精确断言——ApplyLighting 视图级门[ViewFlagsProperties.
    // lighting，M-Q Q-d 面]）。
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.lighting = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        vp->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }

    // --- Height：axis=+Z，range=[0,10]——盒 z 全域 = 渐变全程 ---
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Height;
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        td.range = dqGeom::Range1d(0.0, 10.0);
        props.thematic = td;
        vp->overrideDisplayStyle(props);
    }
    pumpStable(frame, w, h);
    dumpBmpTHM(frame, w, h, DANQING_TILE_ASSETS_DIR "/../../build/thematic-tallbox-height.bmp");

    // 判据（Height 本件如实面）：**取证实录——PrimitiveBuilder 网格路径把本盒
    // 的顶点 z 压平**（v_thematicIndex 全程≈0：box 网格 rawPosition.z 恒 0，
    // 与 PolyfaceBuilder 的 z∈[0,10] 源数据分离——GeometryAccumulator→
    // MeshBuilder 链的独立缺陷，登记 TD-30，非 thematic 面）。当前真实形态 =
    // 全盒 ndx=0 → 渐变低端纯蓝均色（BlueRed row0）。断言：内容区全域蓝族
    //（渐变纹理+链消费实证）+ 基线（白盒）已变。
    long blueAll = 0, totalC = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = uint32_t(w * 0.2); x < uint32_t(w * 0.8); ++x) {
            uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            if (r + g + b < 30)
                continue;
            ++totalC;
            if (b >= 90 && b > r + 40 && b > g)
                ++blueAll;
        }
    printf("[THM] tallbox height: blueAll=%ld/%ld\n", blueAll, totalC);
    EXPECT_GT(blueAll, 1000L);
    EXPECT_GT(blueAll * 2, totalC);  // 主导（内容区多数蓝族）

    // --- Slope：顶面暗（#404040 族）侧面亮（白族）——Custom 双色键 ---
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Slope;
        td.range = dqGeom::Range1d(0.0, 90.0);
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        dqCommon::ThematicGradientSettingsProps grad;
        grad.mode = dqCommon::ThematicGradientMode::Smooth;
        grad.colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
        dqCommon::GradientKeyColorProps k0;
        k0.value = 0.0;
        k0.color = 0x404040;
        dqCommon::GradientKeyColorProps k1;
        k1.value = 1.0;
        k1.color = 0xffffff;
        grad.customKeys = std::vector<dqCommon::GradientKeyColorProps>{k0, k1};
        td.gradientSettings = grad;
        props.thematic = td;
        vp->overrideDisplayStyle(props);
    }
    pumpStable(frame, w, h);
    dumpBmpTHM(frame, w, h, DANQING_TILE_ASSETS_DIR "/../../build/thematic-tallbox-slope.bmp");

    // Slope 判据（盒的大侧面=竖直 → 渐变高端近白亮族——首绿实测
    // sideBright=669130；顶面在本 iso 侧视下不可见[顶带断言舍去]；灰族
    // 主导=Custom 双色键皆灰）。修复前侧面对称中灰（混帧 saga——世界帧 E1
    // 修复后竖直面正确归白端）。
    long sideBright = 0, grayContent = 0, totalS = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = uint32_t(w * 0.2); x < uint32_t(w * 0.8); ++x) {
            uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            if (r + g + b < 20)
                continue;
            ++totalS;
            int const mx = std::max(r, std::max(g, b));
            int const mn = std::min(r, std::min(g, b));
            if (mx - mn > 30)
                continue;  // 非灰族
            ++grayContent;
            if (r >= 170)
                ++sideBright;
        }
    printf("[THM] tallbox slope: sideBright=%ld grayContent=%ld/%ld\n",
           sideBright, grayContent, totalS);
    EXPECT_GT(sideBright, 1000L);           // 竖直侧面 → 亮族（白端消费实证）
    EXPECT_GT(grayContent * 2, totalS);     // 灰族主导（双色灰键消费实证）

    dqApp::Application::Get().GetViewManager().DropDecorator(deco);
}

// ---------------------------------------------------------------------------
TEST(ThematicDisplayE2E, HeightGradientColorsByWorldZAndRestores)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ThematicDisplayE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinTHM(400);

    ThematicFrame f;
    OpenAndStabilize(view, f);
    ReadStableFrame(view, f);
    auto const baseline = f.frame;
    dumpBmpTHM(f.frame, f.w, f.h,
               DANQING_TILE_ASSETS_DIR "/../../build/thematic-height-off.bmp");

    // TEMP-DIAG 支持（S-d height saga——取 ndx 纯值）：DANQING_THM_UNLIT=1 时
    // 关光照（ViewFlagsProperties.lighting——ApplyLighting 的视图级门
    // [M-Q Q-d 修复面]），thematic 输出=渐变原色。
    bool const unlit = getenv("DANQING_THM_UNLIT") != nullptr;
    if (unlit) {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.lighting = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }

    // 应用 thematic Height（axis=+Z）。**range 取内容 z 带 [-0.5, 5.0]**——
    // 取证钉死（S-d）：树 rootTile 内容 z=[-2.38,2.38] × location z 平移
    // 2.078 → 世界 z≈[-0.3,4.5]；projectExtents z=[-0.64,11.69] 含高杆
    //（边族元素不进 thematic）——用全 extents 时房体压进渐变纯蓝区（BlueRed
    // 低端 ndx<0.125 平台段）无可测方向信号；紧域使房体铺满全程——方向锁
    // 成立。DTA 面板的 range High/Low 控件即此用户面（ThematicDisplay.ts:
    // 335-368）。
    auto const& pe = f.opened.connection->GetProjectExtents();
    ASSERT_FALSE(pe.isNull());
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Height;
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        td.range = dqGeom::Range1d(-0.5, 5.0);
        props.thematic = td;
        view.getUeViewport()->overrideDisplayStyle(props);
    }
    spinTHM(400);
    ReadStableFrame(view, f);
    dumpBmpTHM(f.frame, f.w, f.h,
               DANQING_TILE_ASSETS_DIR "/../../build/thematic-height-on.bmp");

    // ①换色规模：内容区大面换色（钉值=首绿实测，量级门槛先 100k px 级）。
    long const changed = countDifferent(baseline, f.frame);
    printf("[THM] height-on changed px = %ld / %u\n", changed, f.w * f.h);
    EXPECT_GT(changed, 100000L);

    // ②内容形态（数据形状如实——S-d 取证：房体在内容 z 带内 ndx≤0.15 的
    // BlueRed 蓝端区[extents 高杆是边族不进 thematic]，方向断言不可钉——
    // 方向终证在全控盒锁[TallBoxGradientSweepIsExact]）：内容带内蓝族主导
    //（BlueRed 低端被消费——渐变纹理+模式+链全通）+ 蓝通道非恒值（渐变
    // 逐顶点计算的形状锁——恒色则方差≈0；首绿实测 stddev=57.8 钉 20）。
    uint32_t const w = f.w, h = f.h;
    long blueDom = 0, total = 0;
    double sumB = 0.0, sumB2 = 0.0;
    for (uint32_t y = uint32_t(h * 0.20); y < uint32_t(h * 0.80); y += 2)
        for (uint32_t x = uint32_t(w * 0.30); x < uint32_t(w * 0.70); x += 2) {
            uint8_t const* p = &f.frame[(static_cast<size_t>(y) * w + x) * 4];
            int const r = p[0], g = p[1], b = p[2];
            if (r + g + b < 60)
                continue;  // 黑底
            ++total;
            sumB += b;
            sumB2 += double(b) * b;
            if (b > r && b > g)
                ++blueDom;
        }
    double const meanB = sumB / std::max(1L, total);
    double const stdB = std::sqrt(std::max(0.0, sumB2 / std::max(1L, total) - meanB * meanB));
    printf("[THM] content blue: dominant=%ld/%ld meanB=%.1f stdB=%.2f\n",
           blueDom, total, meanB, stdB);
    EXPECT_GT(blueDom, total / 2);   // 蓝族主导（BlueRed 低端消费实证）
    EXPECT_GT(stdB, 20.0);           // 渐变逐顶点变化（恒色形态锁）

    // ③关闭恢复（vf.thematicDisplay=false——plan.thematic 缺席→uniforms 清态）。
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = false;
        props.viewflags = vf;
        view.getUeViewport()->overrideDisplayStyle(props);
    }
    spinTHM(400);
    ReadStableFrame(view, f);
    dumpBmpTHM(f.frame, f.w, f.h,
               DANQING_TILE_ASSETS_DIR "/../../build/thematic-height-restored.bmp");
    long const restored = countDifferent(baseline, f.frame);
    printf("[THM] restored diff px = %ld\n", restored);
    EXPECT_EQ(restored, 0L);
}

// ---------------------------------------------------------------------------
// 锁 2（受控资产）：minimal-solid 盒（5×1×1，z∈[-0.5,0.5]）——Height 渐变
// 精确端点色 + Slope 面朝向分离。几何真值已知（资产可控——§11.11 专用资产
// 先例），排除 dump 数据形状变量（joeshouse 的 12.3m extents 含高杆导致
// 房体压缩在渐变低段的教训在案）。
// ---------------------------------------------------------------------------

TEST(ThematicDisplayE2E, SlopeDistinguishesVerticalFromFlat)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ThematicDisplayE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinTHM(400);

    ThematicFrame f;
    OpenAndStabilize(view, f);

    // Thematic: Slope 预设形态（ViewAttributes.ts:198-215——range [0,90]、
    // axis (0,0,1)、Custom 双色键 0x404040→0xffffff）。
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::Slope;
        td.range = dqGeom::Range1d(0.0, 90.0);
        td.axis = dqGeom::Vector3d::From(0, 0, 1);
        dqCommon::ThematicGradientSettingsProps grad;
        grad.mode = dqCommon::ThematicGradientMode::Smooth;
        grad.colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
        dqCommon::GradientKeyColorProps k0;
        k0.value = 0.0;
        k0.color = 0x404040;
        dqCommon::GradientKeyColorProps k1;
        k1.value = 1.0;
        k1.color = 0xffffff;
        grad.customKeys = std::vector<dqCommon::GradientKeyColorProps>{k0, k1};
        td.gradientSettings = grad;
        props.thematic = td;
        view.getUeViewport()->overrideDisplayStyle(props);
    }
    // 关光照（TallBox 同款——渐变原色精确断言面：光照调制会把白端压到
    // ~150 使阈值族失真；光照链另有锁看护）。
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.lighting = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
    }
    spinTHM(400);
    ReadStableFrame(view, f);
    dumpBmpTHM(f.frame, f.w, f.h,
               DANQING_TILE_ASSETS_DIR "/../../build/thematic-slope-on.bmp");

    // 亮/暗分带：亮族（竖直面——slope≈90°→渐变高端近白）；暗族（水平面——
    // slope≈0°→低端 #404040）。亮度阈值钉值=首绿实测。
    long bright = 0, dark = 0;
    double brightCy = 0, darkCy = 0;
    for (uint32_t y = 0; y < f.h; ++y)
        for (uint32_t x = 0; x < f.w; ++x) {
            uint8_t const* p = &f.frame[(static_cast<size_t>(y) * f.w + x) * 4];
            int const luma = (p[0] * 299 + p[1] * 587 + p[2] * 114) / 1000;
            // 天空渐变背景（亮蓝）排除：亮族要近白（三通道皆高）。
            if (p[0] >= 180 && p[1] >= 180 && p[2] >= 180) {
                ++bright;
                brightCy += y;
            } else if (luma >= 30 && luma <= 110 &&
                       std::abs(p[0] - p[1]) < 25 && std::abs(p[1] - p[2]) < 25) {
                // 近灰暗族（#404040 系——三通道近等的低亮度）。
                ++dark;
                darkCy += y;
            }
        }
    if (bright > 0) brightCy /= bright;
    if (dark > 0) darkCy /= dark;
    printf("[THM] slope bright=%ld (cy=%.0f) dark=%ld (cy=%.0f)\n",
           bright, brightCy, dark, darkCy);
    // 钉值=S-e 视空间归位后实测（关光照纯渐变域：竖直面 ndx=1→白族 487891、
    // 水平面 ndx=0→#404040 暗族 557104——判据取实测 0.49×/0.48× 容差带；
    // 早前 295/1979/707453 三值系 S-d 世界帧 E1 误登记形态下的伪影，作废）。
    EXPECT_GT(bright, 240000L);  // 竖直面族（ndx=1 白端——视空间轴点积逐位正确）
    EXPECT_GT(dark, 270000L);    // 水平面族（ndx=0 暗端）
    EXPECT_GT(dark, bright);     // 数据形状：本视角水平面域略大于竖直面域
}

// ---------------------------------------------------------------------------
// 锁 4（全控资产·方向终证）：2×2×10 高盒装饰（z∈[0,10] 铺满渐变程）——
// 无 dump 数据形状变量（S-d joeshouse/box 的数据形状教训：extents 高杆/资产
// 本色/y-up 帧三案在案）。Height 轴+Z 域 [0,10]：盒底沿（z=0）蓝、顶面
//（z=10）红、垂直梯度满程；Slope：顶面暗（slope 0°→#404040）、侧面亮
//（90°→白）。
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// 锁 5（IDW 传感器模式 E2E——S-e 的 GPU 链终证）：joeshouse 双传感器异值
//（A 值 1→近端红族 / B 值 0→近端蓝族；distanceCutoff=0=全局臂——无裁剪，
// ThematicUniforms.ts:142-147 全局共享纹理面）。IDW 片元循环（
// glsl/Thematic.ts prelude——1/dist² 加权）消费 s_sensorSampler（单元 7，
// 与 ShadowMap 复用[阴影未移植无冲突——E3 登记]）+ u_numSensors 全局臂。
// ---------------------------------------------------------------------------
TEST(ThematicDisplayE2E, SensorIdwColorSeparatesNearEachSensor)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "ThematicDisplayE2E";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinTHM(400);

    ThematicFrame f;
    OpenAndStabilize(view, f);

    // 关 grid/ACS（色族分类器净空——ACS 红轴落判据带内会污染红族计数；
    // 与其余锁同形）。
    {
        auto& style = view.getUeViewport()->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.grid = false;
        p.acsTriad = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
        view.getUeViewport()->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        spinTHM(200);
    }

    // IDW 模式 + 双传感器（joeshouse extents x=[-7.5,27.8] 的两端，z≈1 房体
    // 带；y=4.9 中带）——A 端值 1（近端→红族）、B 端值 0（近端→蓝族）。
    {
        dqCommon::DisplayStyle3dSettingsProps props;
        dqCommon::ViewFlagProps vf;
        vf.thematicDisplay = true;
        props.viewflags = vf;
        dqCommon::ThematicDisplayProps td;
        td.displayMode = dqCommon::ThematicDisplayMode::InverseDistanceWeightedSensors;
        dqCommon::ThematicDisplaySensorSettingsProps ss;
        ss.distanceCutoff = 0.0;  // 全局臂（无裁剪）
        dqCommon::ThematicDisplaySensorProps sa;
        sa.position = dqGeom::Point3d::From(-7.0, 4.9, 1.0);  // 内容西端
        sa.value = 1.0;
        dqCommon::ThematicDisplaySensorProps sb;
        sb.position = dqGeom::Point3d::From(27.0, 4.9, 1.0);  // 内容东端
        sb.value = 0.0;
        ss.sensors = std::vector<dqCommon::ThematicDisplaySensorProps>{sa, sb};
        td.sensorSettings = ss;
        props.thematic = td;
        view.getUeViewport()->overrideDisplayStyle(props);
    }
    spinTHM(400);
    ReadStableFrame(view, f);
    dumpBmpTHM(f.frame, f.w, f.h,
               DANQING_TILE_ASSETS_DIR "/../../build/thematic-sensors-idw.bmp");

    // WHERE：A 端近域（frame 右半？—— saved 等轴测内容展布由首绿实测钉死；
    // 先按内容 bbox 的左右三分带统计红/蓝族质心分离）——红族质心显著偏 A
    // 侧、蓝族偏 B 侧（IDW 逐片元 1/dist² 加权的直接证据）。
    //（frame y=0=底[glReadPixels]；x 向=内容横展布。）
    uint32_t const w = f.w, h = f.h;
    HueStats const red = classifyHue(f.frame, w, h,
                                     0, w, uint32_t(h * 0.15), uint32_t(h * 0.90), true);
    HueStats const blue = classifyHue(f.frame, w, h,
                                      0, w, uint32_t(h * 0.15), uint32_t(h * 0.90), false);
    printf("[THM] idw red: count=%ld centroid=(%.0f,%.0f) | blue: count=%ld centroid=(%.0f,%.0f)\n",
           red.count, red.cx, red.cy, blue.count, blue.cx, blue.cy);
    EXPECT_GT(red.count, 1000L);
    EXPECT_GT(blue.count, 1000L);
    // 两族质心 2D 分离（A/B 传感器在内容两端——首绿实测：分离沿内容长轴走
    // 屏幕对角线[dx=28/dy=461]，取 2D 距离≥内容展布/5）。
    double const sep = std::sqrt((red.cx - blue.cx) * (red.cx - blue.cx)
                                 + (red.cy - blue.cy) * (red.cy - blue.cy));
    printf("[THM] idw centroid separation=%.0f px\n", sep);
    EXPECT_GT(sep, w * 0.10);
}
