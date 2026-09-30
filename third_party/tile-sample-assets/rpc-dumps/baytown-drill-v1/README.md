# baytown-drill-v1 — Baytown.bim（视口请求面 drill 采集——三目标取景）

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`** @
`7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入 rpc-dump.js +
capture.mjs `--drill` 模式 + imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `Baytown.bim`（22,745,088 B——与 baytown-v1 同一源文件） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04` |
| 树 | 1（`25_1d-E:0_0x20000000002`） |
| 采集方式 | **M-K 首个多目标 drill**：3 目标取景（projectExtents 质心 [407.79, 122.70, 22.48] + 质心 ±1/4 域偏移两点）+ 各 10 级 `viewport.zoom(undefined, 0.5)` 泵（553m→0.29m 视域）。**坑 16 在本模型不复现**（取景后 zoom 泵继续产请求） |
| `provenance.drill` | targets×3 / zoomPerTarget=10 / radius=150.02 / framingApi=viewport.zoom(center,factor) / levelsReached=[{levels:10,tilesAtEnd:26},{levels:10,tilesAtEnd:28},{levels:10,tilesAtEnd:61}] |
| defaultView | 与 baytown-v1 全精度同值（同模型跨会话确定性） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（61 瓦 + 1 树 props，0 mismatch） |

## iModelRpc 数据面（imodel.json）

本 dump 会话独立采集一份（与 baytown-v1 同源同值，`gaps=[]`；5 视图/默认 0x20000000006/1 model）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 61 |
| 总字节 | 8,417,468（8.03 MB） |
| 深度分布 | d1:1 / d2:1 / d3:5 / d4:18 / d5:22 / d6:12 / d7:2 |
| mult 分布 | ×1:45 / ×2:11 / ×4:4 / ×8:1 |
| 下潜终点 | 各目标 10 级全程下潜（无机械钳制）；target[2]（-1/4 域支）瓦最多（61）——三支合计 61 去重 |

**视口 LOD 形态**：细分链承载（d4-d6 为主）+ 浅放大链（×8 仅 1 瓦）——与 House_Model 同族、
与 joeshouse（放大链顶 ×32）互补。

## drill 键 vs sweep 键域

**61/61 键全部在 baytown-v1 sweep 域内，且逐键 byteLength+sha256 同值**（2026-09-30 全量对账）。
本 dump 价值=多目标视口请求面同构证据 + d7 深键。

## 已知限制

1. 三目标取景自 projectExtents 派生（质心 ± 1/4 域）——非几何感知的刻意选点；覆盖广度如实以
   provenance.drill.levelsReached 记录。
2. 本 dump 不含 d≥8 键（视口未请求——非采集缺失；sweep 域覆盖至 d10[部分]）。

## manifest 契约

与 baytown-v1/housemodel-drill-v1 同。
