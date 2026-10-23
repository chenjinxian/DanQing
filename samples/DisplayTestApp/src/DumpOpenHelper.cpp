// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — 打开链装配实现（M-H Task 3；流程与参考锚见
// DumpOpenHelper.h 文件头）。
#include "DumpOpenHelper.h"

#include "Gui/View3DInventor.h"

#include <dqApp/tile/SpatialTileTreeReferences.h>  // setCreateOverride（frontend-tiles 缝）

#include <QPointer>

#include <map>
#include <cstdlib>  // std::getenv（DANQING_RPC_DUMP——dumpPackageForModel）
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <dqRender/WorkerProxy.h>  // P-1b P-b1：worker 载体（参考 WorkerProxy.ts 公开面）
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

namespace dta {

namespace {
// 打开链失败原因轨迹（DANQING_OPEN_TRACE=1 门控——main.cpp 的 [OPEN] 计时
// 桩同门；M-K(2) baytown 打开失败取证的插桩面）。
bool openTrace()
{
    return std::getenv("DANQING_OPEN_TRACE") != nullptr;
}

// ---------------------------------------------------------------------------
// P-1b P-b4 两段拆分：stage 1 = 重解析段（纯 CPU 零 GL 零 viewport——worker
// 可下放；M-I(1) 实测的打开链热点 = manifest 解析 2.87s），stage 2 = 视图/树
// 装配段（主线程 only——viewport/RenderSystem）。
// Ported from: itwinjs-core 打开链的 async 面（openFile await RPC 数据面 →
// views 装载）。
// ---------------------------------------------------------------------------
struct DumpOpenStage1 {
    dqBase::RefPtr<dqApp::DumpIModelConnection> connection;
    std::optional<dqApp::DumpTileTreeProps> props;
    std::unique_ptr<dqApp::DumpTileFetcher> fetcher;
    std::string failReason;  // empty = ok
};

// Stage 1（worker 可执行——纯 CPU）：①imodel.json 连接面解析 + manifest/树
// props 解析 + 多根 fetcher 构造（manifest 单次解析共享，M-I(1)）。
DumpOpenStage1 openDumpStage1(DumpOpenPackage const& pkg)
{
    DumpOpenStage1 out;
    if (pkg.imodelRoot.empty() || pkg.tileRoots.empty())
    {
        out.failReason = "empty package";
        return out;
    }

    // ① 打开连接（imodel.json——iModelRpc 数据面回放）。
    out.connection = dqApp::DumpIModelConnection::open(pkg.imodelRoot + "/imodel.json");
    if (!out.connection.IsValid())
    {
        out.failReason = "imodel.json unreadable (" + pkg.imodelRoot + "/imodel.json)";
        out.connection = nullptr;
        return out;
    }

    // 树 props（主根——PrimaryTreeSupplier 对 requestTileTreeProps 返回树的
    // 逐树 createTileTree（PrimaryTileTree.ts:63-80）的离线对应物，同
    // DumpMount.h 注释）。
    out.props = dqApp::DumpTileTreeProps::load(pkg.tileRoots[0]);
    if (!out.props.has_value())
    {
        out.failReason = "manifest/tree props unreadable (" + pkg.tileRoots[0] + ")";
        out.connection = nullptr;
        return out;
    }

    // fetcher 构造（注入留在 stage 2 的 TileAdmin 主线程缝）。多根 = drill
    // 域外键补字节，M-H Task 2；单次解析共享（M-I(1)——用户报告的打开长等待
    // 主根因）：props 装载的 manifest 移交 fetcher（takeManifest——树条目
    // 复制留存 byTreeId，瓦键域移交）。
    std::vector<std::string> fallbacks(pkg.tileRoots.begin() + 1, pkg.tileRoots.end());
    out.fetcher = std::make_unique<dqApp::DumpTileFetcher>(out.props->takeManifest(),
                                                           pkg.tileRoots[0],
                                                           std::move(fallbacks));
    if (!out.fetcher->isValid())
    {
        out.failReason = "fetcher invalid (" + pkg.tileRoots[0] + ")";
        out.fetcher.reset();
        out.props.reset();
        out.connection = nullptr;
    }
    return out;
}

// Stage 1 的失败轨迹（主线程——worker 段不做 IO 轨迹，失败原因随载荷带回）。
void traceStage1Failure(DumpOpenPackage const& pkg, std::string const& failReason)
{
    if (!openTrace())
        return;
    if (failReason.rfind("imodel.json", 0) == 0)
        fprintf(stderr, "[OPEN] fail: %s\n", failReason.c_str());
    else if (failReason.rfind("manifest", 0) == 0)
        fprintf(stderr, "[OPEN] fail: %s\n", failReason.c_str());
    else if (failReason.rfind("fetcher", 0) == 0)
        fprintf(stderr, "[OPEN] fail: %s\n", failReason.c_str());
    else
        fprintf(stderr, "[OPEN] fail: %s\n", failReason.c_str());
    (void)pkg;
}

// Stage 2（主线程）：fetcher 注入（TileAdmin DI 缝 §8.4）+ ②视图装载 +
// ③changeView + ④逐 model 树装配 + ⑤provider 注册 + ⑥坑 24 取景。
// 体 = 原 openDumpIModel 的后半（逐段未动）。
std::optional<DumpOpenResult> finishDumpOpen(Gui::View3DInventor& view,
                                             DumpOpenPackage const& pkg,
                                             DumpOpenStage1& s1)
{
    dqRender::TileAdmin::instance().setFetcher(std::move(s1.fetcher));

    // ② 默认视图装载（Viewer.ts:184-185：ViewList.create → getDefaultView；
    //    ViewPicker.ts:57-61 create → populate（getViewList 经回放缝）→
    //    :53-55 getDefaultView → :34-51 getView（views.load + clone））。
    //
    // ④b（前置）：本连接的树由 provider 通道供给（⑤——参考的
    //    addTiledGraphicsProvider 应用通道，Viewport.ts:1729-1732）。引擎内
    //    默认 SpatialTileTreeReferences 工厂的 per-model **占位 refs**
    //    （ModelSelectorSpatialTileTreeReferences——unit-scaled 假根，
    //    "stands in for requestTileTreeProps"（SpatialTileTreeReferences.
    //    cpp:66-68 注释）——G10 登记的 seam 占位）在本流程会产生参考中不
    //    存在的请求（"<modelId>/0/0/0/0" NotFound——首跑实测：占位树与
    //    provider 真树并存双驱动），污染"请求序列同构"判据。经
    //    frontend-tiles 替换缝（SpatialTileTreeReferences.create 的
    //    override——参考 FrontendTiles.ts:215 正是替换该工厂）在视图
    //    装载/克隆窗口内抑制为 EMPTY refs；窗口结束即 clear（占位默认对
    //    其他路径零影响）。参考侧打开视图的 _treeRefs 非空（真 refs——
    //    computeFitRange 等消费面），EMPTY 化使引擎内 refs 面为空——
    //    本任务不依赖该面（画树走 provider），SpatialRefs 全移植归 G10
    //    后续里程碑（登记）。
    dqApp::SpatialTileTreeReferences::setCreateOverride(
        [](dqApp::SpatialViewState&) {
            return std::make_unique<DumpOpenEmptyTileTreeReferences>();
        });
    dqApp::ViewList views = dqApp::ViewList::create(s1.connection.Get());
    auto viewState = views.getDefaultView(s1.connection.Get());
    dqApp::SpatialTileTreeReferences::clearCreateOverride();
    if (!viewState.IsValid())
    {
        if (openTrace())
            fprintf(stderr, "[OPEN] fail: default view not loadable\n");
        return std::nullopt;
    }
    auto* view3d = viewState->AsViewState3d();
    if (view3d == nullptr)
    {
        if (openTrace())
            fprintf(stderr, "[OPEN] fail: default view is not 3d\n");
        return std::nullopt;
    }
    auto* spatial = view3d->AsSpatialViewState();
    if (spatial == nullptr)
    {
        if (openTrace())
            fprintf(stderr, "[OPEN] fail: default view is not spatial\n");
        return std::nullopt;
    }

    // ③ viewport changeView（Viewer.ts:231 ScreenViewport.create(contentDiv,
    //    view) → Viewport.ts:3196-3204 vp.changeView(view)——参考 create 内
    //    直接 changeView，无 fit 补偿（ViewPicker.ts:29 openView 语义））。
    dqApp::Viewport* viewport = view.getUeViewport();
    viewport->ChangeView(viewState);

    // ④ 按 modelSelector 逐 model 建 TileTreeRef（SpatialViewState.ts:109
    //    _treeRefs = SpatialTileTreeReferences.create(this) → SpatialRefs 逐
    //    model PrimaryTreeReference（PrimaryTileTree.ts:608-861）；DanQing
    //    ModelSelectorState 容器是 bset（数值序）——参考 TS Set 插入序；两
    //    模型的采集序恰为数值升序（joeshouse 0x26..0x4d 单调、instances60
    //    单 model），容器序差对本资产不可观测——登记）。
    DumpOpenResult out;
    out.provider = std::make_unique<DumpOpenTreeProvider>();  // 堆持有——viewport 注册的地址稳定（见 .h 注释）
    // 坑 24 取景域（frameToWorldContent——DumpOpenPackage 注释）：逐树
    // contentRange × location 的世界域并集。contentRange 是模型几何域
    //（TileTreeProps.contentRange）；rootTile.range 是瓦树根域（bridge-edit
    // 根瓦 range = ±9871 对称大盒，远大于几何——两者只有 location 平移链
    // 共享）。采集会话的 zoomToVolume 喂的是世界域 contentRange（README
    // "默认视图空域"节——"取景后视域"由此派生）。
    dqGeom::Range3d contentWorldRange = dqGeom::Range3d::CreateNull();
    for (auto modelId : spatial->GetModelSelector().getModels()) {
        // treeId 派生 = PrimaryTreeReference.createTreeId
        //（PrimaryTileTree.ts:268-290）：edgesRequired = vf.visibleEdges ||
        // RenderMode.SmoothShade !== renderMode || alwaysRequestEdges（:283-
        // 285——_viewFlagOverrides 缺省（model.jsonProperties.viewFlagOverrides
        // 无载体——登记），alwaysRequestEdges=false 缺省）→ edges =
        // tileAdmin.edgeOptions = defaultTileOptions.edgeOptions
        //（:286——{compact,smooth}，TileMetadata.ts:327-330）；无
        // animationId/sectionCut（displayStyle scheduleScript 未移植——登记）。
        auto const& vf = spatial->getViewFlags();
        bool const edgesRequired = vf.visibleEdges()
            || dqCommon::RenderMode::SmoothShade != vf.renderMode();
        dqRender::PrimaryTileTreeId treeIdObj;
        if (edgesRequired)
            treeIdObj.edges = dqRender::TileAdmin::instance().edgeOptions();
        // PrimaryTreeSupplier.createTileTree（PrimaryTileTree.ts:65）的
        // iModelTileTreeIdToString 调用（options = tileAdmin ——M-O(4) P6 起
        // edgeOptions 读 TileAdmin 权威源，缺省值不变[捕获键域稳定]）。
        std::string const treeId = dqRender::iModelTileTreeIdToString(
            modelId.ToString(), treeIdObj, dqRender::TileOptions{});

        // requestTileTreeProps 回放（:66——DumpTileTreeProps.byTreeId）；
        // modelSelector 引用的树不在 dump 域 = 采集缺口 → 打开失败。
        auto treeProps = s1.props->byTreeId(treeId);
        if (!treeProps.has_value())
        {
            if (openTrace())
                fprintf(stderr, "[OPEN] fail: tree props not in dump domain "
                                "(model=%s derived treeId=%s)\n",
                        modelId.ToString().c_str(), treeId.c_str());
            return std::nullopt;
        }

        // createTileTree 装配（:79-81——iModelTileTreeParamsFromJSON →
        // IModelTileTree）；location → TileTree.iModelTransform
        //（TileTree.ts:122——iModelTileTreeParamsFromJSON :50/:71）。
        auto tree = std::make_unique<dqRender::ImdlTileTree>(
            treeProps->id, treeProps->rootTile.contentId, treeProps->rootTile.range,
            treeProps->metadata);
        tree->setRenderSystem(viewport->renderSystem());
        if (treeProps->hasLocation)
            tree->setIModelTransform(treeProps->location);

        // M-L(3) Models 面板数据面：逐树条目（modelId + 派生 treeId + 世界域）。
        // 世界域 = rootTile.range × location（参考 fit 的场景域来源——
        // computeViewRange 的 TileTree range 并集经 iModelTransform；rootTile
        // range 是瓦局部域，8 角点变换后重张成盒）。
        dqGeom::Range3d worldRange = treeProps->rootTile.range;
        if (treeProps->hasLocation) {
            auto const& xf = treeProps->location;
            auto const diag = treeProps->rootTile.range.Diagonal();
            worldRange = dqGeom::Range3d::CreateNull();
            for (int c = 0; c < 8; ++c) {
                dqGeom::Point3d const corner(
                    treeProps->rootTile.range.low.x + ((c & 1) ? diag.x : 0.0),
                    treeProps->rootTile.range.low.y + ((c & 2) ? diag.y : 0.0),
                    treeProps->rootTile.range.low.z + ((c & 4) ? diag.z : 0.0));
                worldRange.ExtendPoint(xf.MultiplyPoint3d(corner));
            }
        }
        out.provider->addTree(tree.get(), modelId.ToString(), treeId, worldRange);
        out.treeLoadLog.push_back(treeId);
        out.trees.push_back(std::move(tree));

        if (pkg.frameToWorldContent && !treeProps->metadata.contentRange.isNull()) {
            auto const& cr = treeProps->metadata.contentRange;
            auto const& xf = treeProps->location;
            auto const diag = cr.Diagonal();
            for (int c = 0; c < 8; ++c) {
                dqGeom::Point3d const corner(
                    cr.low.x + ((c & 1) ? diag.x : 0.0),
                    cr.low.y + ((c & 2) ? diag.y : 0.0),
                    cr.low.z + ((c & 4) ? diag.z : 0.0));
                contentWorldRange.ExtendPoint(xf.MultiplyPoint3d(corner));
            }
        }
    }
    if (out.trees.empty())
    {
        if (openTrace())
            fprintf(stderr, "[OPEN] fail: no trees assembled (modelSelector "
                            "models=%zu)\n",
                    spatial->GetModelSelector().getModels().size());
        return std::nullopt;
    }

    // ⑤ provider 注册（Viewport.ts:1729-1732 addTiledGraphicsProvider）。
    viewport->AddTiledGraphicsProvider(out.provider.get());

    // ⑥ 坑 24 取景路径（frameToWorldContent——DumpOpenPackage 注释）：参考
    //    zoomToVolume（Viewport.ts:2316-2319）=`view.lookAtVolume(volume,
    //    this.viewRect.aspect, options)` + `synchWithView(options)`——volume
    //    = 上面逐树 contentRange × location 的世界域并集；aspect = 视口
    //    viewRect.aspect（参考原样）。invalid 取景域（空域树）→ 跳过（无
    //    几何可取景——saved 视图原样）。
    if (pkg.frameToWorldContent && !contentWorldRange.isNull()) {
        double const aspect = viewport->viewRect().aspect();
        view3d->LookAtVolume(contentWorldRange, &aspect);
        if (openTrace()) {
            fprintf(stderr,
                    "[OPEN] framing: aspect=%.9f ext=(%.4f,%.4f,%.4f) org=(%.4f,%.4f,%.4f)\n",
                    aspect, view3d->GetExtents().x, view3d->GetExtents().y,
                    view3d->GetExtents().z, view3d->GetOrigin().x,
                    view3d->GetOrigin().y, view3d->GetOrigin().z);
        }
        viewport->InvalidateController();
        viewport->synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
        if (openTrace()) {
            fprintf(stderr,
                    "[OPEN] framing: after synch ext=(%.4f,%.4f,%.4f) org=(%.4f,%.4f,%.4f)\n",
                    view3d->GetExtents().x, view3d->GetExtents().y,
                    view3d->GetExtents().z, view3d->GetOrigin().x,
                    view3d->GetOrigin().y, view3d->GetOrigin().z);
        }
    }

    out.connection = s1.connection;
    out.viewState = viewState;
    out.props = std::move(*s1.props);
    out.fetcher = static_cast<dqApp::DumpTileFetcher*>(
        &dqRender::TileAdmin::instance().getFetcher());
    return out;
}

}  // namespace

std::optional<DumpOpenResult> openDumpIModel(Gui::View3DInventor& view,
                                             DumpOpenPackage const& pkg)
{
    // 同步入口 = 两段 inline 串行（既有调用方——DtaToolBars/SurfaceTools/
    // 各测试——行为零变化；轨迹逐字保留）。
    DumpOpenStage1 s1 = openDumpStage1(pkg);
    if (!s1.failReason.empty())
    {
        traceStage1Failure(pkg, s1.failReason);
        return std::nullopt;
    }
    return finishDumpOpen(view, pkg, s1);
}

void openDumpIModelAsync(Gui::View3DInventor& view, DumpOpenPackage const& pkg,
                         std::function<void(std::optional<DumpOpenResult>)>&& onOpened)
{
    // Ported from: itwinjs-core 打开链的 async 面（openFile await——RPC 数据
    // 面就绪后 views 装载；E5）。noWorker → 两段 inline 同步后回调（E3）。
    if (dqRender::WorkerProxy::instance().isNoWorker())
    {
        DumpOpenStage1 s1 = openDumpStage1(pkg);
        if (!s1.failReason.empty())
        {
            traceStage1Failure(pkg, s1.failReason);
            if (onOpened)
                onOpened(std::nullopt);
            return;
        }
        if (onOpened)
            onOpened(finishDumpOpen(view, pkg, s1));
        return;
    }

    // worker 路径：stage 1（纯 CPU——imodel.json + manifest 解析 + fetcher
    // 构造）下放 WorkerProxy 单例；完成经 TileAdmin::process 的帧内 drain 回
    // 主线程续 stage 2。QPointer 守卫 view 的存活（QObject 析构即置空——
    // 用户在解析完成前关窗的角案；参考的 promise 链对 closed connection
    // 同样静默失效）。
    QPointer<Gui::View3DInventor> viewGuard(&view);
    auto slot = std::make_shared<std::optional<DumpOpenStage1>>();
    dqRender::WorkerProxy::instance().submit(
        [slot, pkg]() {
            // worker 线程——纯 CPU 段（零 GL 零 viewport 零 Qt 对象触碰）。
            *slot = openDumpStage1(pkg);
        },
        [slot, viewGuard, pkg, onOpened = std::move(onOpened)]() {
            // 主线程（TileAdmin::process 的 drain 点）。
            if (!slot->has_value() || !(*slot)->failReason.empty()
                || !(*slot)->connection.IsValid())
            {
                // 解析段失败（或未达）——轨迹 + 失败回调。
                if (slot->has_value())
                    traceStage1Failure(pkg, (*slot)->failReason);
                else if (openTrace())
                    fprintf(stderr, "[OPEN] fail: stage1 result missing\n");
                if (onOpened)
                    onOpened(std::nullopt);
                return;
            }
            if (viewGuard.isNull())
            {
                // 视图已死——放弃装配（stage 1 载荷随闭包释放）。
                if (openTrace())
                    fprintf(stderr, "[OPEN] fail: view closed during async open\n");
                if (onOpened)
                    onOpened(std::nullopt);
                return;
            }
            if (openTrace())
                fprintf(stderr, "[OPEN] stage1 done (worker) — main leg starts\n");
            auto const tMain0 = std::chrono::steady_clock::now();
            std::optional<DumpOpenResult> result =
                finishDumpOpen(*viewGuard, pkg, **slot);
            if (openTrace())
            {
                // P-1b P-b4 计时面：主线程阻塞时长（stage 2——解析下放后的
                // UI 冻结面；对拍基线 = joeshouse Debug 2.87s 全阻塞 → 目标
                // <50ms 量级）。
                auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - tMain0).count();
                fprintf(stderr, "[OPEN] main-block ms=%lld\n",
                        static_cast<long long>(ms));
            }
            if (onOpened)
                onOpened(std::move(result));
        });
}

// ─── 模型↔包映射（M-Y Y-b2：自 main.cpp 匿名命名空间迁出——ReopenIModelTool
// 与 Start 页入口同源消费；包定义逐字未动）───

// 模型→包根映射表（一处集中，M-H(4) Step 2）：
//   joeshouse   = imodel joeshouse-v1/imodel.json + tiles [joeshouse-v1]
//                 + fallback [joeshouse-drill-v1（drill 域外键补字节，M-H Task 2）,
//                             joeshouse-drill-v2（M-I(5) P2c——缩远态 depth-1 键补字节）]
//   instances60 = imodel instances60-imodel-v1/imodel.json + tiles [instances60-v1]
//                 + fallback [instances60-drill-v1]
//   housemodel  = imodel housemodel-v1/imodel.json + tiles [housemodel-v1]
//                 + fallback [housemodel-drill-v1]（M-K(1)——House_Model.bim，
//                 首个 cameraOn=true 透视默认视图）
//   baytown     = imodel baytown-v1/imodel.json + tiles [baytown-v1]
//                 + fallback [baytown-drill-v1]（M-K(1)——Baytown.bim）
//   bridge-edit = imodel bridge-edit-v1/imodel.json + tiles [bridge-edit-v1]
//                 + fallback [bridge-edit-drill-v1（M-K(1)——编辑大桥测试.bim，
//                             ASCII 目录名映射；默认视图空域坑 24 → frameToWorldContent）,
//                             bridge-edit-sweep-v1（M-M(5)——全树 sweep 380,738 瓦/
//                             4.96GB[用户解除存储限制]；400k 瓦预算 cap 触顶，
//                             浏览深度界扩至 sweep 域；>100MB 大瓦本地持有）,
//                             bridge-edit-mag-missing-v1（2026-10-20 LOD 取证——
//                             子瓦范围 longest-axis 修复后深层泵请求的放大链
//                             m16..m131072 + d13 空间瓦 254 键，真后端 key-list
//                             回放 6.57MB）]
//   compatseed  = imodel+tiles 单根 compatseed-sweep-v1（P-3——M-D 早期 compatseed-v1
//                 的全量重采：CompatibilityTestSeed.bim 212 瓦全树自然收敛 +
//                 imodel.json；正交默认视图非坑 24 直渲）
//   mirukuru    = imodel+tiles 单根 mirukuru-sweep-v1（P-3——mirukuru.ibim 双树
//                 根即叶 2 瓦全树固有形态 + imodel.json；默认视图完整包含
//                 内容域，saved 直渲）
// （与 DumpOpenChainTest.cpp 各锁的包定义一致。）
std::optional<DumpOpenPackage> dumpPackageForModel(QString const& modelId)
{
    std::string dumpRoot = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";
    if (char const* env = std::getenv("DANQING_RPC_DUMP"))
        dumpRoot = env;
    DumpOpenPackage pkg;
    if (modelId == QLatin1String("joeshouse")) {
        pkg.imodelRoot = dumpRoot + "/joeshouse-v1";
        pkg.tileRoots = {dumpRoot + "/joeshouse-v1", dumpRoot + "/joeshouse-drill-v1",
                         dumpRoot + "/joeshouse-drill-v2",
                         dumpRoot + "/joeshouse-mag-missing-v1"};
    }
    else if (modelId == QLatin1String("instances60")) {
        pkg.imodelRoot = dumpRoot + "/instances60-imodel-v1";
        pkg.tileRoots = {dumpRoot + "/instances60-v1", dumpRoot + "/instances60-drill-v1",
                         dumpRoot + "/instances60-mag-missing-v1"};
    }
    else if (modelId == QLatin1String("housemodel")) {
        pkg.imodelRoot = dumpRoot + "/housemodel-v1";
        pkg.tileRoots = {dumpRoot + "/housemodel-v1", dumpRoot + "/housemodel-drill-v1"};
    }
    else if (modelId == QLatin1String("baytown")) {
        pkg.imodelRoot = dumpRoot + "/baytown-v1";
        pkg.tileRoots = {dumpRoot + "/baytown-v1", dumpRoot + "/baytown-drill-v1"};
    }
    else if (modelId == QLatin1String("bridge-edit")) {
        pkg.imodelRoot = dumpRoot + "/bridge-edit-v1";
        pkg.tileRoots = {dumpRoot + "/bridge-edit-v1", dumpRoot + "/bridge-edit-drill-v1",
                         dumpRoot + "/bridge-edit-sweep-v1",
                         dumpRoot + "/bridge-edit-mag-missing-v1",
                         dumpRoot + "/bridge-edit-mag-missing-v2",
                         dumpRoot + "/bridge-edit-mag-missing-v3",
                         dumpRoot + "/bridge-edit-mag-missing-v4",
                         dumpRoot + "/bridge-edit-coverage-v1"};
        // d13 空间前沿（2026-10-21 全树继续采集——~828k 瓦/1.7GB，本地持有
        // 不入库[.gitignore]）：manifest 存在即接入；缺席（新检出）时静默
        // 跳过，深放大退回父瓦 LOD（与既有大瓦本地持有同策略）。
        {
            std::string const frontier = dumpRoot + "/bridge-edit-frontier-d13-v1";
            std::error_code ec;
            if (std::filesystem::exists(frontier + "/manifest.json", ec))
                pkg.tileRoots.push_back(frontier);
        }
        // 坑 24：默认视图 0x99 指向原点附近空域（不含几何——直接回放=白屏
        // 零请求，README"默认视图空域"节）→ 打开链取景到世界域几何
        // contentRange（DumpOpenHelper 的 zoomToVolume 应用面）。
        pkg.frameToWorldContent = true;
    }
    else if (modelId == QLatin1String("newbridge")) {
        // P-2 P2-d：newBridge.bim（2.95GB 种子，10km 级大桥——树根域
        // ±4940×±425×±1294，root contentId "0/0/0/0/1" 叶）全树采集 8 根：
        //   主根 newbridge-v1（imodel.json + 树 props——**0 瓦**：P2-a 基础面
        //   采集在 10km 根内容到达前收口；整根小文件入 git）；fallback =
        //   d9-v1 → sweep-v1 → d10 → d11 → d12 → d13 → d9b（候选源有序序列
        //   ——sweep d1-d8 完整 5407 瓦 20GB[budget-capped 但逐层零缺] + d9/
        //   d9b 两波补齐 d9 层 + d10-d13 逐层波次，**169,920 瓦 sha256 全量
        //   校验 0 mismatch**；47.9GB files/ 本地持有经 .gitignore，每根
        //   manifest.json 存在才接入——缺席静默跳过，bridge-edit frontier
        //   同策略）。各 fallback 根的 imodel.json（采集器每根落一份）不
        //   参与连接面——imodelRoot 恒指主根（DumpTileFetcher 多根 fallback
        //   只查找瓦字节，树 props/imodel 单源主根）。
        pkg.imodelRoot = dumpRoot + "/newbridge-v1";
        pkg.tileRoots = {dumpRoot + "/newbridge-v1"};
        for (char const* root : {"/newbridge-d9-v1", "/newbridge-sweep-v1",
                                 "/newbridge-d10-v1", "/newbridge-d11-v1",
                                 "/newbridge-d12-v1", "/newbridge-d13-v1",
                                 "/newbridge-d9b-v1"}) {
            std::string const candidate = dumpRoot + root;
            std::error_code ec;
            if (std::filesystem::exists(candidate + "/manifest.json", ec))
                pkg.tileRoots.push_back(candidate);
        }
        // 坑 24 同型空域默认视图（P-2 R8 预案 + 几何实证）：默认视图 0x5387
        // 视域 x∈[-0.40,12.04]（origin [-0.398,-0.361,-31.231] + extents
        // [12.44,6.52,46.19]，rotation identity）与世界域几何 x∈[1192.46,
        // 2375.36]（contentRange × location[6131.91,529.69,1154.68] = 恰
        // projectExtents）x 向相离 ~1180m → 直接回放根瓦 OutsideFrustum
        // 零请求 → 打开链取景到世界域几何 contentRange（bridge-edit 坑 24
        // 取景路径同机制）。
        pkg.frameToWorldContent = true;
    }
    else if (modelId == QLatin1String("compatseed")) {
        // P-3：compatseed-sweep-v1 单根包（imodel.json + 树 props + 212 瓦同根
        // 齐备，无 fallback）。默认视图 0x41 正交 iso（origin [0.133,2.733,
        // -2.033] + extents [3.677,4.246,4.503]）与世界内容域 ≈[-0.25,2.25]³
        // 相交 → 非坑 24，saved 视图直渲（frameToWorldContent=false）。
        pkg.imodelRoot = dumpRoot + "/compatseed-sweep-v1";
        pkg.tileRoots = {dumpRoot + "/compatseed-sweep-v1"};
    }
    else if (modelId == QLatin1String("mirukuru")) {
        // P-3：mirukuru-sweep-v1 单根包（imodel.json + 2 树 props + 2 瓦——
        // 双树根即叶的全树固有形态）。默认视图 0x24 identity 旋转视域
        // x∈[288444,289588]/y∈[3803583,3804137] 完整包含内容域
        //（projectExtents [288874..289161]×[3803761..3803959]）→ 非坑 24，
        // saved 视图直渲（frameToWorldContent=false）。
        pkg.imodelRoot = dumpRoot + "/mirukuru-sweep-v1";
        pkg.tileRoots = {dumpRoot + "/mirukuru-sweep-v1"};
    }
    else {
        return std::nullopt;
    }
    return pkg;
}

// iModel key → modelId（E-Y7——ReopenIModelTool 的 openFile(key) 等价面）。
// key 实态 = 各 dump imodel.json 的 connection.rpcProps.key（源 bim 绝对
// 路径，2026-10-15 五包实测）：basename（正/反斜杠双形态）+ 大小写不敏比对。
std::optional<QString> modelIdForIModelKey(std::string const& key)
{
    if (key.empty())
        return std::nullopt;
    size_t const sep = key.find_last_of("/\\");
    std::string const base = key.substr(sep == std::string::npos ? 0 : sep + 1);
    QString const lower = QString::fromStdString(base).toLower();
    if (lower == QLatin1String("joeshouse.bim"))
        return QStringLiteral("joeshouse");
    if (lower == QLatin1String("properties_60instanceswithurl2.ibim"))
        return QStringLiteral("instances60");
    if (lower == QLatin1String("house_model.bim"))
        return QStringLiteral("housemodel");
    if (lower == QLatin1String("baytown.bim"))
        return QStringLiteral("baytown");
    if (lower == QString::fromUtf8("编辑大桥测试.bim"))
        return QStringLiteral("bridge-edit");
    // P-2 P2-d：newBridge.bim（rpcProps.key = D:\test\newBridge.bim——basename
    // 大小写不敏比对）。
    if (lower == QLatin1String("newbridge.bim"))
        return QStringLiteral("newbridge");
    // P-3：compatseed/mirukuru（rpcProps.key = itwinjs-core DTA assets 绝对路径；
    // 注意一 .bim 一 .ibim 后缀差异）。
    if (lower == QLatin1String("compatibilitytestseed.bim"))
        return QStringLiteral("compatseed");
    if (lower == QLatin1String("mirukuru.ibim"))
        return QStringLiteral("mirukuru");
    return std::nullopt;
}

// ─── 打开产物生命周期注册表（M-L(2)：main.cpp 迁入——Models/瓦树面板同源消费）───

std::map<Gui::View3DInventor*, std::unique_ptr<DumpOpenResult>>& openedDumps()
{
    static auto* s_registry =
        new std::map<Gui::View3DInventor*, std::unique_ptr<DumpOpenResult>>();
    return *s_registry;
}

void registerOpenedDump(Gui::View3DInventor* view, std::unique_ptr<DumpOpenResult> result)
{
    openedDumps().emplace(view, std::move(result));
}

void forgetOpenedDump(Gui::View3DInventor* view)
{
    openedDumps().erase(view);
}

DumpOpenResult* findOpenedDump(Gui::View3DInventor* view)
{
    auto it = openedDumps().find(view);
    return it == openedDumps().end() ? nullptr : it->second.get();
}

}  // namespace dta
