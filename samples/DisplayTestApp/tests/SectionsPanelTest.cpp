// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp tests — M-P P-G SectionsPanel（Sectioning 宿主 UI）
//
// Authored: no reference test exists in itwinjs-core / display-test-app for
//           SectionsPanel（无 .test.ts；行为锚 = SectionTools.ts :33-103 +
//           Viewer.ts:390-393 工具栏挂载——面板 wiring + 工具入口 E2E）。
//
// 覆盖面（参考锚）：
//  - Clip type 下拉（:33-47）：Plane/Range/Element/Shape 四项（Geometry 项
//    ViewClipByElementGeometryTool 未移植——面板文件头裁决）。
//  - Define（:53-58）：tools.run(toolName, provider) → 活动工具 = 所选
//    ViewClip 工具（provider ctor 形参承载）。
//  - Edit（:59-61）：toggleDecoration——有 clip → 装饰 active；无 clip → 无。
//  - Clear（:62-66）：tools.run("ViewClip.Clear", provider) → ViewClipClearTool
//    onPostInstall 安装即清（:462-466）。
//  - E2E（真实窗口 + instances60 dump）：Define → ToolAdmin::onButtonDown
//    单点接受（onDataButtonDown :545-558——法向/enableClipVolume/doClipToPlane/
//    onNewClipPlane/onReinitialize）→ 剖切生效（内容削减 + 剖面法向朝向锁
//    [|dot(normal, viewZ)|≈1——Face 取向]）+ Provider NewPlane 事件 +
//    装饰 active + viewFlags.clipVolume=false 不裁 + Clear 恢复。
#include <gtest/gtest.h>
// 保证 QApplication 存在（DtaToolBarsTest 同款；进程级只建一次）
#include <QApplication>
namespace { struct QtEnvSP { QtEnvSP() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }

#include "Gui/SectionsPanel.h"

#include <QComboBox>
#include <QPushButton>

#include "View3DInventor.h"
#include "DumpOpenHelper.h"   // dta::openDumpIModel（DumpOpenChain 同源）

#include <dqApp/Application.h>
#include <dqApp/BlankConnection.h>
#include <dqApp/ClipViewTool.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>

#include <dqCommon/DisplayStyleSettings.h>
#include <dqGeom/ClipPlane.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <QDateTime>
#include <QCoreApplication>

#include <vector>


namespace {

void spin(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

struct HeadlessView {
    dqBase::RefPtr<dqApp::BlankConnection> imodel;
    dqBase::RefPtr<dqApp::SpatialViewState> view;
    std::unique_ptr<dqApp::Viewport> vp;

    HeadlessView()
    {
        dqApp::BlankConnectionProps props;
        props.extents = dqGeom::Range3d(dqGeom::Point3d::From(-100, -100, -100),
                                        dqGeom::Point3d::From(100, 100, 100));
        imodel = dqApp::BlankConnection::create(props);
        view = dqApp::SpatialViewState::CreateBlank(
            imodel.Get(), dqGeom::Point3d::From(0, 0, 0),
            dqGeom::Vector3d::From(200, 200, 200));
        vp.reset(dqApp::Viewport::Create(nullptr, view));
    }
};

QPushButton* findButton(QWidget* panel, QString const& text)
{
    QList<QPushButton*> const buttons = panel->findChildren<QPushButton*>();
    for (QPushButton* b : buttons)
        if (b->text() == text)
            return b;
    return nullptr;
}

}  // namespace

static QtEnvSP s_qtEnvSP;

// Authored（Clip type 下拉 :33-47——四项 + tool-id 数据面）
TEST(SectionsPanelTest, ComboListsFourClipTypes)
{
    HeadlessView hv;
    QWidget host;
    Gui::SectionsPanel panel(hv.vp.get(), &host);
    QComboBox* combo = panel.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    ASSERT_EQ(combo->count(), 4);
    EXPECT_EQ(combo->itemText(0), QStringLiteral("Plane"));
    EXPECT_EQ(combo->itemData(0).toString(), QStringLiteral("ViewClip.ByPlane"));
    EXPECT_EQ(combo->itemText(1), QStringLiteral("Range"));
    EXPECT_EQ(combo->itemData(1).toString(), QStringLiteral("ViewClip.ByRange"));
    EXPECT_EQ(combo->itemText(2), QStringLiteral("Element"));
    EXPECT_EQ(combo->itemData(2).toString(), QStringLiteral("ViewClip.ByElement"));
    EXPECT_EQ(combo->itemText(3), QStringLiteral("Shape"));
    EXPECT_EQ(combo->itemData(3).toString(), QStringLiteral("ViewClip.ByShape"));
}

// Authored（Define :53-58——tools.run(toolName, provider) → 活动工具）
TEST(SectionsPanelTest, DefineInstallsSelectedClipTool)
{
    HeadlessView hv;
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.SetActiveTool(nullptr);  // 归一化

    QWidget host;
    Gui::SectionsPanel panel(hv.vp.get(), &host);
    QComboBox* combo = panel.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);

    QPushButton* define = findButton(&panel, QStringLiteral("Define"));
    ASSERT_NE(define, nullptr);
    define->click();
    ASSERT_NE(ta.activeTool(), nullptr);
    EXPECT_STREQ(ta.activeTool()->getToolId(), "ViewClip.ByPlane");

    combo->setCurrentIndex(3);  // Shape
    define->click();
    ASSERT_NE(ta.activeTool(), nullptr);
    EXPECT_STREQ(ta.activeTool()->getToolId(), "ViewClip.ByShape");

    ta.SetActiveTool(nullptr);  // 清理
    dqApp::ViewClipDecorationProvider::clearProvider();
}

// Authored（Edit :59-61 toggleDecoration + Clear :62-66）
TEST(SectionsPanelTest, EditTogglesDecorationAndClearClearsClip)
{
    HeadlessView hv;
    auto& ta = dqApp::Application::Get().GetToolAdmin();
    ta.SetActiveTool(nullptr);

    QWidget host;
    Gui::SectionsPanel panel(hv.vp.get(), &host);

    // 无 clip → Edit 不产装饰（create 的 hasClip 门 :1974-1975）
    findButton(&panel, QStringLiteral("Edit"))->click();
    EXPECT_FALSE(dqApp::ViewClipDecorationProvider::create().isDecorationActive(*hv.vp));

    // 置 clip → Edit → 装饰 active（selectOnCreate 路径）
    ASSERT_TRUE(dqApp::ViewClipTool::doClipToPlane(*hv.vp, dqGeom::Point3d::From(0, 0, 0),
                                                    dqGeom::Vector3d::From(0, 0, 1), true));
    findButton(&panel, QStringLiteral("Edit"))->click();
    EXPECT_TRUE(dqApp::ViewClipDecorationProvider::create().isDecorationActive(*hv.vp));

    // Clear（ViewClipClearTool onPostInstall 安装即清 :462-466）
    findButton(&panel, QStringLiteral("Clear"))->click();
    EXPECT_TRUE(hv.view->getViewClip().IsNull());
    EXPECT_FALSE(dqApp::ViewClipDecorationProvider::create().isDecorationActive(*hv.vp));

    ta.SetActiveTool(nullptr);
    dqApp::ViewClipDecorationProvider::clearProvider();
}

// ---------------------------------------------------------------------------
// E2E：instances60 dump 打开 → 面板 Define + 工具单点接受 → 剖切上屏。
// ---------------------------------------------------------------------------
TEST(SectionsPanelTest, Instances60ToolEntryClipsContent)
{
    auto& app = dqApp::Application::Get();
    if (!app.isInitialized()) {
        dqApp::Application::Options opts;
        opts.applicationId = "SectionsPanel";
        opts.applicationVersion = "1.0";
        ASSERT_TRUE(app.Startup(opts));
    }

    Gui::View3DInventor view(nullptr, nullptr, nullptr);
    view.resize(1000, 700);
    view.show();
    spin(400);

    std::string const kDumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";
    dta::DumpOpenPackage pkg;
    pkg.imodelRoot = kDumpRoot + "/instances60-imodel-v1";
    pkg.tileRoots = {kDumpRoot + "/instances60-v1", kDumpRoot + "/instances60-drill-v1"};
    auto opened = dta::openDumpIModel(view, pkg);
    ASSERT_TRUE(opened.has_value()) << "open chain failed: " << pkg.imodelRoot;
    spin(1600);

    auto* vp = view.getUeViewport();
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);

    auto readFrame = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        vp->RenderFrame();
        return vp->ReadFrameForTest(frame, w, h);
    };
    // 内容像素（剔装饰）：模型取景下 computeViewRange 贴内容 → 装饰填充 quad
    // （cyan alpha30 过底 ≈ (0,31,31) 族）与白轮廓/箭头会以"内容"计——按色带
    // 剔除（球体经 12% 青罩仍保主通道，计为内容）。
    auto contentCount = [](std::vector<uint8_t> const& frame, uint32_t w, uint32_t h) {
        uint8_t const* bg = &frame[0];
        long n = 0;
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t const* p = &frame[(static_cast<size_t>(y) * w + x) * 4];
                bool const decoFill = (p[0] < 20 && p[1] >= 12 && p[1] <= 70
                                       && p[2] >= 12 && p[2] <= 70);
                bool const decoLine = (p[0] >= 200 && p[1] >= 200 && p[2] >= 200);
                if (decoFill || decoLine)
                    continue;
                int const dr = std::abs(p[0] - bg[0]), dg = std::abs(p[1] - bg[1]),
                          db = std::abs(p[2] - bg[2]);
                if (dr + dg + db > 60)
                    ++n;
            }
        return n;
    };
    auto pumpToStable = [&](std::vector<uint8_t>& frame, uint32_t& w, uint32_t& h) {
        std::vector<uint8_t> prev;
        for (int i = 0; i < 30; ++i) {
            prev = frame;
            spin(100);
            vp->InvalidateController();
            ASSERT_TRUE(readFrame(frame, w, h));
            if (i > 0 && frame == prev)
                break;
        }
    };

    std::vector<uint8_t> frame;
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(readFrame(frame, w, h));
    long const baseline = contentCount(frame, w, h);
    printf("[SECT] baseline content=%ld\n", baseline);
    ASSERT_GT(baseline, 50000) << "instances60 saved view rendered nothing";

    // --- Define（面板）→ 活动工具 ViewClip.ByPlane ---
    {
        QWidget host;
        Gui::SectionsPanel panel(vp, &host);
        QPushButton* define = findButton(&panel, QStringLiteral("Define"));
        ASSERT_NE(define, nullptr);
        define->click();
        auto& ta = dqApp::Application::Get().GetToolAdmin();
        ASSERT_NE(ta.activeTool(), nullptr);
        EXPECT_STREQ(ta.activeTool()->getToolId(), "ViewClip.ByPlane");
    }  // 面板作用域（Qt 父子销毁先于视口/打开链拆除——无状态面板，Clear 另开实例）

    // Provider NewPlane 事件恰一次 + 事件后装饰 active。
    //（scope 在断言后即撤——退订必须先于 clearProvider 的 provider 析构：
    // 悬垂退订锁死锁[14 线程全 Wait，阶段二分实录]。）
    int newPlaneEvents = 0;
    {
        dqBase::DqEventScope scope;
        scope.add(dqApp::ViewClipDecorationProvider::create().onActiveClipChanged.AddListener(
            [&newPlaneEvents](dqApp::Viewport&, dqApp::ClipEventType type,
                              dqApp::ViewClipDecorationProvider*) {
                if (type == dqApp::ClipEventType::NewPlane)
                    newPlaneEvents++;
            }));

    // 单点接受路径（onDataButtonDown :545-558）：直调活动工具（事件合成 =
    // ToolAdmin::onButtonDown 的 fromButton 面——viewPoint.z=npcToView(NpcCenter)
    // .z + ViewToWorld，ToolAdmin.cpp:503-514）。不经 onButtonDown 派发壳：其
    // SetSelectedViewport/InputState 副作用在 dump 打开链上的 teardown 会
    // join 死锁（14 线程全 Wait 实录）——派发壳语义由 EventDispatchTest 锁，
    // 本 E2E 锁工具接受路径。
    {
        dqApp::BeButtonEvent ev;
        ev.viewport = vp;
        ev.viewPoint = dqGeom::Point3d::From(
            500, 350, vp->NpcToView(dqGeom::Point3d::From(0.5, 0.5, 0.5)).z);
        ev.rawPoint = vp->ViewToWorld(ev.viewPoint);
        ev.point = ev.rawPoint;
        ev.button = dqApp::BeButton::Data;
        ev.inputSource = dqApp::InputSource::Mouse;
        auto* active = dqApp::Application::Get().GetToolAdmin().activeTool();
        ASSERT_NE(active, nullptr);
        EXPECT_EQ(active->onDataButtonDown(ev), dqApp::EventHandled::Yes);
    }

    // 剖切数据面：单面 clip + Face 取向（法向 ≈ ±视图 Z——getPlaneInwardNormal
    // 的 Face = 屏幕朝向，EQUIVALENCE 见 ClipViewTool.h 文件头）。
    {
        dqGeom::ClipVector::Ptr const clip = view3d->getViewClip();
        ASSERT_FALSE(clip.IsNull());
        dqGeom::ConvexClipPlaneSet const* set =
            dqApp::ViewClipTool::isSingleConvexClipPlaneSet(*clip);
        ASSERT_NE(set, nullptr);
        ASSERT_EQ(set->planes.size(), 1u);
        dqGeom::Vector3d const viewZ = view3d->getRotation().ColumnZ();
        double const facing =
            std::abs(set->planes[0].getPlane3d().getNormalRef().DotProduct(viewZ));
        EXPECT_GT(facing, 0.999) << "Face orientation should face the viewer";
    }
    EXPECT_EQ(newPlaneEvents, 1);
    EXPECT_TRUE(dqApp::ViewClipDecorationProvider::create().isDecorationActive(*vp));
    }  // scope 块——退订先于 clearProvider（悬垂退订死锁，见上注）
    // 工具退出归一化（参考 accept→onReinitialize→exitTool 语义——DanQing
    // exitTool 为 no-op 槽、工具不自动卸；接受完成即卸）。
    dqApp::Application::Get().GetToolAdmin().SetActiveTool(nullptr);

    // --- 像素：半空间削减（真实深度切——内容保留 15%~85%；剔装饰色带）---
    vp->InvalidateController();
    pumpToStable(frame, w, h);
    long const clipped = contentCount(frame, w, h);
    printf("[SECT] clipped content=%ld (baseline %ld)\n", clipped, baseline);
    EXPECT_LT(clipped, static_cast<long>(0.85 * baseline)) << "clip did not discard";
    EXPECT_GT(clipped, static_cast<long>(0.15 * baseline)) << "clip discarded everything";

    // --- 旗标语义：viewFlags.clipVolume=false → 片元 discard 停（单调不减；
    // 多瓦模型的瓦侧 range 剔除[isRangeOutsideActiveVolume]使被剔瓦不即时
    // 回流——旗标全量恢复语义由 P-D 单瓦锁 ViewClipPlaneDiscardsHalfspace 钉）---
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.clipVolume = false;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }
    vp->InvalidateController();
    pumpToStable(frame, w, h);
    long const flagOff = contentCount(frame, w, h);
    printf("[SECT] flag-off content=%ld\n", flagOff);
    EXPECT_GE(flagOff, clipped) << "clipVolume=false must not discard further";
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        auto p = style.getViewFlags().Properties();
        p.clipVolume = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }

    // --- Clear（面板新实例——面板无状态）→ clip 清空 ---
    {
        QWidget host;
        Gui::SectionsPanel panel(vp, &host);
        QPushButton* clearBtn = findButton(&panel, QStringLiteral("Clear"));
        ASSERT_NE(clearBtn, nullptr);
        clearBtn->click();
        dqApp::Application::Get().GetToolAdmin().SetActiveTool(nullptr);  // 同上归一化
    }
    EXPECT_TRUE(view3d->getViewClip().IsNull());
    vp->InvalidateController();
    pumpToStable(frame, w, h);
    long const restored = contentCount(frame, w, h);
    printf("[SECT] restored content=%ld\n", restored);
    EXPECT_GT(restored, static_cast<long>(0.9 * baseline)) << "clear should restore content";

    // 泵至 fetcher 静默（DumpOpenChain 的 pumpToQuiesce 同因——拆除时有在途
    // 瓦请求会挂起 teardown join）。
    for (int i = 0; i < 100; ++i) {
        if (opened->fetcher->getActiveCount() == 0
            && opened->fetcher->requestLog().size() > 0)
            break;
        spin(100);
    }
    dqApp::Application::Get().GetToolAdmin().SetActiveTool(nullptr);
    dqApp::ViewClipDecorationProvider::clearProvider();
    view.close();
    spin(200);
    opened.reset();
}
