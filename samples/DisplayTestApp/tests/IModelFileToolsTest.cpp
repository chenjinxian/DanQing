// IModelFileToolsTest — M-Y Y-b2 CloseIModel/ReopenIModel 两 keyin 锁
//（Surface.ts:645-665）。
//
// Authored: no reference test exists in itwinjs-core for CloseIModelTool /
//           ReopenIModelTool（display-test-app 无前端测试套件；场景逐行锚定
//           Surface.ts:645-665 + Surface.firstViewer :216-221/closeAllViewers
//           :412-415，映射断言值 = 五 dump 的 imodel.json rpcProps.key 实测
//           basename——非自造场景）。
//
// RED（Y-b2 落地前）：Gui::CloseIModelTool/ReopenIModelTool 与
// dta::modelIdForIModelKey/dta::dumpPackageForModel 不存在——编译期缺 API
// 红（MacroToolTest 同型先例）；注册锁另经 stash 撤注册运行期实红。
//
// harness 说明：无头锁经 Surface::setMdiAreaForTest 注入裸 QMdiArea；
// dump 重开 E2E（真窗+真 dump）只写不跑——归主会话收口。
#include <gtest/gtest.h>
#include <filesystem>
#include <QApplication>
#include <QCoreApplication>
#include <QMdiArea>
#include <QMdiSubWindow>

#include "Gui/SurfaceTools.h"
#include "Gui/DtaTools.h"        // registerDtaTools（注册面锁）
#include "View3DInventor.h"

#include <DumpOpenHelper.h>      // dta::modelIdForIModelKey/dumpPackageForModel

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/Viewport.h>

namespace { struct QtEnvFT { QtEnvFT() { if (!qApp) { static int argc = 1; static char n[] = "t"; static char* av[] = {n, nullptr}; new QApplication(argc, av); } } }; }
static QtEnvFT s_qtFT;

namespace {

struct MdiAreaFixtureFT {
    QMdiArea area;
    MdiAreaFixtureFT()
    {
        area.resize(900, 600);
        area.viewport()->resize(900, 600);  // viewport 不随 area.resize（Y-b1 仪器订正）
        Gui::Surface::setMdiAreaForTest(&area);
    }
    ~MdiAreaFixtureFT() { Gui::Surface::resetForTest(); }
};

// WA_DeleteOnClose 落删冲刷：View3DInventor 的删除经 deleteLater 在关闭级联
// 中排入（二跑探针实锤——裸 processEvents 不冲中段排队的 DeferredDelete）。
void flushDeletes()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

QMdiSubWindow* makeViewerSubFT(MdiAreaFixtureFT& f)
{
    auto* view = new Gui::View3DInventor(nullptr, nullptr, nullptr);
    auto* sub = new QMdiSubWindow(f.area.viewport());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWidget(view);
    f.area.addSubWindow(sub);
    sub->show();
    return sub;
}

// "Start 页型"无视口子窗（plain widget——closeAllViewers 的存活面对照）。
QMdiSubWindow* makePlainSubFT(MdiAreaFixtureFT& f, char const* name)
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

}  // namespace

// CloseIModelTool（:645-652）：关全部 viewer 子窗、非 viewer 子窗存活
//（closeAllViewers 的 `filter(x => x instanceof Viewer)` 门 :413——Start 页
// 回落语义 = 非 viewer 子窗不关）。
TEST(SurfaceFileTools, CloseIModelClosesAllViewersKeepsOthers)
{
    MdiAreaFixtureFT f;
    makeViewerSubFT(f);
    makeViewerSubFT(f);
    auto* plain = makePlainSubFT(f, "start-like");
    auto* named = makePlainSubFT(f, "panel-a");
    ASSERT_EQ(f.area.subWindowList().size(), 4);

    Gui::CloseIModelTool tool;
    EXPECT_EQ(tool.minArgs(), 0);   // 参考无覆写 = Tool 默认 0/0
    EXPECT_EQ(tool.maxArgs(), 0);
    EXPECT_TRUE(tool.run());
    flushDeletes();  // WA_DeleteOnClose 落删（见 flushDeletes 注）

    auto const subs = f.area.subWindowList();
    ASSERT_EQ(subs.size(), 2);  // 双 viewer 关闭、两非 viewer 存活
    for (auto* sub : subs)
        EXPECT_EQ(qobject_cast<Gui::View3DInventor*>(sub->widget()), nullptr);
    EXPECT_TRUE(subs.contains(plain));
    EXPECT_TRUE(subs.contains(named));
}

// firstViewer 激活序（Surface.ts:216-221——焦点序首个 Viewer；E-Y6 追踪器）。
TEST(SurfaceFileTools, FirstViewerFollowsActivationOrder)
{
    MdiAreaFixtureFT f;
    auto* v1 = makeViewerSubFT(f);
    auto* v2 = makeViewerSubFT(f);
    ASSERT_NE(v1, v2);
    auto* view1 = qobject_cast<Gui::View3DInventor*>(v1->widget());
    auto* view2 = qobject_cast<Gui::View3DInventor*>(v2->widget());
    ASSERT_NE(view1, nullptr);
    ASSERT_NE(view2, nullptr);

    // 初态：激活序追踪器未见激活 → 创建序首 = v1。
    EXPECT_EQ(Gui::Surface::firstViewer(), view1);
    // 激活 v2 → 追踪器前置 → firstViewer=v2（参考 focus() 提前 :331-332）。
    f.area.setActiveSubWindow(v2);
    EXPECT_EQ(Gui::Surface::firstViewer(), view2);
    f.area.setActiveSubWindow(v1);
    EXPECT_EQ(Gui::Surface::firstViewer(), view1);

    // v1 关闭后 firstViewer 落 v2（QPointer 防悬垂——死引用不出表）。
    v1->close();
    flushDeletes();
    EXPECT_EQ(Gui::Surface::firstViewer(), view2);
    v2->close();
    flushDeletes();
    EXPECT_EQ(Gui::Surface::firstViewer(), nullptr);
}

// ReopenIModelTool：无视口 no-op 恒 true（:660-662 的 undefined 门）+ blank
// connection（key 空）→ 不打开（E-Y8——参考 openFile("")→openIModel 失败
// alert 的等价消息面；无 MainWindow 时消息面静默跳过）。
TEST(SurfaceFileTools, ReopenNoViewerAndBlankKeyAreNoOps)
{
    // 无 viewer（裸 area 无视口子窗）→ run 恒 true、零副作用。
    {
        MdiAreaFixtureFT f;
        makePlainSubFT(f, "panel-only");
        Gui::ReopenIModelTool tool;
        EXPECT_EQ(tool.minArgs(), 0);
        EXPECT_EQ(tool.maxArgs(), 0);
        EXPECT_TRUE(tool.run());
        EXPECT_EQ(f.area.subWindowList().size(), 1);  // 无增无减
    }
    // blank connection viewer（ctor 默认 blank——key 空）→ 映射缺席 → 不
    // 重开（连接对象同一性对账：run 前后同一 connection）。
    {
        MdiAreaFixtureFT f;
        auto* sub = makeViewerSubFT(f);
        auto* view = qobject_cast<Gui::View3DInventor*>(sub->widget());
        ASSERT_NE(view, nullptr);
        auto* vp = view->getUeViewport();
        ASSERT_NE(vp, nullptr);
        auto* connBefore = vp->GetIModel();
        ASSERT_NE(connBefore, nullptr);
        Gui::ReopenIModelTool tool;
        EXPECT_TRUE(tool.run());
        EXPECT_EQ(vp->GetIModel(), connBefore);  // 未重开——同连接
        sub->close();
        flushDeletes();
    }
}

// dta::modelIdForIModelKey——五 dump rpcProps.key 实测 basename 映射（E-Y7）。
// 钉值 = 各 dump imodel.json 的 connection.rpcProps.key 真值。
TEST(SurfaceFileTools, ModelIdForIModelKeyMapsDumpKeys)
{
    // 五已知键（imodel.json 实测——正/反斜杠双形态 + 大小写不敏）。
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:\\Github\\tiangong-kaiwu\\itwinjs-core\\test-apps\\display-test-app\\assets\\JoesHouse.bim"),
                  std::optional<QString>(QStringLiteral("joeshouse")));
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:/Github/itwinjs-core/test-apps/display-test-app/assets/House_Model.bim"),
                  std::optional<QString>(QStringLiteral("housemodel")));
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:\\Github\\itwinjs-core\\test-apps\\display-test-app\\assets\\Baytown.bim"),
                  std::optional<QString>(QStringLiteral("baytown")));
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:\\Github\\tiangong-kaiwu\\itwinjs-core\\test-apps\\display-test-app\\assets\\Properties_60InstancesWithUrl2.ibim"),
                  std::optional<QString>(QStringLiteral("instances60")));
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:\\Github\\itwinjs-core\\test-apps\\display-test-app\\assets\\编辑大桥测试.bim"),
                  std::optional<QString>(QStringLiteral("bridge-edit")));
    // P-2 P2-d：newBridge.bim 的 rpcProps.key = D:\test\newBridge.bim（大小写
    // 不敏——newbridge-v1/imodel.json 实测）。
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:\\test\\newBridge.bim"),
                  std::optional<QString>(QStringLiteral("newbridge")));
    EXPECT_EQ(dta::modelIdForIModelKey(
                  "D:/test/newbridge.bim"),
                  std::optional<QString>(QStringLiteral("newbridge")));

    // 未映射/空键 → nullopt（E-Y8 no-op 腿）。
    EXPECT_EQ(dta::modelIdForIModelKey(""), std::nullopt);
    EXPECT_EQ(dta::modelIdForIModelKey("C:/somewhere/Unknown.bim"), std::nullopt);
    EXPECT_EQ(dta::modelIdForIModelKey("no-separator"), std::nullopt);
}

// dta::dumpPackageForModel——main.cpp 迁出的集中映射面（五模型包定义钉值：
// imodelRoot/主瓦根/fallback 序/bridge 取景旗标）。
TEST(SurfaceFileTools, DumpPackageForModelTableIntact)
{
    auto const joe = dta::dumpPackageForModel(QStringLiteral("joeshouse"));
    ASSERT_TRUE(joe.has_value());
    EXPECT_TRUE(joe->imodelRoot.find("joeshouse-v1") != std::string::npos);
    ASSERT_EQ(joe->tileRoots.size(), 4u);  // 主+drill-v1+drill-v2+放大补采
    EXPECT_FALSE(joe->frameToWorldContent);

    auto const inst = dta::dumpPackageForModel(QStringLiteral("instances60"));
    ASSERT_TRUE(inst.has_value());
    EXPECT_TRUE(inst->imodelRoot.find("instances60-imodel-v1") != std::string::npos);
    ASSERT_EQ(inst->tileRoots.size(), 3u);  // 主+drill+d1 补采

    auto const house = dta::dumpPackageForModel(QStringLiteral("housemodel"));
    ASSERT_TRUE(house.has_value());
    ASSERT_EQ(house->tileRoots.size(), 2u);

    auto const bay = dta::dumpPackageForModel(QStringLiteral("baytown"));
    ASSERT_TRUE(bay.has_value());
    ASSERT_EQ(bay->tileRoots.size(), 2u);

    auto const bridge = dta::dumpPackageForModel(QStringLiteral("bridge-edit"));
    ASSERT_TRUE(bridge.has_value());
    // 主+drill+sweep+放大链补采×4+邻域覆盖（=8；d13 前沿本地持有——
    // manifest 存在时条件接入为第 9 根）。
    {
        std::error_code ec;
        bool const hasFrontier = std::filesystem::exists(
            std::string(DANQING_TILE_ASSETS_DIR)
                + "/rpc-dumps/bridge-edit-frontier-d13-v1/manifest.json", ec);
        ASSERT_EQ(bridge->tileRoots.size(), hasFrontier ? 9u : 8u);
    }
    EXPECT_TRUE(bridge->frameToWorldContent);  // 坑 24 取景路径

    // P-2 P2-d：newbridge 六键域映射——主根 newbridge-v1（imodel.json+树 props
    // 0 瓦，整根入 git）+ 7 个本地持有 fallback 根（sweep d1-d8 / d9 / d9b /
    // d10-d13——47.9GB files/ 经 .gitignore 本地持有，manifest 存在才接入，
    // bridge-edit frontier 先例形态）。帧到世界域几何（saved 视图 0x5387 为
    // 坑 24 型空域——见 DumpOpenHelper 注释）。
    auto const nb = dta::dumpPackageForModel(QStringLiteral("newbridge"));
    ASSERT_TRUE(nb.has_value());
    EXPECT_TRUE(nb->imodelRoot.find("newbridge-v1") != std::string::npos);
    {
        // 候选源序（P2-d 指定）：v1 → d9 → sweep → d10 → d11 → d12 → d13 →
        // d9b；每根 manifest.json 存在才接入（本机全在盘 → 8）。
        char const* const kExpected[] = {
            "/newbridge-v1", "/newbridge-d9-v1", "/newbridge-sweep-v1",
            "/newbridge-d10-v1", "/newbridge-d11-v1", "/newbridge-d12-v1",
            "/newbridge-d13-v1", "/newbridge-d9b-v1"};
        int expected = 0;
        for (char const* root : kExpected) {
            std::error_code ec;
            if (std::filesystem::exists(std::string(DANQING_TILE_ASSETS_DIR)
                                            + "/rpc-dumps" + root + "/manifest.json",
                                        ec))
                ++expected;
        }
        ASSERT_EQ(nb->tileRoots.size(), static_cast<size_t>(expected));
        for (size_t i = 0, k = 0; i < nb->tileRoots.size(); ++i, ++k) {
            // 跳过 manifest 缺席的期望根（缺席静默跳过——接入序保持指定序）。
            while (!std::filesystem::exists(
                std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps"
                    + kExpected[k] + "/manifest.json"))
                ++k;
            EXPECT_NE(nb->tileRoots[i].find(kExpected[k]), std::string::npos)
                << "root[" << i << "] order drifted";
        }
    }
    EXPECT_TRUE(nb->frameToWorldContent);  // 坑 24 型空域默认视图 → 取景路径

    EXPECT_EQ(dta::dumpPackageForModel(QStringLiteral("bogus")), std::nullopt);
}

// 注册面（SVTTools.json:106-111 en 键 + App.ts:401-441 工具表）。
TEST(SurfaceFileTools, RegisteredWithKeyins)
{
    auto& admin = dqApp::Application::Get().GetToolAdmin();
    admin.OnInitialized();
    Gui::registerDtaTools();

    auto* closeTool = admin.GetRegistry().Create("CloseIModel");
    ASSERT_NE(closeTool, nullptr);
    EXPECT_STREQ("CloseIModel", closeTool->getToolId());
    EXPECT_EQ(closeTool->englishKeyin(), "dta file close");
    EXPECT_EQ(closeTool->minArgs(), 0);
    EXPECT_EQ(closeTool->maxArgs(), 0);
    delete closeTool;

    auto* reopenTool = admin.GetRegistry().Create("ReopenIModel");
    ASSERT_NE(reopenTool, nullptr);
    EXPECT_STREQ("ReopenIModel", reopenTool->getToolId());
    EXPECT_EQ(reopenTool->englishKeyin(), "dta file reopen");
    delete reopenTool;
}

// ---------------------------------------------------------------------------
// dump 重开 E2E（真 dump——按双代理契约只写不跑，归主会话收口；无像素断言，
// 纯打开链对账故仍非真窗件）。
// Authored: no reference test exists（行为锚定 Surface.ts:657-664 的
// `firstViewer.openFile(viewport.iModel.key)` + Viewer.ts:545-552
// resetIModel 的同 viewer 换绑语义）。
// 复现配方：View3DInventor（不 show）→ dta::openDumpIModel(instances60)
// → 工具 run() → 新连接同 key + 注册表换件 + 树数对账。
// ---------------------------------------------------------------------------
TEST(SurfaceFileToolsE2E, ReopenReloadsSameViewWithSameKey)
{
    // 视图堆持有（setWidget 移交子窗——子窗 WA_DeleteOnClose 连删；栈对象会
    // 被 deleteLater 双释放）。
    auto* view = new Gui::View3DInventor(nullptr, nullptr, nullptr);
    auto* vp = view->getUeViewport();
    ASSERT_NE(vp, nullptr);

    auto const pkg = dta::dumpPackageForModel(QStringLiteral("instances60"));
    ASSERT_TRUE(pkg.has_value());
    auto opened = dta::openDumpIModel(*view, *pkg);
    ASSERT_TRUE(opened.has_value());
    dta::registerOpenedDump(
        view, std::make_unique<dta::DumpOpenResult>(std::move(*opened)));
    auto* before = dta::findOpenedDump(view);
    ASSERT_NE(before, nullptr);
    std::string const key = vp->GetIModel()->GetKey();
    ASSERT_FALSE(key.empty());
    size_t const treeCount = before->trees.size();
    ASSERT_GT(treeCount, 0u);

    // firstViewer 解析：参考读 Surface 窗表（_windows）——DanQing 承载 =
    // MDI 子窗（SurfaceTools.h 头注 E-Y1）；故视图须以子窗入册（生产路径 =
    // MainWindow::addWindow）。
    QMdiArea area;
    area.resize(900, 600);
    area.viewport()->resize(900, 600);
    auto* sub = new QMdiSubWindow(area.viewport());
    sub->setAttribute(Qt::WA_DeleteOnClose);
    sub->setWidget(view);           // 所有权移交子窗（收尾连 view 一起删）
    area.addSubWindow(sub);
    sub->show();
    Gui::Surface::setMdiAreaForTest(&area);

    Gui::ReopenIModelTool tool;
    EXPECT_TRUE(tool.run());

    // 复活对账：注册表换件（新对象）+ 连接更换 + key 守恒 + 树数守恒。
    auto* after = dta::findOpenedDump(view);
    ASSERT_NE(after, nullptr);
    EXPECT_NE(after, before);
    ASSERT_NE(vp->GetIModel(), nullptr);
    EXPECT_EQ(vp->GetIModel()->GetKey(), key);
    EXPECT_EQ(after->trees.size(), treeCount);

    Gui::Surface::resetForTest();
    // 注册表先卸（防 destroyed 链与手工卸叠序——forget 幂等）。
    dta::forgetOpenedDump(view);
    sub->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}
