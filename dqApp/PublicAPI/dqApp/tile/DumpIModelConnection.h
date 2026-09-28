// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DumpIModelConnection（已存 iModel 数据的打开连接回放）
//
// Authored: no reference equivalent exists — 参考侧打开连接的数据面经 RPC
//           路由（IModelReadRpcInterface——连接 props/getViewList/
//           getDefaultViewId/getViewStateData/getModelProps）从后端取数；
//           本类是 §8.2 零网络协议下"打开已存 iModel 数据包"的宿主缝：
//           imodel.json（仓外 danqing-rpc-tools 采集的 RPC 载荷原样，只读
//           §11.11）经文件 I/O 进入，回放同一数据面。与 itwinjs-core 前端
//           打开流程的一致性由"请求序列同构"判据保证（DumpOpenChain 锁）。
//
// 参考对齐面（字段映射的唯一规范来源）：
//   - IModelConnection open 数据面（IModelConnection.ts——name/projectExtents/
//     key/iTwinId + onOpen 事件 :796/:845/:863）；
//   - IModelConnection.Views 三查询（:1518-1526/:1535-1538/:1541-1551——
//     经 Views::RpcHooks 缝注入，见 IModelConnection.h）；
//   - ViewSpec 映射（:1520-1524——id/code.value/classFullName；采集侧已按
//     DTA getViewList 返回形态落盘）；
//   - ViewStateProps（ViewStateProps.h——getViewStateData 的返回载体）；
//   - ModelProps（core-common ModelProps——models.getModelProps 消费面子集：
//     id/name/classFullName；parentModel/modeledElement/jsonProperties 登记
//     未消费）。
#pragma once

#include "../Export.h"
#include "../IModelConnection.h"

#include <dqBase/DqId.h>
#include <dqGeom/Range3d.h>

#include <optional>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_APP_NAMESPACE
#define BEGIN_DQ_APP_NAMESPACE namespace dqApp {
#define END_DQ_APP_NAMESPACE }
#endif

BEGIN_DQ_APP_NAMESPACE

// DumpIModelConnection — 从 imodel.json 构造的"打开的连接"（isClosed=false）。
// 数据面 = imodel.json 五段：connection（name/guid/projectExtents/rpcProps）、
// views（list/defaultViewId/defaultViewState）、models。
class DQ_APP_EXPORT DumpIModelConnection : public IModelConnection {
public:
    // imodel.json models[] 条目（ModelProps 消费面子集——parentModel/
    // modeledElement/jsonProperties 登记未消费）。
    struct ModelInfo {
        dqBase::DqId id;                // ← ModelProps.id
        std::string name;               // ← ModelProps.name
        std::string classFullName;      // ← ModelProps.classFullName
    };

    // views.list[] 条目（采集器按 DTA getViewList 返回落盘 + isPrivate
    // 扩展——ViewPicker.ts:10-12 的 ViewSpec 形态）。
    struct ViewInfo {
        dqBase::DqId id;
        std::string name;
        std::string className;  // ← ViewSpec.class（C++ 关键字避让，§3.4）
        bool isPrivate = false;
    };

    ~DumpIModelConnection() override = default;

    // 打开 <imodelJsonPath>（imodel.json 全路径）。缺失/坏 JSON/必需段缺失
    // → 无效 RefPtr（打开失败语义——§3.4 error-return 适配）。
    // 成功：连接 props 应用 + Views::RpcHooks 安装（三查询的回放实现）+
    // OnOpen 事件（IModelConnection.ts:796/:845/:863 同款）。
    static dqBase::RefPtr<DumpIModelConnection> open(std::string const& imodelJsonPath);

    // 打开的连接（参考 SnapshotConnection 的 isClosed=false 语义 :824-826）。
    bool IsClosed() const override { return m_closed; }

    // --- 采集面访问器（断言/装配面） ---
    std::string const& getGuid() const noexcept { return m_guid; }
    std::vector<ViewInfo> const& getCapturedViews() const noexcept { return m_capturedViews; }
    dqBase::DqId getCapturedDefaultViewId() const noexcept { return m_defaultViewId; }
    std::vector<ModelInfo> const& getModels() const noexcept { return m_models; }
    // 默认视图的 ViewStateProps（getViewStateData 回放源——装配面直读用）。
    std::optional<ViewStateProps> const& getDefaultViewState() const noexcept
    {
        return m_defaultViewState;
    }

private:
    DumpIModelConnection() = default;

    std::string m_guid;
    std::vector<ViewInfo> m_capturedViews;  // views.list（基类 m_views 是 Views 查询面——采集清单另名）
    dqBase::DqId m_defaultViewId;
    std::optional<ViewStateProps> m_defaultViewState;
    std::vector<ModelInfo> m_models;
};

END_DQ_APP_NAMESPACE
