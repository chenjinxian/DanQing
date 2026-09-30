# rpc-dumps — itwinjs-core 真实后端 RPC 数据回放资产（M-D Task 0+1 采集）

**来源**：itwinjs-core display-test-app（electron + vite dev）经 CDP 注入 patch
（`IModelApp.tileAdmin.generateTileContent` / `requestTileTreeProps` 实例级包裹，
打点对齐参考 `IModelTile.ts:91` / `PrimaryTileTree.ts:66` / `TileAdmin.ts:648-707`）
采集的真实后端产出数据。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`
（§8.2 零网络约束：DanQing 侧只做本地文件回放，网络获取归宿主层）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## 共同 provenance

| 项 | 值 |
|---|---|
| 参考仓 commit | `7e57d0182ee375e34f3f1be840bbc623c6776e04`（itwinjs-core，采集日 HEAD） |
| imdl formatVersion | 2424832（= 0x250000，major 37 minor 0——`CurrentImdlVersion.Major = 37` 域） |
| 采集日期 | 2026-09-27 |
| 采集方式 | `node capture.mjs --mode launch --zoom 6`（vite:3000 + electron cdp:9223 + collector:8787；zoom 驱动 LOD 细化产生子瓦） |
| 校验 | 入库前 curate 全量 sha256+byteLength 复算一致（不只抽检）；imdl 头逐字段解析核对（TileFormat "iMdl" / headerLength 88 / tileLength==文件长度） |

## 目录

### compatseed-v1 — CompatibilityTestSeed.bim（计划指定资产）

| 项 | 值 |
|---|---|
| 种子 | `core/backend/lib/cjs/test/assets/CompatibilityTestSeed.bim`（拷至 DTA assets 后打开） |
| 统计 | 1 树 / 7 瓦 / 71432 B |
| 树 | `25_1d-E:6_0x1c`（iModelId `048a431f-0398-4091-acbd-c92657680797`） |
| 瓦 contentId | `-b-6-0-0-0-1` … `-b-0-0-0-0-1`（zoom 驱动的 LOD 链：6 层细化，root 25KB → 末级 4.3KB） |
| root 瓦头 | 6 元素入/0 出，tolerance 0.00132，contentRange ≈ [-100.0, -97.5]³（与树 props contentRange 一致） |

### mirukuru-v1 — mirukuru.ibim（多树覆盖）

| 项 | 值 |
|---|---|
| 种子 | `core/backend/lib/cjs/test/assets/mirukuru.ibim` |
| 统计 | 2 树 / 1 瓦 / 1652 B（模型极小；价值在第二个 treeId `25_1d-E:6_0x28`——回放 fetcher 的 byTreeId 键分发需多树才能被测到） |

### instances60-v1 — Properties_60InstancesWithUrl2.ibim（BFS 全树 + instances 修饰）

| 项 | 值 |
|---|---|
| 种子 | `full-stack-tests/presentation/assets/datasets/Properties_60InstancesWithUrl2.ibim`（拷至 DTA assets） |
| 统计 | 1 树 / 3587 瓦 / 289.67 MB（深度 d0:4..d7:2880；BFS 未完成——depth-7 仍扩展；详见该目录 README） |
| 价值 | 瓦 JSON 含 per-primitive `instances{count:60,transforms,featureIds}` 修饰——M-E TD-25（instances 未消费）的直接输入 |

### instances60-drill-v1 — 同上（视口请求面 drill 采集，M-G）

| 项 | 值 |
|---|---|
| 统计 | 1 树 / 5 瓦 / 289,572 B（depth-2 放大链 `-b-2-0-0-0-{1,2,4,8,10}`——×16 键在 sweep 域外） |
| 价值 | 首个真实视口请求面 dump——"视口请求面=回放可达面"同构锁（M-G）的输入 |

### joeshouse-v1 — JoesHouse.bim（BFS 全树，M-H）

| 项 | 值 |
|---|---|
| 种子 | `test-apps/display-test-app/test-models/JoesHouse.bim`（拷至 DTA assets） |
| 统计 | 10 树 / 173,876 瓦 / 291,514,784 B（depth≤10 cap 登记；10 model⇔10 树；imodel.json[iModelRpc 面]在根） |
| 价值 | 首个多 model 全树资产——特征盘点：纹理 0 命中、instances 67 瓦/795 实例、compact 边缘 75,336 瓦 |

### joeshouse-drill-v1 — 同上（视口请求面 drill 采集，M-H）

| 项 | 值 |
|---|---|
| 统计 | 10 树 / 15 瓦 / 125,516 B（4 小树跳级键 + 两大树放大链 ×1..×32；3 键在 sweep 域外[×16×2/×32×1]） |

### instances60-imodel-v1 — Properties_60InstancesWithUrl2.ibim（iModelRpc 面，M-H）

| 项 | 值 |
|---|---|
| 统计 | 1 树 / 1 瓦（活性证据）/ imodel.json 主产物（4 视图/默认 0x25/1 model——instances60 的 iModelRpc 面自包含） |

## M-K 目录（2026-09-30——三模型数据面采集，新检出 `D:\Github\itwinjs-core` @ `7e57d018…`）

| 项 | 值 |
|---|---|
| 源模型 | `D:\test\` 用户指定三件：House_Model.bim（24,535,040 B）/ Baytown.bim（22,745,088 B）/ 编辑大桥测试.bim（954,888,192 B，**中文名**——目录名 ASCII，映射在各 README） |
| 采集面 | 每模型：imodel.json（iModelRpc 面，全 dump 各一份同源同值）+ sweep 全树（预算 cap 内）+ drill 视口请求面（编辑大桥无 sweep——预算实证裁决） |
| 预算裁决 | House_Model sweep=瓦数 cap 触顶（120k 请求→106,658 瓦/490MB）；Baytown sweep=字节 cap 触顶（1000MB→81,268 瓦/768MB）；编辑大桥 **sweep 跳过**（d2 瓦 34/141MB 实证——全树超 2GB 硬门不可行），imodel+drill 为主。cap 值随 `provenance.sweep.caps` 落盘 |
| 校验 | 入库前**全量** sha256+byteLength 复算（188,055 条目 + bridge drill 31 条目，0 mismatch）；瓦头 "iMdl" magic + major≤37 全量自检 |

### housemodel-v1 — House_Model.bim（sweep 全树，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树（0x26 "House_Model"）/ 106,658 瓦 / 489.91 MB（depth≤10；**d10 层瓦数 cap 截断不完整**——d9 峰层 59,296 瓦完整） |
| 价值 | **首个 textured 命中模型**（36 瓦 materialsWithTexture/namedTextures/uvParams——TD-20 遗留 textured 变体的直接输入）+ polylines 1,514 图元（探针修正后首现）+ instances 214 瓦/3,984 实例；默认视图**开透视相机**（cameraOn=true，M-H 链首个 cameraOn 用例） |
| imodel.json | 42 视图（34 非私有）/默认 0xdb/1 model/25 categories |

### housemodel-drill-v1 — 同上（视口请求面 drill，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树 / 61 瓦 / 23.56 MB（d1-d6——视口浏览最深层 6；放大链顶 ×4） |
| 交叉验证 | 61/61 键全在 housemodel-v1 域内且逐键同字节——**无**坑 18 族域外键（与 joeshouse 相反形态） |

### baytown-v1 — Baytown.bim（sweep 全树，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树（0x20000000002 "ProcessPhysicalModel"，OpenPlant 工艺厂）/ 81,268 瓦 / 768.32 MB（depth≤10；**d10 层字节 cap 截断不完整**——d9 峰层 45,534 瓦完整） |
| 价值 | **polylines 21,323 图元/19,507 瓦 + pointString 6,136 图元**（未消费形态大头命中）；曲线瓦 96.5%；源 changeset 非空（096f5152…） |
| imodel.json | 5 视图（OpenPlant 3D 正交默认 + OPPID×4）/1 model/36 categories |

### baytown-drill-v1 — 同上（视口请求面 drill——三目标，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树 / 61 瓦 / 8.03 MB（d1-d7；projectExtents 质心 ±1/4 域三目标取景——坑 16 不复现） |
| 交叉验证 | 61/61 键全在 baytown-v1 域内且逐键同字节 |

### bridge-edit-v1 — 编辑大桥测试.bim（iModelRpc 面主产物 + 取景粗瓦，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树（0x48 "大体量模型测试"，1.5km 桥）/ 2 瓦 / 166.55 MB（d2 粗瓦 34/141MB——大瓦形态实证） |
| 关键事实 | **默认视图空域（坑 24）**——bim 保存默认视图不含几何（合法零请求）；本 dump 2 瓦为 zoomToVolume 世界域取景后粗瓦；**DanQing 打开链须备取景路径**否则白屏 |
| imodel.json | 4 视图/默认 0x99（空域）/1 model/16 categories；中文文件名四层编码链全验证（provenance.seed 原样） |

### bridge-edit-drill-v1 — 同上（取景到几何域的细节面 drill，M-K）

| 项 | 值 |
|---|---|
| 统计 | 1 树 / 30 瓦 / 722.02 MB（d2-d6——桥跨两点 x=500/1400，radius=200 显式；**浏览深度界如实登记**：其他桥跨位置与 d≥7 无瓦，回放走父瓦 LOD 兜底） |
| 特征 | edges compact 27/30 瓦（joeshouse 后首个 compact 命中）+ polylines 27/30 瓦 + 曲线 30/30；跨会话 2 键与 bridge-edit-v1 同 sha256（确定性实证） |

## manifest 契约（Task 2 DumpTileFetcher/DumpTileTreeProps 消费）

```jsonc
{
  "provenance": { "seed", "seedPath", "collected", "itwinjsCommit", "formatVersion", "iModelId", "changesetId", "pageHref", "userAgent", "tool" },
  "stats": { "trees", "tiles", "dupTiles", "dupTrees", "bytes" },
  "trees": [ { "treeId", "iModelId", "formatVersion", "byteLength", "file", "propsFile" } ],  // file==propsFile（别名）
  "tiles": [ { "treeId", "contentId", "guid", "iModelId", "changesetId", "byteLength", "sha256", "file" } ]
}
```

- `files/<n>.json` = IModelTileTreeProps 原文（键：id/maxTilesToSkip/maxInitialTilesToSkip/formatVersion/
  tileScreenSize/location/contentRange/contentIdQualifier/rootTile{isLeaf,maximumSize,contentId,range}/
  extentsBasis/baseExtents——Task 2 的 ImdlTreeMetadata + root 瓦 props 映射面）。
- `files/<n>.imdl` = `generateTileContent` 返回的**最终 imdl 字节**（前端 ImdlReader 直喂的同一字节）。
- 瓦键 = `contentId` 的**原样字符串**（含 `-b-` 等内部限定形态）——回放键必须与 `IModelTile.contentId`
  完全一致（前端请求时用的就是这串）。
- 序号文件名规避 contentId 含 `/` 的路径问题；键映射全在 manifest。

## 复采

工具与坑清单见仓外 `D:\Github\danqing-rpc-tools\README.md`（vite EBUSY 轮询、core-electron esm 构建、
core-frontend 以 .ts 源码伺服的模块发现、注入后换文档、Windows /quit 收口等）。重采输出到**新目录名**再入库。
