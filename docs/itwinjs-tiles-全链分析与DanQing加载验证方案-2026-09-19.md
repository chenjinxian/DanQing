# itwinjs frontend tiles 全链分析与 DanQing 示例程序加载验证方案

> 日期：2026-09-19。三路并行调研汇总：① itwinjs-core 实现链 ② 测试与数据 ③ DanQing 移植现状。
> 关键结论已抽查复核（AddTileTree/addTilesForUser 零调用者、channel dispatch 不取内容、RenderSystem::get() 空桩）。
> 本文是分析文档，不含代码变更；实施方案见 §4，落地时遵循 CLAUDE.md §0/§5/§11.10-12。

---

## 1. 参考侧（itwinjs-core）实现全景

仓库：`D:\Github\itwinjs-core`。tiles 子系统分两层：`core/frontend/src/tile/`（公共 API）与 `core/frontend/src/internal/tile/`（内部实现），统一由 `core/frontend/src/tile/internal.ts:19-113` 导出。

### 1.1 核心文件清单

| 文件 | 关键类 | 职责 |
|---|---|---|
| `tile/TileAdmin.ts`（1413 行） | `TileAdmin`（:107） | 全局瓦片总管：请求调度（`process` :445 / `processQueue` :829 / `processRequests` :897）、LRU/GPU 上限（`freeMemory` :844）、过期 `pruneAndPurge` :849、`generateTileContent` :684、事件 `onTileLoad/onTileTreeLoad/onTileChildrenLoad` :783-789（构造器 :319-329 注册→`invalidateAllScenes` :581-585） |
| `tile/Tile.ts`（678 行） | `abstract Tile`（:63） | 状态机 `loadStatus`（:271-295）、`loadChildren`（:353-375）、`computeVisibility`（:429-453）、`setContent`（:309-324）；`TileLoadStatus`（:591-604）、`TileLoadPriority`（:626-639） |
| `tile/TileTree.ts` | `abstract TileTree`（:51） | `selectTiles`（:136-142，选完经 `addTilesForUser` 上报 LRU） |
| `tile/TileTreeSupplier.ts` / `TileTreeOwner.ts` / `Tiles.ts` | 树工厂/生命周期/注册表 | `iModel.tiles.getTileTreeOwner(id, supplier)`（Tiles.ts:166-180 按 supplier+id 缓存）；`TreeOwner._load`（Tiles.ts:47-66） |
| `tile/TileTreeReference.ts` | `TileTreeReference`（:46） | 视图侧入口：`addToScene`→`createDrawArgs`（:156-176，内含 `treeOwner.load()` :157）→`draw` |
| `tile/TileDrawArgs.ts` | `TileDrawArgs`（:81） | 选择/绘制上下文：`getPixelSize`（:138）、`computePixelSizeInMetersAtClosestPoint`（:190）、`insertMissing`（:402）、`maximumScreenSpaceError` 默认 16（:279） |
| `tile/TileRequest.ts` / `TileRequestChannel.ts` / `TileRequestChannels.ts` | 请求状态机/通道/注册表 | `dispatch`（TileRequest.ts:80-120）→ 字节→`tile.readContent`→`setContent`；通道 `process`（:232-266）重算优先级→排序→取消→`while (active<concurrency) dispatch`；`httpConcurrency = 6`（TileRequestChannels.ts:57） |
| `internal/tile/IModelTileTree.ts` / `IModelTile.ts` | iModel 树/tile | `_selectTiles`（:435-445）；`IModelTile.selectTiles` 递归（:205-334）；`maxDepth=32`（:418）；`_loadChildren`→`computeChildTileProps` 纯前端计算（:153-169） |
| `internal/tile/IModelTileRequestChannels.ts` | 三级通道 | `getChannelForTile`（:190-192：`tile.requestChannel \|\| metadataCache \|\| cloudStorage \|\| rpc`） |
| `tile/ImdlReader.ts` + `internal/tile/ImdlDecoder.ts`/`ImdlParser.ts`/`ImdlGraphicsCreator.ts` + `common/imdl/ParseImdlDocument.ts` | iMdl 解码栈 | `readImdlContent`（ImdlReader.ts:74-140）：`decodeTileContentDescription`→`parseImdlDocument`→`decodeImdlGraphics`→`system.createBatch`→rtcCenter 包装 |
| `internal/tile/RealityTileTree.ts` / `RealityTile.ts` / `RealityModelTileTree.ts` / `RealityTileLoader.ts` | reality/3D Tiles 体系 | tileset.json 驱动；`selectRealityTiles`（RealityTile.ts:340-390）；loader 按 magic 分发 Imdl/Pnts/B3dm/I3dm/Gltf/Cmpt（RealityTileLoader.ts:195-310） |
| `internal/tile/LRUTileList.ts` | `LRUTileList`（:208） | 侵入式双向链表 + 哨兵分区；`freeMemory`（:321-333） |

### 1.2 完整调用链（帧驱动视角）

```
IModelApp.eventLoop（IModelApp.ts:614-630）每帧：
  viewManager.renderLoop() → vp.renderFrame() ；然后 tileAdmin.process()

Viewport.renderFrame（Viewport.ts:2582-2739）场景失效时：
  clearTilesForUser/clearUsageForUser（:2650-2651）
  → createScene → ViewState.createScene（ViewState.ts:635-639）
  → for ref of getTileTreeRefs() → TileTreeReference.addToScene
  → treeOwner.load() → supplier.createTileTree → requestTileTreeProps RPC（首帧）
  → tree.draw(args) → selectTiles 递归（computeVisibility/屏幕误差判定）
       就绪 → args.markReady + graphics 入 Scene
       太粗 → loadChildren()（iModel tile 纯前端二分；reality 按 contentId 下钻 tileset.json）
       未就绪 → args.insertMissing → SceneContext.missingTiles
  → context.requestMissingTiles()（ViewContext.ts:432-434 → TileAdmin.requestTiles :498-500）

tileAdmin.process()（TileAdmin.ts:445-453）：
  processQueue → channels.swapPending → processRequests（new TileRequest 挂 tile.request，入通道）
  → channel.process → dispatch → tile.requestContent → 字节回来
  → tile.readContent（ImdlReader / RealityTileLoader 解码 → RenderGraphic + createBatch）
  → tile.setContent → setIsReady

重绘级联（四条，全部 → 场景失效 → 次帧重选）：
  ① onTileLoad → invalidateAllScenes（TileAdmin.ts:319-321/581-585）
  ② TileRequest.notify → Viewport.onRequestStateChanged → invalidateScene（Viewport.ts:3082-3084）
  ③ onTileChildrenLoad → invalidateAllScenes（Tile.ts:366-371）
  ④ onTileTreeLoad → invalidateController（Tiles.ts:64 → TileAdmin.ts:322-328）
```

### 1.3 数据格式要点

- **iMdl 二进制**（`core/common/src/tile/IModelTileIO.ts`）：magic `"iMdl"`=0x6c644d69（TileIO.ts:20）；当前版本 Major 37（IModelTileIO.ts:33-45）；`ImdlHeader`（:50-102）→ `FeatureTableHeader`（:107-130，12 字节）→ glTF 段（ParseImdlDocument.ts:1286-1316）。
- **tree props**（requestTileTreeProps RPC → `IModelTileTreeProps`，`core/common/src/TileProps.ts:57-70`）：rootTile/contentId/location/maxTilesToSkip/tileScreenSize（默认 512）/formatVersion 等；treeId 由 `iModelTileTreeIdToString`（TileMetadata.ts:487-532）编码版本+flags+classifier/edges/animation+modelId。
- **子 tile 纯前端计算**（iModel tile 不发请求）：`computeChildTileProps`（TileMetadata.ts:777-853）——magnification 分支或按最长轴二分（3d→8 子、2d→4 子），`emptySubRangeMask` 跳空；contentId 版本化方案 `ContentIdProvider`（:596-721）。
- **leaf 判定**：`decodeTileContentDescription`（TileMetadata.ts:880-940）：空/分类器→leaf；`tolerance ≤ maxLeafTolerance=1.0` 且元素数 ≤ `minElementsPerTile=100` 等条件（:910-928）。
- **iModel tile 内容三级来源**（TileAdmin.ts:684-702）：RPC generateTileContent → ExternalCache（blob storage `tiles/{treeId}/{guid}/{contentId}`）/ Backend（retrieveTileContent RPC）。
- **reality tile 内容**：tileset.json 节点 `content.url` 相对路径拼 baseUrl（RealityDataSourceTilesetUrlImpl.ts:120-133），外部 tileset.json 子树惰性展开（RealityModelTileTree.ts:421-432）。

### 1.4 关键常量（行号出处）

HTTP 并发 6（TileRequestChannels.ts:57）；treeProps 并发 10（TileAdmin.ts:244）；tile 过期 20s/树过期 300s（:303/:306）；GPU 上限桌面默认 1GB（:1322-1326）；maximumScreenSpaceError=16（TileDrawArgs.ts:279）；tileScreenSize=512（TileProps.ts:66）；IModelTileTree.maxDepth=32（IModelTileTree.ts:418）；maxLeafTolerance=1.0m、minElementsPerTile=100（TileMetadata.ts:910/919）；imdl 当前版本 37.0（IModelTileIO.ts:33-45）；TileLoadPriority Primary=20/Context=40/Classifier=50（Tile.ts:626-639）。

---

## 2. 参考侧测试与测试数据

### 2.1 测试清单（`core/frontend/src/test/tile/`，vitest 浏览器模式）

| 文件 | 测什么 |
|---|---|
| `TileAdmin.test.ts`（759 行） | 内存上限配置 + **全链集成**（真 ScreenViewport + MockRender，手动泵 `renderFrame()`+`tileAdmin.process()`，验证 LRU/驱逐/多视口/dispose）+ Cesium |
| `TileRequestChannel.test.ts`（796 行） | 通道状态机全集：调用序列断言（:328）、maxActive 上限（:346）、优先级派发（:399）、取消语义（:454）、动态并发（:515）、ServerTimeout 重试（:614）、onNoContent 换通道（:717） |
| `TileRequestChannels.test.ts`（125 行） | 通道注册表/名唯一/http 惰性/并发级联 |
| `Tiles.test.ts`（211 行） | `iModel.tiles` owner 缓存与 reset |
| `TiledGraphicsProvider.test.ts`（301 行） | `areAllTileTreesLoaded` 状态机、provider 参与 fit 范围 |
| `LRUTileList.test.ts`（333 行） | 纯逻辑（已移植到 DanQing ✓） |
| `ImdlParser.test.ts`（492 行） | worker 解析器缓存 + imdl 文档解析（内联字节） |
| `GltfReader.test.ts`（2395 行） | GLB 头/节点遍历/纹理/扩展（builder 现场合成数据） |
| `RealityTile.test.ts`（615 行） | 重投影、iModelTransform、GLB/JSON 识别、URL 解析 |
| `map/`（23 文件） | 影像/地形 provider 层 |

### 2.2 测试数据形态（关键事实）

**无真实 .imdl 文件、无 .bim 进 frontend 单测。** 全部内嵌合成：
- imdl 二进制 = 内联 `Uint8Array` 十六进制字面量：`ImdlParser.test.ts:173-268`（未压缩 quad tile，"iMdl" magic 开头）、`:284-443`（meshopt 压缩，期望值 :446-447）；`render/webgl/tile-data.ts`（1871 行，`TILE_DATA_1_1` 三变体 rectangle/triangles/cylinder，注意是 v1.1 旧格式，`RenderDisposable.test.ts:214` 有更新 TODO）。
- glTF = builder 函数现场合成（`GltfReader.test.ts:18-83` 的 `makeGlb` 等）。
- 磁盘上唯一 tile 资产 = 地图 capabilities XML（`src/test/public/assets/`）。
- mock/fake 全部 in-file：`TestTile/TestTree/Supplier`（TileAdmin.test.ts:93-254）、`LoggingChannel`（TileRequestChannel.test.ts:187-298）、`TestRealityTileLoader`（RealityTile.test.ts:20-173，几何用 PolyfaceBuilder 现场生成）。

### 2.3 无后端测试策略（DanQing 可直接复用的四层 seam）

1. **BlankConnection**（`test/createBlankConnection.ts:11-16`）：纯内存 IModelConnection，自带 tiles 注册表。
2. **MockRender.App**（`src/internal/render/MockRender.ts:193-209`）：替代 IModelApp.startup，所有 create* 返回 no-op graphic；CI 无 WebGL 也能跑全链逻辑。
3. **Tile 子类化**（主手段）：测试 tile 重写 `requestContent()`（返回手工 promise）与 `readContent()`（返回合成 TileContent），内容永不走网络（TileRequestChannel.test.ts:81-99、TileAdmin.test.ts:137-143）。
4. **TileTreeSupplier 注入**：测试 supplier 的 `createTileTree` 直接返回预构造树。

补充 hook：`overrideRequestTileTreeProps`（TileAdmin.ts:1410-1412，"Strictly for tests"）；内容侧无等价 override——内容一律靠第 3 条绕开。

**对 DanQing 的含义**：LRU/通道状态机/内存驱逐/supplier 缓存等全部行为测试零网络依赖，可离线移植。

### 2.4 DTA 的 tile 验证手段

DTA 仅支持 standalone（本地 .bim + 本地后端产 tile）。活体验证入口：`GenerateTileContentTool`（`src/frontend/TileContentTool.ts:13-65`，key-in 触发 generateTileContent → ImdlReader 全往返）；`TileLoadIndicator`（numReadyTiles 进度条）；姊妹 app `display-performance-test-app` 逐 view 产 PNG + readPixels（最接近"tile 渲染验证 harness"，但仍依赖真后端）。

---

## 3. DanQing 移植现状：两端各有一截、中间全断

### 3.1 已移植资产

| 组件 | 状态 |
|---|---|
| `LRUTileList` | **完整** + 13 测试（dqRender/tests/LRUTileListTest.cpp）；但 TileAdmin 不持有它（ported-but-uncalled，TD-10 已登记 map 偏差） |
| `TileContent`/`TileLoadStatus`/`TileLoadPriority` | 完整（枚举对齐） |
| `ITileFetcher` DI 层 | 完整：NullTileFetcher（默认，立即 onError）/ QtTileRequestFetcher（dqApp，Application::Startup :75-78 注入）；ITileFetcherTest 已建 |
| `TileAdmin::process` 帧泵 | 已接入 `Application::EventLoop`（dqApp/src/Application.cpp:212），顺序对齐参考 |
| `Tile`/`TileTree`/`RealityTile`/`RealityTileTree`/`GltfTile(Tree)` | 骨架~部分（详见下） |
| 像素级真窗口 harness | 成熟：DisplayTestAppDtaTest + ReadFrameForTest（Viewport.cpp:1930-1941）+ 位置断言先例（GltfStandardViewTest.cpp:250 起） |

### 3.2 五断点（本次已逐条复核）

| # | 断点 | 证据 |
|---|---|---|
| 1 | **树进不了视口**：无 TileTreeReference/Owner/Supplier；`Viewport::AddTileTree`（Viewport.h:432 / Viewport.cpp:1299）全仓零调用者 | grep 确认：仅定义，无调用 |
| 2 | **选中 tile 的请求进不了 admin**：`args.requestedTiles` 有生产端（RealityTileTree.cpp:74/89）但无消费端；`TileAdmin::addTilesForUser`（TileAdmin.cpp:122）零调用者；无 clearTilesForUser/requestMissingTiles 等价物 | grep 确认 |
| 3 | **channel 从不取内容**：`TileRequestChannel::process` dispatch 后直接 erase（TileRequestChannel.cpp:36-54，注释自认 "In a real implementation, we'd track active requests separately"）；`Tile::requestContent()` 全仓零生产调用者 | 已读源码确认 |
| 4 | **decode 产不出图形（架构性）**：`RealityTile::readContent`（:179）/`GltfTile::readContent`（:72）用全局 `RenderSystem::get()` = NullRenderSystem 空桩（DqRender.cpp:76 起）；真实系统是 per-viewport `RenderPipeline`（Viewport.cpp:296-308），tile 层拿不到。参考机制：`TileTree.system` 持有系统、readContent 经参数链传入（RealityTileLoader.ts:195） | grep+源码确认 |
| 5 | **无加载后失效级联**：`onTileLoad/onTileTreeLoad/onTileChildrenLoad` 已声明（TileAdmin.h:74-76）但从不 raise 也无订阅者 | grep 确认零 Raise |

次级问题（验证多层树前必须处理）：`CreateScene` 的 TileDrawArgs 填充简化——eyePos 取视域原点而非相机眼点（Viewport.cpp:1721-1724）、`frustumPlanes` 从不填充（无视锥裁剪）、pixelSizeRatio=extents.y×0.5 近似（:1728-1732）；`GltfTileTree` 构造函数建 rootTile 局部变量却从不 `setRootTile`（GltfTileTree.cpp:25-30，基类持 nullptr → draw 恒空返回）；`RealityTileTree` 用手写字符串扫描解析 JSON（RealityTileTree.cpp:111-338，参考用 JSON.parse，属自创解析隐患）；`TileTree::prune` 空转、`pruneAndPurge/freeMemory` no-op（TileAdmin.cpp:140-151）。

### 3.3 示例程序现有内容装载模板（全部走 decorations，无一走 tile 通道）

- **GltfDecoration**（最完整模板）：`View3DInventor::loadGltf`（samples/.../View3DInventor.cpp:409-473）→ `InstallGltfDecoration`（dqApp/src/GltfImport.cpp:21-53）→ BuildGraphic 经 `vp.createGraphicFromPolyface/createBatch/createGraphicOwner`（GltfDecoration.cpp:50-222，走视口 RenderPipeline）→ 每帧 CollectDecorations 收集。
- **Grid**：ViewState::Decorate → GridDecorator → `vp.createPlanarGrid`（Viewport.cpp:334-346）。
- **DecorationGeometryExample**：PolyfaceBuilder → createBatch 多特征 → GeometryDecorator。
- **harness 驱动模式**：栈上 View3DInventor + resize/show + spin3 事件泵 + loadXxx + synchWithView + ReadFrameForTest 整 FBO 回读 + contentBBox/位置断言 + BMP dump 目检。

---

## 4. 加载验证方案

> **2026-09-19 第二轮修正**：本节初版基于 `core/frontend` 消费子系统单轮调研；`@itwin/frontend-tiles` 包（见 §5）读完后的关键修正——① 参考侧"无后端看 iModel"的官方路径就是 frontend-tiles（静态 tileset.json + imdl 内容），其进入视口的 seam 是 **`SpatialTileTreeReferences.create` 工厂替换**，不是 AddTileTree 式旁路；② batched tile 的**内容仍是 iMdl**（`BatchedTileTree.decoder = acquireImdlDecoder`），3D Tiles JSON 只是信封——所以 ImdlReader 解码栈（路径 B 的核心）在两条路径下都绕不开，区别只在树来源与内容获取方式。

### 4.1 两条路径对比

| | 路径 A：离线 3D Tiles（reality）树 | 路径 B：iModel imdl 全链 |
|---|---|---|
| 数据 | 最小 tileset.json + b3dm（内嵌 GLB），本地文件 | imdl 字节（可复用参考内联夹具）+ tree props JSON |
| 需移植 | 接通五断点（共享基建）+ 本地文件 fetcher | 五断点 + ImdlReader/ImdlDecoder/ParseImdlDocument/ImdlGraphicsCreator/IModelTile(Tree)/PrimaryTileTree/Tiles 注册表/TileTreeReference/Owner（数千行）+ requestTileTreeProps override |
| 参考测试锚 | TileRequestChannel.test.ts、TileAdmin.test.ts、RealityTile.test.ts | + ImdlParser.test.ts、Tiles.test.ts、TiledGraphicsProvider.test.ts |
| 风险 | 小-中（DanQing 已有 RealityTileTree/b3dm 骨架） | 大（且 iModel tile 本质依赖后端语义，离线只能走测试 override 通道） |
| 价值 | 验证"选择→请求→解码→上屏→重绘"全链 + 基建落地 | BIM 主战场语义（feature table/batch/element 拾取） |

**推荐：先 A 后 B。** 路径 A 修复的五断点（TileAdmin 接线/通道状态机/RenderSystem 注入/失效级联）是路径 B 也必须依赖的共享基建；A 落地后 B 单独立项。

### 4.2 路径 A 步骤分解（每步：改动点 + 参考锚 + 判据）

> **2026-09-21 状态：P0-P6 全部完成，RED→GREEN 双向验证通过**（`TileTreeRender.MinimalTilesetRendersColoredBoxes` PASSED：红@左/绿@中/蓝@右上屏，位置断言全过；回归门 DisplayTestAppTest 202/202 + 相邻 DtaTest 单跑全绿）。
>
> 实施落点：P0 资产（`third_party/tile-sample-assets/minimal/`，生成器+tileset.json+root.b3dm+root.glb 诊断副本）；P1 `Viewport::AddTileTree` 首个调用者 + `RealityTile.h/RealityTileTree.h` 提升到 PublicAPI；P2 `FileTileFetcher`（dqApp，http 透传+本地文件，`Application::Startup` 注入）；P3 `CreateScene` 帧首 `clearTilesForUser` + 帧尾 `addTilesForUser`（Viewport.ts:2650-2656 锚）；P4 通道真派发（dispatch→`tile.requestContent`+active 跟踪+settle；完成汇点 `TileAdmin::deliverTileContent/reportTileFetchError` 对齐 TileRequest.ts:156-195）；P5 `TileTree::setRenderSystem` 由 AddTileTree 注入（参考 system 显式传参链的 per-viewport 适配，禁全局单例）；P6 Viewport 订阅 `onTileLoad`→`InvalidateScene`（Viewport.ts:3082-3084 锚）+ `RealityTile::readContent` 补纹理上传（对齐 GltfReader resolveTextures→createTexture）。
>
> 过程中发现并登记 **TD-17**：无 UV/无纹理纯色渲染路径缺陷（两链 `createGraphicFromPolyface` defaultColor-only 路径全黑/错色；PGV/VAO 探针证明顶点/VBO 全对，缺陷在着色分支）——资产第 2 版改走纹理路径绕行。诊断探针 `DANQING_TILE_TRACE`（[TILE]：CreateScene 选择/graphics/requestContent/ReadContent 各环节）随代码入库（§13.1 惯例）。
>
> 遗留（后续 pass）：P7 取景正确性（eyePos=视域原点而非相机眼点、frustumPlanes 恒空、refine REPLACE 语义）——多层 LOD 树前置；P8 LRU/过期真实现；忠实路径（TileTreeReference/Owner/Supplier + SpatialTileTreeReferences.create 工厂 seam，见 §5.5）；路径 B（ImdlReader 栈移植，§5.7 档 2 夹具驱动）。
>
> **2026-09-21 P7 完成注记**：多层 LOD 树打通（`TileTreeRender.LodTilesetRendersChildren` GREEN，红/蓝 child 下钻上屏位置断言全过）。落点：①`RealityTileTree.cpp` 手写 JSON 扫描器重写为**递归下降结构化解析**（根因修复：旧扫描器把 children 的第一个 content 归属给 contentless root → root 冒领 child-red 内容、蓝 child 永不加载——参考机制=JSON.parse 结构化后逐层取字段）；②`Tile::hasContent()` 虚方法 + `TileTree::selectTilesRecursive` descend 语义（无 content 中间层恒下钻，参考 RealityTile.ts:340-390）；③SSE 判定对齐参考公式 `SSE = GE/pixelSize > 16`（RealityTile.ts:535-542 + TileDrawArgs.ts:279 `kMaximumScreenSpaceError`）；④`pixelSizeRatio` 量纲修正为 world/pixel（`extents.y / viewport height`，TileDrawArgs.ts:138 getPixelSize）；⑤**Viewport teardown 释放 tile 内容**（`TileTree::freeContents` + `Viewport::Shutdown` 调用——GL 生命周期倒置修复：树活得比视口久时 graphic 析构触碰已死 driver，SEH 0xc0000005，崩溃栈经 dbghelp 符号化定位 ~PolyfaceGraphic）。资产：`minimal` GE 100→0.01（SSE 公式联动修正）+ 新建 `minimal-lod/`（两层，§11.11 不突变既有资产）。回归门：DisplayTestAppTest 202/202 + 相邻 DtaTest 单跑全绿。P7 残余（frustumPlanes 视锥裁剪、refine REPLACE 的父隐藏、eyePos 相机视图修正）随多层视锥测试推进。
>
> **2026-09-21 P7c 完成注记（视锥裁剪）**：`TileTreeRender.FrustumCullsOffscreenChildren` GREEN——判据 = dispatch 计数（取景只含红半边，`totalDispatchedRequests` 前后差必须 =1；裁剪缺失时 =2）。落点：①`dqCommon/FrustumPlanes.h` 全新移植（`Ported from: core/common/src/geometry/FrustumPlanes.ts`——5 平面叉积 + front 由 back 导出的退化规避、球快测 + 8 角点测的 `computeContainment`，Npc 角序与参考一致已核对）；②`TileDrawArgs.frustumPlanes` 成员落地（float[6][4] 占位替换为真类型），`CreateScene` 每帧从 `getFrustum(world)` 构造（TileDrawArgs.ts:93-96 锚）；③`RealityTileTree::selectTile` 剪枝（`computeContainment==Outside` 直接 return——不请求不显示，RealityTile.ts:516-527 isFrustumCulled 语义）。RED→GREEN 双向验证；回归门全绿（DisplayTestAppTest 202/202）。
>
> **2026-09-21 P7d/P7e/P8 完成注记（计划任务全部收官）**：
> - **P7d（REPLACE + ancestor 顶替）**：三层资产 `minimal-replace/`（root 绿板 GE=0.1 + 红/蓝 children GE=0.01，SSE 窗口设计见生成器注释）；`ReplaceRefinementSwapsParentForChildren` 锁定远景只显父（dispatch=1）/近景子替父（绿消失）。实现 = 选择链对齐参考重构：`TileTree::computeVisibility` 虚方法（Tile.ts:429-453 形态，替代 selectTile/shouldRefine 对）+ `selectTilesRecursive` 带 `closestDisplayableAncestor` 参数（BatchedTile.ts:76-110——TooCoarse 有子只递归不显示自己；分支不可用时 missing + 最近已加载祖先顶替，消除细化空窗）。重构暴露并修复真缺口：**missing 接线只报 readyTiles**——CreateScene 现把 requestedTiles 并入 selected 上报（参考 requestMissingTiles 通道，Viewport.ts:2656）。
> - **P7e（eyePos）**：CreateScene 改用 ViewingSpace::getEyePoint（TileDrawArgs.ts:34-79 锚）；透视 SSE（computePixelSizeInMetersAtClosestPoint，TileDrawArgs.ts:190-206）登记 TODO（正交近似覆盖现有全部测试，待相机开启的 tile 测试驱动）。
> - **P8（内存预算驱逐）**：`TileAdmin::freeMemory` 真实现（预算默认 1GB=TileAdmin.ts:1322-1326 桌面 default；`setMaxTotalTileContentBytes(0)`=不限；process 每帧调，TileAdmin.ts:451-453）+ 单测 `TileAdminMemoryTest`（Ported from TileAdmin.test.ts "enforces memory limits" 的 demote-then-evict 模式——**selected 分区不驱逐**是参考语义，须先 clearTilesForUser 降级）。连带修复真 bug：**~Tile 不通知 LRU**（悬空指针跨测试 SEH——现对齐 Tile.dispose→onTileContentDisposed，Tile.ts:160-166；`TileAdmin::hasInstance` 防 static 析构序）。pruneAndPurge 的按时间过期（tree.prune/usageMarker）登记后续（需 usage-marker + 子树重建机制配套，DanQing children 解析期建好删了不可重建）。
> - **脚手架标注**：`Viewport::AddTileTree` 注释标明临时地位与忠实替换路径（TileTreeReference/Owner/Supplier + SpatialTileTreeReferences.create 工厂 seam，PrimaryTileTree.ts:601-606 + SpatialViewState.ts:109 + FrontendTiles.ts:215 替换点）。
> - **遗留待查**：dqRenderTest 全量 13 项 RenderSmokeTest/PlanarGridRender/OrientationGt 失败（本 session 首跑无本机基线；失败模式=纯色 (0,255,0) 未渲染，TD-17 家族嫌疑；与 tile 改动零 API 交集——RenderSmokeTest 直连渲染层不经 tile/dqApp）。
> - **忠实路径规模评估**（下一个大 pass，独立立项）：TileTreeReference.ts（352 行）+ TileTreeOwner.ts（48）+ Tiles.ts（245）+ TileTreeSupplier.ts（48）+ DisclosedTileTreeSet.ts + PrimaryTileTree.ts 的 SpatialRefs 工厂族（~600）+ ViewState.getModelTreeRefs/createScene 接线 + TiledGraphicsProvider（应用注入通道）。约 1300+ 行 TS→C++，且需决定 iModel/tiles 注册表在无仓外数据层时的宿主（BlankConnection 侧）。完成后 AddTileTree 脚手架退役。
>
> **2026-09-22 忠实路径 F1-F6 完成（TileTreeReference 工厂 seam 立项交付）**：
> - **F3 注册表**：`dqApp/PublicAPI/dqApp/tile/` 新层——`TileTreeOwner.h`（接口；iModel 字段省略+登记）、`Tiles.h/.cpp`（getTileTreeOwner/resetTileTreeOwner/dropSupplier/forEachTreeOwner + TreeOwner 状态机 NotLoaded→Loading→Loaded/NotFound，NotFound 弃树+仅 Loaded raise onTileTreeLoad——两处登记适配）。单测 `TileTreeRegistryTest`（**Ported from Tiles.test.ts**：owner 缓存/load-一次-事件/NotFound 语义/reset 只处置目标/dropSupplier——6 项中 5 项）。
> - **F4 引用层**：`TileTreeReference.h/.cpp`（getTreeOwner 抽象+addToScene→createDrawArgs→draw 链+unionFitRange/collectStatistics/isLoadingComplete/computeTransform/computeWorldContentRange；tooltip/decorate 等未移植面登记）、`DisclosedTileTreeSet.h/.cpp`（直接移植）、`SceneContext.h`（轻量 ViewContext 子集：viewport 级 TileDrawArgs 模板+graphics 出口+missing 收集；**批式上报等价**已登记）。
> - **F5 工厂 seam + 链切换**：`SpatialTileTreeReferences.h/.cpp`（**可覆写工厂**——TS 命名空间函数赋值 → `setCreateOverride/clearCreateOverride`，FrontendTiles.ts:215 替换点的 C++ 对应物；默认空 refs——DanQing 无 per-model 树生产，登记）；`SpatialViewState` 构造接 `SpatialTileTreeReferences::create`（SpatialViewState.ts:109 锚）+ `ForEachModelTreeRef`（getModelTreeRefs :544-548 锚）；**Viewport::CreateScene 切换为忠实链**：view refs（工厂 seam）→ providers（应用通道）→ scaffold refs（AddTileTree 桥接，`SimpleTileTreeReference`/`DirectTileTreeOwner`）→ addToScene → SceneContext 收集 → 批式上报+foreground。`AddTiledGraphicsProvider/Drop/Has`（Viewport.ts:1729-1745 锚）。
> - **F6 验证**：`SpatialTileTreeReferencesFactory.OverrideReplacesDefaultRefs`（**seam 覆写证明**——frontend-tiles initializeFrontendTiles 的 DanQing 等价：setCreateOverride 后 refs 来自注入、clear 恢复默认空）；`TileTreeRender.ProviderChannelRendersTileset`（**provider 通道窗口端到端**——红/蓝像素断言全过）。**全回归**：TileTreeRender 5/5（全部走新链）、Registry+Factory 6/6、DisplayTestAppTest 202/202、相邻 DtaTest、dqRenderTest tile 套件 8/8。
> - **遗留**：参考的 model-selector 驱动 SpatialRefs 族（PrimaryTileTree.ts:608-861，per-model PrimaryTreeReference）待 per-model 树生产（imdl 链）落地时移植——seam 已就位（覆写工厂即其宿主）；AddTileTree 脚手架保留（桥接走同一链，退役条件改为“示例全部迁 provider/seam 后”）。

- **P0 像素回归先行（§5(g)/§11.11）**：按 GltfStandardViewTest 模板新建 `TileTreeRenderTest`（DisplayTestAppDtaTest 目标）：真窗口 + 加载最小 tileset + spin 泵至加载收敛 + ReadFrameForTest + **位置断言**（内容在哪一侧/朝向）。先 RED（断链现状画面无内容）再 GREEN。
- **P1 树进视口**：示例新增"Load Tileset"入口：ifstream 读 tileset.json → `RealityTileTree::loadTileset` → `vp.AddTileTree(tree)`（已有，零调用者的最小路径）。忠实路径（TileTreeReference/Owner + ViewState.createScene）后续按参考 TileTreeReference.ts:46-352 补。
- **P2 本地内容获取**：QtTileRequestFetcher 不支持 `file://` → 新建 `FileTileFetcher`（dqApp 层，Authored 平台适配，注释登记：Qt NAM 无 file scheme；对应参考 standalone 由本地后端供 tile 的角色），Application::Startup 按 scheme 分流或示例侧注入。
- **P3 missing-tile 接线**：RenderFrame createScene 段帧首补 `clearTilesForUser/clearUsageForUser`（参考 Viewport.ts:2650-2651）；`args.requestedTiles` → `TileAdmin::requestTiles/addTilesForUser`（参考 ViewContext.ts:421-434、TileAdmin.ts:498-500/514-533）。
- **P4 通道状态机重写**：`TileRequestChannel::process` 按参考 :232-266 重写（重算优先级→排序→取消无人要→while active<concurrency dispatch）；`TileRequest::dispatch` 按参考 TileRequest.ts:80-120 接通 `tile.requestContent`（经 channel 转发 :305-306）；active 集合跟踪 + 完成回调 `handleResponse`（readContent→setContent）。异步模型对齐：DanQing fetcher 轮询式（processCompleted 由 TileAdmin::process 每帧调），状态迁移在回调投递时发生——与参考 promise settle 语义对应。RED-GREEN：先移植 TileRequestChannel.test.ts 状态机用例。
- **P5 RenderSystem 注入（架构性）**：按参考机制（system 为显式参数链：TileTree 持有 → readContent 传入，RealityTileLoader.ts:195），TileTree 构造/AddTileTree 时由视口 RenderPipeline 注入。**禁止** `RenderSystem::setInstance` 全局单例（多视口互踩；CLAUDE.md 已明 DTA 不装全局单例）。
- **P6 失效级联**：`setContent` 后 raise `onTileLoad`（参考 TileAdmin.ts:759-764/319-321）；Viewport 订阅 → `InvalidateScene`（参考 Viewport.ts:3082-3084）；`loadChildren` 完成 raise `onTileChildrenLoad`（Tile.ts:366-371）。否则画面停在空场景。
- **P7 取景正确性（多层树前置）**：TileDrawArgs 填充按参考 TileDrawArgs.ts:138/190 修（相机眼点、frustumPlanes、pixelSizeRatio）；`refine REPLACE`/`parentsAndChildrenExclusive` 语义。单层固定视角验证可推迟。
- **P8 LRU/过期（可推迟）**：LRUTileList 接入 TileAdmin（参考 TileAdmin.ts:182/516-520/846）。

### 4.3 测试数据方案

新建专用资产（§11.11 禁止原地突变既有资产）：`third_party/tile-sample-assets/minimal/tileset.json` + `root.b3dm`（28 字节头 + feature table JSON + batch table + GLB 段）。GLB 用既有不对称标记资产 BoxTexturedDots 体系（色点/缺口角）以支持位置断言；b3dm 包装用一次性 Python 脚本生成并随资产入库（脚本留档可复现）。DanQing 侧 `RealityTile::readContent` 已解析 b3dm 头 + GLB 交 `GltfReader::LoadFromMemory`（RealityTile.cpp:84-173），与资产形态匹配。

### 4.4 单元测试移植清单（§5 来源优先级，全部有参考）

1. `TileRequestChannel.test.ts`（796 行）→ `dqRender/tests/TileRequestChannelTest.cpp`——通道状态机全移植（LoggingChannel 同构 C++ 复写）。
2. `TileAdmin.test.ts` 内存/驱逐段 → 需 MockRender seam 的 DanQing 对应物（可先用 mock fetcher + 假 tile 复写 §2.3 第 3 条策略）。
3. `LRUTileList.test.ts` → 已移植 ✓。
4. `RealityTile.test.ts`（visibility/URL 解析段）→ 后续。
5. harness 集成像素锁 → Authored（§5(g) 授权，注释记录复现配方与证据链）。

### 4.5 规则锚点与风险

- **§0/§11.8**：五断点修复全部照参考机制（行号已定位），禁止自创替代算法；`RealityTileTree` 手写 JSON 扫描与 `computeScreenSize` 自研近似属存量自创隐患，触及即按参考换写（JSON 解析器选型需架构决策或批准偏差登记）。
- **§11.10**：本次诊断本身是 ported-but-uncalled 的教科书案例（LRUTileList 完整无人持有、AddTileTree 零调用者、onTileLoad 从不 raise）——移植完成度以调用链为单位验收。
- **§5(g)/§11.11**：像素锁先行；位置断言钉住朝向；新建专用测试资产。
- **§11.12**：修复后全量复扫（八视图/缩放/resize 既有像素回归不得回退）；交付前全量重建核对 exe 时间戳。
- **GltfTileTree 构造 bug**（rootTile 从不安装）登记为存量缺陷，路径 A 不依赖它（走 RealityTileTree），修不修独立决策。

---

## 5. @itwin/frontend-tiles 专项（第二轮调研，2026-09-19）

> 第一轮调研的主体是 `core/frontend` 的 tile **消费**子系统；frontend-tiles 是独立扩展包（`extensions/frontend-tiles/`，源码 ~2600 行 + 测试 ~900 行），本轮直接通读全部核心文件。**定性修正**：它不是"前端生成 tiles"——README 自述 "experimental **alternative technique for visualizing** the contents of an iModel"。tiles 由 **mesh export 服务**（developer.bentley.com/apis/mesh-export）离线预发布，本包只负责**消费静态 tileset 并替换默认 RPC 链**。零 core-backend 依赖、无 worker（peerDeps 仅 core-bentley/core-common/core-frontend/core-geometry，package.json）。

### 5.1 入口 seam：一行替换全局工厂

`initializeFrontendTiles`（FrontendTiles.ts:195-216）的全部副作用 = 校验 options + **:215**：
```ts
SpatialTileTreeReferences.create = (view) => createBatchedSpatialTileTreeReferences(view, computeUrl, nopFallback);
```
被替换的 seam 本体是 core-frontend 的可覆写命名空间函数（`core/frontend/src/internal/tile/PrimaryTileTree.ts:601-606`），由 `SpatialViewState` 构造器调用（`SpatialViewState.ts:109`：`this._treeRefs = SpatialTileTreeReferences.create(this)`）。options 默认：`maxLevelsToSkip=4`、`enableEdges=false`、`useIndexedDBCache=false`（FrontendTiles.ts:186-190）；`computeSpatialTilesetBaseUrl` 缺省走 mesh export 服务查询（`obtainIModelTilesetUrl`，GraphicsProvider/GraphicsProvider.ts:39-67）。

### 5.2 发现与异步占位（Proxy 模式）

`createBatchedSpatialTileTreeReferences`（BatchedSpatialTileTreeRefs.ts:316-347）：per-iModel 缓存 `iModelToTilesetSpec`（:278，onClose 清理）；`fetchTilesetSpec` 用裸 `fetch` 拉 `{baseUrl}/tileset.json`（:280-295，URL search 透传）。
- **加载中**：返回 `ProxySpatialTileTreeReferences`（:200-276）——迭代只产 `ProxyTileTreeReference`（owner 永不加载，保证 `areAllTileTreesLoaded` 不早熟，:170-196）；spec 落地后 `setTreeRefs` 换真实现 + `invalidateSymbologyOverrides` + **`invalidateScene()`**（:226-235）。
- **无 tileset**：默认回退 `createSpatialTileTreeReferences(view)`（原 RPC 链，:340）；`nopFallback=true` 则空 refs。
- **混合模式**：不在 tileset 里的模型走 `_excludedRefs = createSpatialTileTreeReferences(view, includedModels)`（:46）——batched 与 RPC tiles 并存。

### 5.3 tileset 格式与树/瓦片类

- **信封**：3D Tiles `Tileset` schema + **`BENTLEY_BatchedTileSet` 扩展**（BatchedTilesetReader.ts:32-41）：`models: { [modelId]: { extents, viewFlags? } }`（模型→元数据映射，fit 范围与按模型显隐的依据）。boundingVolume 支持 box（12 浮点：中心+三轴向量）/sphere（:88-110）；节点 transform 列主序 16 浮点（:112-121）；`contentId = content.uri`；`maximumSize = 8 × maximumSizeFromGeometricTolerance(range, geometricError)`（:158-165，**maximumSizeScale=8**，注释自述 "geometric errors seem far too small"）。
- **树**：`BatchedTileTreeSupplier`（cache key = baseUrl+modelGroups guid+script，BatchedTileTreeSupplier.ts:27-31）→ `iModel.tiles.getTileTreeOwner`（:52-54）；`BatchedTileTree extends TileTree`（BatchedTileTree.ts:30）：构造即 `acquireImdlDecoder({type: Primary, is3d: true, containsTransformNodes: false, noWorker: !decodeImdlInWorker})`（:47-55）——**tile 内容就是 iMdl，复用消费子系统同一解码栈**；`viewFlagOverrides` 默认强制 SmoothShade+无边（:75-77）；`_selectTiles` 直接 `rootTile.selectTiles`（:80-84）。
- **瓦片**：`BatchedTile extends Tile`（BatchedTile.ts:28）：
  - 不可跳过层级：`_unskippable = (depth % maxLevelsToSkip) === 0`（:43，root 无内容不占名额 :42）——免驱逐 + 必显示。
  - `selectTiles`（:76-110）：OutsideFrustum 剪掉；TooCoarse 且（就绪或可跳过）→ markUsed/markReady + `loadChildren` + 递归；Visible 未就绪 → `args.insertMissing(this)`；显示最近可显示祖先（closestDisplayableAncestor）。
  - **子 tile 纯本地展开**：`_loadChildren` 遍历内嵌 `childrenProps` → `reader.readTileParams`（:112-131），零请求。
  - 内容获取：`requestContent` = `new URL(contentId, baseUrl)` + search 透传 + `_localCache.fetch`（:142-147；LocalCache = PassThroughCache 或 IndexedDBCache，IndexedDBCache.ts:10-22）；专用通道 `"itwinjs-batched-models"` **并发 20**（:135）。
  - 解码：`readContent` → `decoder.decode({stream, system, modelGroups, tileData:{ecefTransform, range, layerClassifiers}})`（:149-170，**system 显式传参**）；`transformToRoot` 包 GraphicBranch（:172-181）；失败静默 `{isLeaf:true}`（:184-186）。
  - `BatchedTileContentReader`（3D Tiles 1.1 glTF+EXT_structural_metadata 特征表读取器）存在但**官方注释 "currently unused"**（BatchedTileContentReader.ts:19）——当前产物全部是 imdl。
- **模型分组**：`groupModels`（ModelGroup.ts:135-148）按显示设置等价性（clip/planProjection/timeline/displayTransform/viewFlags）分组；`BatchedModelGroups.guid` = 各组 CompressedId64Set 以 "_" 连接（BatchedModelGroups.ts:126），作为树缓存 key 的一部分；分组在场景失效/显示样式事件时重估（:44-48/56-65）。每组一个 `BatchedTileTreeReference`（+时间线每 transformBatchId 一个，BatchedSpatialTileTreeRefs.ts:107-114），承载 planProjection 高程/overlay、displayTransform、timeline 动画、viewFlagOverrides；`getFeatureAppearance` 拦截未查看模型的外观（BatchedTileTreeReference.ts:92-105 → 模型级显隐过滤）。

### 5.4 测试与数据（frontend-tiles 包自身）

| 文件 | 内容 |
|---|---|
| `test/FrontendTiles.test.ts`（28 行） | 仅 options 初始化默认值/自定义值——琐碎 |
| `test/ModelGroup.test.ts`（243 行） | `groupModels` 分组逻辑，in-file 假 GroupingContext（:26-65） |
| `test/ModelGroupDisplayTransforms.test.ts`（68 行） | 显示变换缓存 |
| `test/IndexedDBCache.test.ts`（145 行） | mocha+sinon+chrome（certa） |
| `test/GraphicsProvider/*.test.ts`（280+228 行） | mesh export 服务 URL 查询逻辑：sinon stub fetch（:37-50）+ BlankConnection 子类 TestConnection（:16-35） |

**关键事实：全仓零 `BENTLEY_BatchedTileSet` 测试夹具**（grep 仅命中 reader 源码）——无 tileset.json 样本、无 batched imdl 内容文件、**无 BatchedTile/BatchedTileTree 的渲染级测试**。结合第一轮结论（imdl 夹具仅 ImdlParser.test.ts/tile-data.ts 内联字节）：**整个 itwinjs 仓库没有任何磁盤上的 frontend-tiles 端到端测试数据**，真实 tileset 只来自 mesh export 服务。

### 5.5 对 DanQing 方案的修正

1. **树入口的忠实形态是 seam 替换，不是 AddTileTree 旁路**：DanQing 终局应移植 `TileTreeReference`/`TileTreeOwner`/`TileTreeSupplier`/`Tiles 注册表` + `SpatialTileTreeReferences.create` 可覆写工厂（参考 PrimaryTileTree.ts:601-606 + SpatialViewState.ts:109），示例/测试经工厂注入离线树——`Viewport::AddTileTree` 只能当临时脚手架，注释标明待替换。
2. **内容解码绕不开 iMdl**：frontend-tiles 的 tile 内容同样是 imdl（走同一 ImdlDecoder）。路径 A（b3dm/glTF 内容经 DanQing 现有 GltfReader）仍是**最小可行验证**，但它验证的是"通用 3D Tiles 消费链"；要对齐参考的 iModel 可视化语义（feature table/拾取/按模型显隐），ImdlReader 栈移植（原路径 B）不可省略——两者共用 §3.2 的五断点基建，先做哪个取决于目标。
3. **ProxyTileTreeReference 异步占位模式**（areAllTileTreesLoaded 不早熟 + 落地后 invalidateScene）值得照搬到 DanQing 的树加载状态机。
4. **BatchedTile.selectTiles 的 unskippable-levels 方案**（depth % maxLevelsToSkip）比 IModelTile.selectTiles 简单得多，是 DanQing 修正 RealityTileTree 自研选择算法（§3.2 P7）时的优选参考蓝本。
5. **测试策略结论不变且加强**：参考侧无端到端数据 → DanQing 的最小 tileset 资产只能 Authored 自建（§5(g) 授权 + 位置断言）；单元层优先移植 ModelGroup.test.ts 式纯逻辑测试与 core-frontend 的通道/LRU 套件。

### 5.6 使用点与在线 tileset 获取（第三轮调研，2026-09-19）

**全仓 `@itwin/frontend-tiles` 使用点共三处**（grep `from "@itwin/frontend-tiles"`）：

1. **display-test-app**：`DisplayTestApp.ts:231-254`——仅当配置 `frontendTilesUrlTemplate` 时调 `initializeFrontendTiles({enableEdges:true, computeSpatialTilesetBaseUrl})`；模板经 token 替换（`{iModel.key}`/`{iModel.filename}`/`{iModel.extension}`，README.md:233-238），先 probe fetch `tileset.json` 探测存在性，不存在返回 `undefined` → 回退默认 RPC 瓦片链。env 变量 `IMJS_FRONTEND_TILES_URL_TEMPLATE`（DtaConfiguration.ts:62/108），README 示例：`http://localhost:8080/MshX/{iModel.filename}{iModel.extension}/`。
2. **display-performance-test-app**：`DisplayPerformanceTestApp.ts:96-120`——同模板机制（TestConfig.ts:126/207），另支持 `frontendTilesNopFallback`（:90-94）；README.md:148 明示 "served over localhost"。
3. **DTA geoscience 工具**：`RealityDataModel.ts:7-40`——key-in `AddSeequentRealityModel` → `attachGeoscienceTileset`（包内 `GraphicsProvider/tileset-creators/GeoscienceTileset.ts` + `url-providers/GeoscienceUrlProvider.ts`），接 Seequent geoscience 对象 tileset（需 endpointUrl/organizationId/workspaceId/geoscienceObjectId/accessToken 五参）。

**在线 tileset 获取链（默认路径，不传 computeSpatialTilesetBaseUrl 时）**：

```
initializeFrontendTiles 缺省 computeUrl（FrontendTiles.ts:205-213）
→ obtainMeshExportTilesetUrl → obtainGraphicRepresentationUrl（GraphicsProvider.ts:234-272：
  首个 changeset 匹配的 export；不匹配且未 requireExactVersion 时回退最新 :255-262）
→ queryGraphicRepresentations（GraphicRepresentationProvider.ts:155-200：分页 _links.next）
→ mesh-export 服务查询 URL（:74-89）：
  https://api.bentley.com/mesh-export/?iModelId=<id>&$orderBy=date:desc&$top=5
    [&changesetId=<id>][&cdn=1]&tileVersion=<前端imdl主版本>&iTwinJS=<core版本>&exportType=IMODEL
  请求头：Authorization: <OIDC token> / Accept: application/vnd.bentley.itwin-platform.v1+json
         / Prefer: return=representation / SessionId（:155-165）
→ 响应 exports[] 过滤 exportType+status=Complete（:179），取 _links.mesh.href 为 baseUrl，
  obtainGraphicRepresentationUrl 拼上 /tileset.json（:269-271）；href 的 query（SAS token 等）
  经 url.search 透传到 tileset.json 与每个 tile 内容请求（BatchedSpatialTileTreeRefs.ts:287、
  BatchedTile.ts:144）
```

**前提与边界**：iTwin 平台账号 + hub 中的 iModel（iTwinId/iModelId/changesetId）+ OIDC access token + **已通过 mesh-export API 为该 iModel 产出过 export**（服务是按需/CI 触发的导出，不是自动存在）。**仓库内零捆绑 tileset 样本数据，也无开箱即用的公共 demo tileset 地址**——免凭证的实测路径只有 localhost/自托管模板（README 示例即 localhost:8080）。

**对 DanQing 的启示**：模板+probe 的 `computeSpatialTilesetBaseUrl` 注入模式正是示例程序该复制的 seam——前端机制不关心字节来源（在线服务/localhost/本地文件均可）；DanQing 缺的"生产者"角色（参考侧=mesh export 服务从 .bim 产 tileset）现阶段由 Authored 自建资产顶替，终局由仓外几何管线承担（几何内核归真形：BRep→mesh→tileset）。

### 5.7 本地测试数据生成能力判定（第四轮调研，2026-09-19）

问题："目前能不能生成 localhost 数据用于测试？"——**能，分三档**，各档解锁的验证能力不同：

**档 1｜今天就能做（零外部依赖）：Authored 最小 3D Tiles 资产**
- 内容：手写 tileset.json（普通 3D Tiles，无 BENTLEY 扩展）+ b3dm 包装现有 GLB（BoxTexturedDots 不对称标记体系）；Python 生成脚本随资产入库。
- 伺服：localhost HTTP（现有 QtTileRequestFetcher 支持 http scheme）或纯文件系统（§4.2 P2 FileTileFetcher）。
- 解锁：DanQing 通用 3D Tiles 消费链（§3.2 五断点）+ 像素回归锁。依据：DanQing `RealityTile::readContent` 已解析 b3dm 头 + GLB 走 `GltfReader::LoadFromMemory`（RealityTile.cpp:84-173）。
- 性质：Authored（§5(g) 授权），不验证 imdl/feature table/按模型显隐。

**档 2｜真 imdl 字节（参考生态内有官方本地机制）**
- **本地产 imdl 的公开 API**：`IModelDb.tiles.requestTileContent(treeId, contentId)` → `getTileContent`（core/backend `IModelDb.ts:3731-3740`）——standalone 打开本地 .bim 即可生成，无需 hub/云端；RPC 实现 `IModelTileRpcImpl.getTileContent`（:131-151）走的就是这条链。full-stack standalone tile 测试（`full-stack-tests/core/src/frontend/standalone/tile/`，9 个测试文件）全部用本地 .bim（`test.bim`/`mirukuru.ibim`，BackendTestAssetResolver 解析）+ 真实本地后端跑通生成链。仓库自带真模型：`test-apps/display-test-app/test-models/JoesHouse.bim`。
- **连收割都可能不必要**：`standalone/tile/data/TileIO.data.{1.1,1.2,1.3,1.4,2.0}.ts`（~1MB TS 模块）= 真后端录制的 imdl 字节流夹具，`TileTestData`（TileIO.data.ts:15-49）每版本 5 个几何场景（绿矩形 [0,0]-[5,10] / 单线串 / 三线串 / 六三角形 RGB+底行半透明 / 圆柱 (0,0,0)-(0,0,6) r=2），配 `TileIO.data.fake.ts` 的版本/header 篡改变体。这是全仓最丰富的 imdl 测试数据（比 frontend 的 tile-data.ts 新——后者只有 v1.1 且带更新 TODO）。注意：夹具版本 1.1-2.0 ≪ 当前 v37（IModelTileIO.ts:33-45），测的是向后兼容路径；meshopt 压缩新格式夹具见 ImdlParser.test.ts:284-443。
- 解锁：ImdlReader 解码栈移植的 RED-GREEN（夹具直接转 C++ 数组/二进制文件，对齐 TileIO.test.ts 移植）；收割脚本（Node + core-backend + JoesHouse.bim）仅当需要 v37 新格式/更大模型时才写。

**档 3｜目前本地不能做：BENTLEY_BatchedTileSet 完整信封**
- 该 tileset.json 信封是 mesh-export 服务私有产物；参考仓库无任何本地生成工具（imodel-native 无 TilePublisher——旧记忆已过时；全仓 grep `tileset.json` 产出代码零命中，已验证）。
- 要喂 frontend-tiles 链只能：① 在线 mesh-export 服务（凭证 + 已产出的 export）；② 自己合成信封（Authored，需复刻 `computeChildTileProps` 的层级/误差结构，工作量大，不划算）。

**推荐组合**：档 1 验证消费链基建（P0-P6 像素锁）→ 档 2 夹具移植驱动 ImdlReader 栈（TileIO.test.ts 是现成移植蓝本）→ 档 2 收割脚本留作 v37 新格式补充 → 档 3 推迟到自有生产管线。

>
> **2026-09-23 超长任务 G-A→G-E 完成注记**：
> - **G-A 透视 SSE**：`TileDrawArgs` 透视输入（cameraOn/cameraEye/perspectiveScale=2·tan(lens/2)/heightPx——computePixelSizeInMetersAtClosestPoint 的针孔公式收敛形式，TileDrawArgs.ts:190-206 锚）+ `computeVisibility` 球面最近点分支（近面 clamp 0.01 保有限）。锁：`TileTreeRender.PerspectiveCameraSseRefines`（minimal-replace 相机近 refine 红蓝/远绿板，双向断言）。
> - **G-B 过期清理**：TileUsageMarker 时间戳子集（Tile.markUsed/isTimestampExpired，TileUsageMarker.ts:22-45）+ `TileTree::prune(cutoff)`（过期 tile 的子树内容释放——DanQing children 不可重建故保结构弃 graphic，登记偏差 vs IModelTile.pruneChildren 可弃可重建）+ `TileAdmin::pruneAndPurge` 真身（20s clamp[5,60] 节流 + users→discloseTileTrees→prune，TileAdmin.ts:849-895 锚）+ 测试时钟注入 seam（setNowOverrideForTest）。锁：TileAdminMemory 2 项新测（窗口内不剪/超时剪）。
> - **G-C TD-17 修复**：PolyfaceGraphic 解包布局错位（字节级核对：绿→品红/蓝→黄/alpha 错位致透明——与两链历史观察逐一吻合）；解包对齐 `(a<<24)|(b<<16)|(g<<8)|r`。锁：`SolidBaseColorFactorRendersMaterialColors`（minimal-solid 独立资产，§11.11）。
> - **G-D imdl 格式层**：G-D1 `ImdlHeader.h/.cpp`（ByteStream 子集 + ImdlFlags/CurrentImdlVersion(37.0)/ImdlHeader/FeatureTableHeader，IModelTileIO.ts 1:1）+ 夹具转换器（TileIO.data.{1.1..2.0} → C++，943KB 头）；G-D2 `ImdlDocument.h/.cpp`（decodeTileContentDescription 的 leaf/sizeMultiplier 启发 1:1 + GltfHeader **Bentley 变体**（20B 头/chunk-type/v1-v2 分支，GltfTileIO.ts）+ glTF 段提取）；G-D3' 缩编——`TileFormat::IModel` 分支接入 RealityTile::readContent（元数据闭环：contentRange/isLeaf 来自头+描述）。**9 项夹具驱动测试全绿**（5 版本头/5 场景计数（1,1,3,6,1 与参考 :123/:151/:179/:207/:235 一致）/非法头/截断/文档内容/isLeaf/TileContent 元数据）。图形层（量化 VertexTable+meshopt ≈2000+ 行）登记 **TD-18** 独立立项；G-D4（per-model 树生产）以其为前置。
> - **G-E 全回归**：TileTreeRender 7/7（含透视/纯色新增）· imdl+tile 套件 19/19 · Registry+Factory 6/6 · DisplayTestAppTest 202/202。
>
> **2026-09-23 H 系列完成注记（imdl 消费收官）**：
> - **H-1/H-2**：精读 Parser/Schema/GraphicsCreator/VertexTable + v1.1 夹具字节级取证——**布局破解**：顶点表按纹理布局（4 RGBA/顶点），每顶点首 3×u16 = 量化 xyz（u16 q→`min+q×(max-min)/65535` 仿射到世界），surface 索引 24-bit LE；binary 段直贴 JSON 后（Bentley v1 降级路径无 chunk 头——取证时曾错跳 8 字节致假顶点错位，修正后四角 ±2.5/±5 与参考注释完全吻合）。
> - **H-3 量化解码**：`decodeImdlGraphics`（dqRender/src/tile/ImdlGraphics.cpp）——bufferViews 定位 + 量化解码 → IndexedPolyface。**锁**：ImdlGraphics 3 项（rectangle 4 顶点 ±2.5/±5/±0（0.0005 容差=参考 :130-140 同值）+ 2 三角；triangles 2 mesh × 9 顶点=参考 :329/:342；readContent 元数据）。
> - **H-4 渲染端收束**：解码/图形创建/Scene 收集全验证（[TILE] graphics=1），DrawFrame 无 tile MVP 提交、三视图不上屏——**TD-19 登记**（对比 b3dm 链差异待查）；窗口测试 #if 0 保留。
> - **H-5 per-model 生产**：`computeImdlChildTileProps`（TileMetadata.ts:777-853 1:1——magnification 单子倍增/3d 8 分/2d 4 分/empty-mask 位序 1<<(i+j·2+k·4)/模型域拒绝）+ ContentIdProvider V1 解析/格式化 + `ImdlTile/ImdlTileTree`（maxDepth 32、contentUrl 组合点）+ `PrimaryTileTreeSupplier`（树 id 编码根 props 代位 RPC，登记）+ **SpatialTileTreeReferences 默认工厂接 model-selector**（每选中模型一 PrimaryTileTreeReference，经 IModelConnection.tiles 注册表）。IModelConnection 挂 Tiles 注册表（Tiles.ts:86 锚）。**锁**：ImdlTileTree 8 项（参考分支语义：8 分/4 分/mask 跳过/域拒绝/magnification/叶无子/loadChildren/往返）。
> - **H-6 全回归**：TileTreeRender 7/7 · imdl+tile 30/30 · Registry+Factory 6/6 · DisplayTestAppTest 202/202 · 相邻 DtaTest 全绿。CLAUDE.md：TD-18 部分解决更新 + **TD-19 新登记**（P1）。
>
> **2026-09-23 TD-19 修复注记（imdl 端到端打通）**：根因 = 24-bit surface 索引 0-based 直传——IndexedPolyface 点索引是 **1-based**（b3dm 对照组 GltfReader.cpp:343 有明确注释与 +1 转换；索引 0 是 PolyfaceGraphic::buildFromPolyface 的跳过哨兵 :83）→ 0 顶点 → draw 空提交。单变量修复（+1）后 `ImdlTilesetRendersRecordedFixture` 一次转绿：**录制后端夹具（TileIO.data.1.1 rectangle）经 tile 链量化解码 → 图形 → 上屏**（绿色矩形 2127508 像素、质心居中）——imdl 消费链端到端闭环。全回归：TileTreeRender 8/8 · imdl+tile 30/30 · DisplayTestAppTest 202/202 · Registry+Factory 6/6。
>
> **2026-09-23 I 系列完成注记（TD-18 收尾消解）**：
> - **I-1 material 色**：`ImdlDocument.materialFillColor`（scene 首材质 fillColor，ParseImdlDocument.ts displayParamsFromJson :1210-1215 锚）→ readContent 0x00BBGGRR→DanQing `(a<<24)|(b<<16)|(g<<8)|r` 转换——去硬编码绿，夹具 fillColor=65280（绿）语义对齐。
> - **I-2 FeatureTable**：parseImdlDocument 增加 feature 数据提取（12B 头后 3×u32/feature packed words——PackedFeatureTable.ts:139-141 布局）→ readContent 转 `dqCommon::FeatureTable`（elementId lo/hi+subCategory→DqId 对）——拾取/hilite 的数据面就位（createBatch 包装随 Batch 渲染落地）。
> - **I-3 回归**：TileTreeRender 8/8（imdl 窗口走 material 色仍绿）· imdl+tile 31/31 · DisplayTestAppTest 202/202 · Registry+Factory 6/6。**TD-18 划掉**（后续增强独立登记：oct 法线/meshopt/多材质/RPC 化）。
>
> **2026-09-23 J 系列（测试门修复 + 增强）完成注记**：
> - **J-1（TD-16①）**：DtaToolBars 两断言测试漂移修正（工具栏 5→6 加 Deco Example 2026-09-17 合法加入；Debug 按钮点亮 2026-09-21 全量移植）。
> - **J-2（TD-12+TD-16②）**：**跨测试污染根因 = AcsTriadDecorator GL 生命周期倒置**——符号化崩溃栈实锤：`CollectDecorations → AcsTriadDecorator::Decorate → RenderGraphicOwner::disposeGraphic → MeshGraphic::~MeshGraphic → m_driver.destroyBufferObject` → SEH 0xc0000005。前一测试视口的 driver 已死但 ACS 缓存 graphic 还在，次测 Decorate 时释放触死 driver。修复：`m_creatingSystem` 记录创建系统，跨视口时指针置空不 dispose。连带修 TileTreeRegistry.DropSupplier 指针同一性断言（堆回收复用地址→改状态判定 NotLoaded）。**结果：dqAppTest 353/353 · DtaTest 73P+2S+0F——全量回归门恢复**。
> - **J-3（TD-15）**：根因完整确认（UniformHandle 纯缓存，绑定已派发但值不上屏）；实施留给独立 session（登记 TD-15 行内）。
> - **J-4（oct 法线）**：imdl 顶点表 bytes 6-7 的 2×u8 oct-encoded 法线解码（Surface.ts:383-398 octDecodeNormal 1:1）→ polyface->AddNormal + AddNormalIndex。8 测试全绿。
> - **J-5（meshopt）**：v36 压缩夹具定位（ImdlParser.test.ts:284-443）；解码器移植登记独立任务。