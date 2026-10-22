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

#include <dqCommon/FeatureOverrides.h>  // M-O(2) I10——updateAppearanceOverrides
#include <dqCommon/FeatureTable.h>
#include <dqCommon/PackedFeatureTable.h>  // 参考 Batch.featureTable: RenderFeatureTable
#include "FeatureOverrideLUT.h"
#include "ThematicSensors.h"  // M-S S-e：逐 batch 传感器缓存

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class BatchState;   // m_batchState 反指针（AV 专案修复轮——GC 生命周期适配）
class Contours;    // M-X X-a3（m_contours unique_ptr 不完全类型面）
class TargetImpl;

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

    /// Construct with a packed feature table (the reference's Batch form —
    /// Graphic.ts:237 `featureTable: RenderFeatureTable`). Packed rows are the
    /// O(1) access face every per-feature LUT path consumes (getFeature), and
    /// the same row indices the GPU feature IDs address.
    Batch(uint32_t featureCount, std::unique_ptr<dqCommon::PackedFeatureTable> featureTable);

    /// Convenience overload for front-end-built tables (glTF/decorations and
    /// tests): packs via PackedFeatureTable::pack — the reference callers'
    /// `createBatch(graphic, PackedFeatureTable.pack(featureTable), range)`.
    Batch(uint32_t featureCount, std::unique_ptr<dqCommon::FeatureTable> featureTable);

    // unique_ptr<Contours> 不完全类型成员要求析构出列（Contours 完整类型在
    // Batch.cpp 面）。M-X X-a3。
    ~Batch() override;

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

    /// Owner-registry back-pointer（AV 专案修复轮——C++ 生命周期适配，
    /// TD-22 同族先例形态）。参考 BatchState._batches 由 GC 保活（注册表持
    /// 强引用至 reset()，死批在 JS 不可能留在表内）；DanQing 的 Batch 归
    /// graphic 树所有——装饰每帧 dispose+重建（TD-35 pickable ACS 原点批）
    /// 使裸指针条目悬垂 → findBatch 二分比较经死批读/写已释放内存
    /// （av-rootcause-report.md §2.3 site A/B/C）。~Batch 经此反指针自摘除
    /// （BatchState::unregisterBatch），注册表任意时刻只含活批。登记于
    /// assignBatchId（参考 getBatchId 的 setContext+push 原子对，
    /// BatchState.ts:112-113）。
    void setBatchState(BatchState* state) noexcept { m_batchState = state; }

    /// Get the number of features in this batch.
    uint32_t getFeatureCount() const noexcept { return m_featureCount; }

    /// Get the feature table (may be nullptr).
    dqCommon::PackedFeatureTable const* getFeatureTable() const noexcept { return m_featureTable.get(); }

    /// Set the feature table (front-end-build convenience — packs, mirroring
    /// PackedFeatureTable.pack at the reference's batch creation points).
    void setFeatureTable(std::unique_ptr<dqCommon::FeatureTable> table);

    /// Set the batch's content range (used by unionRange).
    /// Ported from: itwinjs-core Batch constructor range (Graphic.ts:262-267)
    void setRange(dqGeom::Range3d const& range) noexcept { m_range = range; }

    /// Get the per-batch thematic sensors（distanceCutoff>0 臂——
    /// Graphic.ts:111-120 的 PerTargetBatchData.getThematicSensors +
    /// BatchUniforms._setCurrentBatch :74-82）。按 batch.range × localToWorld
    /// （世界域）过滤 + 缓存（settings 指针同一性失效——参考 matchesTarget
    /// 语义）+ 创建即按 viewMatrix 打包 eye-space（参考 create(target, range)
    /// 内 _update(frustum.viewMatrix)——视空间打包实测归位，S-e）。
    /// @return 恒引用（可能为空集——域内零传感器时 numSensors=0）。
    ThematicSensors const& getThematicSensors(
        dqCommon::ThematicDisplaySensorSettings const& settings,
        dqGeom::Transform const& localToWorld,
        dqGeom::Transform const& viewMatrix,
        rhi::Driver* driver);

    /// Get the per-batch contour group LUT（M-X X-a3——Graphic.ts:135-146 的
    /// PerTargetBatchData.getContours + Batch.getContours :327-329）。
    /// 缓存失效 = matchesTargetAndFeatureCount（Contours.ts:37-40——target
    /// 同一性 + numFeatures 等；Contours.update 的 matchesSubCategories 腿
    /// [:42-54]在 update 内）。
    /// EQUIVALENCE（§11.10）：参考缓存是 per-target-per-batch 二维
    /// （PerTargetBatchData）；DanQing 缓存挂 batch 单维（ThematicSensors
    /// 缓存同族先例——Batch.h:226）——多 target 共享 batch 且各 target
    /// contours 不同值时逐帧重建（matchesTargetAndFeatureCount 的 target
    /// 同一性腿兜住正确性；本仓 DTA 面单视口形态为主）。
    /// 验证法 = ContoursTest.BatchGetContoursCachesPerFeatureCount。
    /// 前置：调用方须保证 wantContourLines（BatchUniforms._setCurrentBatch
    /// :91 的门）且 featureTable 非空（Contours._initialize :57 assert）。
    Contours& getContours(TargetImpl& target);
    Contours const* peekContours() const noexcept { return m_contours.get(); }

    /// Get the feature override LUT (may be nullptr).
    FeatureOverrideLUT const* getFeatureOverrideLUT() const noexcept { return m_featureOverrideLUT.get(); }

    /// Get or create the feature override LUT (nullptr when the batch has no
    /// feature table).
    /// Ported from: itwinjs-core FeatureOverrides.initFromMap (:397-410) — the
    /// overrides object is built FROM the batch's feature table (`assert(0 <
    /// nFeatures)`, :399); a batch without a feature table carries no override
    /// data, so no LUT object exists for it. The prior form conjured a dead
    /// LUT for a tableless batch, making hasFeatureOverrides() report true
    /// while the draw path bound no texture for it — the Overrides variant
    /// would sample whatever LUT the previous batch left on the sampler unit
    /// (cross-batch override contamination — TD-28③).
    FeatureOverrideLUT* getOrCreateFeatureOverrideLUT();

    /// Update this batch's LUT hilite/flash flags from the target's state when
    /// either changed since the last update (the reference keeps per-batch
    /// observers on the target's hiliteSyncTarget + flashedId and recomputes
    /// the LUT lazily at draw time — full recompute covers add AND remove).
    /// Ported from: itwinjs-core FeatureOverrides.update → updateHilite
    ///               (:300-336) + updateFlashed (:338-375), invoked from
    ///               update (:412-441)
    void updateFeatureStates(std::vector<uint32_t> const& hiliteElementIds,
                             uint32_t flashedElementId);

    /// Apply subCategory-based visibility: every feature whose subCategoryId is
    /// in `invisibleSubCategories` gets its LUT Visibility marker cleared
    /// (fragment override path discards it), all others restored. Full
    /// recompute like updateFeatureStates — covers both directions per pass.
    /// The reference builds FeatureOverrides._visibleSubCategories from the
    /// view's categorySelector + iModel.subcategories table
    /// (FeatureSymbology.ts:134-143) and marks features outside it invisible
    /// at appearance lookup; DanQing's dump-replay data plane has no
    /// subCategory RPC table, so the invisible SET arrives pre-expanded by
    /// subCategoryId (the key the reference's set itself holds). PushBatch
    /// drives this on subCategory-visibility version change (M-N(1)).
    void applySubCategoryVisibility(std::set<uint64_t> const& invisibleSubCategories);

    /// M-O(2) I10：元素 appearance 覆盖的全量重算——逐 feature 经 viewport
    /// overrides 的 getAppearance 解析（webgl FeatureOverrides.update :412-441
    /// 的 ovrsUpdated 分支——_update 的 per-feature appearance 打包），写入
    /// LUT 行（Rgb/Alpha/Weight/LineRgb + Emphasized/IgnoreMaterial/
    /// NonLocatable/ViewIndependentTransparency 位；Hilited/Flashed/Visibility
    /// 位保留）。触发面 = PushBatch 观测 overrides 版本过期（参考为
    /// currentFeatureSymbologyOverrides 对象同一性——EQUIVALENCE 见
    /// RenderTarget.h 注）。LineCode 覆盖随 TD-23 既有登记（u_lineCode 仅实线）。
    void updateAppearanceOverrides(dqCommon::FeatureOverrides const& ovrs);

    /// The overrides version this batch's LUT appearance was last updated from.
    uint32_t getLastAppearanceOverridesVersion() const noexcept { return m_lastAppearanceOverridesVersion; }
    void setLastAppearanceOverridesVersion(uint32_t v) noexcept { m_lastAppearanceOverridesVersion = v; }

    /// The subCategory-visibility version this batch's LUT was last updated
    /// from (M-N(1)——与 m_lastHiliteVersion 同构的批次侧版本比对位)。
    uint32_t getLastSubCategoryVisibilityVersion() const noexcept { return m_lastSubCategoryVisibilityVersion; }
    void setLastSubCategoryVisibilityVersion(uint32_t v) noexcept { m_lastSubCategoryVisibilityVersion = v; }

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

    /// Returns this — the -fno-rtti equivalent of `graphic instanceof Batch`
    /// (PlanarClassifier.ts:374).
    /// Ported from: itwinjs-core Graphic.ts Batch (instanceof Batch checks)
    Batch* asBatch() noexcept override { return this; }

    /// Batch context accessor.
    BatchContext const& getContext() const noexcept { return m_context; }

private:
    std::unique_ptr<Graphic> m_child;
    // 参考 Batch.featureTable: RenderFeatureTable（Graphic.ts:237）——packed
    // 行访问 O(1)。此前持 unpacked FeatureTable 并以 findFeature(i)（线性扫）
    // 逐特征读取 → O(n²)：bridge-edit 108,778 特征 PUSH 实测 4.86s/次
    // （2026-10-20 UI 卡顿根因；TD-33 清偿面）。
    std::unique_ptr<dqCommon::PackedFeatureTable> m_featureTable;
    std::unique_ptr<FeatureOverrideLUT> m_featureOverrideLUT;
    std::unique_ptr<Contours> m_contours;                  // M-X X-a3：逐 batch contour LUT 缓存
    std::optional<ThematicSensors> m_thematicSensors;      // M-S S-e：逐 batch 传感器缓存
    dqCommon::ThematicDisplaySensorSettings const* m_thematicSensorSettings = nullptr;  // 缓存失效判据（指针同一性）
    BatchContext m_context;
    // Range stored at creation (Graphic.ts:262-267) — the child graphic's
    // primitives are CachedGeometry whose unionRange is a no-op, so without a
    // stored range a Batch contributes nothing to culling bounds.
    dqGeom::Range3d m_range;
    uint32_t m_featureCount = 0;
    uint32_t m_batchId = 0;
    BatchState* m_batchState = nullptr;  // ← 注册表反指针（setBatchState 注）
    uint32_t m_lastHiliteVersion = 0;
    uint32_t m_lastSubCategoryVisibilityVersion = 0;  // M-N(1)
    uint32_t m_lastAppearanceOverridesVersion = 0;    // M-O(2) I10
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

    // 反指针生命周期闭合（AV 修复轮 2——X-batch-fix2）：本表先于批死亡时
    //（dump 族 teardown 实测序——view[TargetImpl/BatchState] 先析构、瓦树
    // 后析构[M-Q 同款析构序注记的对面形态]）清各批反指针，否则 ~Batch 经
    // 悬垂 m_batchState 调 unregisterBatch = UAF（dump 族 12 败簇 SEH
    // 0xc0000005 崩点——Mirukuru content 打印后崩）。参考 GC 无此面（无
    // 手动析构序）；批先亡→unregister / 表先亡→此处摘除——双向闭合。
    ~BatchState();

    /// Assign a batch ID to the given batch.
    /// Ported from: itwinjs-core BatchState.getBatchId() (line 104-117)
    void assignBatchId(Batch& batch);

    /// Self-deregistration from ~Batch（C++ 生命周期适配——TD-22 同族；
    /// 参考 GC 保活语义在 C++ 无对应物，见 Batch::setBatchState 注）。
    /// 幂等：不在表内时 no-op（reset 后死亡/重复摘除安全）。
    /// Authored: no reference equivalent exists（GC 保活的 C++ 显式所有权
    /// 适配面——av-rootcause-report.md §4.2）。
    void unregisterBatch(Batch& batch);

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
