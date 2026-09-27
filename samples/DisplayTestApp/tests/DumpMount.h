// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — RPC-dump 多树全量挂载入口（阶段1 M-E Task 2；
// M-D(3) 单树挂载模式的归并重构：manifest trees 全量迭代，每树一
// ImdlTileTree，fit 域 = iModel 元数据否则树 contentRange 并集）。
//
// Authored: no reference equivalent exists — replaying captured RPC bytes is
// the host-side seam of the §8.2 zero-network protocol; this helper is the
// test-host counterpart of itwinjs's per-tree createTileTree consumption of
// the requestTileTreeProps result set (PrimaryTileTree.ts:63-80) plus the
// iModel-extents-driven fit idiom a host application performs after
// IModelConnection opens.
//
// ---------------------------------------------------------------------------
// 装载契约（一次调用完成四件事）：
//   1. DumpTileTreeProps::load(dumpRoot) —— manifest + provenance.iModel；
//   2. loadDumpManifest(dumpRoot) —— 瓦键域（请求键覆写断言的消费面）；
//   3. DumpTileFetcher 注入 TileAdmin（DI 缝 §8.4；gtest_discover_tests
//      逐测试独立进程——不回溢其他 ctest 条目）；
//   4. trees() 全量迭代：每树 byTreeId → ImdlTileTree 装配
//      （PrimaryTileTree.createTileTree 的离线对应物）→ provider 装树。
// fit 域（mountDumpFitVolume 的消费输入）：
//   iModelInfo().extents（provenance.iModel 有——iModel 坐标域的项目范围）
//   否则各树 contentRange 并集（TileTreeProps.contentRange :51，null range
//   跳过——空树/unknown 不污染并集）；全无 → 装载失败（无取景对象）。
// ---------------------------------------------------------------------------
#pragma once

#include <dqApp/Viewport.h>
#include <dqApp/tile/DumpTileFetcher.h>
#include <dqApp/tile/DumpTileTreeProps.h>
#include <dqApp/tile/SimpleTileTreeReference.h>
#include <dqApp/tile/TiledGraphicsProvider.h>
#include <dqGeom/Range3d.h>
#include <dqRender/tile/ImdlTileTree.h>
#include <dqRender/tile/TileAdmin.h>
#include <dqRender/tile/TileTree.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dta {

// TiledGraphicsProvider 装树（TileTreeRenderTest 的 TreeSetProvider /
// RpcDumpRenderTest 的 DumpTreeProvider 同款——应用通道
// Viewport.ts:1729-1732 addTiledGraphicsProvider）。
class DumpTreeProvider final : public dqApp::TiledGraphicsProvider {
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

// 多树全量装载结果。manifest/props 保留供测试断言（计数/键域/树 props）；
// trees 与 provider 由本结构持有（ImdlTileTree 非拷贝——unique_ptr 所有权）。
// props 为 optional——DumpTileTreeProps 构造私域（load 静态工厂），DumpMount
// 需默认构造后移入。fetcher 为非拥有观察指针（所有权经 setFetcher 移入
// TileAdmin）——完整加载对账锁读 requestLog 的入口（M-E Task 4）。
struct DumpMount {
    dqApp::DumpManifest manifest;
    std::optional<dqApp::DumpTileTreeProps> props;
    std::vector<std::unique_ptr<dqRender::ImdlTileTree>> trees;
    DumpTreeProvider provider;
    dqGeom::Range3d fitRange;
    dqApp::DumpTileFetcher* fetcher = nullptr;
};

// fit 取景域：30% 外扩（对象完整居中、四角留背景的先验——M-D(3) 两只锁
// 的既有取景参数归并，非新设）；zEps 给 z 向亚厘米薄盒的 Iso 视线留深度
// （mirukuru 的 +1.0 同款语义）。
inline dqGeom::Range3d mountDumpFitVolume(dqGeom::Range3d const& fit,
                                          double zEps = 0.0)
{
    double const dx = 0.3 * (fit.high.x - fit.low.x);
    double const dy = 0.3 * (fit.high.y - fit.low.y);
    double const dz = 0.3 * (fit.high.z - fit.low.z) + zEps;
    return dqGeom::Range3d::CreateXYZXYZ(
        fit.low.x - dx, fit.low.y - dy, fit.low.z - dz,
        fit.high.x + dx, fit.high.y + dy, fit.high.z + dz);
}

// 装载 <dumpRoot> 的全部树并挂载到 viewport（fetcher 注入 + provider 装树
// + setRenderSystem）。失败（manifest 坏 / props 不可达 / 无 fit 域）→
// nullopt。
inline std::optional<DumpMount> mountDump(dqApp::Viewport& viewport,
                                          std::string const& dumpRoot)
{
    auto loadedProps = dqApp::DumpTileTreeProps::load(dumpRoot);
    if (!loadedProps.has_value())
        return std::nullopt;
    auto manifest = dqApp::loadDumpManifest(dumpRoot);
    if (!manifest.has_value())
        return std::nullopt;

    // DI 缝注入（§8.4）：替换 Startup 的 FileTileFetcher。
    auto fetcher = std::make_unique<dqApp::DumpTileFetcher>(dumpRoot);
    if (!fetcher->isValid())
        return std::nullopt;
    dqRender::TileAdmin::instance().setFetcher(std::move(fetcher));

    DumpMount out;
    out.manifest = std::move(*manifest);
    out.props = std::move(*loadedProps);
    // 非拥有观察指针——所有权已随 setFetcher 移入 TileAdmin；供完整加载
    // 对账锁读 requestLog（M-E Task 4）。
    out.fetcher = static_cast<dqApp::DumpTileFetcher*>(
        &dqRender::TileAdmin::instance().getFetcher());
    auto& props = *out.props;

    // fit 域：iModel 级元数据优先（provenance.iModel）；缺省回退树
    // contentRange 并集（iModel 坐标域——瓦顶点同域；null range 跳过）。
    bool haveFit = false;
    if (auto const& imodel = props.iModelInfo()) {
        if (!imodel->extents.isNull()) {
            out.fitRange = imodel->extents;
            haveFit = true;
        }
    }
    for (auto const& entry : props.trees()) {
        auto treeProps = props.byTreeId(entry.treeId);
        if (!treeProps.has_value())
            return std::nullopt;  // manifest 与 props 文件不一致 → 装载失败
        if (!treeProps->metadata.contentRange.isNull()) {
            if (!haveFit) {
                out.fitRange = treeProps->metadata.contentRange;
                haveFit = true;
            } else {
                out.fitRange.ExtendRange(treeProps->metadata.contentRange);
            }
        }
        auto tree = std::make_unique<dqRender::ImdlTileTree>(
            treeProps->id, treeProps->rootTile.contentId, treeProps->rootTile.range,
            treeProps->metadata);
        tree->setRenderSystem(viewport.renderSystem());
        out.provider.addTree(tree.get());
        out.trees.push_back(std::move(tree));
    }
    if (out.trees.empty() || !haveFit)
        return std::nullopt;
    return std::optional<DumpMount>(std::move(out));
}

}  // namespace dta
