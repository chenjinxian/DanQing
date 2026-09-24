// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewUndoTest — Viewport 视图撤销/重做栈
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics (reference exercises via app-level flows, no direct unit test);
//           scenarios from Viewport.ts:3634-3718.
#include <gtest/gtest.h>
#include <memory>
#include <QApplication>
#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewTool.h>  // complete type for ViewTool downcast (adopt)
#include <dqApp/ViewManager.h>
#include <dqApp/ViewPicker.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>


using dqApp::ViewUndoEvent;  // 任务书测试体在全局作用域非限定使用该枚举

namespace { struct QtEnv { QtEnv() { if (!qApp) { static int argc=1; static char n[]="t"; static char* av[]={n,nullptr}; new QApplication(argc,av);} } }; }
static QtEnv s_qt;

namespace {
struct VpGuard {
    dqBase::RefPtr<dqApp::BlankConnection> conn;
    dqApp::Viewport* vp = nullptr;
    VpGuard() {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(-1000,-1000,-100, 1000,1000,100);
        conn = dqApp::BlankConnection::create(props);
        auto view = dqApp::ViewList::create(conn.Get()).getDefaultView(conn.Get());
        vp = dqApp::Viewport::Create(nullptr, view);
        dqApp::Application::Get().GetViewManager().AddViewport(vp);
    }
    ~VpGuard() {
        dqApp::Application::Get().GetViewManager().DropViewport(vp);
        delete vp;
    }
};
}  // namespace

// saveViewUndo：首次调用建基线不产条目；改动后产 1 条目并清 forward（Viewport.ts:3648-3676）。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3648-3676.
TEST(ViewUndo, SaveViewUndoBaselineAndFirstEntry)
{
    VpGuard g;
    EXPECT_FALSE(g.vp->isUndoPossible());   // 基线期无条目

    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(42, 0, 0));
    g.vp->synchWithView();                   // 触发保存
    EXPECT_TRUE(g.vp->isUndoPossible());     // 产生 1 条
    EXPECT_FALSE(g.vp->isRedoPossible());

    g.vp->synchWithView();                   // 无新变化 → equalState 早退，不增条目
    // 再改一次 → 第二条目
    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(43, 0, 0));
    g.vp->synchWithView();
    // 注意：0.5s 防抖窗口内第二条目不追加（Viewport.ts:3664-3673）→ 仍 1 条
    // （无法等待 0.5s 的防抖过期——用 clearViewUndo 验证栈清空代替精确计数）
    g.vp->clearViewUndo();
    EXPECT_FALSE(g.vp->isUndoPossible());
    EXPECT_FALSE(g.vp->isRedoPossible());
}

// doUndo/doRedo：姿态恢复 + 栈移动（Viewport.ts:3678-3704）。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3678-3704.
TEST(ViewUndo, DoUndoRedoRestoresPose)
{
    VpGuard g;
    auto* view = g.vp->GetView();
    auto const origin0 = view->GetOrigin();

    view->SetOrigin(dqGeom::Point3d::From(origin0.x + 100, origin0.y, origin0.z));
    g.vp->synchWithView();
    ASSERT_TRUE(g.vp->isUndoPossible());

    ViewUndoEvent lastEvent = ViewUndoEvent::Redo;  // 哨兵（期望被覆写为 Undo）
    int eventCount = 0;
    dqBase::DqEventScope scope;
    scope.add(g.vp->OnViewUndoRedo.AddListener(
        [&](dqApp::Viewport*, ViewUndoEvent e) { lastEvent = e; ++eventCount; }));

    g.vp->doUndo();
    EXPECT_NEAR(g.vp->GetView()->GetOrigin().x, origin0.x, 1e-9);  // 回到改动前
    EXPECT_TRUE(g.vp->isRedoPossible());
    EXPECT_EQ(eventCount, 1);
    EXPECT_EQ(lastEvent, ViewUndoEvent::Undo);

    g.vp->doRedo();
    EXPECT_NEAR(g.vp->GetView()->GetOrigin().x, origin0.x + 100, 1e-9);
    EXPECT_EQ(lastEvent, ViewUndoEvent::Redo);
}

// 防抖窗口：0.5s 内连续保存只记 1 条（Viewport.ts:3664-3673）。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3664-3673.
TEST(ViewUndo, RapidSavesCoalesceWithinUndoDelay)
{
    VpGuard g;
    auto* view = g.vp->GetView();
    auto const origin0 = view->GetOrigin();   // manufacture 初始（先捕获，不硬编码）
    for (int i = 1; i <= 5; ++i) {
        view->SetOrigin(dqGeom::Point3d::From(origin0.x + double(i), origin0.y, origin0.z));
        g.vp->synchWithView();
    }
    g.vp->doUndo();   // 应一步回到改动前（全部合并为 1 条目）
    EXPECT_NEAR(view->GetOrigin().x, origin0.x, 1e-9);
    EXPECT_FALSE(g.vp->isUndoPossible());  // 只有 1 条，undo 完即空
}

// 20 条上限：满 20 shift 最老（Viewport.ts:3660-3662）。
// Authored: no reference test exists in itwinjs-core for Viewport undo stack
//           max-steps eviction; scenario from Viewport.ts:3660-3662 + :3669
//           （undoDelay 置零关防抖的参考逃生口）。
TEST(ViewUndo, MaxUndoStepsEvictsOldestEntry)
{
    VpGuard g;
    auto* view = g.vp->GetView();
    auto const origin0 = view->GetOrigin();   // 基线（改动前）

    // Viewport.ts:3669 逃生口——s_undoDelay 置零关防抖（RAII 恢复，避免污染其他测试）。
    struct UndoDelayGuard {
        dqBase::DqDuration saved;
        UndoDelayGuard() : saved(dqApp::Viewport::s_undoDelay) {
            dqApp::Viewport::s_undoDelay = dqBase::DqDuration::Zero();
        }
        ~UndoDelayGuard() { dqApp::Viewport::s_undoDelay = saved; }
    } guard;

    // 连续改动 21 次，每次都产 1 条目（防抖已关）。
    for (int i = 1; i <= 21; ++i) {
        view->SetOrigin(dqGeom::Point3d::From(origin0.x + double(i), origin0.y, origin0.z));
        g.vp->synchWithView();
    }
    EXPECT_TRUE(g.vp->isUndoPossible());

    // 栈满 20：最老条目（第 1 次改动前的 origin0）已淘汰。undo×20 后栈恰空，
    // 且只能回到第 2 次改动前（origin0.x + 1），不能再回到 origin0。
    for (int i = 0; i < 20; ++i)
        g.vp->doUndo();
    EXPECT_FALSE(g.vp->isUndoPossible());
    EXPECT_NEAR(view->GetOrigin().x, origin0.x + 1.0, 1e-9);
}

// ChangeView 的 doAnimate 分支（Viewport.ts:3618-3626）：新视图与当前视图
// hasSameCoordinates（双 spatial 共享同一坐标系，ViewState.ts:1225-1227）且未显式
// animateFrustumChange:false → 撤销栈**保留**（"if we can animate, don't throw out
// view undo" :3619 注释的反向）。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3613-3627.
TEST(ViewUndo, ChangeViewSameCoordinatesPreservesUndo)
{
    VpGuard g;
    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(7, 0, 0));
    g.vp->synchWithView();
    ASSERT_TRUE(g.vp->isUndoPossible());

    auto newView = dqApp::ViewList::create(g.conn.Get()).getDefaultView(g.conn.Get());
    g.vp->ChangeView(newView);   // 同连接双 spatial → hasSameCoordinates → doAnimate
    EXPECT_TRUE(g.vp->isUndoPossible())   // :3618-3620 — 可动画 ⇒ 不清栈
        << "doAnimate branch must preserve the undo stack (Viewport.ts:3618-3620)";
    EXPECT_FALSE(g.vp->isRedoPossible());
}

// !doAnimate 分支（Viewport.ts:3619-3620, 3626）：显式 animateFrustumChange:false →
// 撤销栈清空、新基线重建。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3613-3627.
TEST(ViewUndo, ChangeViewExplicitFalseClearsUndoStack)
{
    VpGuard g;
    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(7, 0, 0));
    g.vp->synchWithView();
    ASSERT_TRUE(g.vp->isUndoPossible());

    dqApp::ViewChangeOptions opts;
    opts.animateFrustumChange = false;   // :3618 — `false !== opts.animateFrustumChange` 不成立
    auto newView = dqApp::ViewList::create(g.conn.Get()).getDefaultView(g.conn.Get());
    g.vp->ChangeView(newView, &opts);
    EXPECT_FALSE(g.vp->isUndoPossible());   // 栈清，新基线（equalState → 无条目）
    EXPECT_FALSE(g.vp->isRedoPossible());
}

// synchWithView noSaveInUndo 选项（Viewport.ts:3593-3594）。
// Authored: no reference test exists in itwinjs-core for Viewport undo/redo stack
//           semantics; scenario from Viewport.ts:3593-3594.
TEST(ViewUndo, SynchWithViewNoSaveInUndoSkipsSave)
{
    VpGuard g;
    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(11, 0, 0));
    dqApp::ViewChangeOptions opts;
    opts.noSaveInUndo = true;
    g.vp->synchWithView(opts);
    EXPECT_FALSE(g.vp->isUndoPossible());
}

// View.Undo/View.Redo 工具：注册表创建 → onPostInstall 触发 doUndo/doRedo → exitTool。
// Authored: no reference test exists in itwinjs-core for ViewUndoTool/ViewRedoTool
//           (ViewTool.ts:4111-4134); scenario: 注册表创建 → onPostInstall 触发
//           doUndo/doRedo → exitTool。
namespace {
void undoRedoToolsBody()
{
    VpGuard g;
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.OnInitialized();  // 幂等注册

    g.vp->GetView()->SetOrigin(dqGeom::Point3d::From(99, 0, 0));
    g.vp->synchWithView();
    ASSERT_TRUE(g.vp->isUndoPossible());

    // Bare pointer + adopt: the production runViewTool pattern (DtaToolBars).
    // ToolAdmin owns adopted tools; the self-exit chain (ViewUndoTool.
    // onPostInstall → exitTool → exitViewTool) defers their delete to the
    // frame boundary. unique_ptr here would double-own (destroys at scope
    // exit, then flush() deletes the same object — the SEH this test hit).
    dqApp::InteractiveTool* undoToolItf =
        ta.GetRegistry().CreateVP("View.Undo", g.vp, false, false);
    ASSERT_NE(undoToolItf, nullptr);
    auto* undoTool = static_cast<dqApp::ViewTool*>(undoToolItf);  // registry's View.* factories return ViewTool
    EXPECT_STREQ(undoTool->getToolId(), "View.Undo");
    ta.adoptViewTool(undoTool);
    EXPECT_TRUE(undoTool->run());          // onPostInstall → doUndo → exitTool
    EXPECT_NEAR(g.vp->GetView()->GetOrigin().x, -1000.0, 1e-9);

    dqApp::InteractiveTool* redoToolItf =
        ta.GetRegistry().CreateVP("View.Redo", g.vp, false, false);
    ASSERT_NE(redoToolItf, nullptr);
    auto* redoTool = static_cast<dqApp::ViewTool*>(redoToolItf);
    EXPECT_STREQ(redoTool->getToolId(), "View.Redo");
    ta.adoptViewTool(redoTool);
    EXPECT_TRUE(redoTool->run());
    EXPECT_NEAR(g.vp->GetView()->GetOrigin().x, 99.0, 1e-9);

    // Deferred-delete flush — destroys the self-exited tools above.
    dqApp::Application::Get().GetToolAdmin().flushDeferredViewToolDeletes();
}
}  // namespace

TEST(ViewUndo, UndoRedoToolsRegisteredAndFunctional)
{
    undoRedoToolsBody();
}

// 滚轮缩放接线：WheelEventProcessor::doZoom 正交分支必须走 vp->synchWithView()
// （参考 ToolAdmin.ts:2113-2120 正交分支 vp.zoom(...) → Viewport.ts:2228
//   this.synchWithView(options)）——缩放后撤销栈应新增 1 条目。
// Authored: no reference test exists in itwinjs-core for wheel-zoom undo wiring
//           (reference exercises via app-level flows, no direct unit test);
//           scenario from ToolAdmin.ts:2113-2120 + Viewport.ts:2228 + :3648-3676.
TEST(ViewUndo, WheelZoomSavesUndoEntry)
{
    VpGuard g;
    ASSERT_FALSE(g.vp->isCameraOn());       // BlankConnection 默认正交 → doZoom 正交分支
    EXPECT_FALSE(g.vp->isUndoPossible());   // Create 建基线后栈为空

    dqApp::BeWheelEvent we;
    we.viewport = g.vp;
    we.rawPoint = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    we.point = we.rawPoint;
    we.wheelDelta = 120.0;                  // 正 = 放大（ToolAdmin.ts:2042-2043）
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.processWheelEvent(we, /*doUpdate=*/true);

    EXPECT_TRUE(g.vp->isUndoPossible());    // doZoom 接 synchWithView 后应有 1 条目
}
