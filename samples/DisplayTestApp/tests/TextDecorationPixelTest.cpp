// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — text decoration E2E pixel lock
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/TextDecoration.ts
//              （命令面消费序）。
// Authored: no reference test exists in itwinjs-core for TextDecoration.ts
//           （display-test-app 无前端测试套件）——像素锁按 §5(g)/§11.11：
//           ①位置断言（红字形像素聚于帧心三分带——`center` 命令语义）；
//           ②clear 往返（字形像素清零——装饰摘除的直接证据）；
//           ③keyin 走 ToolRegistry 注册面（与真实 app 同路径）。
//
// 生命周期纪律（§12.9 DqEventScope 悬垂退订同族教训）：装饰器必须在所属
// Viewport 销毁**之前**摘除（clear() → DropDecorator），否则 ViewManager 持
// 悬垂视口指针，后续渲染/退出路径在已亡视口取装饰 → 主线程挂死。故测试用
// **局部 host** 并在 ViewGuard 作用域内 clear；参考的进程级单例（module 级
// `const editor`）只承载真实 app 的跨 keyin 编辑态，测试不触碰。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QImage>

#include "Gui/DtaTools.h"
#include "Gui/SaveImageTool.h"
#include "Gui/TextDecorationTool.h"
#include "View3DInventor.h"

#include <App/Application.h>

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace { struct QtEnvTD { QtEnvTD() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvTD s_qtTD;

namespace {

// DtaToolsWiring 同型的 App/引擎就绪 + 注册面（真实 app 的 main.cpp 对应物）。
void ensureAppReadyTD()
{
    if (!App::Application::_pcSingleton)
        App::Application::_pcSingleton = new App::Application();
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "TextDecorationPixel";
        opts.applicationVersion = "1.0";
        app.Startup(opts);
    }
    Gui::registerDtaTools();
}

void spinTD(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

struct ViewGuardTD {
    std::unique_ptr<Gui::View3DInventor> view;
    explicit ViewGuardTD(int w = 800, int h = 600)
    {
        view = std::make_unique<Gui::View3DInventor>(nullptr, nullptr, nullptr);
        view->resize(w, h);
        view->show();
        spinTD(300);  // 视口注册进 ViewManager（ViewGuardDtw 同型）
    }
    ~ViewGuardTD() { view.reset(); }
    dqApp::Viewport* vp() const { return view->getUeViewport(); }
};

// host 生命周期守卫：断言提前失败时也必须 DropDecorator（§12.9——装饰器
// 悬垂于 ViewManager 的装饰列表 → 视口销毁后收集装饰 UAF）。
struct HostGuardTD {
    dta::TextAnnotationHost& host;
    ~HostGuardTD() { host.clear(); }
};

// 局部 host 的命令驱动（不经过 ToolRegistry——测试不触碰进程级单例编辑态；
// keyin 注册面由独立断言覆盖）。
bool runCmd(dqApp::Viewport& vp, dta::TextAnnotationHost& editor,
            std::vector<std::string> const& args, std::string* err = nullptr)
{
    return dta::runTextDecorationCommand(vp, editor, args, err);
}

struct FrameScan {
    long red = 0;
    bool inCenterThird = false;
};

FrameScan scanFrame(dqApp::Viewport& vp)
{
    vp.RenderFrame();
    QApplication::processEvents();
    FrameScan out;
    std::string const path
        = std::string(DANQING_TILE_ASSETS_DIR) + "/../../build/td-pixel-frame.png";
    if (!Gui::SaveImageTool::writeFrameToFile(vp, QString::fromStdString(path)))
        return out;
    QImage image(QString::fromStdString(path));
    if (image.isNull())
        return out;
    int const w = image.width();
    int const h = image.height();
    int minX = w, maxX = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb const px = image.pixel(x, y);
            int const r = qRed(px), g = qGreen(px), b = qBlue(px);
            if (r > 120 && r > g + 50 && r > b + 50) {  // R 主导（背景深灰/网格白）
                ++out.red;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
            }
        }
    }
    if (out.red > 0) {
        int const lo = w / 3, hi = 2 * w / 3;
        out.inCenterThird = (minX < hi && maxX >= lo);
    }
    return out;
}

}  // namespace

// 全链：命令面 → TextBlock 编辑 → layout（QRawFont 度量缝）→ 几何生成
// → GeometryStream → GraphicBuilder → 上屏 → clear 摘除。
TEST(TextDecorationPixel, RendersGlyphsAtCenterAndClearRestores)
{
    ensureAppReadyTD();
    ViewGuardTD guard;
    ASSERT_NE(guard.vp(), nullptr);
    dta::TextAnnotationHost editor;
    editor.init(*guard.vp());
    HostGuardTD const hostGuard{editor};

    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"center"}));
    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"color", "red"}));
    // 空连接视图量值域宽（±数百米），1m 文字在 800×600 上不足 1px——取
    // 20m 保证字形轮廓 ≥10px 高（判据 >50 像素有 ≥3× 余量；DTA 演示中的
    // `textheight` 同为米制量级参数）。
    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"textheight", "20"}));
    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"text", "HELLO"}));
    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"update"}));
    spinTD(200);

    FrameScan const with = scanFrame(*guard.vp());
    EXPECT_GT(with.red, 50) << "red glyph pixels absent after update";
    EXPECT_TRUE(with.inCenterThird) << "glyph pixels not in center third (WHERE anchor)";

    // clear 往返——装饰摘除（DropDecorator）+ 像素清零。
    ASSERT_TRUE(runCmd(*guard.vp(), editor, {"clear"}));
    spinTD(120);
    FrameScan const after = scanFrame(*guard.vp());
    EXPECT_LT(after.red, with.red / 10) << "glyph pixels persisted after clear";
}

// 命令面宽锁（frame/leader/anchor/margin/fraction/justify/width……）+
// ToolRegistry keyin 注册面（真实 app 同路径；末尾 clear 摘除全局单例装饰）。
TEST(TextDecorationPixel, CommandSurfaceAndKeyinRegistration)
{
    ensureAppReadyTD();
    ViewGuardTD guard;
    ASSERT_NE(guard.vp(), nullptr);
    dta::TextAnnotationHost editor;
    editor.init(*guard.vp());
    HostGuardTD const hostGuard{editor};

    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"frame", "shape", "rectangle"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"frame", "borderWeight", "2"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"anchor", "center"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"anchor", "middle"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"margin", "all", "0.2"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"leader"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"fraction", "1", "2"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"fractiontype", "diagonal"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"justify", "center"}));
    EXPECT_TRUE(runCmd(*guard.vp(), editor, {"width", "20"}));

    // 未知名 → false + error 文本（参考 switch default 抛错面）。
    std::string err;
    EXPECT_FALSE(runCmd(*guard.vp(), editor, {"bogus-command"}, &err));
    EXPECT_FALSE(err.empty());

    // ➖9 编辑写入族结案：insert/insertstyle/updatestyle/deletestyle 明确缺席。
    for (char const* cmd : {"insert", "insertstyle", "updatestyle", "deletestyle"}) {
        std::string e;
        EXPECT_FALSE(runCmd(*guard.vp(), editor, {cmd}, &e)) << cmd;
        EXPECT_NE(e.find("not supported"), std::string::npos) << cmd;
    }

    // keyin 注册面（ToolRegistry 路径——`dta text clear` 收尾：全局单例编辑器
    // 若已 init 则在此摘除装饰，Viewport 销毁前闭环）。
    auto& registry = dqApp::Application::Get().GetToolAdmin().GetRegistry();
    EXPECT_EQ(registry.parseAndRun("dta text clear"), dqApp::ParseAndRunResult::Success);
    // 局部 host 摘除由 HostGuardTD 在作用域出口执行（Viewport 销毁前——
    // 生命周期纪律；断言提前失败同样覆盖）。
}
