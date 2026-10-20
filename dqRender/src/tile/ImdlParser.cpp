// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl parse seam implementation (P-1b P-b2)
// Ported from: itwinjs-core core/frontend/src/internal/tile/ImdlParser.ts
//              (acquireImdlParser's two faces :39-58) and the parse legs of
//              core/frontend/src/tile/ImdlReader.ts readImdlContent (:62-84)
#include "ImdlParser.h"

#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/TileFormat.h"

#include "dqRender/WorkerProxy.h"

#include <algorithm>
#include <cassert>
#include <utility>

BEGIN_DQ_RENDER_NAMESPACE

ImdlParsedContent parseImdlContent(uint8_t const* data, size_t dataSize)
{
    // The WORKER HALF of the content read — pure CPU, zero GL (the graphics
    // half is ImdlTile::createContentGraphics, main-thread only).
    // Body = the former ImdlTile::readContent front half, moved verbatim:
    // Ported from: readImdlContent's parse legs (ImdlReader.ts:62-84 —
    // decodeTileContentDescription TileMetadata.ts:880-940 + parseDocument
    // ParseImdlDocument.ts:1269-1327).
    ImdlParsedContent out;
    if (!data || dataSize < 4)
        return out;
    if (DetectTileFormat(data, dataSize) != TileFormat::IModel)
        return out;

    ImdlByteStream stream(data, dataSize);
    auto const header = ImdlHeader::readFrom(stream);
    if (!header.isValid())
        return out;

    // Feature table: read the 12-byte header + the packed words (3×u32/feature
    // + 2×u32/subcategory tail) instead of skipping them — the stream ends up
    // at the same position as the reference's skip-then-seek-back.
    // Ported from: ParseImdlDocument.ts:1278-1284 (ftStartPos +
    // FeatureTableHeader.readFrom + seek to ftStartPos + length) +
    // FeatureTableHeader.ts layout (length includes the 12-byte header).
    ImdlFeatureTableHeader ftHeader;
    if (!ImdlFeatureTableHeader::readFrom(stream, ftHeader))
        return out;  // InvalidFeatureTable (ParseImdlDocument.ts:1281-1282)
    std::vector<uint32_t> featureWords;
    size_t const ftBodyBytes = ftHeader.length >= ImdlFeatureTableHeader::sizeInBytes
        ? ftHeader.length - ImdlFeatureTableHeader::sizeInBytes : 0;
    if (ftBodyBytes > 0) {
        featureWords.resize(ftBodyBytes / sizeof(uint32_t));
        stream.readBytes(featureWords.data(), featureWords.size() * sizeof(uint32_t));
        if (stream.isPastTheEnd())
            return out;
    }

    // Header-only description (the caller consumed the feature table — the
    // leaf heuristic derives purely from the imdl header).
    auto const desc = decodeImdlContentDescriptionHeaderOnly(header);
    if (!desc.has_value())
        return out;
    out.description = *desc;
    out.imdlFlags = static_cast<uint32_t>(header.flags);

    auto doc = parseImdlDocument(stream, &ftHeader, &featureWords);
    if (!doc.has_value())
        return out;

    out.document = std::move(*doc);
    out.valid = true;
    return out;
}

// ---------------------------------------------------------------------------
// ImdlParser faces (acquireImdlParser, ImdlParser.ts:37-68)
// ---------------------------------------------------------------------------

namespace {

// The noWorker face — the main-thread synchronous parser.
// Ported from: ImdlParser.ts:39-47 (`parse: async (options) =>
// parseImdlDocument(...)` on the main thread; `release: () => undefined`).
class SyncImdlParser final : public ImdlParser {
public:
    void parse(std::vector<uint8_t>&& data, ParseCallback&& onParsed) override
    {
        ImdlParsedContent parsed = parseImdlContent(data.data(), data.size());
        if (onParsed)
            onParsed(std::move(parsed));
    }
    void release() override {}
};

// The defaultParser face — the WorkerProxy-backed singleton (E2).
// Ported from: ImdlParser.ts:49-58 (`worker.parse(options,
// [options.data.buffer])` — the transferable data hand; `release: () =>
// undefined` — the singleton is process-persistent).
class WorkerImdlParser final : public ImdlParser {
public:
    void parse(std::vector<uint8_t>&& data, ParseCallback&& onParsed) override
    {
        // The worker leg touches NOTHING but the shared slot — the parse
        // result crosses back to the main thread through it, so a completed
        // parse never references the (possibly dead) requester.
        auto slot = std::make_shared<std::optional<ImdlParsedContent>>();
        WorkerProxy::instance().submit(
            [slot, data = std::move(data)]() mutable {
                *slot = parseImdlContent(data.data(), data.size());
            },
            [slot, onParsed = std::move(onParsed)]() {
                // Main thread (WorkerProxy::drainCompleted) — the reference's
                // promise resolution face (WorkerProxy.ts:108-121).
                if (onParsed && slot->has_value())
                    onParsed(std::move(**slot));
            });
    }
    void release() override {}
};

}  // namespace

ImdlParser* acquireImdlParser(AcquireImdlParserArgs const& args)
{
    // Ported from: acquireImdlParser (ImdlParser.ts:37-68) — the noWorker
    // branch (:39-47) and the defaultParser singleton (:49-58). The timeline
    // branch (:61-67) is not ported — no DanQing imdl-parse timeline
    // consumer (TODO, anchored in ImdlParser.h).
    static SyncImdlParser sSyncParser;
    static WorkerImdlParser sDefaultParser;  // E2: defaultParser 单例
    if (args.noWorker)
        return &sSyncParser;
    return &sDefaultParser;
}

// ---------------------------------------------------------------------------
// PendingImdlParses (header note — the C++ stand-in for the promise
// continuations pending inside readImdlContent)
// ---------------------------------------------------------------------------

PendingImdlParses::PendingImdlParses() = default;

PendingImdlParses::~PendingImdlParses() = default;

void PendingImdlParses::push(
    Tile* tile, std::shared_ptr<std::optional<ImdlParsedContent>> slot,
    Finisher finisher)
{
    m_entries.push_back(Entry{tile, std::move(slot), std::move(finisher)});
}

void PendingImdlParses::scrub(Tile const* tile)
{
    // TD-22 family: the dying tile drops its own pending entries (~Tile →
    // TileAdmin::onTileContentDisposed), so the drain never touches a dead
    // Tile*. The finisher closures (the only tile referents) die with the
    // entries.
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [tile](Entry const& entry) {
                                       return entry.tile == tile;
                                   }),
                    m_entries.end());
}

void PendingImdlParses::drain()
{
    for (size_t i = 0; i < m_entries.size();) {
        Entry& entry = m_entries[i];
        if (!entry.slot->has_value()) {
            ++i;  // parse still running — stays queued for a later frame
            continue;
        }
        Finisher finisher = std::move(entry.finisher);
        ImdlParsedContent parsed = std::move(**entry.slot);
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
        if (finisher)
            finisher(std::move(parsed));
    }
}

END_DQ_RENDER_NAMESPACE
