// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl parse seam (P-1b P-b2): the parser interface +
// acquisition + the worker/sync parse split
// Ported from: itwinjs-core core/frontend/src/internal/tile/ImdlParser.ts
//              (ImdlParser :23-26, AcquireImdlParserArgs :28-33,
//              acquireImdlParser :37-68, defaultParser :70) and the parse legs
//              of core/frontend/src/tile/ImdlReader.ts readImdlContent
//              (:62-84 — description decode + parseDocument via the parser).
#pragma once

#include "dqRender/tile/ImdlDocument.h"  // ImdlDocument/ImdlContentDescription/ImdlParsedContent

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class Tile;

// The pure-CPU imdl parse: header → feature table → content description →
// glTF-section document. This is the WORKER HALF of the content read (zero
// GL — the graphics half is ImdlTile::createContentGraphics, main-thread
// only). Ported from: readImdlContent's parse legs — the description decode
// (ImdlReader.ts:62-72 → TileMetadata.ts:880-940) + the document parse
// (ImdlReader.ts:74-84 → ParseImdlDocument.ts:1269-1327 via the acquired
// parser). EQUIVALENCE: 解析体 = 现行 ImdlTile::readContent 前半逐段移入；
// 语义零变化（同函数体的搬运——验证法 = ImdlParserTest.WorkerParseEquals
// SyncParseByteForByte + 既有 TileTreeRender/RpcDumpRender 像素锁回归）。
ImdlParsedContent parseImdlContent(uint8_t const* data, size_t dataSize);

// ---------------------------------------------------------------------------
// ImdlParser — parses binary iMdl content into a document, off the main
// thread when a worker is available.
// Ported from: ImdlParser (ImdlParser.ts:23-26 — `parse(options) →
// Promise<Document|ParseError>; release()`).
// EQUIVALENCE E1: 参考的 Promise 返回 → 完成回调承载——noWorker 解析器在
// parse 调用内同步跑回调（主线程）；worker 解析器经 WorkerProxy 的完成队列
// 在主线程 drain 时跑回调（promise 续体恒主线程的语义合同）。
// data 形参按值搬移 = 参考 transferable（`[options.data.buffer]`，
// ImdlParser.ts:53/:86——零拷贝移交；EQUIVALENCE：TileAdmin 交付点从轮询
// fetcher 的 const& 完成项拷贝一次后即 move——拷贝成本 O(bytes)，远小于解析，
// 登记于 TileAdmin::deliverTileContent）。
// release()：defaultParser 单例的 release 为 no-op（参考原样，
// ImdlParser.ts:54）。timeline 分支（ParserWithTimeline，ImdlParser.ts:61-97）
// 未移植——DanQing 无 imdl-parse timeline 消费面（TODO，锚 ImdlParser.ts:61）。
// ---------------------------------------------------------------------------
class ImdlParser {
public:
    using ParseCallback = std::function<void(ImdlParsedContent&&)>;

    virtual ~ImdlParser() = default;

    // Ported from: ImdlParser.parse (ImdlParser.ts:24) — promise → callback.
    // The callback runs on the main thread: inline for the noWorker parser,
    // at WorkerProxy::drainCompleted for the worker parser.
    virtual void parse(std::vector<uint8_t>&& data, ParseCallback&& onParsed) = 0;

    // Ported from: ImdlParser.release (ImdlParser.ts:25).
    virtual void release() = 0;
};

// Ported from: AcquireImdlParserArgs (ImdlParser.ts:28-33 — the timeline
// field is not ported; TODO with the imdl-parse timeline consumption face).
struct AcquireImdlParserArgs {
    bool noWorker = false;
};

// Ported from: acquireImdlParser (ImdlParser.ts:37-68):
//   - args.noWorker → the main-thread synchronous parser (:39-47);
//   - otherwise → the defaultParser singleton backed by the WorkerProxy
//     worker (:49-58 — E2).
// Returns a non-owning pointer (the defaultParser singleton is process-
// persistent; release() is a no-op on it — the reference's shape).
ImdlParser* acquireImdlParser(AcquireImdlParserArgs const& args);

// ---------------------------------------------------------------------------
// PendingImdlParses — the main-thread registry of in-flight imdl parses whose
// bytes were handed to the worker. The completion leg (finisher) touches the
// tile, so entries are scrubbed the moment the tile dies
// (TileAdmin::onTileContentDisposed — the TD-22 liveness-adaptation family:
// the reference's JS closure keeps the tile alive; C++ must drop the entry).
// Drained by TileAdmin::process at the same pre-dispatch point as the polling
// fetcher's completion drain — the M-F(1) ordering invariant holds (a parse
// submitted during frame N's dispatch is delivered at the earliest at frame
// N+1's drain, i.e. >=1 selection pass after dispatch).
// Authored: no reference counterpart (the reference mediates through the
// TileRequest promise closure); the DanQing equivalent of the pending
// continuations inside readImdlContent.
// ---------------------------------------------------------------------------
class PendingImdlParses {
public:
    using Finisher = std::function<void(ImdlParsedContent&&)>;

    PendingImdlParses();
    ~PendingImdlParses();

    PendingImdlParses(PendingImdlParses const&) = delete;
    PendingImdlParses& operator=(PendingImdlParses const&) = delete;

    // Register an entry: the worker writes the shared slot (the parse
    // callback touches nothing but the slot), the finisher delivers on the
    // main thread at drain. Both movable — no tile capture in the worker leg.
    void push(Tile* tile, std::shared_ptr<std::optional<ImdlParsedContent>> slot,
              Finisher finisher);

    // Drop every entry for a dying tile (TileAdmin::onTileContentDisposed).
    void scrub(Tile const* tile);

    // Deliver every finished parse (main thread). Unfinished entries stay
    // queued for a later frame.
    void drain();

    size_t size() const { return m_entries.size(); }

private:
    struct Entry {
        Tile* tile;
        std::shared_ptr<std::optional<ImdlParsedContent>> slot;
        Finisher finisher;
    };
    std::vector<Entry> m_entries;
};

END_DQ_RENDER_NAMESPACE
