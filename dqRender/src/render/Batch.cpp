// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Batch implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Graphic.ts + BatchState.ts
#include "Batch.h"
#include "RenderCommands.h"

#include <algorithm>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Batch
// ---------------------------------------------------------------------------
Batch::Batch(uint32_t featureCount)
    : m_featureCount(featureCount)
{
}

Batch::Batch(uint32_t featureCount, std::unique_ptr<dqCommon::FeatureTable> featureTable)
    : m_featureTable(std::move(featureTable))
    , m_featureCount(featureCount)
{
}

FeatureOverrideLUT* Batch::getOrCreateFeatureOverrideLUT()
{
    if (!m_featureOverrideLUT) {
        // Ported from: itwinjs-core FeatureOverrides.initFromMap (:397-410) —
        // overrides are built from the batch's feature table only (`assert(0 <
        // nFeatures)`, :399). A tableless batch has no override data — no LUT
        // object (TD-28③; see the header note).
        if (!m_featureTable)
            return nullptr;
        m_featureOverrideLUT = std::make_unique<FeatureOverrideLUT>();
        // Initialize LUT from the feature table.
        dqCommon::PackedFeatureTable packed = dqCommon::PackedFeatureTable::pack(*m_featureTable);
        m_featureOverrideLUT->initialize(packed);
    }
    return m_featureOverrideLUT.get();
}

// ---------------------------------------------------------------------------
// Batch — updateFeatureStates
// Ported from: itwinjs-core FeatureOverrides.updateHilite (:300-336) +
//               updateFlashed (:338-375), invoked from update (:412-441)
//
// Full recompute over the batch's feature table: LUT row i gets the Hilited
// bit iff the feature's element id is in the hilite set, and the Flashed bit
// iff it equals the flashed element id (covers both add and remove — the
// reference updates flags in both directions each pass).
// ---------------------------------------------------------------------------
void Batch::updateFeatureStates(std::vector<uint32_t> const& hiliteElementIds,
                                uint32_t flashedElementId)
{
    if (!m_featureTable)
        return;

    FeatureOverrideLUT* lut = getOrCreateFeatureOverrideLUT();
    if (!lut)
        return;
    int const n = m_featureTable->getSize();
    for (int i = 0; i < n; ++i) {
        auto const feature = m_featureTable->findFeature(i);
        uint32_t elemId = 0u;
        if (feature) {
            elemId = static_cast<uint32_t>(feature->elementId.GetValue());
        }
        bool const hilited = std::find(hiliteElementIds.begin(), hiliteElementIds.end(),
                                       elemId) != hiliteElementIds.end();
        bool const flashed = (flashedElementId != 0u) && (elemId == flashedElementId);
        lut->setFeatureHilited(static_cast<uint32_t>(i), hilited);
        lut->setFeatureFlashed(static_cast<uint32_t>(i), flashed);
    }
}

// ---------------------------------------------------------------------------
// Batch — applySubCategoryVisibility
// M-N(1)：subCategory 可见性全量重算（参考 FeatureOverrides._visibleSubCategories
// 语义——特征 subCategoryId 落在不可见集合 → LUT Visibility 标记清除 → 分片
// override 路径 discard）。与 updateFeatureStates 独立正交：hilite/flash 只动
// 各自的位，Visibility 标记在此路径独占（两路径可任意序重算不互踩）。
// ---------------------------------------------------------------------------
void Batch::applySubCategoryVisibility(std::set<uint64_t> const& invisibleSubCategories)
{
    if (!m_featureTable)
        return;

    FeatureOverrideLUT* lut = getOrCreateFeatureOverrideLUT();
    if (!lut)
        return;
    int const n = m_featureTable->getSize();
    // TEMP-DIAG（M-N(1)）：特征表 subCategoryId 实态探针。
    static bool const s_catTrace = std::getenv("DANQING_CAT_TRACE") != nullptr;
    std::set<uint64_t> traceIds;
    for (int i = 0; i < n; ++i) {
        auto const feature = m_featureTable->findFeature(i);
        uint64_t subCatId = feature ? feature->subCategoryId.GetValue() : 0u;
        if (s_catTrace)
            traceIds.insert(subCatId);
        bool const visible = invisibleSubCategories.find(subCatId) == invisibleSubCategories.end();
        lut->setFeatureVisibility(static_cast<uint32_t>(i), visible);
    }
    if (s_catTrace) {
        std::fprintf(stderr, "[CAT] batch=%u features=%d invisibleSet=%zu subCats:",
                     m_batchId, n, invisibleSubCategories.size());
        for (uint64_t id : traceIds)
            std::fprintf(stderr, " 0x%llx", static_cast<unsigned long long>(id));
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }
}

// ---------------------------------------------------------------------------
// Batch — addCommands
// Ported from: itwinjs-core Batch.addCommands() (line 288-293)
//
// Delegates to RenderCommands.addBatch() which handles visibility overrides,
// frustum culling, opaque/translucent overrides, and hilite routing.
// ---------------------------------------------------------------------------
void Batch::addCommands(RenderCommands& commands)
{
    commands.addBatch(*this);
}

// ---------------------------------------------------------------------------
// Batch — addHiliteCommands
// Ported from: itwinjs-core Batch.addHiliteCommands()
//
// In itwinjs-core, hilite commands for batches are generated by
// RenderCommands.addBatch() when it detects hilited features.
// This override handles the case where addHiliteCommands is called
// directly on a Batch (e.g., from Branch.addHiliteCommands).
// ---------------------------------------------------------------------------
void Batch::addHiliteCommands(RenderCommands& commands, RenderPass pass)
{
    // Delegate to addBatch which handles the full batch routing
    // including hilite detection.
    commands.addBatch(*this);
    (void)pass;
}

// ---------------------------------------------------------------------------
// Batch — collectStatistics
// Ported from: itwinjs-core Batch.collectStatistics()
// ---------------------------------------------------------------------------
void Batch::collectStatistics(/* RenderMemory::Statistics& stats */) const
{
    if (m_child) m_child->collectStatistics(/* stats */);
    // Ported from: itwinjs-core Batch.collectStatistics()
    // FeatureTable byte length and perTargetData statistics will be
    // wired when RenderMemory::Statistics interface is ported.
    // The feature table size is: m_featureCount * sizeof(Feature)
    // PerTargetData includes the FeatureOverrideLUT texture size.
}

// ---------------------------------------------------------------------------
// Batch — unionRange
// Ported from: itwinjs-core Batch.unionRange()
// ---------------------------------------------------------------------------
void Batch::unionRange(dqGeom::Range3d& range) const
{
    // Ported from: itwinjs-core Batch.unionRange()
    range.ExtendRange(m_range);
    if (m_child) m_child->unionRange(range);
    // The feature table itself doesn't contribute additional spatial extent;
    // the child graphic's geometry already covers the batch's spatial bounds.
    // The feature table is a mapping from feature index to element ID,
    // not a spatial structure.
}

// ---------------------------------------------------------------------------
// BatchState — assignBatchId
// Ported from: itwinjs-core BatchState.getBatchId() (line 104-117)
// ---------------------------------------------------------------------------
void BatchState::assignBatchId(Batch& batch)
{
    batch.setContext(m_nextBatchId);
    m_batches.push_back(&batch);

    // advance by the number of features (at least 1 to avoid zero-width range).
    uint32_t numFeatures = batch.getFeatureCount();
    if (numFeatures == 0) numFeatures = 1;
    m_nextBatchId += numFeatures;
}

// ---------------------------------------------------------------------------
// BatchState — reset
// Ported from: itwinjs-core BatchState.reset() (line 81-87)
// ---------------------------------------------------------------------------
void BatchState::reset()
{
    // Reset context on all registered batches.
    for (auto* batch : m_batches) {
        batch->resetContext();
    }
    m_batches.clear();
    m_nextBatchId = 1;
    m_currentBatchId = 0;
    m_currentBatch = nullptr;
}

// ---------------------------------------------------------------------------
// BatchState — push/pop
// Ported from: itwinjs-core BatchState.push() (line 68-77)
// ---------------------------------------------------------------------------
void BatchState::push(Batch& batch)
{
    m_batchStack.push_back({m_currentBatchId, m_currentBatch});
    m_currentBatchId = batch.getBatchId();
    m_currentBatch = &batch;
}

void BatchState::pop()
{
    if (!m_batchStack.empty()) {
        m_currentBatchId = m_batchStack.back().batchId;
        m_currentBatch = m_batchStack.back().batch;
        m_batchStack.pop_back();
    } else {
        m_currentBatchId = 0;
        m_currentBatch = nullptr;
    }
}

// ---------------------------------------------------------------------------
// BatchState — findBatch
// Ported from: itwinjs-core BatchState.find() (line 141-149)
// Binary search through the ordered batch list to find the batch containing
// the given feature ID.
// ---------------------------------------------------------------------------
Batch* BatchState::findBatch(uint32_t featureId) const
{
    if (featureId == 0 || m_batches.empty())
        return nullptr;

    // Binary search: find the last batch whose batchId <= featureId.
    auto it = std::upper_bound(m_batches.begin(), m_batches.end(), featureId,
        [](uint32_t id, Batch const* batch) {
            return id < batch->getBatchId();
        });

    if (it == m_batches.begin())
        return nullptr;

    --it;
    Batch* batch = *it;
    uint32_t batchId = batch->getBatchId();
    uint32_t numFeatures = batch->getFeatureCount();
    if (numFeatures == 0) numFeatures = 1;

    // Check if featureId falls within this batch's range.
    if (featureId >= batchId && featureId < batchId + numFeatures)
        return batch;

    return nullptr;
}

// ---------------------------------------------------------------------------
// BatchState — findBatchId
// Ported from: itwinjs-core BatchState.findBatchId()
// ---------------------------------------------------------------------------
uint32_t BatchState::findBatchId(uint32_t featureId) const
{
    Batch* batch = findBatch(featureId);
    return batch ? batch->getBatchId() : 0;
}

// ---------------------------------------------------------------------------
// BatchState — getNextBatchId
// Ported from: itwinjs-core BatchState.nextBatchId
// ---------------------------------------------------------------------------
uint32_t BatchState::getNextBatchId() const
{
    if (m_batches.empty())
        return 1;

    auto const* prev = m_batches.back();
    uint32_t numFeatures = prev->getFeatureCount();
    if (numFeatures == 0) numFeatures = 1;

    return prev->getBatchId() + numFeatures;
}

// ---------------------------------------------------------------------------
// BatchState — getElementId
// Ported from: itwinjs-core BatchState.getElementId()
// ---------------------------------------------------------------------------
uint64_t BatchState::getElementId(uint32_t featureId) const
{
    Batch* batch = findBatch(featureId);
    if (!batch)
        return 0;

    auto const* table = batch->getFeatureTable();
    if (!table)
        return 0;

    uint32_t featureIndex = featureId - batch->getBatchId();
    auto feature = table->findFeature(static_cast<int>(featureIndex));
    if (!feature.has_value())
        return 0;

    return feature->elementId.GetValue();
}

// ---------------------------------------------------------------------------
// BatchState — getFeature
// Ported from: itwinjs-core BatchState.getFeature()
// ---------------------------------------------------------------------------
bool BatchState::getFeature(uint32_t featureId, dqCommon::Feature& result) const
{
    Batch* batch = findBatch(featureId);
    if (!batch)
        return false;

    auto const* table = batch->getFeatureTable();
    if (!table)
        return false;

    uint32_t featureIndex = featureId - batch->getBatchId();
    auto feature = table->findFeature(static_cast<int>(featureIndex));
    if (!feature.has_value())
        return false;

    result = feature.value();
    return true;
}

END_DQ_RENDER_NAMESPACE
