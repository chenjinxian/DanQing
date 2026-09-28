// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — 已存 iModel 数据包的打开链装配（M-H Task 3）。
//
// 与 itwinjs-core 前端打开流程完全一致（无 backend——数据全部经本地文件
// 回放，§8.2 零网络）：
//   ①打开连接（DumpIModelConnection——imodel.json 回放 iModelRpc 数据面；
//     参考 BriefcaseConnection/SnapshotConnection.open 的数据面语义）；
//   ②视图清单 + 默认视图装载（Viewer.ts:183-188 create → ViewList.create →
//     populate（ViewPicker.ts:69-144）→ getDefaultView（:53-55）→ views.load
//     （:34-51——完整 ViewState 原样 clone 应用，openView 无 fit 补偿））；
//   ③viewport changeView（Viewer.ts:231 ScreenViewport.create(contentDiv,
//     view) → Viewport.ts:3196-3204 vp.changeView(view)）；
//   ④按 modelSelector 逐 model 建 TileTreeRef（SpatialViewState 构造的
//     SpatialTileTreeReferences.create（SpatialViewState.ts:109 →
//     PrimaryTileTree.ts:594-606 → SpatialRefs :608-861 → 每 model 一
//     PrimaryTreeReference :155-195）→ 树装载 = PrimaryTreeSupplier.
//     createTileTree（:63-98——treeId 派生（createTreeId :268-290 →
//     iModelTileTreeIdToString :65）→ requestTileTreeProps → IModelTileTree）；
//     回放对应物 = 逐 model 派生同一 treeId → DumpTileTreeProps.byTreeId →
//     ImdlTileTree + location（TileTree.iModelTransform TileTree.ts:122）；
//   ⑤provider 注册（TiledGraphicsProvider 应用通道——Viewport.ts:1729-1732
//     addTiledGraphicsProvider；引擎内 SpatialRefs 全移植登记后续里程碑
//     （SpatialTileTreeReferences.h:11-16 的登记在案），宿主通道即参考的
//     frontend-tiles 替换缝同款应用面）。
//
// 判据（DumpOpenChain 锁）：请求序列同构（视图装载 → 逐 model 树 props →
// 逐 TileID 瓦——与采集期 DTA 实际序列同键同序）+ 初始视图参数 = saved
// ViewState 逐项（非 fit）。
#pragma once

#include <dqApp/IModelConnection.h>
#include <dqApp/ViewPicker.h>                  // ViewList（ViewPicker.ts 移植面）
#include <dqApp/ViewState.h>
#include <dqApp/Viewport.h>
#include <dqApp/tile/DumpIModelConnection.h>
#include <dqApp/tile/DumpTileFetcher.h>
#include <dqApp/tile/DumpTileTreeProps.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/tile/SpatialTileTreeReferences.h>  // DumpOpenEmptyTileTreeReferences 基类
#include <dqApp/tile/TiledGraphicsProvider.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Gui {
class View3DInventor;
}

namespace dta {

// TiledGraphicsProvider 装树——与 DumpMount.h 的 DumpTreeProvider 同构（独立
// 类名避免 ODR 冲突；注释见 DumpMount.h:47-49——应用通道
// Viewport.ts:1729-1732 addTiledGraphicsProvider）。
class DumpOpenTreeProvider final : public dqApp::TiledGraphicsProvider {
public:
    void addTree(dqRender::TileTree* tree)
    {
        m_refs.push_back(std::make_unique<dqApp::SimpleTileTreeReference>(tree));
    }

    void forEachTileTreeRef(
        dqApp::Viewport& /*viewport*/,
        std::function<void(dqApp::TileTreeReference&)> const& func) const override
    {
        for (auto& ref : m_refs)
            func(*ref);
    }

private:
    std::vector<std::unique_ptr<dqApp::SimpleTileTreeReference>> m_refs;
};

// EMPTY refs 工厂产物（frontend-tiles 替换缝——openDumpIModel 注释④b 同款；
// 打开链的视图装载/克隆窗口内经 SpatialTileTreeReferences::setCreateOverride
// 安装，抑制引擎内占位 refs 的 "<modelId>/0/0/0/0" 噪音请求）。
class DumpOpenEmptyTileTreeReferences final : public dqApp::SpatialTileTreeReferences {
public:
    void forEachTileTreeRef(
        std::function<void(dqApp::TileTreeReference&)> const& /*func*/) const override
    {
    }
};

// 打开包：imodel.json 根 + 瓦根清单（[0]=主根——树 props 源（同 treeId
// 语义单源，DumpTileFetcher 契约）；其余 = fallback 序（drill 域外键补字节））。
struct DumpOpenPackage {
    std::string imodelRoot;
    std::vector<std::string> tileRoots;
};

// 打开产物（断言面 + 生命周期持有）：trees/provider 由本结构持有
//（ImdlTileTree 非拷贝）；fetcher 非拥有观察指针（所有权经 setFetcher 移入
// TileAdmin——requestLog 对账入口）；treeLoadLog = 树装载序（modelSelector
// 驱动序——"请求序列 = 逐 model 树 props"的对账面）。
// provider 为 unique_ptr 堆持有：**viewport 注册的是 provider 的地址**
//（AddTiledGraphicsProvider 裸指针——Viewport.ts:1729-1732），本结构经
// optional<DumpOpenResult> 移动返回时值成员的地址会变（悬垂——首跑 SEH
// 实锤：TiledGraphicsProviders::addToScene 解引用死 provider）；堆对象的
// 地址随指针移动保持稳定。
struct DumpOpenResult {
    dqBase::RefPtr<dqApp::DumpIModelConnection> connection;
    dqBase::RefPtr<dqApp::ViewState> viewState;  // changeView 后的 saved 视图
    std::optional<dqApp::DumpTileTreeProps> props;
    std::vector<std::unique_ptr<dqRender::ImdlTileTree>> trees;
    std::unique_ptr<DumpOpenTreeProvider> provider;
    std::vector<std::string> treeLoadLog;
    dqApp::DumpTileFetcher* fetcher = nullptr;  // 非拥有（TileAdmin 持有）
};

// 打开链装配（流程①-⑤，见文件头）。失败（imodel.json 坏 / 树 props 不可达 /
// modelSelector 引用的树不在 dump 域 / fetcher 无效）→ nullopt。
std::optional<DumpOpenResult> openDumpIModel(Gui::View3DInventor& view,
                                             DumpOpenPackage const& pkg);

}  // namespace dta
