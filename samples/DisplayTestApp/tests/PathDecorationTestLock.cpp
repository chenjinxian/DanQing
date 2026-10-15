// PathDecorationTestLock — M-W W-b1 PathDecorationTest 锁族（几何钉值 /
// toggle 状态机 / 拾取对账面 / canvas 占位调用序 / 工具注册面）。
//
// Authored: no reference test exists in itwinjs-core for PathDecorationTest
//           （display-test-app 无前端测试套件；行为锚定
//           PathDecorationTest.ts:22-109 逐行——几何钉值由参考公式
//           :84-109 直接计算，非自造场景）。
//
// RED（W-b1 落地前）：Gui::PathDecorationTest / getPath / 注册面不存在——
// 编译期缺 API 红（MacroToolTest 同型先例）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "Gui/PathDecorationTest.h"
#include "Gui/DtaTools.h"  // registerDtaTools（宿主注册面）
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include <dqGeom/Arc3d.h>
#include <dqGeom/LineString3d.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace { struct QtEnvPD { QtEnvPD() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvPD s_qtPD;

namespace {

dqGeom::Range3d const kExtents(0.0, 0.0, 0.0, 100.0, 100.0, 20.0);

// mock CanvasContext——记录调用序（CanvasDecoration.drawDecoration 契约的
// 无头锁面；CanvasDecoration.h:41-80 全虚面）。
class RecordingCanvasContext final : public dqRender::CanvasContext {
public:
    void save() override { ++saveCount; }
    void restore() override { ++restoreCount; }
    void translate(double x, double y) override { translates.emplace_back(x, y); }
    void beginPath() override { ++beginCount; }
    void moveTo(double x, double y) override { moves.emplace_back(x, y); }
    void lineTo(double x, double y) override { lines.emplace_back(x, y); }
    void stroke() override { ++strokeCount; }
    void setStrokeStyle(dqCommon::ColorDef color) override { strokeStyles.push_back(color); }
    void setLineWidth(double w) override { lineWidths.push_back(w); }
    void setFillStyle(dqCommon::ColorDef color) override { fillStyles.push_back(color); }
    void setGlobalAlpha(double alpha) override { alphas.push_back(alpha); }
    void arc(double, double, double, double, double) override { ++arcCount; }
    void fill() override { ++fillCount; }
    void drawImage(dqRender::rhi::TextureHandle, uint32_t, uint32_t, double, double) override
    {
        ++imageCount;
    }

    int saveCount = 0, restoreCount = 0, beginCount = 0, strokeCount = 0;
    int arcCount = 0, fillCount = 0, imageCount = 0;
    std::vector<std::pair<double, double>> translates, moves, lines;
    std::vector<dqCommon::ColorDef> strokeStyles, fillStyles;
    std::vector<double> lineWidths, alphas;
};

}  // namespace

// _getPath 几何钉值（:84-109）——1000 迭代 ×（线段串 [a,b,c,d] + 90° 弧）
// 交替；首/末代参数由参考公式逐位计算（extents (0,0,0)-(100,100,20)）。
TEST(PathDecorationTestLock, PathGeometryAlternatesLineStringsAndArcs)
{
    auto const path = Gui::getPath(kExtents);
    ASSERT_TRUE(path.IsValid());
    auto const& children = path->Curves();
    ASSERT_EQ(children.size(), 2000u);  // :93-105 每代 2 子曲线

    for (size_t i = 0; i < children.size(); ++i) {
        // :104 偶位=LineString3d / :105 奇位=Arc3d（交替序）。
        dqGeom::CurveType const expect =
            (i % 2 == 0) ? dqGeom::CurveType::LineString : dqGeom::CurveType::Arc;
        ASSERT_EQ(children[i]->GetCurveType(), expect) << "child " << i;
    }

    // 首代（i=0）：fract=0.0002、halfFract=0.0001（:94-95）。
    {
        auto const& ls = static_cast<dqGeom::LineString3d const&>(*children[0]);
        ASSERT_EQ(ls.PointCount(), 4u);  // :104 [a,b,c,d]
        // a = range.fractionToPoint(0,0,0) = low（:97）。
        EXPECT_NEAR(ls.Points()[0].x, 0.0, 1e-12);
        EXPECT_NEAR(ls.Points()[0].y, 0.0, 1e-12);
        EXPECT_NEAR(ls.Points()[0].z, 0.0, 1e-12);
        // b = (halfFract,0,0)（:98）= (0.01,0,0)。
        EXPECT_NEAR(ls.Points()[1].x, 0.01, 1e-12);
        EXPECT_NEAR(ls.Points()[1].y, 0.0, 1e-12);
        EXPECT_NEAR(ls.Points()[1].z, 0.0, 1e-12);
        // c = (halfFract,halfFract,0)（:99）= (0.01,0.01,0)。
        EXPECT_NEAR(ls.Points()[2].x, 0.01, 1e-12);
        EXPECT_NEAR(ls.Points()[2].y, 0.01, 1e-12);
        EXPECT_NEAR(ls.Points()[2].z, 0.0, 1e-12);
        // d = (halfFract,halfFract,halfFract)（:100）= (0.01,0.01,0.002)。
        EXPECT_NEAR(ls.Points()[3].x, 0.01, 1e-12);
        EXPECT_NEAR(ls.Points()[3].y, 0.01, 1e-12);
        EXPECT_NEAR(ls.Points()[3].z, 0.002, 1e-12);

        auto const& arc = static_cast<dqGeom::Arc3d const&>(*children[1]);
        // 弧心 = d（:105 首参）。
        EXPECT_NEAR(arc.CenterRef().x, 0.01, 1e-12);
        EXPECT_NEAR(arc.CenterRef().y, 0.01, 1e-12);
        EXPECT_NEAR(arc.CenterRef().z, 0.002, 1e-12);
        // vector0 = d→e（:101/:105）= (0.015-0.01, 0.02-0.01, 0.004-0.002)。
        EXPECT_NEAR(arc.Vector0().x, 0.005, 1e-12);
        EXPECT_NEAR(arc.Vector0().y, 0.01, 1e-12);
        EXPECT_NEAR(arc.Vector0().z, 0.002, 1e-12);
        // vector90 = d→f（:102/:105）= (0.02-0.01, 0.02-0.01, 0.004-0.002)。
        EXPECT_NEAR(arc.Vector90().x, 0.01, 1e-12);
        EXPECT_NEAR(arc.Vector90().y, 0.01, 1e-12);
        EXPECT_NEAR(arc.Vector90().z, 0.002, 1e-12);
        // sweep 0°→90°（:105 AngleSweep.createStartEndDegrees(0, 90)）。
        EXPECT_NEAR(arc.Sweep().StartDegrees(), 0.0, 1e-12);
        EXPECT_NEAR(arc.Sweep().EndDegrees(), 90.0, 1e-12);
    }

    // 末代（i=999）：fract=0.2、halfFract=0.1。
    {
        auto const& arc = static_cast<dqGeom::Arc3d const&>(*children[1999]);
        // d = (10,10,2)；e=(15,20,4)；f=(20,20,4)。
        EXPECT_NEAR(arc.CenterRef().x, 10.0, 1e-9);
        EXPECT_NEAR(arc.CenterRef().y, 10.0, 1e-9);
        EXPECT_NEAR(arc.CenterRef().z, 2.0, 1e-9);
        EXPECT_NEAR(arc.Vector0().x, 5.0, 1e-9);   // e-d
        EXPECT_NEAR(arc.Vector0().y, 10.0, 1e-9);
        EXPECT_NEAR(arc.Vector0().z, 2.0, 1e-9);
        EXPECT_NEAR(arc.Vector90().x, 10.0, 1e-9); // f-d
        EXPECT_NEAR(arc.Vector90().y, 10.0, 1e-9);
        EXPECT_NEAR(arc.Vector90().z, 2.0, 1e-9);
        EXPECT_NEAR(arc.Sweep().EndDegrees(), 90.0, 1e-12);
    }
}

// toggle 三态（:66-71）+ 静态 decorator 指针语义（:23）+ ViewManager
// add/drop 接线（:55/:61）+ ctor 路径接线（:30）+ useCachedDecorations（:34）。
TEST(PathDecorationTestLock, ToggleStartsAndStopsDecorator)
{
    auto& vm = dqApp::Application::Get().GetViewManager();

    // 前置态：若他测试残留激活态则先停（幂等对齐——Stop 分支忽略 extents）。
    if (Gui::PathDecorationTest::s_decorator != nullptr)
        Gui::PathDecorationTest::Toggle(kExtents);
    ASSERT_EQ(Gui::PathDecorationTest::s_decorator, nullptr);  // :23 初态

    Gui::PathDecorationTest::Toggle(kExtents);  // :67-68 → start
    Gui::PathDecorationTest* const dec = Gui::PathDecorationTest::s_decorator;
    ASSERT_NE(dec, nullptr);
    // :55 addDecorator 接线（ViewManager 表含本装饰器）。
    auto const& decorators = vm.GetDecorators();
    EXPECT_NE(std::find(decorators.begin(), decorators.end(), dec), decorators.end());
    // :30 ctor 建路径（2000 子曲线——getPath 同族钉值）。
    ASSERT_TRUE(dec->path().IsValid());
    EXPECT_EQ(dec->path()->Curves().size(), 2000u);
    // :34 useCachedDecorations=true。
    EXPECT_TRUE(dec->UseCachedDecorations());

    Gui::PathDecorationTest::Toggle(kExtents);  // :69-70 → stop
    EXPECT_EQ(Gui::PathDecorationTest::s_decorator, nullptr);  // :62
    // :61 dropDecorator 接线（指针级——dec 已 delete，只比地址）。
    auto const& after = vm.GetDecorators();
    EXPECT_EQ(std::find(after.begin(), after.end(), dec), after.end());
}

// testDecorationHit / getDecorationGeometry 面（:47/:50）。
TEST(PathDecorationTestLock, DecorationHitMatchesPickId)
{
    Gui::PathDecorationTest dec(kExtents);

    // _pickId 未分配（:27 undefined——Decorate 未跑）：任何 id 不命中。
    EXPECT_FALSE(dec.TestDecorationHit(0u));
    EXPECT_FALSE(dec.TestDecorationHit(0xFF000001u));

    dec.AssignPickIdForTest();  // :38-39 惰性赋值行
    ASSERT_TRUE(dec.pickId().has_value());
    uint32_t const id = *dec.pickId();
    // :47 id === _pickId。
    EXPECT_TRUE(dec.TestDecorationHit(id));
    EXPECT_FALSE(dec.TestDecorationHit(id + 1));

    // ViewManager 对账链（ViewManager.cpp:215-223 FindDecoratorForHit 经
    // TestDecorationHit 逐装饰器查询）。
    auto& vm = dqApp::Application::Get().GetViewManager();
    vm.AddDecorator(&dec);
    EXPECT_EQ(vm.FindDecoratorForHit(id), &dec);
    EXPECT_EQ(vm.FindDecoratorForHit(id + 7), nullptr);
    vm.DropDecorator(&dec);
    EXPECT_EQ(vm.FindDecoratorForHit(id), nullptr);

    // getDecorationGeometry（:50 恒 undefined）——DanQing IDecorator 无该虚面
    // （Decorator.h）；参考恒返 undefined ≡ 方法缺席（无拾取几何）。登记于
    // PathDecorationTest.h 头注 E4b。
}

// canvas 文本占位调用序（E7——PathCanvasDecoration :10-18；无头 mock 面）。
TEST(PathDecorationTestLock, CanvasDecorationDrawsPlaceholderAt256)
{
    auto const& decoration = Gui::PathDecorationTest::s_canvasDecoration;
    // 参考未设 position（:10-18——drawDecoration 内绝对坐标 (256,256)）。
    EXPECT_FALSE(decoration.position.has_value());
    ASSERT_TRUE(static_cast<bool>(decoration.drawDecoration));

    RecordingCanvasContext ctx;
    decoration.drawDecoration(ctx);

    // 矩形路径：beginPath ×1 + moveTo ×1 + lineTo ×4（闭合）+ fill ×1。
    EXPECT_EQ(ctx.beginCount, 1);
    ASSERT_EQ(ctx.moves.size(), 1u);
    ASSERT_EQ(ctx.lines.size(), 4u);
    EXPECT_EQ(ctx.fillCount, 1);
    EXPECT_EQ(ctx.strokeCount, 0);
    EXPECT_EQ(ctx.imageCount, 0);

    // fillStyle=white（:15）。
    ASSERT_EQ(ctx.fillStyles.size(), 1u);
    EXPECT_TRUE(ctx.fillStyles[0].equals(dqCommon::ColorDef::white));

    // 占位框几何：居中 (256,256)（:16——textAlign center / textBaseline
    // middle），宽 = 32 × 14 × 0.6 = 268.8、高 = 14（E7 公式）。
    constexpr double kHalfW = 32.0 * 14.0 * 0.6 * 0.5;  // 134.4
    constexpr double kHalfH = 7.0;
    EXPECT_NEAR(ctx.moves[0].first, 256.0 - kHalfW, 1e-9);
    EXPECT_NEAR(ctx.moves[0].second, 256.0 - kHalfH, 1e-9);
    EXPECT_NEAR(ctx.lines[0].first, 256.0 + kHalfW, 1e-9);
    EXPECT_NEAR(ctx.lines[0].second, 256.0 - kHalfH, 1e-9);
    EXPECT_NEAR(ctx.lines[1].first, 256.0 + kHalfW, 1e-9);
    EXPECT_NEAR(ctx.lines[1].second, 256.0 + kHalfH, 1e-9);
    EXPECT_NEAR(ctx.lines[2].first, 256.0 - kHalfW, 1e-9);
    EXPECT_NEAR(ctx.lines[2].second, 256.0 + kHalfH, 1e-9);
    // 闭合段回起点。
    EXPECT_NEAR(ctx.lines[3].first, ctx.moves[0].first, 1e-9);
    EXPECT_NEAR(ctx.lines[3].second, ctx.moves[0].second, 1e-9);
}

// 工具注册面（App.ts 注册表 + SVTTools.json:67-68 keyin）+ run 无视口恒
// true（:76-81）。
TEST(PathDecorationTestLock, ToolRegisteredWithKeyin)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    Gui::registerDtaTools();

    auto* tool = admin.GetRegistry().Create("TogglePathDecoration");
    ASSERT_NE(tool, nullptr);
    EXPECT_STREQ("TogglePathDecoration", tool->getToolId());
    EXPECT_EQ(tool->englishKeyin(), "dta path decoration");
    // 参考未覆写 minArgs/maxArgs = Tool 默认 0/0。
    EXPECT_EQ(tool->minArgs(), 0);
    EXPECT_EQ(tool->maxArgs(), 0);
    delete tool;

    // run() 恒 true（:80——无视口时不动装饰器）。本二进制为全量同进程——
    // 他测试的真视口可能在册（GetActiveViewport 非空则真实 toggle 一次），
    // 断言后幂等清场（Stop 分支忽略 extents）。
    Gui::PathDecorationTestTool direct;
    EXPECT_TRUE(direct.run());
    if (Gui::PathDecorationTest::s_decorator != nullptr)
        Gui::PathDecorationTest::Toggle(kExtents);
    EXPECT_EQ(Gui::PathDecorationTest::s_decorator, nullptr);
}

// ---------------------------------------------------------------------------
// 真窗像素锁（主会话收口跑——W-b1 真窗件，本代理只验证编译不执行）。
// 复现配方：View3DInventor 真窗 → resetBlankConnection(0,0,0,100,100,20)
// → LookAtVolume 取景 → 基线帧 → Toggle 装装饰 → 渲染帧对拍 → Toggle 拆
// 装饰 → 恢复对拍。
// Authored: no reference test exists（渲染输出级回归；行为锚定
// PathDecorationTest.ts:37-44 的 WorldDecoration+canvas 双挂载）。
// ---------------------------------------------------------------------------
namespace {

void spinPD(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 区域近白像素计数（占位框=白填充——E7）。
size_t countNearWhite(std::vector<uint8_t> const& f, uint32_t w, uint32_t h,
                      uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    size_t n = 0;
    for (uint32_t y = y0; y < y1 && y < h; ++y)
        for (uint32_t x = x0; x < x1 && x < w; ++x) {
            size_t const px = (static_cast<size_t>(y) * w + x) * 4;
            if (f[px] > 220 && f[px + 1] > 220 && f[px + 2] > 220)
                ++n;
        }
    return n;
}

// 全帧差计数（逐字节容差 8——AA 抖动带）。
size_t countFrameDiff(std::vector<uint8_t> const& a, std::vector<uint8_t> const& b)
{
    size_t n = 0;
    size_t const count = std::min(a.size(), b.size());
    for (size_t i = 0; i < count; i += 4)
        if (std::abs(int(a[i]) - int(b[i])) > 8 ||
            std::abs(int(a[i + 1]) - int(b[i + 1])) > 8 ||
            std::abs(int(a[i + 2]) - int(b[i + 2])) > 8)
            ++n;
    return n;
}

}  // namespace

// Authored（§5(g) 窗口/呈现回归授权——参考无 canvas 装饰像素的对应测试）。
TEST(PathDecorationPixel, ToggleOnShowsPathAndCanvasPlaceholder)
{
    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spinPD(400);

    // 参考 DTA 入口经 blank connection（Surface.ts:155-163 先例形态——示例
    // 运行在自己的 extents 上）。
    view.resetBlankConnection(dqGeom::Range3d(0.0, 0.0, 0.0, 100.0, 100.0, 20.0),
                              "Path Decoration Test");
    spinPD(400);

    auto* vp = view.getUeViewport();
    ASSERT_NE(vp, nullptr);
    // 取景到 projectExtents（路径布点域——DecorationGeometryExample.cpp:466-467
    // 先例）。
    if (auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr)
        view3d->LookAtVolume(vp->GetIModel()->GetProjectExtents());
    spinPD(300);

    vp->RenderFrame();
    std::vector<uint8_t> baseline;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(vp->ReadFrameForTest(baseline, w, h));

    // Toggle 装装饰（:66-68）+ 失效重收（UseCachedDecorations=true 的缓存
    // 语义——显式 InvalidateDecorations 保收集）。
    Gui::PathDecorationTest::Toggle(kExtents);
    ASSERT_NE(Gui::PathDecorationTest::s_decorator, nullptr);
    vp->InvalidateDecorations();
    vp->InvalidateScene();
    spinPD(300);
    vp->RenderFrame();
    std::vector<uint8_t> decorated;
    ASSERT_TRUE(vp->ReadFrameForTest(decorated, w, h));

    // 判据 1（路径上屏）：帧差显著（2000 子曲线的路径跨 projectExtents——
    // 取景后占屏中央带）。校准待主会话首跑钉值；阈值取保守下界。
    size_t const diffOn = countFrameDiff(baseline, decorated);
    printf("[PATHDECO] diffOn=%zu w=%u h=%u\n", diffOn, w, h);
    EXPECT_GT(diffOn, 500u) << "path decoration produced no visible change";

    // 判据 2（canvas 占位——WHERE）：(256,256) 占位框区近白像素新增
    //（框 ≈ 269×14≈3766 px；GL_LINES 栅格化后端按线族落——保守下界 200）。
    // 设备像素 == CSS 像素（测试 harness dpr=1——DecoGeometryPixel 先例同假设）。
    uint32_t const bx0 = 256 - 140, bx1 = 256 + 140, by0 = 256 - 10, by1 = 256 + 10;
    size_t const whiteBefore = countNearWhite(baseline, w, h, bx0, by0, bx1, by1);
    size_t const whiteAfter = countNearWhite(decorated, w, h, bx0, by0, bx1, by1);
    printf("[PATHDECO] placeholder white before=%zu after=%zu\n", whiteBefore, whiteAfter);
    EXPECT_GT(whiteAfter, whiteBefore + 200)
        << "canvas placeholder produced no new white pixels at (256,256)";

    // Toggle 拆装饰（:69-70）→ 占位区回落。
    Gui::PathDecorationTest::Toggle(kExtents);
    EXPECT_EQ(Gui::PathDecorationTest::s_decorator, nullptr);
    vp->InvalidateDecorations();
    vp->InvalidateScene();
    spinPD(300);
    vp->RenderFrame();
    std::vector<uint8_t> restored;
    ASSERT_TRUE(vp->ReadFrameForTest(restored, w, h));
    size_t const whiteRestored = countNearWhite(restored, w, h, bx0, by0, bx1, by1);
    printf("[PATHDECO] placeholder white restored=%zu\n", whiteRestored);
    EXPECT_LE(whiteRestored, whiteBefore + 50)
        << "canvas placeholder did not disappear after toggle off";

    view.close();
    spinPD(200);
}
