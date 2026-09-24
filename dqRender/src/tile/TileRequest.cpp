// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TileRequest implementation
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts
#include "dqRender/tile/TileRequest.h"

BEGIN_DQ_RENDER_NAMESPACE

TileRequest::TileRequest(Tile& tile, TileRequestChannel& channel)
    : m_tile(tile)
    , m_channel(channel)
{
}

TileRequest::~TileRequest() = default;

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
