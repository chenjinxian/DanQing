// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Batch (feature grouping node)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Graphic.ts Batch (line 235-334)
//              + BatchState.ts
//
// A Batch groups geometry under a shared feature table.  Each batch has a
// unique 32-bit ID that, combined with the per-vertex feature index, yields a
// globally unique feature ID for picking and symbology overrides.
#pragma once

#include "Graphic.h"

#include <dqCommon/FeatureTable.h>
#include "FeatureOverrideLUT.h"

#include <cstdint>
#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// BatchContext — context for a batch during rendering
// Ported from: itwinjs-core Graphic.ts BatchContext (line 83-89)
// ---------------------------------------------------------------------------
struct BatchContext {
    uint32_t batchId = 0;
    // Ported from: itwinjs-core Graphic.ts BatchContext (line 83-89)
    // These properties support multi-iModel rendering and will be added
    // when the platform layer provides iModel types:
    //   - iModel: the iModel this batch belongs to
    //   - transformFromIModel: transform from iModel coordinates to world
    //   - viewAttachmentId: view attachment for drawing production
};

// ---------------------------------------------------------------------------
// Batch — feature grouping node
// Ported from: itwinjs-core Graphic.ts Batch (line 235-334)
// ---------------------------------------------------------------------------
class Batch : public Graphic {
public:
    /// @param featureCount Number of features in this batch.
    explicit Batch(uint32_t featureCount);

    /// Construct with a feature table.
    Batch(uint32_t featureCount, std::unique_ptr<dqCommon::FeatureTable> featureTable);

    /// Set the child graphic (the geometry to render under this batch).
    void setChild(std::unique_ptr<Graphic> child) { m_child = std::move(child); }

    /// Get the child graphic (may be nullptr).
    /// Ported from: itwinjs-core Batch.graphic
    Graphic* getChild() const noexcept { return m_child.get(); }

    /// Get the batch ID (assigned by BatchState during rendering).
    uint32_t getBatchId() const noexcept { return m_batchId; }

    /// Set the batch ID (called by BatchState).
    void setBatchId(uint32_t id) noexcept { m_batchId = id; }

    /// Set the batch context from the current branch state.
    /// Ported from: itwinjs-core Batch.setContext() (line 270-276)
    void setContext(uint32_t batchId) {
        m_batchId = batchId;
        m_context.batchId = batchId;
    }

    /// Reset the batch context.
    /// Ported from: itwinjs-core Batch.resetContext() (line 278-280)
    void resetContext() {
        m_batchId = 0;
        m_context = BatchContext{};
    }

    /// Get the number of features in this batch.
    uint32_t getFeatureCount() const noexcept { return m_featureCount; }

    /// Get the feature table (may be nullptr).
    dqCommon::FeatureTable const* getFeatureTable() const noexcept { return m_featureTable.get(); }

    /// Set the feature table.
    void setFeatureTable(std::unique_ptr<dqCommon::FeatureTable> table) {
        m_featureTable = std::move(table);
    }

    /// Set the batch's content range (used by unionRange).
    /// Ported from: itwinjs-core Batch constructor range (Graphic.ts:262-267)
    void setRange(dqGeom::Range3d const& range) noexcept { m_range = range; }

    /// Get the feature override LUT (may be nullptr).
    FeatureOverrideLUT const* getFeatureOverrideLUT() const noexcept { return m_featureOverrideLUT.get(); }

    /// Get or create the feature override LUT.
    FeatureOverrideLUT& getOrCreateFeatureOverrideLUT();

    /// Update this batch's LUT hilite/flash flags from the target's state when
    /// either changed since the last update (the reference keeps per-batch
    /// observers on the target's hiliteSyncTarget + flashedId and recomputes
    /// the LUT lazily at draw time — full recompute covers add AND remove).
    /// Ported from: itwinjs-core FeatureOverrides.update → updateHilite
    ///               (:300-336) + updateFlashed (:338-375), invoked from
    ///               update (:412-441)
    void updateFeatureStates(std::vector<uint32_t> const& hiliteElementIds,
                             uint32_t flashedElementId);

    /// The hilite-set version this batch's LUT was last updated from.
    uint32_t getLastHiliteVersion() const noexcept { return m_lastHiliteVersion; }
    void setLastHiliteVersion(uint32_t v) noexcept { m_lastHiliteVersion = v; }

    /// Check if this batch has feature overrides.
    bool hasFeatureOverrides() const noexcept { return m_featureOverrideLUT != nullptr; }

    /// Whether this batch is locate-only (not rendered, only pickable).
    /// Ported from: itwinjs-core Batch.locateOnly
    bool isLocateOnly() const noexcept { return m_locateOnly; }
    void setLocateOnly(bool v) noexcept { m_locateOnly = v; }

    // --- Graphic interface ---

    /// add draw commands (pushes batch, adds child commands, pops batch).
    /// Ported from: itwinjs-core Batch.addCommands() (line 288-293)
    void addCommands(RenderCommands& commands) override;

    /// add hilite draw commands.
    /// Ported from: itwinjs-core Batch.addHiliteCommands()
    void addHiliteCommands(RenderCommands& commands, RenderPass pass) override;

    /// Collect render memory statistics.
    /// Ported from: itwinjs-core Batch.collectStatistics()
    void collectStatistics(/* RenderMemory::Statistics& stats */) const override;

    /// Extend the range by this batch's feature table range.
    /// Ported from: itwinjs-core Batch.unionRange()
    void unionRange(dqGeom::Range3d& range) const override;

    /// Batch is always pickable.
    /// Ported from: itwinjs-core Batch.isPickable (line 311)
    bool isPickable() const override { return true; }

    /// Batch context accessor.
    BatchContext const& getContext() const noexcept { return m_context; }

private:
    std::unique_ptr<Graphic> m_child;
    std::unique_ptr<dqCommon::FeatureTable> m_featureTable;
    std::unique_ptr<FeatureOverrideLUT> m_featureOverrideLUT;
    BatchContext m_context;
    // Range stored at creation (Graphic.ts:262-267) — the child graphic's
    // primitives are CachedGeometry whose unionRange is a no-op, so without a
    // stored range a Batch contributes nothing to culling bounds.
    dqGeom::Range3d m_range;
    uint32_t m_featureCount = 0;
    uint32_t m_batchId = 0;
    uint32_t m_lastHiliteVersion = 0;
    bool m_locateOnly = false;
};

// ---------------------------------------------------------------------------
// BatchState — assigns transient batch IDs during rendering
// Ported from: itwinjs-core BatchState.ts
//
// Each batch gets a contiguous range of IDs: [batchId, batchId + numFeatures).
// The combination of batchId + featureIndex yields a globally unique feature ID
// for picking and symbology overrides.
// ---------------------------------------------------------------------------
class BatchState {
public:
    BatchState() = default;

    /// Assign a batch ID to the given batch.
    /// Ported from: itwinjs-core BatchState.getBatchId() (line 104-117)
    void assignBatchId(Batch& batch);

    /// Reset for a new frame.
    /// Ported from: itwinjs-core BatchState.reset() (line 81-87)
    void reset();

    /// Get the current batch ID (for uniform upload).
    uint32_t getCurrentBatchId() const noexcept { return m_currentBatchId; }

    /// Get the current batch pointer (for feature override queries).
    /// Returns nullptr if no batch is active.
    Batch* getCurrentBatch() const noexcept { return m_currentBatch; }

    /// Push/pop batch stack.
    /// Ported from: itwinjs-core BatchState.push() (line 68-77)
    void push(Batch& batch);
    void pop();

    // --- Feature ID lookup (for picking) ---
    // Ported from: itwinjs-core BatchState.ts getElementId, getFeature, find, indexOf

    /// Find the batch containing the given feature ID.
    /// Returns nullptr if not found.
    Batch* findBatch(uint32_t featureId) const;

    /// Get the batch ID for the batch containing the given feature ID.
    /// Returns 0 if not found.
    uint32_t findBatchId(uint32_t featureId) const;

    /// Get the total number of feature IDs across all batches.
    uint32_t getNumFeatureIds() const noexcept { return m_nextBatchId; }

    /// Get the number of registered batches.
    size_t getNumBatches() const noexcept { return m_batches.size(); }

    /// Check if no batches are registered.
    /// Ported from: itwinjs-core BatchState.isEmpty
    bool isEmpty() const noexcept { return m_batches.empty(); }

    /// Get the next batch ID that would be assigned.
    /// Ported from: itwinjs-core BatchState.nextBatchId
    uint32_t getNextBatchId() const;

    /// Get the element ID for a given feature ID.
    /// Returns 0 if not found.
    /// Ported from: itwinjs-core BatchState.getElementId()
    uint64_t getElementId(uint32_t featureId) const;

    /// Get the Feature for a given feature ID.
    /// Returns nullptr if not found.
    /// Ported from: itwinjs-core BatchState.getFeature()
    bool getFeature(uint32_t featureId, dqCommon::Feature& result) const;

private:
    uint32_t m_nextBatchId = 1;
    uint32_t m_currentBatchId = 0;
    Batch* m_currentBatch = nullptr;

    struct BatchStackEntry {
        uint32_t batchId;
        Batch* batch;
    };
    std::vector<BatchStackEntry> m_batchStack;

    // Persistent ordered list of all batches for feature ID lookup.
    // Ported from: itwinjs-core BatchState._batches
    std::vector<Batch*> m_batches;
};

END_DQ_RENDER_NAMESPACE
