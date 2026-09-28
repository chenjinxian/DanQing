# M-F 里程碑（子瓦 contentId 派生链修复）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复子瓦 contentId 派生链——瓦内容到达后经参考 `IModelTile.setContent` 语义把 header 描述（sizeMultiplier/emptySubRangeMask）接回瓦自身（含 contentId 覆写），使 instances60 的放大-细分混合树正确展开；E2E 覆盖率从 1/3587 升到实测新覆盖数。

**Architecture:** 根因已取证锁定：参考 `IModelTile.setContent`（IModelTile.ts:134-148）在内容到达时消费 `content.sizeMultiplier`（来自瓦 IMDL header 的 `decodeTileContentDescription`）——①写入 `_sizeMultiplier`（含 ">current 才升"的门控）②**用 `idFromParentAndMultiplier` 覆写瓦自身 contentId**（放大键域）③子代多于 1 时 disposeChildren。DanQing 的 `ImdlTile::readContent` 只接了 desc 的 contentRange/isLeaf，**sizeMultiplier 与 emptySubRangeMask 全丢** → 根瓦恒 m_sizeMultiplier=0 → loadChildren 走细分分支派生 `-b-1-0-0-0-1`（采集域外），而非放大子 `-b-0-0-0-0-2`。修复 = 参考 setContent 语义在 DanQing `Tile::setContent`/`ImdlTile` 的归位 + TileContent 增字段 + E2E 对账锁钉值更新。

**Tech Stack:** C++20 / GoogleTest / instances60-v1 dump（已入库，只读）。

**Spec:** 本计划自带设计（用户指令"子瓦 contentId 派生链修复"）+ 取证锚：`IModelTile.ts:134-148`（setContent）/ `:81`（hasSizeMultiplier）/ `TileMetadata.ts:777-853`（computeChildTileProps——DanQing 已移植）/ `TileMetadata.ts:880-940`（decodeTileContentDescription——DanQing 已移植）。

## Global Constraints

- §0：setContent 语义以 `IModelTile.ts:134-148` 逐行为准；真实 `// Ported from:`。
- §11.11：instances60-v1 只读；E2E 新覆盖钉值=首绿实测的钉死比例（非调参）。
- §11.9：渲染相关改动像素锁门禁（`ctest -R "TileTreeRender|RpcDump|DumpTile|Imdl|SelectTiles"` + 全量）。
- §11.10：contentId 覆写时序（参考在 setContent 即改，DanQing 的 DumpTileFetcher 记录键的时机——若 fetch 以覆写后键重发，请求/回放的键域一致性必须核实并登记）。
- 提交规范/禁破坏性 git 操作同既往。
- 构建/测试（仓库根）：`cmake --build build -j --config Debug --target <t>`；`ctest --test-dir build -C Debug -R "<sel>" --output-on-failure`。

## 现状关键事实（已侦察核实）

- **参考 setContent 三段语义**（`core/frontend/src/internal/tile/IModelTile.ts:134-148`）：
  ```ts
  public override setContent(content) {
      super.setContent(content);
      this._emptySubRangeMask = content.emptySubRangeMask;
      if (undefined !== content.graphic && 0 === this.maximumSize)
          this._maximumSize = this.iModelTree.tileScreenSize;
      const sizeMult = content.sizeMultiplier;
      if (undefined !== sizeMult && (undefined === this._sizeMultiplier || sizeMult > this._sizeMultiplier)) {
          this._sizeMultiplier = sizeMult;
          this._contentId = this.iModelTree.contentIdProvider.idFromParentAndMultiplier(this.contentId, sizeMult);
          if (undefined !== this.children && this.children.length > 1)
              this.disposeChildren();
      }
  }
  ```
- **DanQing 现状**：`TileContent`（dqRender/PublicAPI/dqRender/tile/TileContent.h:27-48）**无** sizeMultiplier/emptySubRangeMask 字段；`ImdlTile::readContent`（ImdlTileTree.cpp:526-530 区）`decodeImdlContentDescriptionHeaderOnly(header)` 仅接 contentRange/isLeaf；`Tile::setContent`（Tile.cpp:132-156）无 mask/mult 消费；`ImdlTile::loadChildren`（:617-641）的 parent.sizeMultiplier=m_sizeMultiplier（恒 0 → 细分 → `-b-1-0-0-0-1`）。
- **采集键域证据**（manifest 实测）：root=`-b-0-0-0-0-1`（V4，mult=1）；depth-0 放大链 `-b-0-0-0-0-{2,4,8,10,20,…,100}`；depth-1 = `-b-1-0-0-0-{2,4,8}`（mult-2/4/8 瓦的细分子代，mult 继承自父 spec）；DanQing 误派生 `-b-1-0-0-0-1`（不在 manifest）。
- **描述来源**：`decodeTileContentDescription`（TileMetadata.ts:880-940，DanQing ImdlDocument.h:61-67 已移植）——sizeMultiplier 语义：magnification 允许+完整+少元素+无曲线 → leaf with mult 1（:910-928）；空/分类器 → leaf（:904-906）。**它是放大树展开的唯一信号源**（树 props 的 rootTile.sizeMultiplier 只定初值，后续每瓦由自身 header 决定）。
- **E2E 对账锁现状**（`RpcDumpRender.Instances60FullLoadReconcilesManifestKeys`，9681b17）：requested=2（根键 Completed + 派生 miss 实钉 `-b-1-0-0-0-1`），像素锚 content=231141/green=56399——**修复后派生 miss 应消失、链通集扩到实测新数**，钉值更新为实测。
- **selectNextLevelMagnification/真实视图的放大上限**：×8（M-E Task 1 分析——视图 selectNextLevelMagnification 挂起去重+有界）；E2E 的驱动面走真实 selectTiles（SSE/SelectParent），放大展开按描述自然终止于合适 LOD。

---

### Task 1: TileContent 增字段 + readContent 全字段接线 + ImdlTile::setContent 归位

**Files:**
- Modify: `dqRender/PublicAPI/dqRender/tile/TileContent.h`（增 `double sizeMultiplier = 0.0;`（0=undefined，与 ImdlTile m_sizeMultiplier 同约定）+ `uint32_t emptySubRangeMask = 0;`）
- Modify: `dqRender/src/tile/ImdlTileTree.cpp`（readContent：desc → content.sizeMultiplier/emptySubRangeMask；`ImdlTile::setContent` override——IModelTile.ts:134-148 逐行归位）
- Modify: `dqRender/PublicAPI/dqRender/tile/ImdlTileTree.h`（ImdlTile::setContent override 声明 + contentId 覆写辅助（ImdlTile 拥有 m_contentId 私有成员））
- Test: `dqRender/tests/ImdlTileTreeTest.cpp`（setContent 语义锁）

**Interfaces:**
- Produces: `ImdlTile::setContent(TileContent) override`——参考三段语义；`ImdlTile` 增 `m_emptySubRangeMask`（参考 `_emptySubRangeMask`，IModelTile.ts:135）+ sizeMultiplier 升门控 + contentId 覆写 + disposeChildren 等价（DanQing `setChildren` 释放）。
- Consumes（Task 2）: 派生链修复后的实际键域行为。

- [ ] **Step 1: 读参考全文**：`IModelTile.ts:78-90`（hasSizeMultiplier/_contentId/_sizeMultiplier 字段语义）、`:134-150`（setContent 全文+disposeChildren 尾部）、`disposeChildren`（同文件）。DanQing 对照：`ImdlTile.m_contentId` 私有成员（ImdlTileTree.h:99）的覆写通道。
- [ ] **Step 2: RED——setContent 语义锁**（ImdlTileTreeTest 追加，桩树+桩瓦+合成 content）：

```cpp
// Ported from: IModelTile.setContent (IModelTile.ts:134-148 — emptySubRangeMask
//              赋值 / sizeMultiplier 升门控 / contentId 覆写 / disposeChildren)。
// Authored: 场景自写（参考无 setContent 直接单测——覆盖在树集成）。
TEST(ImdlTileTreeTest, SetContentConsumesSizeMultiplierAndRewritesContentId) {
    // 桩树 V4 provider（-b- 键域）+ 根瓦 "-b-0-0-0-0-1"（m_sizeMultiplier=0=undefined 态）
    // content{graphic!=null, sizeMultiplier=1.0}（该模型的 header 描述形态）：
    // ① m_sizeMultiplier==1.0 且 hasSizeMultiplier（m_sizeMultiplier>0.0）
    // ② contentId 覆写 = idFromParentAndMultiplier(root,1)（V4 形，钉死值）
    // ③ emptySubRangeMask 写入可观察
    // ④ 预置 2 个假子代 → setContent 后子代被 dispose（children 空）
}
TEST(ImdlTileTreeTest, SetContentSizeMultiplierNeverDowngrades) {
    // 现值 2.0，content.sizeMultiplier=1.0 → 不变（sizeMult > current 门控，:142）。
}
```

（桩树/provider 复用该文件既有形态；V4 键域与实例资产一致。）
- [ ] **Step 3: 运行确认失败**：`ctest -R "SetContentConsumes|SetContentSizeMultiplier"` → FAIL。
- [ ] **Step 4: GREEN 实现**：
  1. `TileContent` 增两字段（默认值=undefined 约定——sizeMultiplier=0.0 即未设，ImdlTileTree.h:35 同约定；emptySubRangeMask=0）。
  2. `readContent`：`content.sizeMultiplier = desc->sizeMultiplier;` + `content.emptySubRangeMask = desc->emptySubRangeMask;`（desc 四字段已全——ImdlDocument.h:33-38）。
  3. `ImdlTile::setContent` override：调 `Tile::setContent` 基类后，按 :134-148 逐行——`m_emptySubRangeMask = content.emptySubRangeMask;`（字段移参前取——注意 TileContent 是 rvalue 已 move 进基类，mask/mult 须在 move 前拷贝本地）；`if (m_graphic && 0.0 == getMaximumSize()) m_maximumSize = tree tileScreenSize;`（**核查现状**：M-B 已做回填——重复/冲突时以参考顺序归位）；`double const sizeMult = sizeMultLocal; if (sizeMult > 0.0 && (!(m_sizeMultiplier > 0.0) || sizeMult > m_sizeMultiplier)) { m_sizeMultiplier = sizeMult; m_contentId = tree.contentIdProvider().idFromParentAndMultiplier(m_contentId, static_cast<uint32_t>(sizeMult)); if (getChildren().size() > 1) setChildren({}); }`
- [ ] **Step 5: 门禁**：`ctest -R "ImdlTileTree|TileTreeRender|RpcDump|DumpTile"` + 全量。
- [ ] **Step 6: Commit**：`M-F(1)：IModelTile.setContent 语义归位（sizeMultiplier/contentId 覆写/mask——IModelTile.ts:134-148）+ readContent 全字段接线`

### Task 2: E2E 覆盖率升级 + 收口

**Files:**
- Modify: `samples/DisplayTestApp/tests/RpcDumpRenderTest.cpp`（对账锁钉值更新：miss 集消失/链通集扩——实测更新断言数字；新增"放大链消费"断言）
- Modify: `CLAUDE.md`（TD-25 遗留勾销派生链差异项 + 进度基准行）
- Test: 同 Files 项

**Interfaces:**
- Consumes: Task 1 的派生修复。
- Produces: `Instances60FullLoadReconcilesManifestKeys` 的修复后钉值（实测驱动）。

- [ ] **Step 1: 先跑现状对账锁**（未改断言前）：`ctest -R "Instances60FullLoadReconciles"`——**预期红灯位置即判据更新点**（派生 miss `-b-1-0-0-0-1` 消失、requested 集变化、树侧零在途终态变化）。记录新实测：requested 键集合/Completed 数/miss 集/像素锚/树侧终态（Ready/NotFound 分布）。
- [ ] **Step 2: 断言更新（实测驱动）**：对账锁改为修复后形态——
  - 链通集仍 ⊆ manifest 且根键在内；**新增**：`-b-0-0-0-0-2`（首个放大子）在链通集（描述消费的直接证据）
  - miss 集：`-b-1-0-0-0-1` **不再出现**；新 miss 集（若有）仍按九维形验（`-b-<d>-<i>-<j>-<k>-<mult>` + 边界）
  - 覆盖率 printf 更新为实测（**预期显著上升**：放大链 -b-0-0-0-0-{2,4,8,…} + depth-1 细分 -b-1-…-{2,4,8} 按真实 LOD 驱动进入消费面——SSE/SelectParent 决定下潜深度，允许部分瓦因"更深 LOD 不需要"而不被请求（判据=凡被请求的都能对上，对不上的有记录——**不是凡 manifest 有的都必须请求**，这与视口驱动语义一致）
  - 像素锚：首绿实测下界（实例上屏像素集合应扩大或不变——60 实例已在 T3 上屏，放大链后内容/视角可能变——实测钉死）
- [ ] **Step 3: 门禁 + 全量**：`ctest -R "RpcDump|TileTreeRender|DumpTile|Imdl"` + 全量 0 失败。
- [ ] **Step 4: 收口登记**：CLAUDE.md——TD-25 遗留清单勾销"子瓦 contentId 派生链差异"项（引用 commit/新覆盖率实测）；进度基准行（覆盖率 1/3587 → 新实测数，注明放大链消费）；TD-25 行 ✅ 行的锁名补 `Instances60FullLoadReconcilesManifestKeys`（M-E(4) 已存在——核对）。
- [ ] **Step 5: Commit**：`M-F(2)：E2E 覆盖率升级（派生链修复后实测钉值）+ TD-25 遗留勾销 + 收口`

---

## 完成定义（M-F DoD）

1. `ImdlTileTreeTest.SetContentConsumesSizeMultiplierAndRewritesContentId` + `SetContentSizeMultiplierNeverDowngrades` 绿（参考三段语义逐行）；
2. E2E 对账锁：派生 miss `-b-1-0-0-0-1` 消失 + `-b-0-0-0-0-2` 入链通集 + 覆盖率实测上升（printf 记录）；
3. 全量 0 失败（环境态族规则）+ TileTreeRender 10/10 + RpcDump 全家；
4. TD-25 遗留勾销派生链项（CLAUDE.md 在案）。

## 风险预登记

- **contentId 覆写 vs DumpTileFetcher 的请求时机**：覆写发生在 setContent（内容到达后）；fetch 请求键是 loadChildren 派生后发出——覆写只影响"瓦自身后续展示/子代派生的父键"，不回溯已发请求。若 DumpTileFetcher 以覆写后键被再次请求（重复请求），走 ITileFetcher 既有路径（manifest 有键即回放）。登记核实即可。
- **disposeChildren 与在途请求**：参考在子代>1 时 dispose——DanQing setChildren 释放 Tile 会触发 TD-22 清扫（在案）——若在途请求 Tile* 悬空，TD-22 的 ~Tile 钩子覆盖（钩子脱钩路径已修）。
- **描述 mult=1 的 leaf 瓦**（:910-928 leaf with sizeMultiplier 1）：hasSizeMultiplier 真但 leaf 真——loadChildren 的 magnification 分支会产子？——按参考 computeChildTileProps（:783-785 `if (parent.isLeaf) return []`）leaf 先短路，**不会**产子。DanQing 同逻辑（:299-300）——无冲突。
