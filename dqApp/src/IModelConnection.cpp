// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — IModelConnection implementation
// Ported from: itwinjs-core core/frontend/src/IModelConnection.ts
#include "dqApp/IModelConnection.h"

#include "dqApp/tile/Tiles.h"

#include "dqApp/ViewState.h"  // complete ViewState for RefPtr<ViewState> destruction in Views::load

namespace dqApp {

// Static event instances
dqBase::DqEvent<IModelConnection*> IModelConnection::OnOpen;
dqBase::DqEvent<IModelConnection*> IModelConnection::OnClose;

IModelConnection::IModelConnection()
    : m_views(*this)
    , m_tiles(std::make_unique<Tiles>(*this))  // ← tiles = new Tiles(this), Tiles.ts:86
{
}


IModelConnection::IModelConnection(IModelConnectionProps const& props)
    : m_name(props.name)
    , m_rootSubject(props.rootSubject)
    , m_projectExtents(props.projectExtents)
    , m_globalOrigin(props.globalOrigin)
    , m_ecefLocation(props.ecefLocation)
    , m_key(props.key)
    , m_iTwinId(props.iTwinId)
    , m_views(*this)  // ← IModelConnection.ts:224 this.views = new IModelConnection.Views(this)
    , m_tiles(std::make_unique<Tiles>(*this))  // ← tiles = new Tiles(this), Tiles.ts:86
{
}

// ---------------------------------------------------------------------------
// IModelConnection.Views (IModelConnection.ts:1461-1566)
// ---------------------------------------------------------------------------

std::vector<IModelConnection::ViewSpec> IModelConnection::Views::getViewList(QueryParams const& params) const
{
    // Ported from: itwinjs-core IModelConnection.Views.getViewList
    //              (IModelConnection.ts:1518-1526) → queryProps (:1488-1505).
    // queryProps: `if (iModel.isClosed) return [];` (:1490-1491). A BlankConnection
    // is always closed, so no RPC/ECSQL is ever issued and the list is empty — this
    // is the short-circuit the display-test-app's ViewList.populate relies on.
    if (m_iModel.IsClosed())
        return {};
    // IModelReadRpcInterface.queryElementProps 回放缝（M-H Task 3——dump 宿主
    // 注入；未安装 = 无后端的空清单语义，与现状一致）。
    // TODO: 真后端分支（queryElementProps RPC + ViewSpec 映射 :1520-1524）
    //       随 SnapshotConnection 落地；回放缝的宿主侧已完成同一映射。
    if (m_rpcHooks.has_value() && m_rpcHooks->getViewList)
        return m_rpcHooks->getViewList(params.wantPrivate);
    return {};
}

dqBase::DqId IModelConnection::Views::queryDefaultViewId() const
{
    // Ported from: itwinjs-core IModelConnection.Views.queryDefaultViewId
    //              (IModelConnection.ts:1535-1538):
    //   iModel.isOpen ? RPC getDefaultViewId : Id64.invalid
    // A BlankConnection is never open (isOpen = !isClosed, :133) → invalid.
    if (!m_iModel.IsOpen())
        return dqBase::DqId();
    // getDefaultViewId 回放缝（M-H Task 3）；未安装 = invalid（现状语义）。
    if (m_rpcHooks.has_value() && m_rpcHooks->getDefaultViewId)
        return m_rpcHooks->getDefaultViewId();
    return dqBase::DqId();
}

dqBase::RefPtr<ViewState> IModelConnection::Views::load(dqBase::DqId viewDefinitionId) const
{
    // Ported from: itwinjs-core IModelConnection.Views.load (IModelConnection.ts:
    //              1541-1551). The reference goes straight to the getViewStateData RPC
    //              with no isClosed guard; for a blank connection that RPC fails and
    //              ViewPicker.ts:39-44 catches the rejection. DanQing has no RPC layer,
    //              so load reports failure as an invalid RefPtr (§3.4 throw →
    //              error-return adaptation) — the ViewList::getView caller treats it
    //              exactly like the reference's catch branch.
    // getViewStateData 回放缝（M-H Task 3——dump 宿主注入；nullopt = RPC 失败
    // 语义（视图未采集/不存在）→ 无效 RefPtr）。
    if (m_rpcHooks.has_value() && m_rpcHooks->getViewStateData) {
        auto viewProps = m_rpcHooks->getViewStateData(viewDefinitionId);
        if (!viewProps.has_value())
            return nullptr;
        // :1549 — convertViewStatePropsToViewState(viewProps)。
        return convertViewStatePropsToViewState(*viewProps);
    }
    return nullptr;
}

dqBase::RefPtr<ViewState> IModelConnection::Views::convertViewStatePropsToViewState(
    ViewStateProps const& viewProps) const
{
    // Ported from: itwinjs-core IModelConnection.Views.convertViewStatePropsToViewState
    //              (IModelConnection.ts:1553-1565)。
    // :1555-1559 — className 判定（参考经 findClassFor 查注册类；DanQing 唯一
    // 移植的具体 ViewState 是 SpatialViewState——"BisCore:SpatialViewDefinition"
    // = schemaName:className（EntityState.ts:74 + SpatialViewState.ts:36）。
    // 其他类（OrthographicViewDefinition/2d 族）未移植 → 无效 RefPtr（参考的
    // WrongClass 分支 :1558-1559 的 §3.4 error-return 适配——登记）。
    std::string const& className = viewProps.viewDefinitionProps.classFullName;
    if (className != "BisCore:SpatialViewDefinition")
        return nullptr;

    // :1561 — expectDefined(ctor.createFromProps(viewProps, this._iModel))。
    auto viewState = SpatialViewState::CreateFromProps(viewProps, &m_iModel);
    if (!viewState.IsValid())
        return nullptr;

    // :1562 — await viewState.load()（loads models for ModelSelector）。
    // DanQing 的 ViewState::Load() 是 virtual no-op（blank 短路 :404-406 的
    // 唯一移植面）；dump 连接的 hydrate 链（hydrateViewState RPC +
    // models.updateLoadedWithModelProps + subcategories.load——:410-418 +
    // SpatialViewState.ts:181-196）登记未消费（DanQing 无 ModelState 容器——
    // 打开链的树装载经 modelSelector.models 集合驱动，不依赖 loaded models）。
    viewState->Load();
    return viewState;
}

IModelConnection::~IModelConnection()
{
    if (!m_closed) {
        Close();
    }
}

void IModelConnection::Close()
{
    // Ported from: itwinjs-core SnapshotConnection.close (IModelConnection.ts:872-884)
    //              — the isClosed-guarded pattern for backend connections. The
    //              reference declares close() abstract on IModelConnection
    //              (:249); DanQing's base carries the guarded implementation for future
    //              file-based connections. BlankConnection overrides it with the
    //              unguarded pattern (:804-806).
    if (m_closed)
        return;
    BeforeClose();
    m_closed = true;
}

void IModelConnection::BeforeClose()
{
    // ← IModelConnection.beforeClose
    OnCloseInstance.Raise(this);  // event for this connection
    OnClose.Raise(this);          // event for all connections
}

}  // namespace dqApp
