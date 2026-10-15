// WindowToolsTest — M-Y Y-b1 窗口 keyin 族七件锁（Surface.ts:451-598 +
// Window.ts dock/undock/resizeContent 段）。
//
// Authored: no reference test exists in itwinjs-core for the DTA window tools
//           （display-test-app 无前端测试套件；场景逐行锚定
//           Surface.ts:451-598 参数面/状态机与 Window.ts:185-335 dock 公式，
//           断言值由参考公式直接计算——非自造场景）。
//
// RED（Y-b1 落地前）：Gui::Surface/CreateWindowTool/... 类型不存在——编译期
// 缺 API 红（MacroToolTest 同型先例）；注册锁另经 stash 撤注册运行期实红。
//
// harness 说明：全文件经 Surface::setMdiAreaForTest 注入**裸 QMdiArea**
// （无 MainWindow/真窗/GL——MDI 状态机断言 = 几何/激活序/存活面）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QMdiArea>
#include <QMdiSubWindow>

#include "Gui/SurfaceTools.h"
#include "Gui/DtaTools.h"        // registerDtaTools（注册面锁）
#include "View3DInventor.h"

#include <dqApp/Application.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>

namespace { struct QtEnvWin { QtEnvWin() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvWin s_qtWin;

namespace {

// 裸 QMdiArea 夹具（900×600——dock 公式整除：半 450×300）。
struct MdiAreaFixture {
    QMdiArea area;
    MdiAreaFixture()
    {
        // 注意：QAbstractScrollArea 的 viewport 不随 area.resize 走（未 show 时
        // 维持 640×480 缺省——首跑实测），dock/ensureInSurface 公式读的是
        // **viewport** 矩形（参考读 surface element clientWidth/Height 同位），
        // 故双写。
        area.resize(900, 600);
        area.viewport()->resize(900, 600);
        Gui::Surface::setMdiAreaForTest(&area);
    }
    ~MdiAreaFixture() { Gui::Surface::resetForTest(); }
};

// 命名窗创建快捷面（经 Surface 层——参考 Surface.createNamedWindow 调用面）。
QMdiSubWindow* makeNamed(MdiAreaFixture& /*f*/, std::string const& id)
{
    Gui::NamedWindowProps props;
    props.id = id;
    return Gui::Surface::createNamedWindow(props);
}

// 无视口活件的"Start 页型"子窗（plain widget——closeAllViewers 存活面对照）。
QMdiSubWindow* makePlainSub(MdiAreaFixture& f, char const* name)
{
    auto* content = new QWidget();
    content->setObjectName(QString::fromLatin1(name));
    auto* sub = new QMdiSubWindow(f.area.viewport());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWidget(content);
    f.area.addSubWindow(sub);
    sub->show();
    return sub;
}

// View3DInventor 子窗（无头——不 show 主区；ctor 链 = DtaToolBarsTest 同款
// blank connection + ViewList + Viewport::Create，零 GL 直至 RenderFrame）。
QMdiSubWindow* makeViewerSub(MdiAreaFixture& f)
{
    auto* view = new Gui::View3DInventor(nullptr, nullptr, nullptr);
    auto* sub = new QMdiSubWindow(f.area.viewport());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWidget(view);
    f.area.addSubWindow(sub);
    sub->show();
    return sub;
}

}  // namespace

// CreateWindowTool：参数面（:466-480）+ 默认几何（Window.ts:358-372）+
// title 缺席=id（:437-439）+ id 缺席不建（:482-484）+ minArgs/maxArgs
// （:454-455——undefined→-1，E-Y2）。
TEST(SurfaceWindowTools, CreateParsesIdTitleGeometryAndDefaults)
{
    MdiAreaFixture f;
    Gui::CreateWindowTool tool;
    EXPECT_EQ(tool.minArgs(), 1);
    EXPECT_EQ(tool.maxArgs(), -1);

    // 全参（:467-480 五键）。
    EXPECT_TRUE(tool.parseAndRun({"id=n1", "title=Hello", "top=10", "left=20",
                                  "width=300", "height=200"}));
    auto subs = f.area.subWindowList();
    ASSERT_EQ(subs.size(), 1);
    auto* n1 = subs[0];
    ASSERT_NE(n1->widget(), nullptr);
    EXPECT_EQ(n1->widget()->objectName(), QStringLiteral("n1"));
    EXPECT_EQ(n1->windowTitle(), QStringLiteral("Hello"));
    EXPECT_EQ(n1->geometry(), QRect(20, 10, 300, 200));
    // addWindow→focus（Surface.ts:315）= MDI 激活。
    EXPECT_EQ(f.area.activeSubWindow(), n1);

    // 仅 id：默认几何 = 面 1/3（900/3=300、600/3=200）@ (0,0)；title=id。
    EXPECT_TRUE(tool.parseAndRun({"id=n2"}));
    subs = f.area.subWindowList();
    ASSERT_EQ(subs.size(), 2);
    auto* n2 = subs[1];
    EXPECT_EQ(n2->windowTitle(), QStringLiteral("n2"));
    EXPECT_EQ(n2->geometry(), QRect(0, 0, 300, 200));

    // id 缺席 → run 不执行（:482 门）→ 无新窗；恒 true（:486）。
    EXPECT_TRUE(tool.parseAndRun({"title=NoId"}));
    EXPECT_EQ(f.area.subWindowList().size(), 2);
}

// createNamedWindow 的 ensureInSurface 钳（Window.ts:247-281）+ 参考出界窗
// 回落语义。
TEST(SurfaceWindowTools, CreateClampsOutOfSurfaceGeometry)
{
    MdiAreaFixture f;
    Gui::CreateWindowTool tool;
    // top=550 + height=200 → 底 750 ≥ 600 → top 钳至 400（:262-263）。
    EXPECT_TRUE(tool.parseAndRun({"id=low", "top=550", "left=850", "width=200",
                                  "height=200"}));
    auto* sub = f.area.subWindowList().first();
    ASSERT_NE(sub, nullptr);
    EXPECT_EQ(sub->geometry(), QRect(700, 400, 200, 200));  // left 850+200≥900→700
}

// FocusWindowTool：by-id + 无参=焦点窗 + 未知 id no-op（:495-505）。
TEST(SurfaceWindowTools, FocusWindowByIdOrActive)
{
    MdiAreaFixture f;
    auto* n1 = makeNamed(f, "n1");
    auto* n2 = makeNamed(f, "n2");  // 后建者激活（addWindow→focus）
    ASSERT_EQ(f.area.activeSubWindow(), n2);

    Gui::FocusWindowTool tool;
    EXPECT_EQ(tool.minArgs(), 0);
    EXPECT_EQ(tool.maxArgs(), 1);
    EXPECT_TRUE(tool.parseAndRun({"n1"}));
    EXPECT_EQ(f.area.activeSubWindow(), n1);

    // 无参 → focusedWindow（:497 否臂）→ execute(焦点窗)=幂等保焦。
    EXPECT_TRUE(tool.parseAndRun({}));
    EXPECT_EQ(f.area.activeSubWindow(), n1);

    // 未知 id → findWindowById 空 → execute 不跑（:498 门）→ 焦点不变。
    EXPECT_TRUE(tool.parseAndRun({"bogus"}));
    EXPECT_EQ(f.area.activeSubWindow(), n1);
}

// Maximize/Restore 往复（:515-528 + WindowHeader.dock/undock :185-213）：
// Full=几何充满（非 Qt 最大化——E-Y1）+ 存档恢复 + 已 dock 不覆写存档
//（MaximizeWindowTool 的 isDocked 门 :519）。
TEST(SurfaceWindowTools, MaximizeRestoreRoundTrip)
{
    MdiAreaFixture f;
    auto* n1 = makeNamed(f, "n1");
    n1->setGeometry(20, 10, 300, 200);

    Gui::MaximizeWindowTool mx;
    EXPECT_TRUE(mx.parseAndRun({"n1"}));
    EXPECT_TRUE(Gui::Surface::isDocked(n1));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 900, 600));  // Dock.Full=全幅（:232 否臂）

    // 已 dock 再 max → :519 门不覆写存档（恢复后须回原几何）。
    EXPECT_TRUE(mx.parseAndRun({"n1"}));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 900, 600));

    Gui::RestoreWindowTool rs;
    EXPECT_TRUE(rs.parseAndRun({"n1"}));
    EXPECT_FALSE(Gui::Surface::isDocked(n1));
    EXPECT_EQ(n1->geometry(), QRect(20, 10, 300, 200));

    // 未 dock 的 restore = no-op（WindowHeader.undock :201-202）。
    EXPECT_TRUE(rs.parseAndRun({"n1"}));
    EXPECT_EQ(n1->geometry(), QRect(20, 10, 300, 200));
}

// CloseWindowTool：by-id + 无参=焦点窗 + 未知 id no-op（:530-535 +
// Surface.close :407-410）。
TEST(SurfaceWindowTools, CloseWindowByIdOrActive)
{
    MdiAreaFixture f;
    makeNamed(f, "n1");
    auto* n2 = makeNamed(f, "n2");
    ASSERT_EQ(f.area.subWindowList().size(), 2);

    Gui::CloseWindowTool tool;
    EXPECT_TRUE(tool.parseAndRun({"bogus"}));  // 未知 id no-op
    EXPECT_EQ(f.area.subWindowList().size(), 2);

    EXPECT_TRUE(tool.parseAndRun({"n1"}));
    QCoreApplication::processEvents();  // WA_DeleteOnClose 落删
    auto subs = f.area.subWindowList();
    ASSERT_EQ(subs.size(), 1);
    EXPECT_EQ(subs[0]->widget()->objectName(), QStringLiteral("n2"));

    // 无参 → 焦点窗（n2——n1 删除后唯一存活且激活）。
    EXPECT_TRUE(tool.parseAndRun({}));
    QCoreApplication::processEvents();
    EXPECT_EQ(f.area.subWindowList().size(), 0);
    (void)n2;
}

// ResizeWindowTool：+4 边框 kludge + min 50 钳 + 清 dock 态（Window.ts:
// 318-335）+ OR 怪癖（Surface.ts:552——单侧 NaN 也 run，E-Y4）。
TEST(SurfaceWindowTools, ResizeAddsFourClampsMinAndClearsDock)
{
    MdiAreaFixture f;
    auto* n1 = makeNamed(f, "n1");
    n1->setGeometry(0, 0, 300, 200);
    Gui::Surface::dock(n1, Gui::Dock::Left);  // 先入 dock 态——resize 须清
    ASSERT_TRUE(Gui::Surface::isDocked(n1));

    Gui::ResizeWindowTool tool;
    EXPECT_EQ(tool.minArgs(), 2);
    EXPECT_EQ(tool.maxArgs(), 3);

    // w=500/h=300 → 目标内容 504×304（:319-320）；delta 语义断言（无头窗
    // 框架宽不可测——以 sub 尺寸增量对账内容增量）。
    QSize const subBefore = n1->size();
    QSize const contentBefore = n1->widget()->size();
    EXPECT_TRUE(tool.parseAndRun({"500", "300", "n1"}));
    EXPECT_FALSE(Gui::Surface::isDocked(n1));  // :323 清 dock
    EXPECT_EQ(n1->size().width() - subBefore.width(),
              504 - contentBefore.width());
    EXPECT_EQ(n1->size().height() - subBefore.height(),
              304 - contentBefore.height());

    // min 50 钳（:321-322——resize(1,1) → 5×5 → 钳 50×50）。
    QSize const contentMid = n1->widget()->size();
    QSize const subMid = n1->size();
    EXPECT_TRUE(tool.parseAndRun({"1", "1", "n1"}));
    EXPECT_EQ(n1->size().width() - subMid.width(), 50 - contentMid.width());
    EXPECT_EQ(n1->size().height() - subMid.height(), 50 - contentMid.height());

    // OR 怪癖腿 1：w 侧 NaN、h=150 → run 照跑（:552），h 侧落 154、w 侧不动
    //（NaN 侧 resizeContent 跳过——E-Y1）。
    QSize const subPreNaN = n1->size();
    QSize const contentPreNaN = n1->widget()->size();
    EXPECT_TRUE(tool.parseAndRun({"abc", "150", "n1"}));
    EXPECT_EQ(n1->size().width(), subPreNaN.width());
    EXPECT_EQ(n1->size().height() - subPreNaN.height(),
              154 - contentPreNaN.height());

    // OR 怪癖腿 2：双 NaN → run 不跑（:552 否臂）→ 尺寸不变。
    QSize const subPreBoth = n1->size();
    EXPECT_TRUE(tool.parseAndRun({"abc", "xyz", "n1"}));
    EXPECT_EQ(n1->size(), subPreBoth);
}

// DockWindowTool：l/r/t/b 位累积 + 半边/四分几何（Window.ts:232-242）+
// 未知字母早退怪癖（:589-590，E-Y5）+ 空串不 run（:592-593）。
TEST(SurfaceWindowTools, DockLetterParsingQuartersAndEarlyReturn)
{
    MdiAreaFixture f;
    auto* n1 = makeNamed(f, "n1");
    n1->setGeometry(20, 10, 300, 200);

    Gui::DockWindowTool tool;
    EXPECT_EQ(tool.minArgs(), 1);
    EXPECT_EQ(tool.maxArgs(), 2);

    // "l" → 左半（:238-239 w=hw，l=0）。
    EXPECT_TRUE(tool.parseAndRun({"l", "n1"}));
    EXPECT_TRUE(Gui::Surface::isDocked(n1));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 450, 600));

    // "rb" → 右下半四分（:240-241 l=w=hw +:235-236 t=h=hh）。
    EXPECT_TRUE(tool.parseAndRun({"rb", "n1"}));
    EXPECT_EQ(n1->geometry(), QRect(450, 300, 450, 300));

    // "tl" → 左上四分。
    EXPECT_TRUE(tool.parseAndRun({"tl", "n1"}));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 450, 300));

    // 未知字母早退（:589-590）——已累积的 'l' 位不落窗：dock 态与几何保持
    // "tl" 旧态。
    EXPECT_TRUE(tool.parseAndRun({"lx", "n1"}));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 450, 300));

    // 空串 → dock=0 → 不 run（:592 门）。
    EXPECT_TRUE(tool.parseAndRun({"", "n1"}));
    EXPECT_EQ(n1->geometry(), QRect(0, 0, 450, 300));

    // undock 恢复 dock("l") 前的存档？——注意存档语义：首次 dock("l") 存档
    // (20,10,300,200)；后续 dock 不覆写（Window.ts:187-194 门）。udock 恢复之。
    Gui::RestoreWindowTool rs;
    EXPECT_TRUE(rs.parseAndRun({"n1"}));
    EXPECT_EQ(n1->geometry(), QRect(20, 10, 300, 200));
}

// findWindowById 双形态（Surface.ts:399-401 + Viewer.ts:609 viewportId 串）。
TEST(SurfaceWindowTools, FindWindowByIdMatchesViewerIdAndNamedId)
{
    MdiAreaFixture f;
    auto* viewerSub = makeViewerSub(f);
    auto* namedSub = makeNamed(f, "panel-a");
    ASSERT_NE(viewerSub, nullptr);
    auto* view = qobject_cast<Gui::View3DInventor*>(viewerSub->widget());
    ASSERT_NE(view, nullptr);
    auto* vp = view->getUeViewport();
    ASSERT_NE(vp, nullptr);

    // 视口窗 id = viewportId 串（Viewer.ts:609）。
    QString const vpId = QString::number(vp->GetViewportId());
    EXPECT_EQ(Gui::Surface::findWindowById(vpId), viewerSub);
    // 命名窗 id = 注册串。
    EXPECT_EQ(Gui::Surface::findWindowById(QStringLiteral("panel-a")), namedSub);
    EXPECT_EQ(Gui::Surface::findWindowById(QStringLiteral("bogus")), nullptr);

    viewerSub->close();
    QCoreApplication::processEvents();
}

// 注册面：七件 keyin + min/maxArgs（SVTTools.json:25-50 en 键 + App.ts
// 工具表 :401-441）。
TEST(SurfaceWindowTools, RegisteredWithKeyins)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    Gui::registerDtaTools();

    struct Expect { char const* id; char const* keyin; int minA; int maxA; };
    Expect const expects[] = {
        {"CreateWindow", "win create", 1, -1},
        {"FocusWindow", "win focus", 0, 1},
        {"MaximizeWindow", "win max", 0, 1},
        {"RestoreWindow", "win restore", 0, 1},
        {"CloseWindow", "win close", 0, 1},
        {"ResizeWindow", "win resize", 2, 3},
        {"DockWindow", "win dock", 1, 2},
    };
    for (auto const& e : expects) {
        auto* tool = admin.GetRegistry().Create(e.id);
        ASSERT_NE(tool, nullptr) << e.id;
        EXPECT_STREQ(e.id, tool->getToolId());
        EXPECT_EQ(tool->englishKeyin(), e.keyin) << e.id;
        EXPECT_EQ(tool->minArgs(), e.minA) << e.id;
        EXPECT_EQ(tool->maxArgs(), e.maxA) << e.id;
        delete tool;
    }
}
