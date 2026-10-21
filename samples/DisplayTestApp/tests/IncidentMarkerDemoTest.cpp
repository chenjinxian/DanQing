// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — IncidentMarkerDemo 锁族（M-X X-b2：toggle 状态机 /
// makeColor 插值钉值 / 聚类合成计数 / loading 门 / 六 SVG 资产装载）。
//
// Authored: no reference test exists in itwinjs-core for IncidentMarkerDemo
//           （display-test-app 无前端测试套件；行为锚定 IncidentMarkerDemo.ts
//           :16-262 逐行 + Marker.ts 移植面）。随机 incidents 用固定种子注入
//           （XE16——ctor 的 rand01 形参；聚类计数钉值为种子 42 的确定性产出，
//           非真随机断言）。
//
// RED（X-b2 落地前）：Gui::IncidentMarkerDemo / IncidentMarkerDemoTool /
// 注册面不存在——编译期缺 API 红（PathDecorationTestLock 同型先例）；资产
// 装载钉值经首跑实测后钉死（QSvg 栅格尺寸为仪器实测值，判据 = 六槽全解码 +
// 自然尺寸与 SVG 根 width/height 属性的截断语义一致）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>

#include "Gui/IncidentMarkerDemo.h"
#include "Gui/DtaTools.h"  // registerDtaTools（宿主注册面）

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace { struct QtEnvIM { QtEnvIM() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvIM s_qtIM;

namespace {

// 视口夹具（dqApp MarkerTest 同型——headless 视口 + ViewingSpace 初始化）。
struct DemoViewportFixture {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    dqApp::Viewport* vp = nullptr;   // 泄漏至进程终（既有约定）
};
DemoViewportFixture makeDemoViewport()
{
    DemoViewportFixture f;
    dqApp::BlankConnectionProps props;
    props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                    dqGeom::Point3d::From(100, 100, 100));
    f.imodel = dqApp::BlankConnection::create(props);
    f.view = dqApp::SpatialViewState::CreateBlank(f.imodel.Get(),
                                                  dqGeom::Point3d::From(0, 0, 0),
                                                  dqGeom::Vector3d::From(200, 200, 200));
    f.vp = dqApp::Viewport::Create(nullptr, f.view);
    float const w = static_cast<float>(f.vp->width());
    float const h = static_cast<float>(f.vp->height());
    f.view->SetExtents(dqGeom::Vector3d::From(200.0, 200.0 * h / w, 200.0));
    f.vp->setupViewFromFrustum(f.vp->getFrustum(true));
    return f;
}

dqGeom::Range3d const kDemoExtents(0.0, 0.0, 0.0, 100.0, 100.0, 20.0);

// 种子 42 确定性随机源（XE16 注入形）。
std::function<double()> seededRand(uint32_t seed)
{
    return [engine = std::mt19937(seed)]() mutable {
        return std::generate_canonical<double, 53>(engine);
    };
}

// 记录 strokeStyle 的 mock CanvasContext（drawFunc 色钉桩——组色 =
// makeColor(最高严重度) 的观测面；m_clusterColor 为参考私有成员，经
// drawFunc 消费面观测）。
class ClusterRecordingContext final : public dqRender::CanvasContext {
public:
    void save() override {}
    void restore() override {}
    void translate(double, double) override {}
    void beginPath() override { ++beginCount; }
    void moveTo(double, double) override {}
    void lineTo(double, double) override {}
    void stroke() override { ++strokeCount; }
    void setStrokeStyle(dqCommon::ColorDef color) override { strokeStyles.push_back(color); }
    void setLineWidth(double w) override { lineWidths.push_back(w); }
    void setFillStyle(dqCommon::ColorDef color) override { fillStyles.push_back(color); }
    void setGlobalAlpha(double) override {}
    void arc(double, double, double r, double, double) override { radii.push_back(r); }
    void fill() override { ++fillCount; }
    void drawImage(dqRender::rhi::TextureHandle, uint32_t, uint32_t, double, double) override {}

    int beginCount = 0, strokeCount = 0, fillCount = 0;
    std::vector<dqCommon::ColorDef> strokeStyles, fillStyles;
    std::vector<double> lineWidths, radii;
};

}  // namespace

// ---------------------------------------------------------------------------
// makeColor（IncidentMarkerDemo.ts:28-31——绿(0,128,0) → 琥珀(255,191,0) →
// 红(255,0,0)；ColorByName 双端 0xBBGGRR 同形[amber=0x00BFFF → R255 G191
// B0]；lerp 截断语义[ColorDef.ts:419-421 的 += 后位打包 = ToInteger 截断]）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, MakeColorInterpolatesGreenAmberRed)
{
    auto colors = [](dqCommon::ColorDef c) { return c.getColors(); };
    // 端点 + 拐点（:29 的分段函数两段）。
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(1));   // t=0 → green
        EXPECT_EQ(c.r, 0);   EXPECT_EQ(c.g, 128); EXPECT_EQ(c.b, 0);
    }
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(16));  // t=1 → amber
        EXPECT_EQ(c.r, 255); EXPECT_EQ(c.g, 191); EXPECT_EQ(c.b, 0);
    }
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(30));  // t=1 → red
        EXPECT_EQ(c.r, 255); EXPECT_EQ(c.g, 0);   EXPECT_EQ(c.b, 0);
    }
    // 段内钉值（截断语义逐位推导）：
    //   severity=8  → t=7/15:  r=255*7/15=119.0, g=128+63*7/15=157.4→157, b=0
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(8));
        EXPECT_EQ(c.r, 119); EXPECT_EQ(c.g, 157); EXPECT_EQ(c.b, 0);
    }
    //   severity=17 → t=1/14:  r=255, g=191-191/14=177.36→177, b=0
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(17));
        EXPECT_EQ(c.r, 255); EXPECT_EQ(c.g, 177); EXPECT_EQ(c.b, 0);
    }
    //   severity=23 → t=0.5:   r=255, g=95.5→95, b=0
    {
        auto const c = colors(Gui::IncidentMarker::makeColor(23));
        EXPECT_EQ(c.r, 255); EXPECT_EQ(c.g, 95);  EXPECT_EQ(c.b, 0);
    }
}

// ---------------------------------------------------------------------------
// IncidentMarker 构造面（:44-53——title/label/imageOffset/imageSize/scaleFactor）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, IncidentMarkerConstructorFaces)
{
    Gui::IncidentMarker marker(dqGeom::Point3d::From(1, 2, 3), /*severity*/ 5,
                               /*id*/ 3, nullptr, /*iconIndex*/ 2);
    // :50——title（XE3 纯文本形：<br> → 换行）。
    ASSERT_TRUE(marker.title.has_value());
    EXPECT_EQ(*marker.title, "Severity: 5\nId: 3");
    // :52——label = id。
    ASSERT_TRUE(marker.label.has_value());
    EXPECT_EQ(*marker.label, "3");
    // :48-49——imageOffset (0,30) / imageSize 40x40。
    ASSERT_TRUE(marker.imageOffset.has_value());
    EXPECT_DOUBLE_EQ(marker.imageOffset->x, 0.0);
    EXPECT_DOUBLE_EQ(marker.imageOffset->y, 30.0);
    ASSERT_TRUE(marker.imageSize.has_value());
    EXPECT_DOUBLE_EQ(marker.imageSize->x, 40.0);
    EXPECT_DOUBLE_EQ(marker.imageSize->y, 40.0);
    // :17——size 30x30；图标缺席（nullptr 形参 = 装载失败形）→ image 不在。
    EXPECT_DOUBLE_EQ(marker.size.x, 30.0);
    EXPECT_FALSE(marker.image.has_value());
}

// ---------------------------------------------------------------------------
// IncidentClusterMarker 合成（:96-136——top-10 严重度降序 title + 计数 label +
// 组色 = 最高严重度色 + drawFunc 白圆 5px 描边）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, ClusterMarkerSynthesizesTopTenTitleAndCount)
{
    // 12 个事故（severity 1..12、id 0..11）直构聚类——确定性注入（非随机）。
    std::vector<std::unique_ptr<Gui::IncidentMarker>> owned;
    std::vector<Gui::IncidentMarker*> refs;
    for (int i = 0; i < 12; ++i) {
        owned.push_back(std::make_unique<Gui::IncidentMarker>(
            dqGeom::Point3d::From(0, 0, 0), /*severity*/ i + 1, /*id*/ i,
            nullptr, /*iconIndex*/ 1));
        refs.push_back(owned.back().get());
    }
    dqApp::Cluster<Gui::IncidentMarker> cluster(refs);
    Gui::IncidentClusterMarker cm(cluster.getClusterLocation(),
                                  dqGeom::Point2d::From(30, 30), cluster, nullptr);

    // :116——label = 组成员数。
    ASSERT_TRUE(cm.label.has_value());
    EXPECT_EQ(*cm.label, "12");
    // :121-128——title = top-10 严重度降序行 + 截断 "..."（12 > 10）。
    ASSERT_TRUE(cm.title.has_value());
    std::string const expected =
        "Severity: 12 Id: 11\nSeverity: 11 Id: 10\nSeverity: 10 Id: 9\n"
        "Severity: 9 Id: 8\nSeverity: 8 Id: 7\nSeverity: 7 Id: 6\n"
        "Severity: 6 Id: 5\nSeverity: 5 Id: 4\nSeverity: 4 Id: 3\n"
        "Severity: 3 Id: 2\n...";
    EXPECT_EQ(*cm.title, expected);
    // :114-115——imageOffset (0,28) / imageSize 30x30。
    ASSERT_TRUE(cm.imageOffset.has_value());
    EXPECT_DOUBLE_EQ(cm.imageOffset->y, 28.0);
    ASSERT_TRUE(cm.imageSize.has_value());
    EXPECT_DOUBLE_EQ(cm.imageSize->x, 30.0);
    // :117-118——labelColor black / labelFont bold 14px sans-serif。
    ASSERT_TRUE(cm.labelColor.has_value());
    EXPECT_TRUE(cm.labelColor->equals(dqCommon::ColorDef::from(0, 0, 0)));
    ASSERT_TRUE(cm.labelFont.has_value());
    EXPECT_EQ(*cm.labelFont, "bold 14px sans-serif");

    // :85-93 + :133——drawFunc：白填充圆 r=13 + 组色描边 5px；组色 =
    // makeColor(sorted[0].severity) = makeColor(12)。
    ASSERT_TRUE(static_cast<bool>(cm.drawFunc));
    ClusterRecordingContext ctx;
    cm.drawFunc(ctx);
    EXPECT_EQ(ctx.beginCount, 1);
    EXPECT_EQ(ctx.fillCount, 1);
    EXPECT_EQ(ctx.strokeCount, 1);
    ASSERT_EQ(ctx.radii.size(), 1u);
    EXPECT_DOUBLE_EQ(ctx.radii[0], 13.0);
    ASSERT_EQ(ctx.lineWidths.size(), 1u);
    EXPECT_DOUBLE_EQ(ctx.lineWidths[0], 5.0);
    ASSERT_EQ(ctx.fillStyles.size(), 1u);
    EXPECT_TRUE(ctx.fillStyles[0].equals(dqCommon::ColorDef::white));
    ASSERT_EQ(ctx.strokeStyles.size(), 1u);
    EXPECT_TRUE(ctx.strokeStyles[0].equals(Gui::IncidentMarker::makeColor(12)));
}

// ---------------------------------------------------------------------------
// 六 SVG 资产装载（:176-183 的 loads 清单 + XE2 QImageReader 面）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, SixSvgAssetsDecode)
{
    DemoViewportFixture f = makeDemoViewport();
    Gui::IncidentMarkerDemo demo(*f.vp, kDemoExtents, seededRand(42));

    // 六槽全解码（资产入库于 samples/DisplayTestApp/assets——XE5）。
    EXPECT_EQ(demo.loadedImageCount(), 6u);
    // 自然尺寸 = SVG 根 width/height 截断（QSvg 栅格：1280.0019→1280 /
    // 1065.9518→1065；156.262→156 / 144.407→144——首跑实测钉死）。
    auto const warning = demo.warningSign();
    ASSERT_TRUE(warning.has_value());
    EXPECT_EQ(warning->width, 1280u);
    EXPECT_EQ(warning->height, 1065u);
    for (int i = 0; i < 5; ++i) {
        auto const& dims = demo.imageDimsForTest(i + 1);
        EXPECT_EQ(dims.first, 156u) << "hazard icon " << i;
        EXPECT_EQ(dims.second, 144u) << "hazard icon " << i;
    }
    QCoreApplication::processEvents();   // 泄 timer（防跨测试悬垂）
}

// ---------------------------------------------------------------------------
// loading 门（:150-151/:219-225 + XE9）+ XE17 计数怪癖
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, LoadingGateBlocksUntilPumpThenBuilds417)
{
    DemoViewportFixture f = makeDemoViewport();
    Gui::IncidentMarkerDemo demo(*f.vp, kDemoExtents, seededRand(42));
    EXPECT_TRUE(demo.isLoading());

    // loading 中的 Decorate → 零产出（:219-225 门）。
    dqRender::Decorations decorations;
    dqApp::DecorationsCache cache;
    {
        dqApp::DecorateContext context(*f.vp, decorations, cache);
        demo.Decorate(context);
        EXPECT_TRUE(decorations.canvasDecorations.empty());
        EXPECT_TRUE(demo.isLoading());
    }

    // :221-224 的 invalidate 观测——泵前置 valid，完成回调落 InvalidateCached
    // Decorations → InvalidateDecorations。
    f.vp->SetDecorationsValidForTest(true);
    QCoreApplication::processEvents();   // XE9 延迟完成点（singleShot 0）
    EXPECT_FALSE(demo.isLoading());
    EXPECT_FALSE(f.vp->GetDecorationsValidForTest());   // invalidate 已发

    // XE17 计数怪癖（:194——(i%6)+1 下标 6 越界不建）：500 迭代 → 417
    // marker（i = 6k+5 ≤ 499 → k ≤ 82 → 83 跳过）。与种子无关（计数只依赖
    // 下标怪癖 + 六图标全载）。
    size_t markerCount = 0;
    demo.incidents().markers().forEach([&markerCount](Gui::IncidentMarker const*) {
        ++markerCount;
    });
    EXPECT_EQ(markerCount, 417u);

    // 完成后的 Decorate → 画布装饰产出（聚类上屏——非空且少于 marker 数
    // = 聚类确在合成）。
    {
        dqApp::DecorateContext context(*f.vp, decorations, cache);
        demo.Decorate(context);
        EXPECT_FALSE(decorations.canvasDecorations.empty());
        EXPECT_LT(decorations.canvasDecorations.size(), markerCount);
    }
}

// ---------------------------------------------------------------------------
// XE17 图标循环面——事故图标只用槽 1..5（warning 槽 0 留予组 marker；
// (i%6)+1 的下标 6 位全部缺席）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, IncidentIconsCycleSlotsOneThroughFive)
{
    DemoViewportFixture f = makeDemoViewport();
    Gui::IncidentMarkerDemo demo(*f.vp, kDemoExtents, seededRand(42));
    QCoreApplication::processEvents();   // 完成装载
    ASSERT_FALSE(demo.isLoading());

    size_t markerCount = 0;
    std::array<size_t, 6> slotCounts = {0, 0, 0, 0, 0, 0};
    demo.incidents().markers().forEach([&](Gui::IncidentMarker const* marker) {
        ++markerCount;
        ASSERT_GE(marker->iconIndex, 1);
        ASSERT_LE(marker->iconIndex, 5);
        ++slotCounts[static_cast<size_t>(marker->iconIndex)];
    });
    EXPECT_EQ(markerCount, 417u);
    EXPECT_EQ(slotCounts[0], 0u);   // warning 槽从不服务事故
    // 槽 1..5 均布（i%6 循环 × 500 迭代去 83 位——每槽 83 或 84）。
    for (size_t slot = 1; slot <= 5; ++slot) {
        EXPECT_GE(slotCounts[slot], 83u) << "slot " << slot;
        EXPECT_LE(slotCounts[slot], 84u) << "slot " << slot;
    }
}

// ---------------------------------------------------------------------------
// toggle 状态机（:229-250——start/stop/toggle + ViewManager 注册对账）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, ToggleStartsAndStopsDecorator)
{
    auto& vm = dqApp::Application::Get().GetViewManager();
    DemoViewportFixture f = makeDemoViewport();

    // 前置幂等清场（他测试残留激活态——DtaTest 全量同进程）。
    Gui::IncidentMarkerDemo::stop();
    ASSERT_EQ(Gui::IncidentMarkerDemo::decorator(), nullptr);

    Gui::IncidentMarkerDemo::toggle(*f.vp, kDemoExtents);   // :246-247 → start
    Gui::IncidentMarkerDemo* const demo = Gui::IncidentMarkerDemo::decorator();
    ASSERT_NE(demo, nullptr);
    auto const& decorators = vm.GetDecorators();
    EXPECT_NE(std::find(decorators.begin(), decorators.end(),
                        static_cast<dqApp::IDecorator*>(demo)),
              decorators.end());   // :231 addDecorator
    EXPECT_TRUE(demo->UseCachedDecorations());   // :206

    Gui::IncidentMarkerDemo::toggle(*f.vp, kDemoExtents);   // :248-249 → stop
    EXPECT_EQ(Gui::IncidentMarkerDemo::decorator(), nullptr);
    auto const& after = vm.GetDecorators();
    EXPECT_EQ(std::find(after.begin(), after.end(),
                        static_cast<dqApp::IDecorator*>(demo)),
              after.end());   // :240 dropDecorator
}

// ---------------------------------------------------------------------------
// 工具注册面（App.ts:416 注册表 + SVTTools.json:64-65 keyin + :255-261 run 门）
// ---------------------------------------------------------------------------
TEST(IncidentMarkerDemoLock, ToolRegisteredWithKeyin)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    Gui::registerDtaTools();

    auto* tool = admin.GetRegistry().Create("ToggleIncidentMarkers");
    ASSERT_NE(tool, nullptr);
    EXPECT_STREQ("ToggleIncidentMarkers", tool->getToolId());
    EXPECT_EQ(tool->englishKeyin(), "dta incident markers");
    EXPECT_EQ(tool->minArgs(), 0);
    EXPECT_EQ(tool->maxArgs(), 0);

    // keyin 解析面（ToolRegistry.parseKeyin 全程匹配）。
    auto const parsed = admin.GetRegistry().parseKeyin("dta incident markers");
    ASSERT_TRUE(parsed.ok);
    EXPECT_EQ(parsed.toolId, "ToggleIncidentMarkers");

    // :255-261——run 恒 true；选中视口缺席/非空间视图 → 不动装饰器。
    Gui::IncidentMarkerDemo::stop();   // 前置清场
    bool const ran = tool->run();
    EXPECT_TRUE(ran);
    // 幂等清场（本二进制他测试的真视口在册时 run 可能真实 toggle——
    // PathDecorationTestLock 同款注释语义）。
    Gui::IncidentMarkerDemo::stop();
    delete tool;
}
