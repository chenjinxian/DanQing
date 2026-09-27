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
