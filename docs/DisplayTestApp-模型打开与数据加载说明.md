# DisplayTestApp — Start 页模型打开与数据加载说明

> **适用范围**：M-H 里程碑（2026-09-29，commits a72f893..09a17f3）之后的 DisplayTestApp。
> **主题**：Start Views 页点击模型入口（Joe's House / 60 Instances）之后，数据如何一步一步加载并渲染；数据的来源、存放位置与打开方式。
> **对齐原则**：加载流程与 itwinjs-core 前端（display-test-app，下称 DTA）打开 bim/ibim 的流程**逐环一致**——唯一差异是数据源：参考经 RPC 从后端取，DanQing 从本地已保存文件回放（§8.2 零网络）。逐环一致的判据由 `DumpOpenChainTest`（请求序列同构锁）与 `DumpBrowseTest`（浏览零缺失锁）钉死。

---

## 1. 一图总览

```
[Start 页卡片点击]
      │ requestOpenDumpModel("joeshouse" | "instances60")        （StartView.cpp:118-121）
      ▼
[main.cpp 槽] dumpPackageForModel → 数据包根目录映射               （main.cpp:92-110）
      │ newDocument() → 新 MDI 视图（View3DInventor）
      ▼
[打开链 openDumpIModel]                                            （DumpOpenHelper.cpp:14-142）
      ① 打开连接       DumpIModelConnection::open(imodel.json)    ← iModelRpc 数据面回放
      ② 装载默认视图   ViewList::create → getDefaultView          ← views.getViewList + views.load
      ③ 应用视图       viewport->ChangeView(viewState)            ← vp.changeView（无 fit 补偿）
      ④ 逐 model 建树  modelSelector → iModelTileTreeIdToString    ← treeId 派生 + requestTileTreeProps
                        → ImdlTileTree + location(iModelTransform)
      ⑤ 注册 provider  AddTiledGraphicsProvider                   ← addTiledGraphicsProvider
      ▼
[视口驱动（每帧）] selectTiles → SSE/SelectParent → insertMissing   （ImdlTileTree.cpp selectTiles）
      ▼
[瓦请求] DumpTileFetcher（多根：主根→fallback 逐根查 manifest）     （DumpTileFetcher.cpp）
      ▼
[字节回放→上屏] ImdlTile::readContent → LUT 顶点表/instances/边缘    （ImdlDocument.cpp / ImdlGraphics.cpp）
                → Branch(location) 包裹 → SceneCompositor 绘制
```

---

## 2. 数据来源（怎么来的）

数据全部来自**真实 itwinjs-core 后端**的采集（不是合成/自造）：

| 环节 | 说明 |
|---|---|
| 参考实现 | itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev），后端为真实 iModel 后端进程 |
| 采集工具 | **仓外** `D:\Github\danqing-rpc-tools\`（§8.2：DanQing 仓内零网络——采集属工具侧）——`collector.mjs`（本地 HTTP 落盘器）+ `rpc-dump.js`（CDP 注入，实例级包裹 `TileAdmin.generateTileContent` / `requestTileTreeProps` / iModelRpc 视图面）+ `capture.mjs`（浏览器驱动） |
| 采集方式 | 打开真实模型 → wrapper 捕获前端实际发出的每个 RPC 的**返回载荷原样字节**：树 props JSON、瓦 imdl 字节、`getViewStateData` 的完整 ViewStateProps、连接/模型信息 |
| 两个采集面 | **sweep**（BFS 全树：鸭子类型逐 contentId 请求，覆盖键域全集前缀——joeshouse-v1 达 10 树/173,876 瓦）+ **drill**（视口请求面：真实视口 zoom 泵下潜时 wrapper 捕获的自然请求——含 sweep 合成 cap 之外的 ×16/×32 放大键） |
| 完整性 | 入库前**全量** sha256 + byteLength 复算（零 mismatch）；`imodel.json` 三 dump 均为 RPC 载荷原样（`gaps=[]`） |
| 原始模型 | JoesHouse：`itwinjs-core/test-apps/display-test-app/test-models/JoesHouse.bim`；60 Instances：`full-stack-tests/presentation/assets/datasets/Properties_60InstancesWithUrl2.ibim`（采集时均已拷至 DTA assets 打开） |

采集坑清单与复采指引：仓外 `danqing-rpc-tools/README.md`（坑 1-23）。

## 3. 存放位置（在哪里、是什么）

入库位置：`third_party/tile-sample-assets/rpc-dumps/<dump 名>/`（**只读**，§11.11——重采=新目录名，不得原地修改）。

### 3.1 每个 dump 的目录结构

```
<dump 名>/
├── README.md          # 该 dump 的 provenance/统计/键域/已知限制
├── imodel.json        # iModelRpc 数据面（M-H 起）——打开链①②的输入
├── manifest.json      # 瓦/树索引（stats/trees[]/tiles[]——含每瓦 sha256）
└── files/
    ├── <n>.json       # 树 props（IModelTileTreeProps 原文——requestTileTreeProps 返回载荷）
    └── <n>.imdl       # 瓦内容字节（generateTileContent 返回的最终 imdl——前端 ImdlReader 直喂的同一字节）
```

### 3.2 `imodel.json` 各段（iModelRpc 面）

```jsonc
{
  "connection":    { "name", "guid", "projectExtents" },          // 连接信息
  "views":         { "list": [...], "defaultViewId": "0x25" },    // getViewList 全量 + 默认视图 id
  "defaultViewState": {                                            // views.load(默认) 的完整载荷原样
      "viewDefinitionProps":  { origin/extents/rotation/angles/camera },
      "modelSelectorProps":   { "models": ["0x26", ..., "0x4d"] },  // 决定建哪些树
      "categorySelectorProps":{ "categories": [...] },
      "displayStyleProps":    { viewflags/背景等 }
  },
  "models":        [ ModelProps... ]                               // 各 model 属性（id/class/name）
}
```

### 3.3 两模型的包组合（`main.cpp dumpPackageForModel`）

| 入口卡片 | modelId | iModel 面（imodel.json） | 瓦主根（树 props 源 + 字节） | fallback 根（域外键补字节） |
|---|---|---|---|---|
| Joe's House | `joeshouse` | `joeshouse-v1/` | `joeshouse-v1/`（10 树/173,876 瓦 sweep 全树） | `joeshouse-drill-v1/`（15 瓦——×16/×32 放大键在 sweep 域外）→ `joeshouse-drill-v2/`（20 瓦——M-I(5) P2c：缩远态 depth-1 键 `-b-1-0-0-0-1`×6 在前两根域外） |
| 60 Instances | `instances60` | `instances60-imodel-v1/` | `instances60-v1/`（1 树/3,587 瓦 sweep） | `instances60-drill-v1/`（5 瓦——×16 键域外） |

多根查找序（`DumpTileFetcher`）：主根 → fallback[0] → …，**首命中即服务**；树 props **仅取自主根**（同 treeId 语义单源）；每请求命中源记 `requestLog.hitRoot`（0=主根，i+1=fallback[i]）。

## 4. 打开方式

| 方式 | 操作 | 说明 |
|---|---|---|
| **Start 页入口**（推荐） | 启动 `build/samples/DisplayTestApp/Debug/DisplayTestApp.exe` → Start 页 **Models** 区点击卡片 | 即本说明主题；两卡片分别对应上表两包 |
| 环境变量覆写 | `DANQING_RPC_DUMP=<dump 根目录>` | 覆写整个 rpc-dumps 根（调试自定义 dump 用；main.cpp:95-96） |
| 测试入口（无 GUI 点击） | `ctest -R "DumpOpenChain|DumpBrowse"` | 打开链同构锁 + 浏览零缺失锁（与 app 同一 `openDumpIModel` 代码路径） |
| 重采数据 | 仓外 `danqing-rpc-tools`（README 坑清单 1-23 先读） | 输出到**新目录名**再入库；既有 dump 永不覆盖 |

构建：`cmake --build build -j --config Debug --target DisplayTestApp`（仓库根）。打开性能（M-I(1) 清偿后实测）：Debug 构建 joeshouse 打开 ~2.9s、instances60 ~0.4s（`DANQING_OPEN_TRACE=1` 计时桩）——manifest 单次解析共享 + 哈希索引 + 流式读取器；RelWithDebInfo 构建可再快一个量级。

---

## 5. 点击之后：逐步加载详解

以下每步注明：**动作 → DanQing 组件（文件锚）→ 参考对齐锚（itwinjs-core）**。参考流程 = DTA `Viewer.create`（Viewer.ts:184-188）打开模型的同一序列。

### 步骤 0：卡片点击 → 信号
- **动作**：点击 Models 区卡片。
- **DanQing**：`StartView.cpp:108-121`——两张卡片（Joe's House / 60 Instances）`clicked` → `Q_EMIT requestOpenDumpModel("joeshouse"|"instances60")`。
- **参考**：DTA Surface 的文件列表点击 → `openFile` → `openIModel`（应用层 UI，无引擎语义——DanQing 以信号槽对齐）。

### 步骤 1：数据包映射 + 新建视图窗口
- **动作**：main.cpp 槽收信号。
- **DanQing**：`main.cpp:254-269`——`dumpPackageForModel(modelId)`（:92-110，包根映射一处集中，与测试同构）→ `Gui::Application::Instance()->newDocument()` 创建新 MDI 视图（View3DInventor——内嵌 dqApp::Viewport 的 GL 窗口）。
- **参考**：DTA 点文件 → `Surface.openIModel` → `Viewer.create`（新 window + viewport）。

### 步骤 2：打开连接（①）
- **动作**：装载 `imodel.json`，建立 iModel 连接等价物。
- **DanQing**：`DumpOpenHelper.cpp:21`——`DumpIModelConnection::open(<imodelRoot>/imodel.json)`（`dqApp/src/tile/DumpIModelConnection.cpp`）：解析五段（connection/views/defaultViewState/models）并安装 `Views::RpcHooks` 回放缝（`IModelConnection.h`——无 RPC 时对非 blank 连接短路的缝，经 hooks 供 getViewList/load 返回 imodel.json 数据；Authored §8.2 宿主缝）。
- **参考**：`SnapshotConnection.open`/`BriefcaseConnection.open` 的**数据面语义**（连接 props/extents/views/models 就位）——DanQing 无 db 层，数据面即 imodel.json。

### 步骤 3：树 props + fetcher 注入（打开链前置）
- **动作**：装载瓦索引、把"瓦字节供给器"注入 TileAdmin。
- **DanQing**：`DumpOpenHelper.cpp:28-40`——`DumpTileTreeProps::load(tileRoots[0])`（manifest + 树 props JSON）→ `DumpTileFetcher(tileRoots[0], fallbacks)` 构造 → `TileAdmin::instance().setFetcher(...)`（DI 缝 §8.4）。
- **参考**：`TileAdmin` 的 RPC 层（`generateTileContent` TileAdmin.ts:694-706 / `requestTileTreeProps`）——打开全程就位；DanQing 的对应物 = fetcher 从本地 manifest 回放同一字节。

### 步骤 4：装载默认视图（②）
- **动作**：视图清单 → 默认视图 → **完整 ViewState 原样装载**。
- **DanQing**：`DumpOpenHelper.cpp:61-67`——`ViewList::create(connection)`（内部 getViewList 经 RpcHooks 回放）→ `getDefaultView()`（ViewPicker.ts:34-51 语义：`views.load(defaultViewId)` + **clone**）→ `SpatialViewState::CreateFromProps`（`dqApp/src/ViewState.cpp`——**saved JSON 全字段应用**：origin/extents/rotation（angles→矩阵，`dqGeom/YawPitchRollAngles.h`）/camera/modelSelector/categorySelector/displayStyle viewflags）。
- **参考**：`Viewer.ts:184-185` `ViewList.create` → `views.getDefaultView` → `ViewPicker.ts:53-55/:34-51`；`SpatialViewState.ts:90-95` + `ViewState.ts:1497-1515`。
- **关键**：**没有 fit 补偿**——参考 openView 直接用 saved 视图（ViewPicker.ts:29）；DanQing 同形。这就是"初始视图与 DTA 一样"的实现机制：DTA 渲染的是 bim 里保存的 ViewState，DanQing 渲染的是采集到的同一份 ViewState JSON。
- 期间经 frontend-tiles 缝（`SpatialTileTreeReferences::setCreateOverride` → EMPTY refs，:61-64）抑制引擎内占位 refs 的噪音请求（保证步骤 8 的请求序列纯净——窗口结束即 clear）。

### 步骤 5：应用视图到视口（③）
- **动作**：视口切换到 saved 视图。
- **DanQing**：`DumpOpenHelper.cpp:80-81`——`viewport->ChangeView(viewState)`（内部 SetupFromView + InvalidateController；窗口 aspect fix 按参考 ViewState.ts:868-880 调整 extents.y——这是参考同款行为，非 fit）。
- **参考**：`Viewer.ts:231` `ScreenViewport.create(contentDiv, view)` → `Viewport.ts:3196-3204` `vp.changeView(view)`。

### 步骤 6：按 modelSelector 逐 model 建 TileTree（④——与参考差异最大的一步，机制逐行对齐）
- **动作**：视图的 modelSelector 里每个 model 各建一棵瓦树。
- **DanQing**：`DumpOpenHelper.cpp:91-129`——遍历 `spatial->GetModelSelector().getModels()`：
  1. **treeId 派生**（:100-109）：由 saved viewflags 算 `edgesRequired`（`PrimaryTileTree.ts:283-285`）→ `iModelTileTreeIdToString(modelId, …, TileOptions{})`（`dqRender/src/tile/ImdlTileTree.cpp`——TileMetadata.ts:487-532 移植）。**派生结果与采集 manifest 的 treeId 逐字符吻合**（11/11——`IModelTileTreeIdTest.CapturedTreeIdsMatchModelSelectorDerivation` 锁）。
  2. **requestTileTreeProps 回放**（:113）：`props->byTreeId(treeId)`（主根 manifest files/<n>.json——即采集期该 RPC 的返回载荷）。
  3. **建树**（:120-125）：`ImdlTileTree` 构造（root contentId 覆写=provider.rootContentId[IModelTileTree.ts:396-398]）+ `setIModelTransform(location)`（树 props 的 3×4 变换——TileTree.ts:122 `iModelTransform`；模型局部→世界）。
  4. 每 tree 入 provider + `treeLoadLog`（对账面：装载序 = modelSelector 驱动序）。
- **参考**：`SpatialViewState.ts:109` `_treeRefs = SpatialTileTreeReferences.create(this)` → `PrimaryTileTree.ts:608-861` 逐 model `PrimaryTreeReference` → `:63-98 createTileTree`（同一 treeId 派生 → requestTileTreeProps RPC → IModelTileTree）。

### 步骤 7：注册图形提供者（⑤）
- **DanQing**：`DumpOpenHelper.cpp:134`——`viewport->AddTiledGraphicsProvider(provider)`（provider 持全部树引用）。
- **参考**：`Viewport.ts:1729-1732` `addTiledGraphicsProvider`（应用注入通道——参考的 frontend-tiles 包即经此类通道替换树供给）。

### 步骤 8：视口驱动逐 TileID 请求瓦内容（每帧发生）
- **动作**：视图就位后，**视口选择语义**决定请求哪些瓦（不是把 dump 全部读入）。
- **DanQing**：每帧 `Viewport::renderFrame` → 场景创建 → 每树 `selectTiles`（`dqRender/src/tile/ImdlTileTree.cpp`——`IModelTile.ts:205-334` 移植）：`computeVisibility`（frustum 剔除[range/球经 location 变换到世界域]）→ SSE 判定（TooCoarse/Visible）→ `insertMissing(瓦)` → TileAdmin 排队 → `DumpTileFetcher.fetch(treeId/contentId, tile)`（多根查 manifest → 读 files/<n>.imdl 字节 → byteLength 校验 → 交付）。
- **参考**：同一协议（selectTiles → TileAdmin → RPC generateTileContent）——请求键域/时序/父子预算（maxInitialTilesToSkip 等 props 字段）逐字段同构。
- **同构证据**（`DumpOpenChainTest` 锁）：instances60 saved 视图请求面 = **恰 1 枚** `-b-2-0-0-0-1`（与采集期 DTA 首键同键同序）；JoesHouse 打开面恰 10 键（10 model 各其首键）== drill 采集首波同集。

### 步骤 9：字节回放 → graphics → 上屏
- **动作**：瓦字节进入与参考相同的解析/渲染管线。
- **DanQing**：`ImdlTile::readContent`（`ImdlTileTree.cpp`）——imdl header 解析（contentRange/sizeMultiplier/emptySubRangeMask——M-F setContent 语义）→ `decodeImdlGraphics`（`ImdlGraphics.cpp`）：LUT 顶点表直传 GPU、instances 修饰→InstanceBuffers 实例化、边缘四形态、fillColor 材质 → `Branch(location)` 包裹（树局部→世界变换——参考 draw 时包裹 TileDrawArgs.ts:373 的创建时等价，EQUIVALENCE 登记）→ SceneCompositor 绘制（不透明/边缘/OIT 合成）上屏。
- **参考**：`ImdlReader`→`ImdlDecoder`→`RenderSystem` 同链。

### 步骤 10：交互中的 LOD 动态加载（浏览阶段）
- **动作**：滚轮缩放/平移改变视锥 → 视口重新选择 → 不同 LOD 的瓦被请求/淘汰。
- **机制**：zoom in → 瓦从 Visible 转 TooCoarse → 放大子（mult×2 派生键）或细分子（depth+1）被 `insertMissing` → 步骤 8-9 循环；zoom out/平移 → LRU 淘汰/复活。**全程只有视口需要的瓦被加载**（浏览零缺失锁：域内键零 NotFound、域外请求白名单逐键钉死、回退+平移零多余请求——`DumpBrowseTest`）。
- **参考**：完全同一套选择协议（这正是"支持改变视图加载不同 LOD"的实现——引擎侧 M-G 起实证，M-H 接入真实 app 交互）。

---

## 6. 新增模型入口的接法（维护指引）

1. **采集**：仓外工具对目标模型采 sweep（全树）+ 视口面 drill + imodel.json（README 坑清单先读）→ 新目录名入库 `third_party/tile-sample-assets/rpc-dumps/`。
2. **映射**：`main.cpp dumpPackageForModel()` 增一个分支（imodelRoot + tileRoots）。
3. **卡片**：`StartView.cpp` 加一张卡（`NewFileButton` 形态）+ `Q_EMIT requestOpenDumpModel("<新 id>")`。
4. **锁**（建议）：DumpOpenChain 同构锁 + DumpBrowse 浏览锁各加一例（treeId 派生对账 + 请求面同集）。

## 7. 已知限制（如实登记，详见 CLAUDE.md §14 与各 dump README）

| 项 | 影响 | 登记号 |
|---|---|---|
| `numRgbaPerVertex=5`（unquantized-LUT）顶点表形态未消费 | JoesHouse ×32 极端放大下 1 枚瓦零 graphic（父瓦 LOD 兜底显示，不可见差异） | TD-27 |
| sweep 采集域 = 全树前缀（joeshouse depth≤10 cap；放大 ×≤8） | 更深细分/更高放大键在非中心支深放大时 NotFound（父瓦兜底——参考后端无更细瓦时同形）；缩远态粗层已由 drill-v2 补齐（M-I(5)） | 各 dump README |
| displayStyle 深层字段（环境/光照/排除元素等）未消费 | 风格细节与参考有差（viewflags/背景已消费） | TD 表/M-H(3) 缺口表 G8 |
| 选择高亮的批次归属粒度（feature 表缺失/粗粒度 batch） | 点选高亮可能覆盖同 batch 的相邻元素（非参考的单元素粒度）——非阻断忠实度台账 | TD-28 |
| "取景杀 selection"（重定中心后零请求） | **instances60 模型特异**（JoesHouse 不复现）；影响该模型补采，不影响回放浏览 | 工具 README 坑 16 |

> M-I(1) 已清偿：joeshouse 打开首轮 >20s（M-H(5) 实测登记）——manifest 单次解析共享 + 哈希索引 + 解析器瘦身，实测 **2.87s**（<3s；instances60 0.42-0.48s）。

---

*本文档描述 M-I 收口时的实态；代码演进以 `DumpOpenHelper.h` 文件头注释（流程①-⑤与参考锚）与 `DumpOpenChainTest.cpp`（判据）为准，两者随代码更新。*
