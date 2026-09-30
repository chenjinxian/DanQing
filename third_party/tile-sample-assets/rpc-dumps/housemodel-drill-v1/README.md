# housemodel-drill-v1 — House_Model.bim（视口请求面 drill 采集）

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`** @
`7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入 rpc-dump.js +
capture.mjs `--drill` 模式 + imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`
（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `House_Model.bim`（24,535,040 B——DTA assets 就位后 standalone 打开；与 housemodel-v1 同一源文件） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04` |
| 树 | 1（`25_1d-E:0_0x26`——与 housemodel-v1 同批同型） |
| 采集方式 | 视口请求面：**带取景**（`viewport.zoom(Point3d, factor)` 重定中心至 projectExtents 质心 [6.013, 6.755, 5.600]，radius=root range 对角线/2=173.23）+ `viewport.zoom(undefined, 0.5)` 泵逐 ×2 下潜 10 级（638m→0.62m 视域），wrapper 捕获 viewport 自然请求的瓦。**坑 16 在本模型不复现**（取景后 zoom 泵继续产请求——第 4 级 4→39、第 5 级 39→61；与 instances60 特异性对照） |
| `provenance.drill` | targets=[[6.013,6.755,5.600]]/zoomPerTarget=10/framingApi=viewport.zoom(center,factor)/zoomInApi=viewport.zoom(undefined,0.5)/windowSize=2000,1400/viewRect=1556×844/cameraOn=true/levelsReached=[{levels:10, tilesAtEnd:61}] |
| defaultView | 与 housemodel-v1 全精度同值（同模型跨会话确定性） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（61 瓦 + 1 树 props，0 mismatch——不只抽检） |

## iModelRpc 数据面（imodel.json）

本 dump 会话独立采集一份（与 housemodel-v1 同源同值——`connection/views.defaultViewState/models`
一致，`gaps=[]`；42 视图/默认 0xdb/1 model）。清单见 housemodel-v1/README.md 的 iModelRpc 节。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 61 |
| 总字节 | 24,704,568（23.56 MB） |
| 深度分布 | d1:1 / d2:1 / d3:2 / d4:9 / d5:38 / d6:10 |
| mult 分布 | ×1:46 / ×2:7 / ×4:8 |
| 下潜终点 | 第 6 级起零新瓦（61 瓦饱和至第 10 级 0.62m 视域）——**视口浏览最深层=depth 6** |

**视口 LOD 形态实证**：10 级 zoom 泵（638m→0.62m）由**细分链承载**（d4→d5 38 瓦为主，
非 joeshouse 的放大链承载形态）；放大链顶只到 ×4（8 瓦），无 ×8/×16/×32 键——
与 joeshouse-drill-v1（两大树中心支放大到 ×32、无 depth-3+ 细分）呈**互补形态**。

## drill 键 vs sweep 键域（与 M-H joeshouse 相反的交叉验证结论）

**61/61 键全部在 housemodel-v1 sweep 域内，且逐键 byteLength+sha256 同值**（2026-09-30 全量对账）——
本模型**无** M-G/M-H 的"视口放大链顶超出 sweep 合成 cap 域"现象（drill 放大顶 ×4 < sweep cap ×8）。
即：housemodel-v1（sweep）∪ 本 dump = housemodel-v1 单根即可覆盖视口请求面；Task 2 接入时
本 dump 的价值=独立会话的请求面同构证据 + d6 深键（sweep d10 截断层之下仍完整覆盖）。

## 已知限制

1. 单目标（projectExtents 质心）——House_Model 坑 16 不复现、多目标取景无机制障碍
   （如需非质心支覆盖，新目录名重采）。
2. 本 dump 不含 d≥7 键（视口未请求——非采集缺失；sweep 域覆盖至 d10[部分]）。
3. 视窗 2000×1400 请求下实际 viewRect=1556×844（Windows 屏幕钳制，provenance.drill 已记）。

## manifest 契约

与 housemodel-v1/joeshouse-drill-v1 同（trees/tiles/sha256 全量落盘；imodel.json 并列）。
