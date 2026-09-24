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
    // TODO: the non-closed branch (IModelReadRpcInterface.queryElementProps + ViewSpec
    //       mapping) requires a backend connection — deferred with SnapshotConnection.
    if (m_iModel.IsClosed())
        return {};
    (void)params;
    return {};
}

dqBase::DqId IModelConnection::Views::queryDefaultViewId() const
{
    // Ported from: itwinjs-core IModelConnection.Views.queryDefaultViewId
    //              (IModelConnection.ts:1535-1538):
    //   iModel.isOpen ? RPC getDefaultViewId : Id64.invalid
    // A BlankConnection is never open (isOpen = !isClosed, :133) → invalid.
    // TODO: the RPC branch is deferred with SnapshotConnection.
    if (!m_iModel.IsOpen())
        return dqBase::DqId();
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
    // TODO: real view loading arrives with SnapshotConnection (viewState load chain).
    (void)viewDefinitionId;
    return nullptr;
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
