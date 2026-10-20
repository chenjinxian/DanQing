// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl parse seam tests (P-1b P-b2)
//
// Ported from: itwinjs-core core/frontend/src/internal/tile/ImdlParser.ts
//   (acquireImdlParser's noWorker branch :39-47 and the defaultParser worker
//   singleton :49-58; parse's transferable data hand :53/:86) and the parse
//   legs of core/frontend/src/tile/ImdlReader.ts readImdlContent (:62-84).
//
// Authored: no reference test exists in <project> for the parser seam itself
//   (the reference's coverage rides the RPC-backed IModelTileReader
//   integration tests); the scenarios here pin the E1/E2/E3 EQUIVALENCE seams
//   (worker parse ≡ sync parse byte-for-byte, parse off the main thread,
//   noWorker inline) using the recorded rectangle fixture (TileIOFixtures.h —
//   the same bytes the reference's TileIO tests parse).

#include "tile/ImdlParser.h"
#include "dqRender/WorkerProxy.h"

#include <gtest/gtest.h>

#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace dqRender;
using namespace dqRender::fixtures;

namespace {

template <typename Pred>
bool waitFor(Pred&& pred, std::chrono::milliseconds budget
                             = std::chrono::milliseconds(10000))
{
    auto const deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred())
            return true;
        std::this_thread::yield();
    }
    return pred();
}

void expectSamePayload(ImdlParsedContent const& a, ImdlParsedContent const& b)
{
    ASSERT_EQ(a.valid, b.valid);
    if (!a.valid)
        return;
    // document: glTF JSON scene (string equality) + BIN chunk (byte equality)
    // + feature words.
    EXPECT_EQ(a.document.sceneJson, b.document.sceneJson);
    ASSERT_EQ(a.document.binary.size(), b.document.binary.size());
    EXPECT_EQ(0, std::memcmp(a.document.binary.data(), b.document.binary.data(),
                             a.document.binary.size()));
    EXPECT_EQ(a.document.featureCount, b.document.featureCount);
    EXPECT_EQ(a.document.featureTableLength, b.document.featureTableLength);
    ASSERT_EQ(a.document.featureData.size(), b.document.featureData.size());
    for (size_t i = 0; i < a.document.featureData.size(); ++i)
        EXPECT_EQ(a.document.featureData[i], b.document.featureData[i]);
    // description + header flags.
    EXPECT_EQ(a.description.isLeaf, b.description.isLeaf);
    EXPECT_EQ(a.description.sizeMultiplier, b.description.sizeMultiplier);
    EXPECT_EQ(a.description.emptySubRangeMask, b.description.emptySubRangeMask);
    EXPECT_EQ(a.imdlFlags, b.imdlFlags);
}

}  // namespace

// The worker-parse path must produce a payload identical to the synchronous
// parse — 同一瓦两路 document 对账（P-b2 合同锁；E4 的解析等价面）。
// Ported from: ImdlParser.ts:39-58 (the two faces of acquireImdlParser — the
// noWorker main-thread parse vs the worker parse) + ImdlReader.ts:74-84
// (parseDocument via the parser).
// Authored: no reference unit test for the seam (see file head).
TEST(ImdlParser, WorkerParseEqualsSyncParseByteForByte)
{
    // Sync face (noWorker branch — parse on this thread).
    ImdlParsedContent syncParsed;
    acquireImdlParser(AcquireImdlParserArgs{/*noWorker=*/true})
        ->parse(std::vector<uint8_t>(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize),
                [&](ImdlParsedContent&& parsed) { syncParsed = std::move(parsed); });
    ASSERT_TRUE(syncParsed.valid);
    EXPECT_FALSE(syncParsed.document.sceneJson.empty());

    // Worker face (defaultParser singleton — parse off-thread, completion at
    // the drain).
    std::atomic<bool> completed{false};
    ImdlParsedContent workerParsed;
    acquireImdlParser(AcquireImdlParserArgs{})
        ->parse(std::vector<uint8_t>(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize),
                [&](ImdlParsedContent&& parsed) {
                    workerParsed = std::move(parsed);
                    completed = true;
                });
    // The completion only runs at drainCompleted — the job must finish first
    // (observable as a queued completion), then the drain resolves it.
    ASSERT_TRUE(waitFor([&] {
        return WorkerProxy::instance().pendingCompletionCount() > 0;
    }));
    EXPECT_FALSE(completed.load());
    EXPECT_NE(WorkerProxy::instance().workerThreadId(), std::this_thread::get_id());
    WorkerProxy::instance().drainCompleted();
    ASSERT_TRUE(completed.load());
    // Completions run on the calling (main) thread — the drain seam.
    EXPECT_EQ(workerParsed.valid, true);
    expectSamePayload(syncParsed, workerParsed);
}

// The noWorker parser parses inline on the calling thread — the callback has
// run by the time parse returns (no drain involved).
// Ported from: ImdlParser.ts:39-47 (acquireImdlParser's noWorker branch —
// `parse: async (options) => parseImdlDocument(...)` on the main thread).
// Authored: no reference unit test for the seam (see file head).
TEST(ImdlParser, NoWorkerParserParsesInlineOnCallingThread)
{
    std::thread::id callbackThreadId;
    bool called = false;
    acquireImdlParser(AcquireImdlParserArgs{/*noWorker=*/true})
        ->parse(std::vector<uint8_t>(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize),
                [&](ImdlParsedContent&& parsed) {
                    called = true;
                    callbackThreadId = std::this_thread::get_id();
                    EXPECT_TRUE(parsed.valid);
                });
    EXPECT_TRUE(called);  // inline — no drain needed
    EXPECT_EQ(callbackThreadId, std::this_thread::get_id());
}

// Malformed bytes → the ImdlParseError face (valid=false) on both paths.
// Ported from: ImdlParser.ts:24 (`Promise<Document | ImdlParseError>` — the
// error leg; ParseImdlDocument's malformed-data read statuses).
// Authored: no reference unit test for the seam (see file head).
TEST(ImdlParser, MalformedBytesYieldInvalidPayloadOnBothPaths)
{
    uint8_t const garbage[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    ImdlParsedContent syncParsed;
    acquireImdlParser(AcquireImdlParserArgs{/*noWorker=*/true})
        ->parse(std::vector<uint8_t>(garbage, garbage + sizeof(garbage)),
                [&](ImdlParsedContent&& parsed) { syncParsed = std::move(parsed); });
    EXPECT_FALSE(syncParsed.valid);
    EXPECT_TRUE(syncParsed.document.sceneJson.empty());
    EXPECT_TRUE(syncParsed.document.binary.empty());

    ImdlParsedContent workerParsed;
    acquireImdlParser(AcquireImdlParserArgs{})
        ->parse(std::vector<uint8_t>(garbage, garbage + sizeof(garbage)),
                [&](ImdlParsedContent&& parsed) { workerParsed = std::move(parsed); });
    ASSERT_TRUE(waitFor([&] {
        return WorkerProxy::instance().pendingCompletionCount() > 0;
    }));
    WorkerProxy::instance().drainCompleted();
    EXPECT_FALSE(workerParsed.valid);
    expectSamePayload(syncParsed, workerParsed);
}
