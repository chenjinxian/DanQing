// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — async imdl content delivery tests (P-1b P-b3)
//
// Ported from: itwinjs-core core/frontend/src/tile/TileRequest.ts:156-195
//   (handleResponse — the readContent continuation: bytes → parse → setContent)
//   + internal/tile/IModelTile.ts:90-131 (readContent's decoder.decode
//   two-phase split — parse off-thread, graphics on the main thread) +
//   tile/ImdlReader.ts:85-86 (the post-parse isCanceled gate) +
//   internal/tile/IModelTile.ts:95-96 (the isCanceled default
//   `() => !this.isLoading`).
//
// Authored: no reference test exists in <project> for the C++ async delivery
//   contract (the reference mediates through the TileRequest promise closure —
//   JS GC keeps the tile alive; the C++ pending-parse registry has no
//   counterpart). The scenarios pin the frame-order invariant (delivery not
//   earlier than a later frame's drain — the M-F(1) ordering note in
//   TileAdmin::process), the completion-time cancel gate, the TD-22-family
//   scrub face, and the noWorker degradation (E3), with content values taken
//   from the recorded rectangle fixture (TileIOFixtures.h).

#include "dqRender/RenderGraphic.h"  // TileContent::graphic unique_ptr 析构需完整类型
#include "dqRender/RenderSystem.h"   // 桩系统基类（RenderTarget/GraphicBuilder 完整型）
#include "tile/ImdlParser.h"
#include "dqRender/tile/ImdlTileTree.h"
#include "dqRender/tile/TileAdmin.h"
#include "dqRender/WorkerProxy.h"

#include <dqCommon/FeatureTable.h>  // TileContent::featureTable unique_ptr 析构需完整类型

#include <gtest/gtest.h>

#include "tile-sample-assets/imdl-fixtures/TileIOFixtures.h"

#include "render/Batch.h"
#include "render/Graphic.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace dqRender;
using namespace dqRender::fixtures;

namespace {

dqGeom::Range3d box(double x0, double y0, double z0, double x1, double y1, double z1)
{
    return dqGeom::Range3d::CreateXYZXYZ(x0, y0, z0, x1, y1, z1);
}

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

// 桩 RenderSystem（ImdlTileTreeTest 的 ReadContentStubSystem 同款——无 GL 环境
// 承接 createGraphicFromPolyface/createGraphicList/createBatch 三调用）。
class AsyncStubGraphic final : public Graphic {
public:
    void addCommands(RenderCommands&) override {}
};

class AsyncStubSystem final : public RenderSystem {
public:
    bool isValid() const noexcept override { return true; }
    std::unique_ptr<RenderTarget> createTarget(void*, uint32_t, uint32_t) override
    {
        return nullptr;
    }
    std::unique_ptr<GraphicBuilder> createGraphicBuilder(
        const GraphicBuilderOptions&) override
    {
        return nullptr;
    }
    GraphicBranch* createBranch(bool = true) override { return nullptr; }
    RenderGraphic* createBranchGraphic(GraphicBranch*) override { return nullptr; }
    RenderGraphic* createGraphicList(std::vector<RenderGraphic*> graphics) override
    {
        if (graphics.empty())
            return nullptr;
        if (graphics.size() == 1)
            return graphics[0];
        auto* arr = new GraphicsArray();
        for (auto* g : graphics)
            arr->add(std::unique_ptr<Graphic>(static_cast<Graphic*>(g)));
        return arr;
    }
    RenderGraphicOwner* createGraphicOwner(RenderGraphic* owned) override
    {
        return owned ? new RenderGraphicOwner(owned) : nullptr;
    }
    RenderGraphic* createGraphicFromPolyface(void const* polyface, uint32_t,
                                             uint32_t,
                                             rhi::TextureHandle) override
    {
        if (!polyface)
            return nullptr;
        return new AsyncStubGraphic();
    }
    RenderGraphic* createBatch(RenderGraphic* graphic,
                               dqCommon::FeatureTable const* featureTable,
                               dqGeom::Range3d const& range,
                               BatchOptions const* /*options*/ = nullptr) override
    {
        if (!graphic)
            return nullptr;
        std::unique_ptr<dqCommon::FeatureTable> table;
        uint32_t featureCount = 1;
        if (featureTable) {
            auto t = std::make_unique<dqCommon::FeatureTable>(
                featureTable->getMaxFeatures(), featureTable->getModelId(),
                featureTable->getType());
            for (int i = 0; i < featureTable->getArraySize(); ++i) {
                auto const& indexed = featureTable->getArray()[i];
                t->insertWithIndex(indexed.value, indexed.index);
            }
            featureCount = static_cast<uint32_t>(t->getSize());
            table = std::move(t);
        }
        auto* batch = new Batch(featureCount, std::move(table));
        batch->setChild(std::unique_ptr<Graphic>(static_cast<Graphic*>(graphic)));
        batch->setRange(range);
        return batch;
    }
};

// The pending-parse registry lives on TileAdmin::instance() (production: the
// app has one admin; the tests route through it — see readContentAsync).
struct Fixture {
    AsyncStubSystem system;
    std::unique_ptr<ImdlTileTree> tree;

    Fixture()
    {
        ImdlTreeMetadata meta;
        meta.contentRange = box(-1, -1, -1, 9, 9, 9);
        tree = std::make_unique<ImdlTileTree>("fixture://minimal-imdl", "0/0/0/0",
                                              box(0, 0, 0, 8, 8, 8), meta);
        tree->setRenderSystem(&system);
    }

    Tile& rootTile() { return *tree->getRootTile(); }

    // The production dispatch face: TileRequestChannel::process marks the
    // tile Loading at dispatch (Tile.ts:271-295 的 DanQing 承载) — mirror it
    // so the completion gate sees a loading tile, as it always does when
    // deliverTileContent runs from a fetch completion.
    void deliverAsIfDispatched(std::vector<uint8_t> const& bytes)
    {
        rootTile().setLoadStatus(TileLoadStatus::Loading);
        TileAdmin::instance().deliverTileContent(rootTile(), bytes);
    }
};

}  // namespace

// M-F(1) 帧序不变量显式锁（P-b3）：worker 模式下 bytes 交付帧不建 graphics，
// 完成只在后续 process() 的 drain 点交付。
// Ported from: TileRequest.ts:156-195 handleResponse（await readContent 的
//              异步交付面）+ TileAdmin::process 的 step-0/step-1 序注。
// Authored: no reference unit test for the C++ drain contract (see file head).
TEST(AsyncImdlContent, ParseDeliversAtLaterFrameNotSameFrame)
{
    Fixture f;
    Tile& root = f.rootTile();

    std::vector<uint8_t> const bytes(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize);
    f.deliverAsIfDispatched(bytes);

    // 交付帧（bytes 到达、解析已 submit）：内容未建——完成不早于次帧 drain
    // （M-F(1) 不变量：交付 ≥1 个选择趟之后）。
    EXPECT_EQ(root.getGraphic(), nullptr);
    EXPECT_EQ(root.getLoadStatus(), TileLoadStatus::Loading);

    // 后续帧的 process()：drain 交付 → Ready + graphic。
    ASSERT_TRUE(waitFor([&] {
        TileAdmin::instance().process();
        return root.getLoadStatus() == TileLoadStatus::Ready;
    }));
    EXPECT_NE(root.getGraphic(), nullptr);
    EXPECT_TRUE(root.isDisplayable());
}

// 完成期取消门（ImdlReader.ts:85-86 + IModelTile.ts:95-96 的 isCanceled 默认
// `() => !this.isLoading`）：完成时已不在 Loading 的瓦不建 graphics——取消的
// 瓦不建 graphics（P-b3 取消语义锁）。
// Authored: no reference unit test for the C++ gate (see file head).
TEST(AsyncImdlContent, CanceledTileDoesNotBuildGraphics)
{
    Fixture f;
    Tile& root = f.rootTile();

    std::vector<uint8_t> const bytes(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize);
    f.deliverAsIfDispatched(bytes);
    EXPECT_EQ(root.getGraphic(), nullptr);

    // Cancel face: the tile stops loading before the parse completes. The
    // pending entry survives (no scrub — the tile is alive), the completion
    // gate drops it.
    root.setLoadStatus(TileLoadStatus::NotLoaded);

    // Pump past the delivery point — no content may appear.
    for (int i = 0; i < 20; ++i) {
        TileAdmin::instance().process();
        std::this_thread::yield();
    }
    waitFor([&] {
        return WorkerProxy::instance().pendingCompletionCount() == 0;
    });
    TileAdmin::instance().process();
    EXPECT_EQ(root.getGraphic(), nullptr);
    EXPECT_NE(root.getLoadStatus(), TileLoadStatus::Ready);
}

// TD-22 家族清扫面（~Tile → onTileContentDisposed）：瓦在解析完成前销毁 →
// pending entry 摘除 → drain 永不触碰死 Tile。
// Authored: no reference test (JS GC keeps the tile alive — the C++ registry
// scrub is the liveness adaptation; see file head).
TEST(AsyncImdlContent, TileDestructionBeforeDeliveryDropsPendingParse)
{
    AsyncStubSystem system;
    {
        ImdlTreeMetadata meta;
        meta.contentRange = box(-1, -1, -1, 9, 9, 9);
        auto tree = std::make_unique<ImdlTileTree>(
            "fixture://minimal-imdl", "0/0/0/0", box(0, 0, 0, 8, 8, 8), meta);
        tree->setRenderSystem(&system);
        Tile& root = *tree->getRootTile();
        root.setLoadStatus(TileLoadStatus::Loading);
        std::vector<uint8_t> const bytes(V1_1::rectangleBytes,
                                         V1_1::rectangleBytes + V1_1::rectangleSize);
        TileAdmin::instance().deliverTileContent(root, bytes);
        ASSERT_EQ(TileAdmin::instance().pendingImdlParses().size(), 1u);
        // The tree (and its root tile) dies here — the registry must drop the
        // entry via ~Tile → onTileContentDisposed.
    }
    EXPECT_EQ(TileAdmin::instance().pendingImdlParses().size(), 0u);

    // The drain runs against an empty registry — no crash, no delivery.
    for (int i = 0; i < 5; ++i) {
        TileAdmin::instance().process();
        std::this_thread::yield();
    }
}

// E3 降级面：noWorker 下 readContentAsync 返回 false → 调用方同步 readContent
// 旧路（交付即 Ready——与 P-1b 前的现行行为逐点一致）。
// Ported from: ImdlParser.ts:39-47 (the noWorker branch — main-thread parse).
// Authored: no reference unit test for the degradation face (see file head).
TEST(AsyncImdlContent, NoWorkerKeepsSynchronousDeliveryFace)
{
    WorkerProxy::setNoWorkerOverrideForTest(true);
    struct Reset {
        ~Reset() { WorkerProxy::setNoWorkerOverrideForTest(std::nullopt); }
    } reset;

    Fixture f;
    Tile& root = f.rootTile();

    std::vector<uint8_t> const bytes(V1_1::rectangleBytes,
                                     V1_1::rectangleBytes + V1_1::rectangleSize);
    f.deliverAsIfDispatched(bytes);

    // Synchronous face: content set inside deliverTileContent — no drain.
    EXPECT_EQ(root.getLoadStatus(), TileLoadStatus::Ready);
    EXPECT_NE(root.getGraphic(), nullptr);
}
