// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Qt-free abstract tile fetcher interface
// Ported from: itwinjs-core core/frontend/src/tile/TileRequestChannel.ts
//              (abstract network fetch contract)
//
// §7.2: This header is in PublicAPI and must contain ZERO Qt types.
// Concrete Qt implementation lives in src/tile/TileRequestFetcher.h.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;

// ---------------------------------------------------------------------------
// ITileFetcher — abstract interface for async tile content fetching.
// Ported from: itwinjs-core TileRequestFetcher (abstract fetch contract)
//
// Concrete implementations (e.g., Qt QNetworkAccessManager-based) live in
// src/ and may use platform-specific types. This interface defines the
// contract using only std types so PublicAPI remains Qt-free (§7.2).
// ---------------------------------------------------------------------------
class ITileFetcher {
public:
    virtual ~ITileFetcher() = default;

    /// Initiate an async HTTP fetch for tile content.
    /// @param url  The resolved content URL (std::string, not QString).
    /// @param tile Reference to the tile being fetched.
    /// @param onComplete Called on success with the downloaded bytes.
    /// @param onError    Called on failure with an error description.
    virtual void fetch(std::string const& url, Tile& tile,
                       std::function<void(Tile&, std::vector<uint8_t> const&)> onComplete,
                       std::function<void(Tile&, std::string const&)> onError) = 0;

    /// Process completed fetches and deliver data to tiles.
    virtual void processCompleted() = 0;

    /// Get the number of currently active (in-flight) requests.
    virtual uint32_t getActiveCount() const noexcept = 0;

    /// Cancel all in-flight requests.
    virtual void cancelAll() = 0;
};

END_DQ_RENDER_NAMESPACE
