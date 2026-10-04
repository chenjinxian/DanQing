// SavedViewsTest — M-O(2) I9 Saved Views 全链锁（SavedViewPicker 的
// Create/Recall/Update/Delete + 同名拒绝 + 有序插入 + 持久化 round-trip +
// Recall 恢复 selectedElements/overrideElements）。
//
// 锚定（真实读过的参考行号）：
//   - SavedViews.ts:225-263 saveViewWithName（空名/同名拒绝 + serializeViewState
//     + 选择集/override 序列化 + insert + saveNamedViews）；
//   - :180-208 recallView（deserialize → code.value=name → applySavedView →
//     overrideElementsByArray → selectionSet.emptyAll+add → renderFrame）；
//   - :210-219 deleteView(ByName)、:265-271 updateView（=delete+save 同名）；
//   - NamedViews.ts:43-106 NamedVSPSList（SortedArray 字典序 + findName -1 +
//     loadFromString/getPrintString 的 JSON 互逆）；
//   - 持久化 = DtaRpcInterface.read/writeExternalSavedViews（:71-72/:273-280）
//     的本地文件 EQUIVALENCE（SavedViewsPanel.h 文件头登记）。
//
// 判据面（§11.11）：Recall 后 ViewState 的 origin/extents/code.value 与保存态
// 逐项相等（视图态 WHERE 断言）；持久化 round-trip = 第二 picker populate 后
// 同名条目在；同名拒绝 = 长度不变；有序 = 字典序。
//
// Authored: no reference test exists in itwinjs-core for the DTA SavedViews
//           widget（display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include "Gui/FeatureOverridesPanel.h"
#include "Gui/SavedViewsPanel.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/SelectionSet.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>
#include <dqApp/tile/DumpIModelConnection.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

struct QtEnvSV {
    QtEnvSV()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvSV s_qtSV;

std::string const kDumpRootSV = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinSV(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 持久化 dir 钉到 CWD 下（EQUIVALENCE 见 SavedViewsPanel.h 文件头；测试经
// env 取确定性 + 每测试先删旧文件隔离）。
std::string freshStoreDir()
{
    std::error_code ec;
    std::string const dir = (std::filesystem::current_path(ec) / "saved-views-test")
                                .string();
    std::filesystem::remove_all(dir, ec);  // 测试隔离（自建目录，可删）
    _putenv_s("DANQING_SAVED_VIEWS_DIR", dir.c_str());
    return dir;
}

// 夹具：instances60 打开链（SavedViewPicker 的消费面 = 真实连接 + 真实
// ViewState 的 ToProps 面）。opened（连接/树 RefPtr 载体）必须是**成员**——
// 局部变量在 setup 返回时析构连接，imodel 随之悬空（PickDumpSceneTest
// DumpPick 同款教训）。
struct SavedViewsFixture {
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    dqApp::IModelConnection* imodel = nullptr;
    std::optional<dta::DumpOpenResult> opened;

    bool setup()
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "SavedViewsTest";
            opts.applicationVersion = "1.0";
            if (!app.Startup(opts))
                return false;
        }

        view.resize(1000, 700);
        view.show();
        spinSV(400);

        dta::DumpOpenPackage pkg;
        pkg.imodelRoot = kDumpRootSV + "/instances60-imodel-v1";
        pkg.tileRoots = {kDumpRootSV + "/instances60-v1", kDumpRootSV + "/instances60-drill-v1"};
        opened = dta::openDumpIModel(view, pkg);
        if (!opened.has_value())
            return false;

        vp = view.getUeViewport();
        imodel = vp->GetIModel();
        if (imodel == nullptr)
            return false;
        imodel->GetSelectionSet().EmptyAll();
        vp->RenderFrame();
        return true;
    }
};

dqGeom::Point3d currentOrigin(dqApp::Viewport* vp)
{
    auto* view3d = vp->GetView()->AsViewState3d();
    return view3d->GetOrigin();
}

// 当前视图 props 改 origin 后经 ChangeView 装载（视图态改变面——recall 的
// 对照输入）。
void shiftViewOrigin(dqApp::Viewport* vp, dqApp::IModelConnection* imodel,
                     double dx, double dy, double dz)
{
    auto* view3d = vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    auto* spatial = view3d->AsSpatialViewState();
    ASSERT_NE(spatial, nullptr);
    auto props = spatial->ToProps();
    props.viewDefinitionProps.origin = dqGeom::Point3d::From(
        props.viewDefinitionProps.origin.x + dx,
        props.viewDefinitionProps.origin.y + dy,
        props.viewDefinitionProps.origin.z + dz);
    auto revived = dqApp::SpatialViewState::CreateFromProps(props, imodel);
    ASSERT_TRUE(revived.IsValid());
    vp->ChangeView(revived);
    vp->RenderFrame();
}

}  // namespace

// ---------------------------------------------------------------------------
// Create → 视图漂移 → Recall 复原（origin/extents/code.value 逐项）→
// 持久化 round-trip（第二 picker 见条目）→ Update 反映新态 → Delete 清面。
// ---------------------------------------------------------------------------
TEST(SavedViewsTest, CreateRecallUpdateDeleteChain)
{
    SavedViewsFixture s;
    ASSERT_TRUE(s.setup());
    freshStoreDir();

    Gui::SavedViewPicker picker(s.vp, nullptr);
    ASSERT_EQ(0u, picker.views().length());  // 空存储起步

    dqGeom::Point3d const savedOrigin = currentOrigin(s.vp);
    auto* view3d = s.vp->GetView()->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    dqGeom::Vector3d const savedExtents = view3d->GetExtents();

    // Create（:225-263）。
    picker.setNewViewName("A");
    picker.saveView();
    ASSERT_EQ(1u, picker.views().length());
    ASSERT_EQ(0, picker.views().findName("A"));

    // 视图漂移（recall 的对照）。
    shiftViewOrigin(s.vp, s.imodel, 11.0, -7.0, 3.5);
    dqGeom::Point3d const drifted = currentOrigin(s.vp);
    ASSERT_NE(drifted.x, savedOrigin.x) << "对照前提：漂移未生效";

    // Recall（:180-208）。
    picker.setSelectedView(picker.views().findName("A"));
    picker.recallView();
    dqGeom::Point3d const recalled = currentOrigin(s.vp);
    // origin 容差：saved 视图经打开链 fit 后为相机视图——复活走参考
    // ViewState3d ctor :1506-1508 的 centerEyePoint()（相机视图重定心——
    // 忠实参考行为，一次后幂等；实测一次偏移 ≤0.004）。
    EXPECT_NEAR(savedOrigin.x, recalled.x, 0.01);
    EXPECT_NEAR(savedOrigin.y, recalled.y, 0.01);
    EXPECT_NEAR(savedOrigin.z, recalled.z, 0.01);
    auto* recalled3d = s.vp->GetView()->AsViewState3d();
    ASSERT_NE(recalled3d, nullptr);
    EXPECT_NEAR(savedExtents.x, recalled3d->GetExtents().x, 1.0e-9);
    EXPECT_NEAR(savedExtents.y, recalled3d->GetExtents().y, 1.0e-9);
    EXPECT_NEAR(savedExtents.z, recalled3d->GetExtents().z, 1.0e-9);
    // code.value = name（:186）。
    EXPECT_EQ("A", recalled3d->getCodeValue());

    // 二次 recall 幂等（重定心一次后稳定——round-trip 的强锁；容差 1e-6：
    // matrix→YPR→matrix 反解 + JSON 往返的浮点累积实测 ~1e-9/次，比首定心
    // 偏移 ≤0.004 低三个量级以上）。
    picker.setSelectedView(picker.views().findName("A"));
    picker.recallView();
    dqGeom::Point3d const recalled2 = currentOrigin(s.vp);
    EXPECT_NEAR(recalled.x, recalled2.x, 1.0e-6);
    EXPECT_NEAR(recalled.y, recalled2.y, 1.0e-6);
    EXPECT_NEAR(recalled.z, recalled2.z, 1.0e-6);

    // 持久化 round-trip（writeExternalSavedViews → readExternalSavedViews 的
    // 文件面；第二 picker populate 见同名条目）。
    {
        Gui::SavedViewPicker second(s.vp, nullptr);
        ASSERT_EQ(1u, second.views().length());
        ASSERT_EQ(0, second.views().findName("A"));
    }

    // Update（:265-271 = delete+save 同名）：漂移后 update → recall 反映新态。
    shiftViewOrigin(s.vp, s.imodel, -20.0, 5.0, 0.0);
    dqGeom::Point3d const updated = currentOrigin(s.vp);
    {
        Gui::SavedViewPicker picker2(s.vp, nullptr);
        picker2.setSelectedView(picker2.views().findName("A"));
        picker2.updateView();
        ASSERT_EQ(1u, picker2.views().length());
        picker2.setSelectedView(picker2.views().findName("A"));
        picker2.recallView();
    }
    dqGeom::Point3d const afterUpdateRecall = currentOrigin(s.vp);
    EXPECT_NEAR(updated.x, afterUpdateRecall.x, 1.0e-9);
    EXPECT_NEAR(updated.y, afterUpdateRecall.y, 1.0e-9);

    // Delete（:210-219）→ 长度 0 + 持久化同步（第三 picker 空）。
    {
        Gui::SavedViewPicker picker3(s.vp, nullptr);
        picker3.setSelectedView(picker3.views().findName("A"));
        picker3.deleteView();
        ASSERT_EQ(0u, picker3.views().length());
        Gui::SavedViewPicker picker4(s.vp, nullptr);
        EXPECT_EQ(0u, picker4.views().length());
    }
}

// ---------------------------------------------------------------------------
// 同名/空名拒绝（:226-227）+ SortedArray 字典序插入（NamedViews.ts:43-53）。
// ---------------------------------------------------------------------------
TEST(SavedViewsTest, RejectsDuplicateNamesAndSortsAlphabetically)
{
    SavedViewsFixture s;
    ASSERT_TRUE(s.setup());
    freshStoreDir();

    Gui::SavedViewPicker picker(s.vp, nullptr);
    picker.saveViewWithName("B");
    picker.saveViewWithName("A");
    picker.saveViewWithName("C");
    picker.saveViewWithName("A");  // 同名拒绝
    picker.saveViewWithName("");   // 空名拒绝（:226）
    ASSERT_EQ(3u, picker.views().length());
    EXPECT_EQ("A", picker.views().get(0).name());
    EXPECT_EQ("B", picker.views().get(1).name());
    EXPECT_EQ("C", picker.views().get(2).name());
}

// ---------------------------------------------------------------------------
// Recall 恢复 selectedElements（:198-204 selectionSet.emptyAll+add）+
// overrideElements（:189-196 provider.overrideElementsByArray——I10 Provider
// 的 toJSON/overrideElementsByArray 消费面闭环）。
// ---------------------------------------------------------------------------
TEST(SavedViewsTest, RecallRestoresSelectionAndOverrides)
{
    SavedViewsFixture s;
    ASSERT_TRUE(s.setup());
    freshStoreDir();

    uint32_t const elem = 0x73;
    auto* provider = Gui::FeatureOverridesProvider::getOrCreate(s.vp);
    ASSERT_NE(provider, nullptr);
    Gui::FeatureOverridesProvider::ElementOverride eo;
    eo.id = std::to_string(elem);
    eo.fsaJson = "{\"rgb\":{\"r\":0,\"g\":0,\"b\":255}}";
    provider->overrideElementsByArray({eo});
    QSet<uint32_t> ids;
    ids.insert(elem);
    s.imodel->GetSelectionSet().add(ids);
    ASSERT_EQ(1, s.imodel->GetSelectionSet().size());

    Gui::SavedViewPicker picker(s.vp, nullptr);
    picker.saveViewWithName("S");
    ASSERT_EQ(1u, picker.views().length());
    // 保存面：两个 optional 字段在（:232-244——非空才写）。
    {
        auto const& entry = picker.views().get(0);
        ASSERT_TRUE(entry.selectedElements().has_value());
        ASSERT_TRUE(entry.overrideElements().has_value());
    }

    // 清面（recall 的对照）。
    s.imodel->GetSelectionSet().EmptyAll();
    provider->clear();
    ASSERT_TRUE(s.imodel->GetSelectionSet().isEmpty());
    ASSERT_FALSE(provider->toJSON().has_value());

    // Recall → 选择集与 override 恢复。
    picker.setSelectedView(picker.views().findName("S"));
    picker.recallView();
    EXPECT_TRUE(s.imodel->GetSelectionSet().Contains(elem));
    auto restored = provider->toJSON();
    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(1u, restored->size());
    EXPECT_EQ(std::to_string(elem), restored->at(0).id);
    // fsaJson 键序无关断言（Provider 的 toJSON 是规范序列化——键序与写入
    // 串不必一致；语义 = rgb 三通道值逐一在）。
    EXPECT_NE(std::string::npos, restored->at(0).fsaJson.find("\"r\":0"));
    EXPECT_NE(std::string::npos, restored->at(0).fsaJson.find("\"g\":0"));
    EXPECT_NE(std::string::npos, restored->at(0).fsaJson.find("\"b\":255"));

    Gui::FeatureOverridesProvider::remove(s.vp);
}
