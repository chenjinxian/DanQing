# instances60-v1 — Properties_60InstancesWithUrl2.ibim（BFS 全树采集 + instances 修饰）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
sweep.js（BFS 全树采集）+ rpc-dump.js（`generateTileContent`/`requestTileTreeProps` 实例级包裹）。
**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `Properties_60InstancesWithUrl2.ibim`（1.5MB，60 实例）——拷至 DTA assets 后打开 |
| 模型源路径 | `full-stack-tests/presentation/assets/datasets/Properties_60InstancesWithUrl2.ibim` |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core` @ `9b3d17b3741da2b7881bd6f40a6538cc9ec0fb21`（采集时 HEAD） |
| iModelId | `dfac2750-4c3e-41de-afe7-5f4d97375006` |
| imdl formatVersion | 2424832（= 0x250000，major 37 minor 0——`CurrentImdlVersion.Major = 37` 域） |
| 树 | `25_1d-E:6_0x1c`（1 树） |
| 采集方式 | `node capture.mjs --sweep`（BFS 全树：合成 iModelTree 鸭子类型经 `generateTileContent` 请求全部 contentId，子瓦经页内 `computeChildTileProps` 派生） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（3587 瓦 + 1 树 props，0 mismatch——不只抽检） |

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 3587 |
| 总字节 | 303740104（289.67 MB） |
| 深度分布 | d0:4 / d1:3 / d2:4 / d3:6 / d4:24 / d5:84 / d6:582 / **d7:2880**（最多 depth-7） |
| BFS 完成态 | **未完成**（本 dump 是 900s 窗口内的最大覆盖——depth-7 仍 2880 分支未解完子瓦） |

**规模说明**：此模型**细分极深**——`decodeTileContentDescription` 对其瓦返回
`sizeMultiplier=undefined` → `computeChildTileProps` 走 `idFromParentAndMultiplier` 无限翻倍
（实测放大链 ×1..×524288 不收敛、内容仍逐瓦唯一），且分支瓦直到 depth-7 仍全不 emptySubRange
剪枝。全树 = 数万瓦级；BFS 受 tiler 实际完整度所限，本 dump 是**观测到的最大完整前缀**
（depth-0..7，根 + 2880 个 depth-7 分支）。放大 cap=×8（真实视图 selectNextLevelMagnification
挂起去重 + adjustedPixelSize/width 有界，实际请求 ≤×8）。

**instances 证据（Task 目标）**：65/600 抽查瓦含 per-primitive instances 修饰，root 瓦原文：

```json
"instances":{"count":60,"featureIds":"bvInstanceFeatures1","symbologyOverrides":"bvInstanceOverrides1",
  "transformCenter":[-77.271176735149510,-37.242898519589879,-25.017626535143606],
  "transforms":"bvInstanceTransforms1"}
```

`count:60` = 模型 60 实例；bufferView 名 `bvInstanceTransforms1`/`bvInstanceFeatures1`/
`bvInstanceOverrides1` 为 per-primitive 实例放置参数（参考 `InstancedGraphicParams.ts:20-37`）。
**这是 M-E TD-25（instances 修饰未消费）的直接输入。**

## manifest 契约

与 `compatseed-v1`/`mirukuru-v1` 同（`provenance.sweep` 记录 BFS 态：tiles/leaves/branches/
providerSource=negotiated/modulesVia/fallbackPurge=用）。注意 `provenance.sweep.done != true`
（BFS 未完成——如实记录）；`trees`/`tiles`/`sha256` 是落盘实态、可回放。
