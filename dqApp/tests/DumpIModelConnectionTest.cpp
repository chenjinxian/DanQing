// DumpIModelConnectionTest — imodel.json 解析锁 + Views 回放缝 + saved 视图
// 应用锁（M-H Task 3 打开链的 dqApp 侧 RED→GREEN）。
//
// Authored: no reference test exists in itwinjs-core for dump-driven connection
//           replay（§5(f)——参考的打开链覆盖在 DTA 集成测试；其 Views 单测
//           驱动真后端）。钉值全部实取 imodel.json（采集 RPC 载荷原样——
//           third_party/tile-sample-assets/rpc-dumps/{joeshouse-v1,
//           instances60-imodel-v1}/imodel.json，只读 §11.11）。
#include <gtest/gtest.h>

#include <dqApp/ViewPicker.h>                       // ViewList（ViewPicker.ts 移植面）
#include <dqApp/ViewState.h>
#include <dqApp/tile/DumpIModelConnection.h>
#include <dqApp/tile/DumpTileTreeProps.h>
#include <dqGeom/YawPitchRollAngles.h>

#include <cmath>
#include <string>

#ifndef DANQING_TEST_ASSET_ROOT
#define DANQING_TEST_ASSET_ROOT "."
#endif

namespace {

std::string const kDumpRoot = std::string(DANQING_TEST_ASSET_ROOT)
    + "/third_party/tile-sample-assets/rpc-dumps";

constexpr double kTol = 1.0e-12;

void expectPoint(dqGeom::Point3d const& p, double x, double y, double z,
                 double tol = kTol)
{
    EXPECT_NEAR(x, p.x, tol);
    EXPECT_NEAR(y, p.y, tol);
    EXPECT_NEAR(z, p.z, tol);
}

void expectMatrix(dqGeom::Matrix3d const& m, double const (&expected)[9],
                  double tol = 1.0e-12)
{
    for (int i = 0; i < 9; ++i)
        EXPECT_NEAR(expected[i], m.coffs[static_cast<size_t>(i)], tol)
            << "rotation differs at coffs[" << i << "]";
}

}  // namespace

// ---------------------------------------------------------------------------
// joeshouse-v1/imodel.json 解析锁（钉值 = 文件实态逐项）。
// ---------------------------------------------------------------------------
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, ParsesJoesHouseIModelJson)
{
    auto conn = dqApp::DumpIModelConnection::open(kDumpRoot + "/joeshouse-v1/imodel.json");
    ASSERT_TRUE(conn.IsValid()) << "open failed (asset missing or schema drift)";
    EXPECT_TRUE(conn->IsOpen());
    EXPECT_FALSE(conn->IsClosed());

    // connection 段。
    EXPECT_EQ("Joe's house.bim", conn->GetName());
    EXPECT_EQ("2b382042-4a92-46a4-a3cf-db4b77085489", conn->getGuid());
    expectPoint(conn->GetProjectExtents().low,
                -7.487673782576411, -9.008715020706598, -0.6428724054036954);
    expectPoint(conn->GetProjectExtents().high,
                27.762144601950517, 18.75342958124392, 11.693023003317192);

    // views 段：1 视图、defaultViewId=0x4e。
    ASSERT_EQ(1u, conn->getCapturedViews().size());
    EXPECT_EQ(dqBase::DqId::FromString("0x4e"), conn->getCapturedViews()[0].id);
    EXPECT_EQ("3D Imperial Design - View 1", conn->getCapturedViews()[0].name);
    EXPECT_EQ("BisCore:SpatialViewDefinition", conn->getCapturedViews()[0].className);
    EXPECT_FALSE(conn->getCapturedViews()[0].isPrivate);
    EXPECT_EQ(dqBase::DqId::FromString("0x4e"), conn->getCapturedDefaultViewId());

    // models 段：10 条（id 钉死——modelSelectorProps.models 同序同集）。
    char const* const kModelIds[] = {"0x26", "0x3d", "0x3f", "0x41", "0x43",
                                     "0x45", "0x47", "0x49", "0x4b", "0x4d"};
    ASSERT_EQ(10u, conn->getModels().size());
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(dqBase::DqId::FromString(kModelIds[i]), conn->getModels()[i].id)
            << "models[" << i << "]";
    EXPECT_EQ("Joe's house", conn->getModels()[0].name);
    EXPECT_EQ("MEP 1", conn->getModels()[9].name);

    // defaultViewState 段（ViewStateProps 逐项）。
    ASSERT_TRUE(conn->getDefaultViewState().has_value());
    auto const& vs = *conn->getDefaultViewState();
    EXPECT_EQ("BisCore:SpatialViewDefinition", vs.viewDefinitionProps.classFullName);
    EXPECT_EQ(dqBase::DqId::FromString("0x4e"), vs.viewDefinitionProps.id);
    EXPECT_FALSE(vs.viewDefinitionProps.cameraOn);
    expectPoint(vs.viewDefinitionProps.origin,
                -0.1890538726091618, 23.970020102289883, -13.95823848302306);
    EXPECT_NEAR(38.46216332077429, vs.viewDefinitionProps.extents.x, kTol);
    EXPECT_NEAR(20.16090372071284, vs.viewDefinitionProps.extents.y, kTol);
    EXPECT_NEAR(35.41381746319353, vs.viewDefinitionProps.extents.z, kTol);
    EXPECT_TRUE(vs.viewDefinitionProps.hasAngles);
    EXPECT_NEAR(29.999999999999932, vs.viewDefinitionProps.yawDegrees, kTol);
    EXPECT_NEAR(-35.264389682754675, vs.viewDefinitionProps.pitchDegrees, kTol);
    EXPECT_NEAR(-45.00000000000011, vs.viewDefinitionProps.rollDegrees, kTol);

    // modelSelectorProps.models = 10 条（models 段同序）。
    ASSERT_TRUE(vs.modelSelectorProps.has_value());
    ASSERT_EQ(10u, vs.modelSelectorProps->models.size());
    for (size_t i = 0; i < 10; ++i)
        EXPECT_EQ(dqBase::DqId::FromString(kModelIds[i]),
                  vs.modelSelectorProps->models[i]);

    // categorySelectorProps.categories = 8 条。
    char const* const kCategoryIds[] = {"0x21", "0x2e", "0x30", "0x32",
                                        "0x34", "0x36", "0x38", "0x3a"};
    ASSERT_EQ(8u, vs.categorySelectorProps.categories.size());
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(dqBase::DqId::FromString(kCategoryIds[i]),
                  vs.categorySelectorProps.categories[i]);

    // displayStyleProps.viewflags（joeshouse：acs/clipVol/grid/noFill/visEdges
    // true + renderMode 6）。
    ASSERT_TRUE(vs.displayStyleProps.viewflags.has_value());
    auto const& vf = *vs.displayStyleProps.viewflags;
    ASSERT_TRUE(vf.renderMode.has_value());
    EXPECT_EQ(dqCommon::RenderMode::SmoothShade, *vf.renderMode);
    ASSERT_TRUE(vf.acs.has_value());
    EXPECT_TRUE(*vf.acs);
    ASSERT_TRUE(vf.clipVol.has_value());
    EXPECT_TRUE(*vf.clipVol);
    ASSERT_TRUE(vf.grid.has_value());
    EXPECT_TRUE(*vf.grid);
    ASSERT_TRUE(vf.noFill.has_value());
    EXPECT_TRUE(*vf.noFill);
    ASSERT_TRUE(vf.visEdges.has_value());
    EXPECT_TRUE(*vf.visEdges);

    // displayStyleProps.hline（M-I(4)——styles.hline 段搬运。钉值 = 文件实态：
    // visible={color:0(黑),ovrColor:true,pattern:0(Solid),width:1}、
    // hidden={color:0,ovrColor:true,pattern:0xCCCCCCCC(HiddenLine),width:1}、
    // transThreshold:0.3——HiddenLine.ts StyleProps/SettingsProps 键名 1:1）。
    ASSERT_TRUE(vs.displayStyleProps.hline.has_value());
    auto const& hl = *vs.displayStyleProps.hline;
    ASSERT_TRUE(hl.visible.has_value());
    EXPECT_TRUE(hl.visible->ovrColor.has_value() && *hl.visible->ovrColor);
    ASSERT_TRUE(hl.visible->color.has_value());
    EXPECT_EQ(0u, *hl.visible->color);
    ASSERT_TRUE(hl.visible->pattern.has_value());
    EXPECT_EQ(dqCommon::LinePixels::Solid, *hl.visible->pattern);
    ASSERT_TRUE(hl.visible->width.has_value());
    EXPECT_EQ(1, *hl.visible->width);
    ASSERT_TRUE(hl.hidden.has_value());
    ASSERT_TRUE(hl.hidden->pattern.has_value());
    EXPECT_EQ(dqCommon::LinePixels::HiddenLine, *hl.hidden->pattern);
    ASSERT_TRUE(hl.transThreshold.has_value());
    EXPECT_NEAR(0.3, *hl.transThreshold, kTol);
}

// ---------------------------------------------------------------------------
// instances60-imodel-v1/imodel.json 解析锁（钉值 = 文件实态逐项）。
// ---------------------------------------------------------------------------
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, ParsesInstances60IModelJson)
{
    auto conn = dqApp::DumpIModelConnection::open(
        kDumpRoot + "/instances60-imodel-v1/imodel.json");
    ASSERT_TRUE(conn.IsValid()) << "open failed (asset missing or schema drift)";

    EXPECT_EQ("DgnV8Bridge", conn->GetName());
    EXPECT_EQ("dfac2750-4c3e-41de-afe7-5f4d97375006", conn->getGuid());
    expectPoint(conn->GetProjectExtents().low,
                -6.394495346137537, -6.1163245885934225, -2.1542829499779104);
    expectPoint(conn->GetProjectExtents().high,
                11.760817485197345, 2.4455957861138393, 3.3936288381019213);

    // 4 视图、defaultViewId=0x25。
    ASSERT_EQ(4u, conn->getCapturedViews().size());
    EXPECT_EQ(dqBase::DqId::FromString("0x25"), conn->getCapturedViews()[0].id);
    EXPECT_EQ("Default - View 1", conn->getCapturedViews()[0].name);
    EXPECT_EQ(dqBase::DqId::FromString("0x25"), conn->getCapturedDefaultViewId());

    // 1 model 0x1c。
    ASSERT_EQ(1u, conn->getModels().size());
    EXPECT_EQ(dqBase::DqId::FromString("0x1c"), conn->getModels()[0].id);
    EXPECT_EQ("Properties_60InstancesWithUrl2", conn->getModels()[0].name);
    EXPECT_EQ("BisCore:PhysicalModel", conn->getModels()[0].classFullName);

    // defaultViewState（任务书判据①的钉值面）。
    ASSERT_TRUE(conn->getDefaultViewState().has_value());
    auto const& vs = *conn->getDefaultViewState();
    EXPECT_FALSE(vs.viewDefinitionProps.cameraOn);
    expectPoint(vs.viewDefinitionProps.origin,
                16.459991045301607, -6.777938783499056, -8.142933015137151);
    EXPECT_NEAR(19.32567417568155, vs.viewDefinitionProps.extents.x, kTol);
    EXPECT_NEAR(10.797911681847634, vs.viewDefinitionProps.extents.y, kTol);
    EXPECT_NEAR(19.557384678145382, vs.viewDefinitionProps.extents.z, kTol);
    EXPECT_NEAR(-5.836131458448762, vs.viewDefinitionProps.yawDegrees, kTol);
    EXPECT_NEAR(-160.98686924400624, vs.viewDefinitionProps.pitchDegrees, kTol);
    EXPECT_NEAR(-107.4190033527506, vs.viewDefinitionProps.rollDegrees, kTol);

    ASSERT_TRUE(vs.modelSelectorProps.has_value());
    ASSERT_EQ(1u, vs.modelSelectorProps->models.size());
    EXPECT_EQ(dqBase::DqId::FromString("0x1c"), vs.modelSelectorProps->models[0]);
    ASSERT_EQ(1u, vs.categorySelectorProps.categories.size());
    EXPECT_EQ(dqBase::DqId::FromString("0x17"),
              vs.categorySelectorProps.categories[0]);

    // viewflags（instances60：acs/clipVol/noFill/visEdges true + renderMode 6；
    // grid 键缺席——ViewFlags.fromJSON 的 asBool(false) 缺省）。
    ASSERT_TRUE(vs.displayStyleProps.viewflags.has_value());
    auto const& vf = *vs.displayStyleProps.viewflags;
    ASSERT_TRUE(vf.renderMode.has_value());
    EXPECT_EQ(dqCommon::RenderMode::SmoothShade, *vf.renderMode);
    ASSERT_TRUE(vf.noFill.has_value());
    EXPECT_TRUE(*vf.noFill);
    EXPECT_FALSE(vf.grid.has_value());  // 键缺席（采集原样）
}

// ---------------------------------------------------------------------------
// Views 回放缝 + saved 视图应用锁（CreateFromProps 的直接证据）：经
// ViewList（ViewPicker.ts:57-144 populate/getDefaultView 移植面）打开
// instances60 默认视图——初始视图参数 = saved ViewState 逐项（**非 fit**）。
// ---------------------------------------------------------------------------
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, ViewsHooksLoadSavedViewWithAllFieldsApplied)
{
    auto conn = dqApp::DumpIModelConnection::open(
        kDumpRoot + "/instances60-imodel-v1/imodel.json");
    ASSERT_TRUE(conn.IsValid());

    // ViewPicker.ts:57-61 create → populate（getViewList 经回放缝）。
    auto views = dqApp::ViewList::create(conn.Get());
    ASSERT_EQ(4, views.length());
    EXPECT_EQ(dqBase::DqId::FromString("0x25"), views.defaultViewId());

    // :53-55 getDefaultView → getView(:34-51 load + clone)。
    auto view = views.getDefaultView(conn.Get());
    ASSERT_TRUE(view.IsValid());
    auto* view3d = view->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    auto* spatial = view3d->AsSpatialViewState();
    ASSERT_NE(spatial, nullptr);

    // ① 初始视图 = saved ViewState（逐项 double 相等——非 fit 近似）。
    expectPoint(view3d->GetOrigin(),
                16.459991045301607, -6.777938783499056, -8.142933015137151);
    auto const ext = view3d->GetExtents();
    EXPECT_NEAR(19.32567417568155, ext.x, kTol);
    EXPECT_NEAR(10.797911681847634, ext.y, kTol);
    EXPECT_NEAR(19.557384678145382, ext.z, kTol);
    EXPECT_FALSE(view3d->IsCameraOn());
    // rotation = YawPitchRollAngles(saved angles).toMatrix3d()——
    // YawPitchRollAnglesTest.Instances60SavedAnglesProduceCapturedRotation 同钉值。
    double const kExpectedRot[9] = {
        -0.94054349819232985, -0.33967326654909852, 0.0,
        0.096136201592062182, -0.26619780905027829, 0.95911237985977538,
        -0.32578483505464967, 0.90208691291288379, 0.28302551616368093,
    };
    expectMatrix(view3d->getRotation(), kExpectedRot);

    // ② modelSelector 驱动面：1 model 0x1c。
    EXPECT_EQ(1u, spatial->GetModelSelector().getCount());
    EXPECT_TRUE(spatial->GetModelSelector().containsModel(
        dqBase::DqId::FromString("0x1c")));
    // categorySelector：1 category 0x17。
    EXPECT_EQ(1u, spatial->GetCategorySelector().getCount());
    EXPECT_TRUE(spatial->GetCategorySelector().containsCategory(
        dqBase::DqId::FromString("0x17")));

    // viewflags 应用（ViewFlags.fromJSON ViewFlags.ts:471-511——renderMode 6
    // → SmoothShade、noFill → fill=false、acs/clipVol/visEdges → true、
    // grid 缺席 → false）。
    auto const& applied = spatial->getViewFlags();
    EXPECT_EQ(dqCommon::RenderMode::SmoothShade, applied.renderMode());
    EXPECT_TRUE(applied.acsTriad());
    EXPECT_TRUE(applied.clipVolume());
    EXPECT_TRUE(applied.visibleEdges());
    EXPECT_FALSE(applied.fill());   // noFill=true → fill=false
    EXPECT_FALSE(applied.grid());   // 键缺席 → asBool(false)

    // hline 应用（M-I(4)——DisplayStyleSettings.ts:1104 ctor 段的 DanQing
    // 等价跳：CreateFromProps → DisplayStyle3dSettings.setHiddenLineSettings。
    // instances60 dump 的 hline 与 joeshouse 同形：visible.color=0 黑覆盖 +
    // ovrColor → HiddenLineStyle.color 置位；transThreshold 0.3）。
    auto const& hlApplied =
        spatial->GetDisplayStyle().getSettings().getHiddenLineSettings();
    ASSERT_TRUE(hlApplied.visible.color.has_value());
    EXPECT_EQ(0u, hlApplied.visible.color->getTbgr());
    ASSERT_TRUE(hlApplied.visible.width.has_value());
    EXPECT_EQ(1, *hlApplied.visible.width);
    EXPECT_NEAR(0.3, hlApplied.transparencyThreshold, kTol);
    // clone 语义（ViewPicker.ts:49-50——缓存视图保持持久态）：再次 getView
    // 与首次逐项相等。
    auto view2 = views.getDefaultView(conn.Get());
    ASSERT_TRUE(view2.IsValid());
    auto* view2d = view2->AsViewState3d();
    ASSERT_NE(view2d, nullptr);
    expectPoint(view2d->GetOrigin(),
                16.459991045301607, -6.777938783499056, -8.142933015137151);

    // ③ ToProps round-trip（M-M(6) 保存方向——ViewState.toProps :327-332 的
    //    对偶）：ToProps → CreateFromProps 复活视图与原视图逐项相等
    //    （origin/extents/cameraOn/rotation[angles 反解]/camera/三 selector/
    //    viewflags/hline/lights）。
    {
        auto props = spatial->ToProps();
        // angles 反解命中（saved rotation 为刚体矩阵——ViewState.ts:1552
        // createFromMatrix3d 非 undefined 分支）。
        ASSERT_TRUE(props.viewDefinitionProps.hasAngles);
        auto revived = dqApp::SpatialViewState::CreateFromProps(props, nullptr);
        ASSERT_TRUE(revived.IsValid());
        auto* r3d = revived->AsViewState3d();
        ASSERT_NE(r3d, nullptr);
        expectPoint(r3d->GetOrigin(),
                    16.459991045301607, -6.777938783499056, -8.142933015137151);
        EXPECT_NEAR(19.32567417568155, r3d->GetExtents().x, kTol);
        EXPECT_NEAR(10.797911681847634, r3d->GetExtents().y, kTol);
        EXPECT_NEAR(19.557384678145382, r3d->GetExtents().z, kTol);
        EXPECT_FALSE(r3d->IsCameraOn());
        // rotation 经 angles 反解 → toMatrix3d 复原（round-trip 精度：
        // CreateFromMatrix3d 的 sanity check 已保证 IsAlmostEqual；此处以
        // 分量级 1e-9 钉）。
        double const kRevivedRot[9] = {
            -0.94054349819232985, -0.33967326654909852, 0.0,
            0.096136201592062182, -0.26619780905027829, 0.95911237985977538,
            -0.32578483505464967, 0.90208691291288379, 0.28302551616368093,
        };
        expectMatrix(r3d->getRotation(), kRevivedRot);
        // M-O(2) 3h：viewDefinitionProps id/code.value round-trip（参考
        // ViewState extends EntityState——id 与 code.value 由 EntityProps 面
        // 进 ViewState ctor；DanQing ViewState 独立基类，就地承载）。saved
        // 值实钉自 imodel.json defaultViewState（id "0x25" / code.value
        // "Default - View 1"）。
        EXPECT_EQ(dqBase::DqId::FromString("0x25"), revived->GetId());
        EXPECT_EQ("Default - View 1", revived->getCodeValue());
        // is2d 基类面（Viewer.ts:459 标题 dim 段的读取面；DanQing 无 2D
        // 视图子类——基类恒 false，2D 子类落地时覆写）。
        EXPECT_FALSE(revived->is2d());
        // 三 selector round-trip。
        EXPECT_EQ(1u, revived->GetModelSelector().getCount());
        EXPECT_TRUE(revived->GetModelSelector().containsModel(
            dqBase::DqId::FromString("0x1c")));
        EXPECT_EQ(1u, revived->GetCategorySelector().getCount());
        EXPECT_TRUE(revived->GetCategorySelector().containsCategory(
            dqBase::DqId::FromString("0x17")));
        // viewflags round-trip（renderMode/acs/clipVol/visEdges/noFill）。
        auto const& rvf = revived->getViewFlags();
        EXPECT_EQ(dqCommon::RenderMode::SmoothShade, rvf.renderMode());
        EXPECT_TRUE(rvf.acsTriad());
        EXPECT_TRUE(rvf.clipVolume());
        EXPECT_TRUE(rvf.visibleEdges());
        EXPECT_FALSE(rvf.fill());
        // hline round-trip（黑覆盖/宽 1/transThreshold 0.3）。
        auto const& rhl =
            revived->GetDisplayStyle().getSettings().getHiddenLineSettings();
        ASSERT_TRUE(rhl.visible.color.has_value());
        EXPECT_EQ(0u, rhl.visible.color->getTbgr());
        ASSERT_TRUE(rhl.visible.width.has_value());
        EXPECT_EQ(1, *rhl.visible.width);
        EXPECT_NEAR(0.3, rhl.transparencyThreshold, kTol);
    }
}

// ---------------------------------------------------------------------------
// 采集缺口的忠实 fallback：load(非默认视图 id) → nullopt（未采集 = RPC 失败
// 语义）→ ViewList.getView 的 catch 分支（ViewPicker.ts:39-44）→
// manufactureSpatialView（projectExtents top view——manufacture 的显示覆写
// 不在本锁断言面）。
// ---------------------------------------------------------------------------
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, LoadOfNonCapturedViewFallsBackToManufacturedView)
{
    auto conn = dqApp::DumpIModelConnection::open(
        kDumpRoot + "/instances60-imodel-v1/imodel.json");
    ASSERT_TRUE(conn.IsValid());

    // 未采集 id 的 load 直接面：无效 RefPtr（RPC 失败语义）。
    auto direct = conn->GetViews().load(dqBase::DqId::FromString("0x2a"));
    EXPECT_FALSE(direct.IsValid());

    // ViewList.getView 的 fallback 面（:39-44 catch → manufactureSpatialView
    // :149-169——projectExtents 原点 top view；clone 语义不影响形态断言）。
    auto views = dqApp::ViewList::create(conn.Get());
    auto fallback = views.getView(dqBase::DqId::FromString("0x2a"), conn.Get());
    ASSERT_TRUE(fallback.IsValid());
    auto* view3d = fallback->AsViewState3d();
    ASSERT_NE(view3d, nullptr);
    // manufactureSpatialView = createBlank(iModel, ext.low, ext.high-ext.low)
    // （ViewPicker.ts:150-153）——非 saved origin（16.46…）。
    expectPoint(view3d->GetOrigin(),
                -6.394495346137537, -6.1163245885934225, -2.1542829499779104);
}

// ---------------------------------------------------------------------------
// 树 props location 解析锁（M-H Task 3——saved 视图下内容上屏的必要链）：
// TileTreeProps.location（TileProps.ts:46-47）3×4 数组形 → Transform。
// ---------------------------------------------------------------------------
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, ParsesTreePropsLocation)
{
    // instances60-v1 单树：location = 纯平移 [81.66…, 35.41…, 24.75…]——
    // contentRange + location ≈ projectExtents（imodel.json 逐项——采集域
    // 自洽性的活体证据；容差 1e-6——两段（树 props 与 iModel 属性）是独立
    // RPC 采集值，各自舍入域不逐位互补，实测残差 ≤ 4.8e-7）。
    auto props = dqApp::DumpTileTreeProps::load(kDumpRoot + "/instances60-v1");
    ASSERT_TRUE(props.has_value());
    auto tree = props->byTreeId("25_1d-E:6_0x1c");
    ASSERT_TRUE(tree.has_value());
    ASSERT_TRUE(tree->hasLocation);
    dqGeom::Point3d const lowAfter =
        tree->location.MultiplyPoint3d(tree->metadata.contentRange.low);
    dqGeom::Point3d const highAfter =
        tree->location.MultiplyPoint3d(tree->metadata.contentRange.high);
    expectPoint(lowAfter, -6.394495346137537, -6.1163245885934225,
                -2.1542829499779104, 1.0e-6);
    expectPoint(highAfter, 11.760817485197345, 2.4455957861138393,
                3.3936288381019213, 1.0e-6);

    // joeshouse-v1 树 0x41：location = [121.84…, 123.09…, 2.08…] 平移。
    auto joeProps = dqApp::DumpTileTreeProps::load(kDumpRoot + "/joeshouse-v1");
    ASSERT_TRUE(joeProps.has_value());
    auto joeTree = joeProps->byTreeId("25_1d-E:6_0x41");
    ASSERT_TRUE(joeTree.has_value());
    ASSERT_TRUE(joeTree->hasLocation);
    dqGeom::Point3d const origin = joeTree->location.MultiplyPoint3d(
        dqGeom::Point3d::From(0.0, 0.0, 0.0));
    expectPoint(origin, 121.8398798277894, 123.09495545205353,
                2.077572932359965, 1.0e-8);
}

// ---------------------------------------------------------------------------
// M-O(2) I9 — ViewStateProps ↔ JSON 序列化缝 round-trip 锁（SavedViews 保存/
// 恢复消费面：frontend-devtools serializeViewState/deserializeViewState 的
// dump 域等价——NamedVSPSProps._viewStatePropsString 载体）。源 = instances60
// 真实 defaultViewState（透视 saved 视图全字段面）。
// ---------------------------------------------------------------------------
// Authored: 见文件头（frontend-devtools 的 serialize/deserialize 无单测——参考
//           SavedViews.ts 直接 stringify/parse 消费；本锁钉 round-trip 契约）。
TEST(DumpIModelConnectionTest, ViewPropsJsonRoundTripRestoresAllFields)
{
    auto conn = dqApp::DumpIModelConnection::open(
        kDumpRoot + "/instances60-imodel-v1/imodel.json");
    ASSERT_TRUE(conn.IsValid());
    ASSERT_TRUE(conn->getDefaultViewState().has_value());
    auto const& src = *conn->getDefaultViewState();

    auto const roundTripped = dqApp::deserializeViewStatePropsJson(
        dqApp::serializeViewStatePropsJson(src));
    ASSERT_TRUE(roundTripped.has_value());
    auto const& rt = *roundTripped;

    // --- viewDefinitionProps（parse 消费面逐项） ---
    auto const& a = src.viewDefinitionProps;
    auto const& b = rt.viewDefinitionProps;
    EXPECT_EQ(a.classFullName, b.classFullName);
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.codeValue, b.codeValue);
    EXPECT_EQ(a.description, b.description);
    EXPECT_EQ(a.isPrivate, b.isPrivate);
    EXPECT_EQ(a.cameraOn, b.cameraOn);
    expectPoint(b.origin, a.origin.x, a.origin.y, a.origin.z);
    EXPECT_NEAR(a.extents.x, b.extents.x, kTol);
    EXPECT_NEAR(a.extents.y, b.extents.y, kTol);
    EXPECT_NEAR(a.extents.z, b.extents.z, kTol);
    EXPECT_EQ(a.hasAngles, b.hasAngles);
    if (a.hasAngles) {
        EXPECT_NEAR(a.yawDegrees, b.yawDegrees, kTol);
        EXPECT_NEAR(a.pitchDegrees, b.pitchDegrees, kTol);
        EXPECT_NEAR(a.rollDegrees, b.rollDegrees, kTol);
    }
    expectPoint(b.camera.eye, a.camera.eye.x, a.camera.eye.y, a.camera.eye.z);
    EXPECT_NEAR(a.camera.focusDist, b.camera.focusDist, kTol);
    EXPECT_NEAR(a.camera.lensDegrees, b.camera.lensDegrees, kTol);

    // --- categorySelectorProps ---
    ASSERT_EQ(src.categorySelectorProps.categories.size(),
              rt.categorySelectorProps.categories.size());
    for (size_t i = 0; i < src.categorySelectorProps.categories.size(); ++i)
        EXPECT_EQ(src.categorySelectorProps.categories[i],
                  rt.categorySelectorProps.categories[i]) << i;

    // --- modelSelectorProps（optional 本体 + models 逐项） ---
    ASSERT_EQ(src.modelSelectorProps.has_value(),
              rt.modelSelectorProps.has_value());
    if (src.modelSelectorProps.has_value()) {
        ASSERT_EQ(src.modelSelectorProps->models.size(),
                  rt.modelSelectorProps->models.size());
        for (size_t i = 0; i < src.modelSelectorProps->models.size(); ++i)
            EXPECT_EQ(src.modelSelectorProps->models[i],
                      rt.modelSelectorProps->models[i]) << i;
    }

    // --- displayStyleProps 三段（optional has_value 守恒 + 值域） ---
    auto const& sds = src.displayStyleProps;
    auto const& rds = rt.displayStyleProps;
    ASSERT_EQ(sds.viewflags.has_value(), rds.viewflags.has_value());
    if (sds.viewflags.has_value()) {
        auto const& svf = *sds.viewflags;
        auto const& rvf = *rds.viewflags;
        auto expectBool = [&](std::optional<bool> x, std::optional<bool> y) {
            EXPECT_EQ(x.has_value(), y.has_value());
            if (x.has_value())
                EXPECT_EQ(*x, *y);
        };
        expectBool(svf.grid, rvf.grid);
        expectBool(svf.acs, rvf.acs);
        expectBool(svf.visEdges, rvf.visEdges);
        expectBool(svf.hidEdges, rvf.hidEdges);
        expectBool(svf.clipVol, rvf.clipVol);
        expectBool(svf.monochrome, rvf.monochrome);
        expectBool(svf.noFill, rvf.noFill);
        expectBool(svf.noTransp, rvf.noTransp);
        EXPECT_EQ(svf.renderMode.has_value(), rvf.renderMode.has_value());
        if (svf.renderMode.has_value())
            EXPECT_EQ(static_cast<int>(*svf.renderMode),
                      static_cast<int>(*rvf.renderMode));
    }
    ASSERT_EQ(sds.hline.has_value(), rds.hline.has_value());
    if (sds.hline.has_value()) {
        EXPECT_EQ(sds.hline->transThreshold.has_value(),
                  rds.hline->transThreshold.has_value());
        if (sds.hline->transThreshold.has_value())
            EXPECT_NEAR(*sds.hline->transThreshold, *rds.hline->transThreshold,
                        kTol);
        EXPECT_EQ(sds.hline->visible.has_value(), rds.hline->visible.has_value());
        if (sds.hline->visible.has_value()) {
            EXPECT_EQ(*sds.hline->visible->color, *rds.hline->visible->color);
            EXPECT_EQ(*sds.hline->visible->width, *rds.hline->visible->width);
        }
    }
    ASSERT_EQ(sds.lights.has_value(), rds.lights.has_value());
    if (sds.lights.has_value()) {
        auto const& sl = *sds.lights;
        auto const& rl = *rds.lights;
        ASSERT_EQ(sl.solar.has_value(), rl.solar.has_value());
        if (sl.solar.has_value()) {
            // instances60 的 lights 源 = sceneLights 旧格式回退（仅 sunDir——
            // intensity/alwaysEnabled/timePoint 缺席保持 nullopt）。
            ASSERT_EQ(sl.solar->intensity.has_value(),
                      rl.solar->intensity.has_value());
            if (sl.solar->intensity.has_value())
                EXPECT_NEAR(*sl.solar->intensity, *rl.solar->intensity, kTol);
            EXPECT_NEAR(*sl.solar->dirX, *rl.solar->dirX, kTol);
            EXPECT_NEAR(*sl.solar->dirY, *rl.solar->dirY, kTol);
            EXPECT_NEAR(*sl.solar->dirZ, *rl.solar->dirZ, kTol);
        }
        ASSERT_EQ(sl.ambient.has_value(), rl.ambient.has_value());
        ASSERT_EQ(sl.hemisphere.has_value(), rl.hemisphere.has_value());
        ASSERT_EQ(sl.portraitIntensity.has_value(), rl.portraitIntensity.has_value());
        if (sl.portraitIntensity.has_value())
            EXPECT_NEAR(*sl.portraitIntensity, *rl.portraitIntensity, kTol);
    }
}

// 缺省面：全 optional 段缺席的 props round-trip 后保持缺席（fromJSON 缺省
// 语义的守恒——序列化不无中生有）。
// Authored: 见文件头。
TEST(DumpIModelConnectionTest, ViewPropsJsonRoundTripKeepsAbsentOptionalsAbsent)
{
    dqApp::ViewStateProps minimal;
    auto const roundTripped = dqApp::deserializeViewStatePropsJson(
        dqApp::serializeViewStatePropsJson(minimal));
    ASSERT_TRUE(roundTripped.has_value());
    EXPECT_FALSE(roundTripped->displayStyleProps.viewflags.has_value());
    EXPECT_FALSE(roundTripped->displayStyleProps.hline.has_value());
    EXPECT_FALSE(roundTripped->displayStyleProps.lights.has_value());
    EXPECT_FALSE(roundTripped->modelSelectorProps.has_value());
    EXPECT_FALSE(roundTripped->viewDefinitionProps.hasAngles);
    EXPECT_TRUE(roundTripped->categorySelectorProps.categories.empty());
    EXPECT_EQ(dqBase::DqId(), roundTripped->viewDefinitionProps.id);
}
