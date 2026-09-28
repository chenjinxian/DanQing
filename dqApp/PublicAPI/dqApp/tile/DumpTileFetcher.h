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
        // 命中源（M-H Task 2 多根合并）：0 = 主根，i+1 = fallbackRoots()[i]。
        // 仅在 manifest 命中（Completed/Error）时有意义；NotFound 恒 0（无命中）。
        size_t hitRoot = 0;
    };

    // 装载 <primaryRoot>/manifest.json；失败 → isValid()==false（fetch 走
    // onError，不崩——MissingDumpFailsLoadGracefully 锁）。
    //
    // 多根合并（M-H Task 2——instances60 sweep∪drill 联合域）：fallbackRoots
    // 为有序只读备根清单（默认空 = 单根现状零行为变化）。查找序 primary →
    // fallback[0] → fallback[1]…，首命中即服务（字节读自命中根目录，命中源记
    // requestLog 的 hitRoot——outcome 语义不变）。树 props 仍仅取自主根
    // （DumpTileTreeProps 不动——同 treeId 语义单源；getTreeCount/
    // getTileCount 同为主根计数）。fallback 根 manifest 缺失/坏 → 记
    // fallbackWarnings() 不致命（主根语义：isValid 只看主根），不进查找域。
    explicit DumpTileFetcher(std::string const& primaryRoot,
                             std::vector<std::string> fallbackRoots = {});

    bool isValid() const noexcept { return m_valid; }
    size_t getTreeCount() const noexcept { return m_manifest.trees.size(); }
    size_t getTileCount() const noexcept { return m_manifest.tiles.size(); }

    // 构造入参的 fallback 根清单（原序，含装载失败者）。
    std::vector<std::string> const& fallbackRoots() const noexcept
    {
        return m_fallbackRoots;
    }

    // manifest 装载失败的 fallback 根（warning 不致命——InvalidFallbackRoot
    // WarnsButPrimaryServes 锁）。
    std::vector<std::string> const& fallbackWarnings() const noexcept
    {
        return m_fallbackWarnings;
    }

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
    // 多根合并（M-H Task 2）：m_fallbackRoots 为构造入参原序（含失败者——
    // 观测面）；m_fallbacks 仅持装载成功的（根目录 + manifest——字节服务
    // 查找域）；m_fallbackWarnings 记装载失败根（warning 不致命）。
    struct FallbackRoot {
        std::string root;
        DumpManifest manifest;
    };
    std::vector<std::string> m_fallbackRoots;
    std::vector<FallbackRoot> m_fallbacks;
    std::vector<std::string> m_fallbackWarnings;
    std::vector<Completed> m_completed;
    std::vector<DumpRequestRecord> m_requestLog;
};

END_DQ_APP_NAMESPACE
