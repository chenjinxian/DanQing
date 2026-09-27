// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — 本地 RPC-dump 回放取数器（零网络——§8.2/TD-24 清退后的取数面）
//
// Authored: no reference equivalent exists — replaying captured RPC bytes is
// the host-side seam replacing itwinjs's TileAdmin RPC layer
// (generateTileContent / requestTileTreeProps, TileAdmin.ts:648-707) under the
// §8.2 zero-network protocol. The url contract follows ImdlTileTree::contentUrl
// 的既有组合约定 "<treeId>/<contentId>"（<dumpRoot>/ 前缀同解析——取末两段为
// 键，manifest 是唯一键域）；键 = manifest tiles[] 的 (treeId, contentId)
// 原样串（前端请求键，Task 1 评审钉死①）。
//
// 投递契约与 FileTileFetcher 同款轮询语义：fetch() 同步读盘/查表，投递只在
// processCompleted()（从不在 fetch() 内联——TileAdmin process 周期语义）。
// 公共头依据 §8.4：DisplayTestApp 宿主跨模块注入（TileAdmin::setFetcher）。
#pragma once

#include "../Export.h"
#include "DumpTileTreeProps.h"  // DumpManifest（dump 元数据域共享）

#include <dqRender/tile/ITileFetcher.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// DumpTileFetcher — manifest 回放的 ITileFetcher（零网络：只有文件 I/O，不
// include 任何网络头——结构零网络，DumpTileFetcherTest.ZeroNetworkByConstruction
// 全仓原语扫描锁）。
class DQ_APP_EXPORT DumpTileFetcher : public dqRender::ITileFetcher {
public:
    // 请求结果（fetch 侧一次性归类——投递前即定，processCompleted 不再改）：
    // Completed = manifest 命中且字节精确回放；NotFound = 键不在 manifest
    // （前端派生键与采集键域的差异——上层按请求失败处置）；
    // Error = 键命中但文件缺失/byteLength 不符（dump 资产自洽性破口）。
    enum class DumpFetchOutcome : uint8_t { Completed, NotFound, Error };

    // 一条请求轨迹（M-E Task 4 完整加载对账锁的消费面——"凡 manifest 有键的
    // 瓦全部消费"的键级清单：requested vs manifest 键集合逐一对账）。
    struct DumpRequestRecord {
        std::string treeId;
        std::string contentId;
        DumpFetchOutcome outcome;
        uint64_t bytes = 0;  // Completed 时的回放字节数（== manifest byteLength）
    };

    // 装载 <dumpRoot>/manifest.json；失败 → isValid()==false（fetch 走 onError，
    // 不崩——MissingDumpFailsLoadGracefully 锁）。
    explicit DumpTileFetcher(std::string const& dumpRoot);

    bool isValid() const noexcept { return m_valid; }
    size_t getTreeCount() const noexcept { return m_manifest.trees.size(); }
    size_t getTileCount() const noexcept { return m_manifest.tiles.size(); }

    // 全部被请求键的轨迹（fetch 调用即追加——每条一次；TileAdmin 对
    // 非 NotLoaded 态不重建请求（TileAdmin.cpp processRequestsForUser），
    // 故每键至多一条）。供完整加载对账测试只读遍历。
    std::vector<DumpRequestRecord> const& requestLog() const noexcept
    {
        return m_requestLog;
    }

    // --- dqRender::ITileFetcher ---
    void fetch(std::string const& url, dqRender::Tile& tile,
               std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete,
               std::function<void(dqRender::Tile&, std::string const&)> onError) override;
    void processCompleted() override;
    uint32_t getActiveCount() const noexcept override;
    void cancelAll() override;

private:
    struct Completed {
        dqRender::Tile* tile = nullptr;
        bool ok = false;
        std::vector<uint8_t> data;
        std::string error;
        std::function<void(dqRender::Tile&, std::vector<uint8_t> const&)> onComplete;
        std::function<void(dqRender::Tile&, std::string const&)> onError;
    };

    std::string m_dumpRoot;
    DumpManifest m_manifest;
    bool m_valid = false;
    std::vector<Completed> m_completed;
    std::vector<DumpRequestRecord> m_requestLog;
};

END_DQ_APP_NAMESPACE
