// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — IModelConnection base class
// Ported from: itwinjs-core core/frontend/src/IModelConnection.ts
//
// IModelConnection is the abstract base class for all iModel connections.
// BlankConnection extends this for no-backend scenarios.
// Future: SnapshotConnection, BriefcaseConnection for file-based iModels.
#pragma once

#include "Export.h"

#include <memory>
#include "EcefLocation.h"
#include "SelectionSet.h"

#include <dqBase/RefCounted.h>
#include <dqBase/DqEvent.h>
#include <dqBase/DqId.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>

#include <string>
#include <vector>

namespace dqApp {

class ViewState;

// ---------------------------------------------------------------------------
// IModelConnectionProps — properties for constructing an IModelConnection
// Ported from: itwinjs-core IModelConnectionProps
// ---------------------------------------------------------------------------
struct IModelConnectionProps {
    std::string name;
    std::string rootSubject;
    dqGeom::Range3d projectExtents;
    dqGeom::Point3d globalOrigin = {0, 0, 0};
    EcefLocation ecefLocation;
    std::string key;
    std::string iTwinId;
};

// ---------------------------------------------------------------------------
// IModelConnection — abstract base for all iModel connections
// Ported from: itwinjs-core IModelConnection.ts (lines 64-268)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT IModelConnection : public dqBase::RefCounted<IModelConnection> {
public:
    // The tile tree registry (← IModelConnection.tiles, Tiles.ts:77 —
    // owner caching per (supplier, id)).
    class Tiles& GetTiles() noexcept { return *m_tiles; }
    class Tiles const& GetTiles() const noexcept { return *m_tiles; }
    // --- IModelConnection.ViewSpec (← IModelConnection.ts:890-898) ---
    // The id/name/class of a ViewDefinition, returned by Views::getViewList.
    // TS `class` field → `className` (C++ keyword collision; §3.4 minimal-suffix).
    struct ViewSpec {
        dqBase::DqId id;        // ← Id64String (DqId() == Id64.invalid)
        std::string name;       // ← ViewSpec.name
        std::string className;  // ← ViewSpec.class (full class name)
    };

    // --- IModelConnection.Views (← IModelConnection.ts:1461-1566) ---
    // The collection of ViewStates for this connection. Only the closed-connection
    // short-circuits exercised by the blank-connection path are implemented; real
    // backend queries arrive with SnapshotConnection (TODO, see .cpp).
    class DQ_APP_EXPORT Views {
    public:
        // ← ViewQueryParams subset (only wantPrivate is passed by the reference's
        //    ViewList caller, ViewPicker.ts:73/79).
        struct QueryParams {
            bool wantPrivate = false;
        };

        explicit Views(IModelConnection& iModel) : m_iModel(iModel) {}

        // ← IModelConnection.Views.getViewList (IModelConnection.ts:1518-1526);
        //    empty for a closed connection (queryProps short-circuit, :1490-1491).
        std::vector<ViewSpec> getViewList(QueryParams const& params) const;
        // ← IModelConnection.Views.queryDefaultViewId (:1535-1538);
        //    Id64.invalid when the connection is not open.
        dqBase::DqId queryDefaultViewId() const;
        // ← IModelConnection.Views.load (:1541-1551). The reference's backend RPC
        //    failure (blank connection) is reported as an invalid RefPtr — the §3.4
        //    throw→error-return adaptation; ViewList::getView treats it exactly like
        //    the reference's catch branch (ViewPicker.ts:39-44).
        dqBase::RefPtr<ViewState> load(dqBase::DqId viewDefinitionId) const;

    private:
        IModelConnection& m_iModel;  // ← _iModel
    };

    virtual ~IModelConnection();

    // --- Properties (← IModelConnection members) ---
    std::string const& GetName() const { return m_name; }
    std::string const& GetRootSubject() const { return m_rootSubject; }
    dqGeom::Range3d const& GetProjectExtents() const { return m_projectExtents; }
    dqGeom::Point3d const& GetGlobalOrigin() const { return m_globalOrigin; }
    EcefLocation const& GetEcefLocation() const { return m_ecefLocation; }
    std::string const& GetKey() const { return m_key; }
    std::string const& GetITwinId() const { return m_iTwinId; }

    // --- Type guards (← IModelConnection type guards) ---
    virtual bool IsBlankConnection() const { return false; }
    virtual bool IsSnapshotConnection() const { return false; }
    virtual bool IsBriefcaseConnection() const { return false; }

    bool IsBlank() const { return IsBlankConnection(); }
    // ← IModelConnection.ts:139 abstract isClosed. Base returns the stored flag
    //    (SnapshotConnection pattern, :824-826); BlankConnection overrides to return
    //    true always (:777-781).
    virtual bool IsClosed() const { return m_closed; }
    // ← IModelConnection.ts:133 isOpen = !isClosed (virtual dispatch).
    bool IsOpen() const { return !IsClosed(); }

    // --- Lifecycle (← IModelConnection.close) ---
    // Base = the guarded SnapshotConnection.close pattern (IModelConnection.ts:872-884).
    // BlankConnection overrides with its unguarded close (:804-806).
    virtual void Close();

    // --- Views (← IModelConnection.views, IModelConnection.ts:71) ---
    Views& GetViews() { return m_views; }
    Views const& GetViews() const { return m_views; }

    // --- Selection (← IModelConnection.hilited) ---
    SelectionSet& GetSelectionSet() { return m_selectionSet; }
    SelectionSet const& GetSelectionSet() const { return m_selectionSet; }
    HiliteSet& GetHiliteSet() { return m_hiliteSet; }
    HiliteSet const& GetHiliteSet() const { return m_hiliteSet; }

    // --- Static events (← IModelConnection.onOpen/onClose) ---
    static dqBase::DqEvent<IModelConnection*> OnOpen;
    static dqBase::DqEvent<IModelConnection*> OnClose;

    // --- Instance events (← IModelConnection.onClose instance) ---
    dqBase::DqEvent<IModelConnection*> OnCloseInstance;

protected:
    IModelConnection();

    explicit IModelConnection(IModelConnectionProps const& props);

    std::unique_ptr<class Tiles> m_tiles;  // ← IModelConnection.tiles

    /// Called before close. override to add cleanup.
    virtual void BeforeClose();

    std::string m_name;
    std::string m_rootSubject;
    dqGeom::Range3d m_projectExtents;
    dqGeom::Point3d m_globalOrigin = {0, 0, 0};
    EcefLocation m_ecefLocation;
    std::string m_key;
    std::string m_iTwinId;
    bool m_closed = false;
    SelectionSet m_selectionSet;
    HiliteSet m_hiliteSet;
    // ← IModelConnection.views (IModelConnection.ts:71/224) — constructed with this
    //    connection, as in the reference ctor. The other frontend-side object
    //    collections (models/elements/codeSpecs/categories/tiles/geoServices/
    //    transientIds, IModelConnection.ts:218-236) are TODO until a backend
    //    connection lands; nothing in the blank-connection path reaches them.
    Views m_views;
};

}  // namespace dqApp
