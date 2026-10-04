// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — 第二 iModel 瓦叠加实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/TiledGraphics.ts
#include "TiledGraphics.h"
#include "../DumpOpenHelper.h"  // findOpenedDump（活动 fetcher 观察面）

#include <dqApp/Application.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/Viewport.h>
#include <dqApp/tile/DumpTileFetcher.h>
#include <dqApp/tile/DumpTileTreeProps.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/ToolAdmin.h>

#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>

#include <QFileDialog>
#include <QFileInfo>

#include <cstdlib>
#include <cstdio>

namespace Gui {

// Ported from: computeTransformFromSecondaryIModel（:81-93）。
dqGeom::Transform computeTransformFromSecondaryIModel(
    dqApp::IModelConnection const& primary,
    dqApp::IModelConnection const& secondary,
    dqGeom::Transform const& tileTreeToWorld)
{
    // :85-87 任一 ecefLocation 无效 → tileTreeToWorld 原样（rpc-dumps 数据面
    // 实态恒走此分支——EQUIVALENCE 见头文件）。
    auto const* primaryDump =
        dynamic_cast<dqApp::DumpIModelConnection const*>(&primary);
    auto const* secondaryDump =
        dynamic_cast<dqApp::DumpIModelConnection const*>(&secondary);
    bool const secondaryValid =
        secondaryDump != nullptr && secondaryDump->hasEcefLocation();
    bool const primaryValid =
        primaryDump != nullptr && primaryDump->hasEcefLocation();
    if (!secondaryValid || !primaryValid)
        return tileTreeToWorld;

    // :89-92 ECEF 复合（dbToEcef ∘ tileTreeToWorld ∘ primary.ecef⁻¹）——
    // EcefLocation 类型未移植（数据面缺席）：不可达（上方守卫恒返回——
    // hasEcefLocation 当前恒 false）。geolocated 数据面落地时补。
    return tileTreeToWorld;
}

namespace {

// Ported from: TiledGraphics.ts Provider（:41-79——forEachTileTreeRef 逐 ref）。
class SecondaryProvider final : public dqApp::TiledGraphicsProvider {
public:
    explicit SecondaryProvider(
        std::vector<std::unique_ptr<TiledGraphicsReference>> const* refs)
        : m_refs(refs)
    {}

    void forEachTileTreeRef(
        dqApp::Viewport& /*viewport*/,
        std::function<void(dqApp::TileTreeReference&)> const& func) const override
    {
        for (auto const& ref : *m_refs)
            func(*ref);
    }

private:
    std::vector<std::unique_ptr<TiledGraphicsReference>> const* m_refs;  // 非拥有
};

// per-viewport 注册表（:95 providersByViewport）。
std::map<dqApp::Viewport*, std::unique_ptr<TiledGraphicsAttachment>>&
attachmentsByViewport()
{
    static auto* s_map =
        new std::map<dqApp::Viewport*, std::unique_ptr<TiledGraphicsAttachment>>();
    return *s_map;
}

}  // namespace

std::unique_ptr<TiledGraphicsAttachment> createSecondaryIModelAttachment(
    dta::DumpOpenPackage const& pkg, dqApp::Viewport* vp)
{
    if (pkg.imodelRoot.empty() || pkg.tileRoots.empty() || vp == nullptr)
        return nullptr;

    // ① 打开第二连接（:113 BriefcaseConnection.openFile 的 dump 域等价）。
    auto connection =
        dqApp::DumpIModelConnection::open(pkg.imodelRoot + "/imodel.json");
    if (!connection.IsValid())
        return nullptr;

    // 树 props（主根——宿主打开链 ④ 同源）。
    auto props = dqApp::DumpTileTreeProps::load(pkg.tileRoots[0]);
    if (!props.has_value())
        return nullptr;

    // 取数扩围（EQUIVALENCE 见头文件）：活动 fetcher 追加第二包根——manifest
    // 单次解析共享（M-I(1) 纪律：props 侧 takeManifest 移交）。
    auto* liveFetcher = dynamic_cast<dqApp::DumpTileFetcher*>(
        &dqRender::TileAdmin::instance().getFetcher());
    if (liveFetcher == nullptr)
        return nullptr;
    liveFetcher->addFallbackRoot(props->takeManifest(), pkg.tileRoots[0]);
    for (size_t i = 1; i < pkg.tileRoots.size(); ++i)
        liveFetcher->addFallbackRoot(pkg.tileRoots[i]);

    auto out = std::make_unique<TiledGraphicsAttachment>();
    out->connection = connection;

    // ② 第二视图的树 refs（:59 `view.getModelTreeRefs()`——ViewCreator3d
    // createDefaultView 的 DanQing 等价 = 连接 defaultViewState 的
    // modelSelector 逐 model 装配[宿主打开链 ④ 同款]；缺省视图缺席 = 全模型）。
    std::vector<dqBase::DqId> models;
    if (connection->getDefaultViewState().has_value()
        && connection->getDefaultViewState()->modelSelectorProps.has_value()) {
        models = connection->getDefaultViewState()->modelSelectorProps->models;
    } else {
        for (auto const& m : connection->getModels())
            models.push_back(m.id);
    }

    for (auto modelId : models) {
        // treeId 派生（PrimaryTileTree.ts:268-290——打开链 ④ 同源；edges
        // 恒 required[缺省 SmoothShade——参考 defaultTileOptions 语义]）。
        dqRender::PrimaryTileTreeId treeIdObj;
        treeIdObj.edges = dqRender::TileOptions{}.edgeOptions;
        std::string const treeId = dqRender::iModelTileTreeIdToString(
            modelId.ToString(), treeIdObj, dqRender::TileOptions{});

        auto treeProps = props->byTreeId(treeId);
        if (!treeProps.has_value())
            continue;  // 该 model 的树不在 dump 域（主连接语义为打开失败；
                       // 叠加面逐树尽力——参考 forEachTileTreeRef 的空 ref 容忍）

        auto tree = std::make_unique<dqRender::ImdlTileTree>(
            treeProps->id, treeProps->rootTile.contentId, treeProps->rootTile.range,
            treeProps->metadata);
        tree->setRenderSystem(vp->renderSystem());
        if (treeProps->hasLocation)
            tree->setIModelTransform(treeProps->location);

        // :32-34/:76-78 computeTransform（Provider.computeTransform——缓存）
        // = computeTransformFromSecondaryIModel（ecef 缺席恒等回退）。
        dqGeom::Transform const world = computeTransformFromSecondaryIModel(
            *vp->GetIModel(), *connection, tree->getIModelTransform());

        auto inner =
            std::make_unique<dqApp::SimpleTileTreeReference>(tree.get());
        out->refs.push_back(std::make_unique<TiledGraphicsReference>(*inner, world));
        out->innerRefs.push_back(std::move(inner));
        out->treeLoadLog.push_back(treeId);
        out->trees.push_back(std::move(tree));
    }
    if (out->trees.empty())
        return nullptr;

    out->provider = std::make_unique<SecondaryProvider>(&out->refs);
    out->props = std::move(*props);
    return out;
}

bool toggleExternalTiledGraphicsProvider(dqApp::Viewport* vp,
                                         dta::DumpOpenPackage const* pkg)
{
    if (vp == nullptr)
        return false;

    // :99-105 在 → drop + 关连接。
    auto& registry = attachmentsByViewport();
    auto it = registry.find(vp);
    if (it != registry.end()) {
        vp->DropTiledGraphicsProvider(it->second->provider.get());
        registry.erase(it);  // ~attachment 释放连接（= existing.iModel.close()）
        vp->InvalidateScene();
        vp->RenderFrame();
        return false;
    }

    // :107 文件选择（pkg 空 = QFileDialog 选 imodel.json）。
    dta::DumpOpenPackage pkgLocal;
    if (pkg == nullptr) {
        QString const selected = QFileDialog::getOpenFileName(
            vp, QStringLiteral("Select imodel.json"), QString(),
            QStringLiteral("imodel.json"));
        if (selected.isEmpty())
            return false;
        QFileInfo info(selected);
        pkgLocal.imodelRoot = info.dir().path().toStdString();
        pkgLocal.tileRoots = {pkgLocal.imodelRoot};
        pkg = &pkgLocal;
    }

    // :112-116 建并 add（异常面 = 失败 nullptr 返回）。
    auto attachment = createSecondaryIModelAttachment(*pkg, vp);
    if (attachment == nullptr)
        return false;
    vp->AddTiledGraphicsProvider(attachment->provider.get());
    attachmentsByViewport()[vp] = std::move(attachment);
    vp->InvalidateScene();
    vp->RenderFrame();
    return true;
}

TiledGraphicsAttachment* findTiledGraphicsAttachment(dqApp::Viewport* vp)
{
    auto& registry = attachmentsByViewport();
    auto it = registry.find(vp);
    return it == registry.end() ? nullptr : it->second.get();
}

// Ported from: ToggleSecondaryIModelTool（:123-135）。
bool ToggleSecondaryIModelTool::run()
{
    auto* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (vp == nullptr)
        return false;
    dta::DumpOpenPackage const* pkg = nullptr;
    dta::DumpOpenPackage pkgLocal;
    if (!m_pkgRoot.empty()) {
        pkgLocal.imodelRoot = m_pkgRoot;
        pkgLocal.tileRoots = {m_pkgRoot};
        pkg = &pkgLocal;
    }
    toggleExternalTiledGraphicsProvider(vp, pkg);
    return true;
}

bool ToggleSecondaryIModelTool::parseAndRun(
    std::vector<std::string> const& args)
{
    if (!args.empty())
        m_pkgRoot = args[0];
    return run();
}

}  // namespace Gui
