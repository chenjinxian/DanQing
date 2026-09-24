// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Blank connection (no-backend iModel)
// Ported from: itwinjs-core core/frontend/src/IModelConnection.ts BlankConnection
#pragma once

#include "IModelConnection.h"

#include <dqCommon/Cartographic.h>
#include <dqGeom/Range3d.h>

#include <string>

namespace dqApp {

// ---------------------------------------------------------------------------
// BlankConnectionProps — properties for creating a BlankConnection
// Ported from: itwinjs-core BlankConnectionProps (lines 47-58)
// ---------------------------------------------------------------------------
struct BlankConnectionProps {
    std::string name = "blank";
    /// Spatial location — either Cartographic (lon/lat/height) or EcefLocation.
    EcefLocation location;
    /// Volume of interest in meters (min/max corners).
    dqGeom::Range3d extents;
    /// Offset applied to all spatial coordinates.
    dqGeom::Point3d globalOrigin = {0, 0, 0};
    /// Optional iTwin identifier.
    std::string iTwinId;
    /// Whether location is a Cartographic (needs conversion to EcefLocation).
    bool locationIsCartographic = false;
    /// Cartographic location (used when locationIsCartographic is true).
    dqCommon::Cartographic cartographicLocation;
};

// ---------------------------------------------------------------------------
// BlankConnection — a connection with no backend data
// Ported from: itwinjs-core IModelConnection.ts BlankConnection (lines 649-695)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT BlankConnection : public IModelConnection {
public:
    // Factory (← BlankConnection.create)
    static dqBase::RefPtr<BlankConnection> create(BlankConnectionProps const& props);

    // Type guard (← BlankConnection overrides)
    bool IsBlankConnection() const override { return true; }

    // Lifecycle (← BlankConnection).
    // isClosed ALWAYS returns true (IModelConnection.ts:777-781): "A BlankConnection
    // is always considered closed because it does not have a specific backend nor
    // associated iModel." — this is what short-circuits every RPC/ECSQL path
    // (e.g. Views::getViewList → queryProps, IModelConnection.ts:1490-1491).
    // isOpen (= !isClosed, IModelConnection.ts:133) is therefore always false.
    bool IsClosed() const override { return true; }

    // ← BlankConnection.close (IModelConnection.ts:804-806): calls beforeClose()
    //    UNCONDITIONALLY — there is no isClosed guard (unlike SnapshotConnection.close,
    //    :872-884), because isClosed is always true and a guard would make close()
    //    unreachable. Each call re-raises the close events.
    void Close() override;

private:
    BlankConnection() = default;
};

}  // namespace dqApp
