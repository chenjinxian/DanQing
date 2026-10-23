# newbridge-v1 — newBridge.bim（P-2 采集主根 + 全树 8 根包总览）

> P-2（2026-10-22/23）：`D:\test\newBridge.bim`（2.95GB 种子）的 10km 级大桥
> 全树采集。**本根 = imodel.json + 树 props（0 瓦）**——瓦字节全部在 7 个
> 本地持有 fallback 根（见下表）。完整 provenance/复现命令/已知限制 =
> `D:/Github/danqing-rpc-tools/dumps/README.md`（采集工作区）。

## Provenance

| 项 | 值 |
|---|---|
| 种子 | `D:\test\newBridge.bim`（3,166,023,680 B，2026-10-20） |
| 模型 | 单 model 0x1b（newBridge）；iModelId `9823750a-aa3c-406c-9e5a-876ce6e4c8e1`；changeset 空 |
| 树 | `25_1d-E:6_0x1b`（visEdges+SmoothShade → edges 6）；root contentId `0/0/0/0/1`（叶形态）；maxInitialTilesToSkip=3 |
| 树根域 | ±4940×±425×±1294（**10km 级**）；location 平移 [6131.91, 529.69, 1154.68] |
| 几何域 | contentRange × location = [1192.46,104.43,-139.01]..[2375.36,206.27,170.80]（= projectExtents，≈1.18km 桥体） |
| 默认视图 | 0x5387（正交、rotation identity）——**坑 24 同型空域**：视域 x∈[-0.40,12.04] 与几何 x∈[1192.46,2375.36] 相离 ~1180m → 直接回放零请求；打开链取景到世界域 contentRange（`dumpPackageForModel` frameToWorldContent） |
| 采集 | 2026-10-22，danqing-rpc-tools capture.mjs（itwinjs @88da8fb98647d37632a5cef4d6bb6e2e5c007855） |

## 全树 8 根（169,920 瓦 / ~47.9GB；sha256 全量 0 mismatch 0 missing；d1-d13 全层完整）

| 根 | 内容 | 瓦数 | git 落位 |
|---|---|---|---|
| `newbridge-v1`（本根） | imodel.json + 树 props | 0 | 整根入 git（小文件） |
| `newbridge-sweep-v1` | d0-d8 BFS（budget-capped 20GB，逐层零缺） | 5,407 | manifest/imodel 入 git；files/ 本地持有 |
| `newbridge-d9-v1` + `newbridge-d9b-v1` | d9 层两波（1,508+597） | 2,105 | 同上 |
| `newbridge-d10-v1`..`newbridge-d13-v1` | d10-d13 逐层波次（19,909+43,985+56,858+41,656） | 162,408 | 同上 |

**47.9GB files/ 本地持有**（`.gitignore`——P-2 R2：P-1b LFS 指针缺瓦 saga
教训，新大体量根不走 LFS）：files/ 目录以 junction 指向
`D:/Github/danqing-rpc-tools/dumps/` 对应根。**后端生成非字节稳定**（根瓦
两轮 135,983,728 vs 107,944,096 B）——sha256 按轮内校验，勿做跨根字节对拍。

## 接入

`dumpPackageForModel("newbridge")`（DumpOpenHelper.cpp）：主根 = 本根；
fallback 候选源序 = d9-v1 → sweep-v1 → d10 → d11 → d12 → d13 → d9b（每根
manifest.json 存在才接入——缺席静默跳过，bridge-edit frontier 先例）。
Start 页卡片 "New Bridge (新大桥测试)"（R6 中文名映射）。

## 锁

`DumpOpenChain.OpensNewBridgeWithFramedContent`（取景面 + 请求面对账 + 像素
锚）+ `SurfaceFileTools.DumpPackageForModelTableIntact`（8 根计数同步）。
