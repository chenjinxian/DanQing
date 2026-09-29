// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — 打开链装配实现（M-H Task 3；流程与参考锚见
// DumpOpenHelper.h 文件头）。
#include "DumpOpenHelper.h"

#include "Gui/View3DInventor.h"

#include <dqApp/tile/SpatialTileTreeReferences.h>  // setCreateOverride（frontend-tiles 缝）
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

namespace dta {

std::optional<DumpOpenResult> openDumpIModel(Gui::View3DInventor& view,
                                             DumpOpenPackage const& pkg)
{
    if (pkg.imodelRoot.empty() || pkg.tileRoots.empty())
        return std::nullopt;

    // ① 打开连接（imodel.json——iModelRpc 数据面回放）。
    auto connection = dqApp::DumpIModelConnection::open(pkg.imodelRoot + "/imodel.json");
    if (!connection.IsValid())
        return std::nullopt;

    // 树 props（主根——PrimaryTreeSupplier 对 requestTileTreeProps 返回树的
    // 逐树 createTileTree（PrimaryTileTree.ts:63-80）的离线对应物，同
    // DumpMount.h 注释）。
    auto props = dqApp::DumpTileTreeProps::load(pkg.tileRoots[0]);
    if (!props.has_value())
        return std::nullopt;

    // fetcher 注入（TileAdmin DI 缝 §8.4——参考的 TileAdmin RPC 层
    //（generateTileContent TileAdmin.ts:694-706）在打开链全程就位；
    // 多根 = drill 域外键补字节，M-H Task 2）。
    // 单次解析共享（M-I(1)——用户报告的打开长等待主根因）：props 装载的
    // manifest 移交 fetcher（takeManifest——树条目复制留存 byTreeId，瓦键
    // 域移交），打开链不再对 59MB manifest 解析两遍（原 DumpOpenHelper
    // props 一遍 + fetcher 路径构造一遍，Debug 实测 12.1s/遍）。
    std::vector<std::string> fallbacks(pkg.tileRoots.begin() + 1, pkg.tileRoots.end());
    auto fetcher = std::make_unique<dqApp::DumpTileFetcher>(props->takeManifest(),
                                                            pkg.tileRoots[0],
                                                            std::move(fallbacks));
    if (!fetcher->isValid())
        return std::nullopt;
    dqRender::TileAdmin::instance().setFetcher(std::move(fetcher));

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
    dqApp::ViewList views = dqApp::ViewList::create(connection.Get());
    auto viewState = views.getDefaultView(connection.Get());
    dqApp::SpatialTileTreeReferences::clearCreateOverride();
    if (!viewState.IsValid())
        return std::nullopt;
    auto* view3d = viewState->AsViewState3d();
    if (view3d == nullptr)
        return std::nullopt;
    auto* spatial = view3d->AsSpatialViewState();
    if (spatial == nullptr)
        return std::nullopt;

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
            treeIdObj.edges = dqRender::TileOptions{}.edgeOptions;
        // PrimaryTreeSupplier.createTileTree（PrimaryTileTree.ts:65）的
        // iModelTileTreeIdToString 调用（options = tileAdmin = defaultTileOptions）。
        std::string const treeId = dqRender::iModelTileTreeIdToString(
            modelId.ToString(), treeIdObj, dqRender::TileOptions{});

        // requestTileTreeProps 回放（:66——DumpTileTreeProps.byTreeId）；
        // modelSelector 引用的树不在 dump 域 = 采集缺口 → 打开失败。
        auto treeProps = props->byTreeId(treeId);
        if (!treeProps.has_value())
            return std::nullopt;

        // createTileTree 装配（:79-81——iModelTileTreeParamsFromJSON →
        // IModelTileTree）；location → TileTree.iModelTransform
        //（TileTree.ts:122——iModelTileTreeParamsFromJSON :50/:71）。
        auto tree = std::make_unique<dqRender::ImdlTileTree>(
            treeProps->id, treeProps->rootTile.contentId, treeProps->rootTile.range,
            treeProps->metadata);
        tree->setRenderSystem(viewport->renderSystem());
        if (treeProps->hasLocation)
            tree->setIModelTransform(treeProps->location);
        out.provider->addTree(tree.get());
        out.treeLoadLog.push_back(treeId);
        out.trees.push_back(std::move(tree));
    }
    if (out.trees.empty())
        return std::nullopt;

    // ⑤ provider 注册（Viewport.ts:1729-1732 addTiledGraphicsProvider）。
    viewport->AddTiledGraphicsProvider(out.provider.get());

    out.connection = connection;
    out.viewState = viewState;
    out.props = std::move(*props);
    out.fetcher = static_cast<dqApp::DumpTileFetcher*>(
        &dqRender::TileAdmin::instance().getFetcher());
    return out;
}

}  // namespace dta
