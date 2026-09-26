// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileRequest implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts
#include "dqRender/tile/TileRequest.h"
#include "dqRender/tile/TileAdmin.h"  // TileUser 完整类型（getTileUserId）

BEGIN_DQ_RENDER_NAMESPACE

TileRequest::TileRequest(Tile& tile, TileRequestChannel& channel, TileUser& user)
    : m_tile(tile)
    , m_channel(channel)
{
    // Ported from: TileRequest.ts:34-39 — the constructor seeds the user set
    // with the requesting user (`this.users =
    // IModelApp.tileAdmin.getTileUserSetForRequest(user)`; the singleton-set
    // role of the UniqueTileUserSets pool, TileUserSet.ts:77-79).
    m_users.push_back(&user);
}

TileRequest::~TileRequest() = default;

void TileRequest::addUser(TileUser& user)
{
    // Ported from: TileRequest.addUser (TileRequest.ts:72-74) — the set union
    // via getTileUserSetForRequest(user, this.users); the reference's
    // SortedArray insert dedups by tileUserId (TileUserSet.ts:17/:37), so a
    // user already awaiting this request is a no-op.
    uint32_t const id = user.getTileUserId();
    for (TileUser* existing : m_users) {
        if (existing->getTileUserId() == id)
            return;
    }
    m_users.push_back(&user);
}

void TileRequest::removeUser(TileUser& user)
{
    // Ported from: TileUserSet.remove (TileUserSet.ts:38 — SortedArray
    // _remove, matched by tileUserId); hosted on TileRequest because DanQing
    // keeps the user set on the request instead of a shared pool (see the
    // EQUIVALENCE note on getUsers).
    uint32_t const id = user.getTileUserId();
    for (auto it = m_users.begin(); it != m_users.end(); ++it) {
        if ((*it)->getTileUserId() == id) {
            m_users.erase(it);
            return;
        }
    }
}

void TileRequest::dispatch()
{
    if (m_state == State::Queued) {
        m_state = State::Dispatched;
    }
}

void TileRequest::startLoading()
{
    if (m_state == State::Dispatched) {
        m_state = State::Loading;
    }
}

void TileRequest::complete()
{
    m_state = State::Completed;
}

void TileRequest::fail()
{
    m_state = State::Failed;
}

END_DQ_RENDER_NAMESPACE
