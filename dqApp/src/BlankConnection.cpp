// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — BlankConnection implementation
// Ported from: itwinjs-core core/frontend/src/IModelConnection.ts BlankConnection
#include "dqApp/BlankConnection.h"

namespace dqApp {

dqBase::RefPtr<BlankConnection> BlankConnection::create(BlankConnectionProps const& props)
{
    // ← BlankConnection.create(props)
    IModelConnectionProps imodelProps;
    imodelProps.name = props.name;
    imodelProps.rootSubject = props.name;
    imodelProps.projectExtents = props.extents;
    imodelProps.globalOrigin = props.globalOrigin;
    imodelProps.iTwinId = props.iTwinId;

    // Convert Cartographic to EcefLocation if needed
    // ← EcefLocation.createFromCartographicOrigin(props.location)
    if (props.locationIsCartographic) {
        imodelProps.ecefLocation = EcefLocation::CreateFromCartographicOrigin(
            props.cartographicLocation
        );
    } else {
        imodelProps.ecefLocation = props.location;
    }

    dqBase::RefPtr<BlankConnection> connection(new BlankConnection());
    connection->m_name = imodelProps.name;
    connection->m_rootSubject = imodelProps.rootSubject;
    connection->m_projectExtents = imodelProps.projectExtents;
    connection->m_globalOrigin = imodelProps.globalOrigin;
    connection->m_ecefLocation = imodelProps.ecefLocation;
    connection->m_key = imodelProps.key;
    connection->m_iTwinId = imodelProps.iTwinId;

    // ← IModelConnection.onOpen.raiseEvent(connection)
    OnOpen.Raise(connection.Get());

    return connection;
}

// Ported from: itwinjs-core BlankConnection.close (IModelConnection.ts:804-806):
//   public async close(): Promise<void> { this.beforeClose(); }
// Unguarded — unlike SnapshotConnection.close (:872-884), BlankConnection has no
// isClosed guard (isClosed is always true; a guard would make close() unreachable).
// Each call re-raises the close events, matching the reference.
// m_closed is set afterwards purely so the C++ RAII destructor (~IModelConnection's
// `if (!m_closed) Close()`) does not re-raise at teardown — the GC'd reference has
// no destructor (§3.4 C++ adaptation; BlankConnection's IsClosed() override ignores
// the flag, so this changes no observable state).
void BlankConnection::Close()
{
    BeforeClose();
    m_closed = true;
}

}  // namespace dqApp
