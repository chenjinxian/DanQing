# M-G 里程碑（zoom-drill 版 dump 采集）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 instances60 的 E2E 覆盖从"合成 BFS 面"转到"真实视口请求面"：采集一条（或多条）沿真实视口 LOD 下潜路径的 zoom-drill dump——视口渐进放大时经 TileAdmin 实例 patch 自然捕获被请求的瓦，使回放侧对账锁在细分派生键域（`-b-1..-b-4-…-…-1` 链）上有真实字节可回放，覆盖从 2/3587 升到 drill 捕获深度。

**Architecture:** ①capture.mjs 增 `--drill` 模式：lookAt 取景到 instances60 几何域（contentRange 实测中心 + 可选边缘目标点——多路径以覆盖多个 octant 分支），再经成熟 zoom 泵（`vp.zoom(undefined, 2)` 每 12s ×2，M-D 已验证）渐进下潜 N 级；**不再用 sweep.js 鸭子类型直请**——每级瓦由真实 DTA 视口的 SelectParent/insertMissing 链自然请求并被 wrapper 捕获。②instances60-drill-v1 入库（provenance 增 `drill: {targetPoints[], zoomCount, levelsReached}` 元数据）。③E2E 对账锁新增 drill 回放场景：断言 drill dump 的全部瓦键 ⊆ 链通集（回放侧同配方下潜同路径——drill 键=视口请求面与回放面同构的证据）+ 覆盖率实测更新。

**Tech Stack:** Node/CDP（仓外工具升级）/ C++20 / GoogleTest。

**Spec:** 本计划自带设计（用户指令"zoom-drill 版 dump 采集"）+ M-F 终审裁定（instances60 采集域=合成 BFS 面 artifact 的论证——本里程碑即"更深覆盖应采 zoom-drill dump"的建议落地）。

## Global Constraints

- §8.2 零网络（工具仓外）；参考仓（tiangong-kaiwu 检出）零源码修改（assets 增量例外）。
- §11.11：drill dump 入库后只读；curate 只删不加；E2E 钉值=首绿实测的钉死比例。
- §11.10：drill dump 与 sweep dump 的关系（互补——sweep=键域全集上限（前缀），drill=视口可达面；对账锁两场景并存）——登记。
- 提交规范/禁破坏性 git 操作同既往。
- 构建/测试（仓库根）：`cmake --build build -j --config Debug --target <t>`；`ctest --test-dir build -C Debug -R "<sel>" --output-on-failure`。

## 现状关键事实（已侦察核实）

- **zoom 泵现成**（`capture.mjs:283-307`）：`--zoom N` → 瓦到达后每 12s `vp.zoom(undefined, 2)` ×2——compatseed-v1 的 7 瓦 LOD 链即其产物（M-D Task 1 验证）。
- **instances60 空间域**（files/6.json 实测）：rootTile.range = `(-88.06,-41.53,-26.91)..(88.06,41.53,26.91)`（项目范围）；**contentRange = `(-88.05,-41.52,-26.91)..(-69.89,-32.96,-21.36)`**（几何在盒的左下角一隅——中心 ≈`(-78.97,-37.24,-24.13)`，区域 ≈18×8.6×5.5m）；tileScreenSize=2048、maximumSize=2048、isLeaf=false。
- **视口语义的下潜路径**（M-F 终审裁定在案）：root(`-b-0-0-0-0-1`, mult=1) → 冷 fit 细分 `-b-1-0-0-0-1` → 再细分 `-b-2-x-x-x-1` 链——**这些正是 M-F(2) 的 2/3587 可达面**；drill 的目标=沿该链下潜到 viewport SSE 允许的最深级（预期 `-b-2..-b-4` 级，几何分辨率 vs tileScreenSize=2048 决定终止点）。
- **采集侧 TileAdmin 实例 patch 已覆盖 viewport 自然请求**（M-D Task 1 的 `generateTileContent` wrapper——viewport 经 IModelTile.ts:91 实例调用即发）。
- **模型文件名含 URL 的注意事项**：`Properties_60InstancesWithUrl2.ibim`——后续采集中 DTA assets 需该文件名原样（M-E Task 1 已拷入）。
- **E2E 对账锁现状**（M-F(2)，f3af5df）：`Instances60FullLoadReconcilesManifestKeys` 九维判据 + 覆盖率 2/3587（root + `-b-2-0-0-0-1` 钻取消耗）；CompatSeed 钻取配方（RpcDumpRenderTest.cpp:459-476 同款确定性钻取——可复用）。
- **drill 深度估计**：根 range 边长 ~176m → d3 子边长 ~22m、d4 ~11m、d5 ~5.5m；几何区 ~18×8.6m 跨度 → d3-d5 内仍含几何；tileScreenSize=2048 像素 vs 子瓦像素尺寸——每 ×2 下潜一级，视口 2000×1400 下 SSE 判定何时不再下潜=实测（d3-d6 区间预计）。

---

### Task 1: capture.mjs --drill 模式 + instances60-drill-v1 采集入库

**Files（仓外 `D:\Github\danqing-rpc-tools\` + DanQing 资产提交）:**
- Modify: `capture.mjs`（增 `--drill <json>` 模式：`{targets:[[x,y,z],...], zoomPerTarget: N}`——逐目标 lookAt 取景后跑 zoom 泵）
- Create: （复用）`rpc-dump.js`（无需改——wrapper 捕获 viewport 自然请求）
- Output: `dumps/instances60-drill-v1/` → 入库 `third_party/tile-sample-assets/rpc-dumps/instances60-drill-v1/`

**Interfaces:**
- Produces: instances60-drill-v1 dump（视口请求面键域——细分派生链为主；provenance 增 `drill` 元数据：targets/zoomPerTarget/levelsReached（各目标实际下潜级数））。

- [x] **Step 1: capture.mjs --drill 实现**（~60 行增量）：
```js
// --drill 模式（在 --sweep/--zoom 之外的第三模式）：
// for (const [x,y,z] of drill.targets) {
//   await cdpEval(`vp.lookAtGlobalLocation(new Point3d(${x},${y},${z}), radius, {fitView: true, animateFrustumChange: false})`);
//   // 等待根瓦到达（stats.tiles 变化）→ 跑 zoom 泵 N 次（复用现有泵逻辑，
//   // 每 ×2 后等新瓦到达计数稳定再下一级——M-D 的"瓦到达计数稳定即完成"信号复用）
// }
// provenance.drill = {targets, zoomPerTarget, levelsReached: {per target}}
```
（实施要点：lookAt 的 API 形态——参考 `IModelConnection.View.lookAtGlobalLocation`（Viewport.lookAtGlobalLocation？——实施时断点打印 `Object.keys(vp)` 定形，或经 `vp.setupFromView`；**以实际可达 API 为准**，勿按记忆写死）；取景 radius 初值=root range 对角线一半；每目标放大 N 级（N=实测到不再下潜为止，上限 6——2000×1400 视口下 SSE 自然终止）。）
- [x] **Step 2: 采集实战**（tiangong-kaiwu 检出 DTA，Properties_60InstancesWithUrl2 已就位 assets）：
  - 目标点组：①几何中心 `(-78.97,-37.24,-24.13)` ②几何盒低角近区 `(-86,-40,-25)` ③几何盒高角近区 `(-72,-34,-23)`——覆盖 octant 分支多样性（多分支路径令不同 (i,j,k) 组合下潜）。
  - 每目标 zoom 泵 ×N（N=6 上限）；**预期键域**：`-b-1-0-0-0-1`、`-b-2-…-1`、更深——**放大链键（-b-0-0-0-0-2/4/8…）不应大量出现**（若出现→drill 取景/SSE 仍触发了放大面，如实报告并分析）。
  - 停止条件：collector 计数稳定（复用"平台期"信号）或 N 用完。
- [x] **Step 3: 统计与校验**：树/瓦/字节/深度分布打印；**键域分析**（逐键解析 mult/depth——确认 drill 键与 manifest 的 sweep 键域关系（drill 键 ⊆ sweep 键域？——sweep dump 的 3587 键是全集前缀，drill 应是其子集的"视口形状"）；放大链混入率统计。全量 sha256 复算。
- [x] **Step 4: 入库**：拷 `instances60-drill-v1/` 入 `third_party/tile-sample-assets/rpc-dumps/` + README provenance（drill 元数据 + sweep vs drill 关系一节）+ DanQing commit（ee3ec7e）。

**Task 1 实测偏差补注（如实，细节见 dump README）**：①三目标 aiming 未达成——任何重定中心取景使后续瓦请求静默归零（danqing-rpc-tools 坑 16，机制未定位），dump 只覆盖默认视图中心（=targets[0]）下潜面；②实测键域 = **100% 放大链** `-b-2-0-0-0-{1,2,4,8,10}`（depth=2 同体积逐倍放大，mult 段 hex "10"=0x10=16）——与计划预期"细分链为主、放大链不应大量出现"相反：该模型每张瓦 sizeMultiplier=1.0，内容到达后子代恒放大分支，视口 SSE 在 ×16 饱和，细分派生链经视口不可得；③"drill ⊆ sweep"被实测推翻（-b-2-0-0-0-10 域外——sweep 合成 cap=×8）——M-F artifact 裁定的交叉验证点成立。

### Task 2: E2E 深化（对账锁新增 drill 场景）+ 收口

**Files:**
- Modify: `samples/DisplayTestApp/tests/RpcDumpRenderTest.cpp`（新增 `Instances60DrillReplaysViewportChain`——drill dump 回放 + 逐键链通断言）
- Modify: `CLAUDE.md`（进度基准行 + TD-25 遗留若有 drill 相关项勾销/追加）
- Test: 同 Files 项

**Interfaces:**
- Consumes: instances60-drill-v1（Task 1）+ 既有 Instances60FullLoad 锁（instances60-v1 sweep dump 驱动——并存）。
- Produces: drill 场景锁（视口请求面=回放可达面的同构证据）+ 覆盖率实测更新。

- [x] **Step 1: RED——drill 场景锁**（判据设计，先写）：

```cpp
// Authored: zoom-drill dump 的视口链同构锁（M-G 设计——2026-09-28 用户指令）。
// 判据：drill dump（视口请求面）回放后，dump 内全部瓦键都进入链通/消费集——
//   ①requested 键集合 ⊇ drill manifest tiles 键集合（回放侧同配方下潜同路径——
//     每键都有请求发出（含 miss 的记录——miss 也计数入"见到"）；
//   ②drill 的 depth≥1 细分键至少 depthMax(dump) 级有字节消费（graphics 提交）；
//   ③最深层键 contentId 实钉（dump 实测最深键——首绿后钉死）入链通集；
//   ④像素锚：首绿实测下界（drill 深潜后内容占比应 ≥ M-F 锁（同模型同几何——
//     更深 LOD = 更细几何，允许不同——实测钉死）。
TEST(RpcDumpRender, Instances60DrillReplaysViewportChain) { ... }
```

（取景/钻取配方：复用 CompatSeed 确定性钻取（:459-476）+ drill dump 的 targets 对应下潜——DumpTileFetcher 已支持任意 manifest（Task 2/3 已吃）；DumpMount 适配（provenance.drill 元数据可忽略——mount 只需 files+manifest tiles）。）
- [x] **Step 2: 跑锁取实测**——首绿后钉死：最深消费键/像素锚/链通计数；若有 drill 键无法消费（不在链通集）→ 分析（dump 键 vs 回放侧 SSE 下潜差异——取景配方未达同级）→ 调配方（非调判据）直至全键链通（目标：drill dump 键 100% 在链通集——这正是"视口请求面=回放可达面"的同构证据）。
- [x] **Step 3: 门禁**：`ctest -R "RpcDump|TileTreeRender|DumpTile|Imdl"` + 全量。
- [x] **Step 4: 收口**：CLAUDE.md——进度基准行（覆盖率 2/3587 → "sweep 2/3587 + drill <实测> 键视口链同构（M-G）"）；TD-25 遗留若有"更深覆盖应采 zoom-drill dump"相关表述勾销/更新；本计划文档入库。
- [x] **Step 5: Commit**：`M-G(2)：drill 回放 E2E 同构锁（视口请求面=回放可达面）+ 收口`

**Task 2 实测判据形态补注（首绿钉值，如实——锁头注释有完整机制锚）**：
- **RED 取证**：drill-only 挂载在存量引擎下链={根 NotFound}、0/5 键——根 NotFound **阻断整树**。按 §11.8 核读参考判定为**移植缺口**（非参考同构）：`maxInitialTilesToSkip` 的 props 载体未接线（TileProps.ts:63 → IModelTileTree.ts:51/:76/:390 `?? 0`；DanQing 原硬编码 0——参考语义下预算=3 使根/d1/d2 恒 canSkip，根 NotFound 不阻下潜）。**按缺口修引擎**（ImdlTreeMetadata 增载体 + 构造器消费 + DumpTileTreeProps 解析——真实 `// Ported from:`），**未走 merged-mount 备案**。
- **链通**：5/5 drill 键全部 Completed（目标 100% 达成——`Instances60DrillReplaysViewportChain`）；最深键 **-b-2-0-0-0-10**（mult=0x10）实钉在链。泵形：kZoom=2^n 迭代 6 级（m1@2、m2@4、m4@8、m8@16、m16@32 连通 + 第 6 级过冲）——m1 于 kZoom=2 以 Visible 干净派生（无 -b-3 细分污染），与活体采集的派生路径同构。
- **miss 集实钉 3 枚**：`-b-1-0-0-0-1`（fit 级 Visible 请求，采集域外——wrapper 竞态+参考跳过语义）、`-b-0-0-0-0-1`（:299-301 NotFound 回退请求根——参考同构机制，采集无根字节）、`-b-2-0-0-0-20`（过冲放大子 mult=0x20——SSE 越过 ×16 后派生，采集域外，机制自洽）。
- **像素锚**：kZoom=8 锚定视图（链上就绪瓦必 Visible 被绘制）——首绿 content=1,197,243 px（帧 42.76%）、质心 (973,700) 对帧心 (1000,700)；判据钉 ≥450000（首绿 0.38×）+ 中央带 ±20%。
- **引擎改动的连带收口（如实）**：①**强制选择帧泵**——NotFound 交付不触发失效级联，:299-301 回退在突发帧停后饥饿（M-F(2) 相 1 实测 log={-b-1} 停 20s）；泵每迭代 InvalidateController 把请求面推到不动点（键级幂等不改请求面）——drill/M-F(2)/M-E/CompatSeed 四锁泵统一；②**M-F(2)/M-E 原钉值全部保持**（根经回退路径 Completed——同键同字节同像素）；③**CompatSeed 锁配方订正**（maxInitialTilesToSkip=6 忠实化后：根恒 canSkip 不被钻取视图请求——参考同构；相序订正为"钻取先于 fit"（d6 细分只能在 d5 内容到达前派生——:282 用当前元数据；fit 先行则 d5 就绪后子代恒放大、d6 永不可达——实测两形态日志在案）；判据演化为 d6+d5 两瓦就绪 + 最深键 -b-6 实钉 + miss 两枚放大过冲（-b-6-0-0-0-{4,2}）+ Completed→graphics 对账）；④计划 Step 1 判据草②"depth≥1 细分键消费"不适用——drill 键域实测为放大链（Task 1 补注），判据按实测形态设计。
- **全量门禁**：见 CLAUDE.md M-G 行（WheelZoomCoalesceTest.AcsDiscSurvivesDeepZoomPixels 失败经 stash 双态对照实锤为环境态呈现族——FBO 全程 48 蓝 vs 物理屏 GDI 探针 0，净基线同败，与 M-G(2) 无关）。

---

## 完成定义（M-G DoD）

1. instances60-drill-v1 入库只读（provenance.drill 元数据 + sweep/drill 关系 README）；
2. `Instances60DrillReplaysViewportChain` 绿——**drill dump 全部瓦键进入链通/消费集**（最深键实钉）；
3. 覆盖率实测更新（CLAUDE.md——sweep 2/3587 之外新增 drill 场景，非简单相加而是场景并集）；
4. 全量 0 失败（环境态族规则）+ TileTreeRender 10/10 + RpcDump 全家。

## 风险预登记

- **drill 深度不达预期**（SSE 在 d2-d3 即停）：如实报告实测深度与机制（tileScreenSize vs 视口参数）——判据随实测形态设计，不降格。
- **放大链混入**：drill 下潜初期冷 fit 可能短暂触发（参考行为同构）——如实统计混入率；若大量混入→取景半径/下潜节奏调配方（仍如实记录）。
- **drill 键与 sweep 键域关系**：若 drill 键 ⊄ sweep 键域（理论上 sweep=前缀应含 drill 全集——除非 sweep 的 mult 继承路径与视口 mult=1 路径分叉——M-F 裁定的核心差异点）→ **如实报告并分析**（这正是 M-F artifact 裁定的交叉验证点）。
- **provenance.drill 字段与 DumpTileTreeProps 兼容**：mount 解析忽略未知 provenance 子字段（向后兼容——已知格式）。
