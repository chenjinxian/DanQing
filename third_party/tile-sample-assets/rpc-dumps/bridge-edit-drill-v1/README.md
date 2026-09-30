# bridge-edit-drill-v1 — 编辑大桥测试.bim（视口请求面 drill 采集——取景到几何域的细节面）

**目录名映射**：`bridge-edit-drill-v1` ⇔ 原模型文件 **`编辑大桥测试.bim`**（中文名——ASCII 目录名，
映射与编码链验证见 bridge-edit-v1/README.md）。

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`** @
`7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入 rpc-dump.js +
capture.mjs `--drill` 模式 + imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `编辑大桥测试.bim`（954,888,192 B——与 bridge-edit-v1 同一源文件） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04` |
| 树 | 1（`25_1d-E:6_0x48`） |
| 采集方式 | **取景到几何域的 drill**（默认视图空域——坑 24，`skipFirstTileWait:true` 跳过首瓦门）：2 目标取景（桥跨 x=500 / x=1400，y=150 z=78——世界域 contentRange 中部/端部，radius=200 显式给定[排除 root 对称域对角线 10km 废值]）+ 各 4 级 `viewport.zoom(undefined, 0.5)` 泵（743m→46m 视域）。坑 16 不复现（取景后泵持续产请求） |
| `provenance.drill` | targets=[[500,150,78],[1400,150,78]]/zoomPerTarget=4/radius=200/skipFirstTileWait=true/framingApi=viewport.zoom(center,factor)/levelsReached=[{levels:4,tilesAtEnd:17},{levels:4,tilesAtEnd:30}] |
| defaultView | 与 bridge-edit-v1 全精度同值（同模型跨会话确定性——含空域视图事实） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（30 瓦 + 1 树 props，0 mismatch）；瓦头形态自检通过 |

## iModelRpc 数据面（imodel.json）

本 dump 会话独立采集一份（与 bridge-edit-v1 同源同值，`gaps=[]`；4 视图/默认 0x99/1 model）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 30 |
| 总字节 | 757,095,192（**722.02 MB**——30 瓦平均 24 MB，大瓦形态贯穿 d2-d6） |
| 深度分布 | d2:2 / d3:3 / d4:7 / d5:12 / d6:6 |
| mult 分布 | ×1:28 / ×2:2 |
| 最大瓦 | d2 134.2 MB / d3 68.6 MB ×2 / d4 41.7 MB、40.1 MB |
| 下潜终点 | 两目标各 4 级全程（46m 视域）——最深层 d6 |

**视口 LOD 形态**：细分链承载（d4-d6 为主）+ 深瓦体量递减缓慢（d6 仍 ~10-40 MB/瓦）——
大瓦模型的 LOD 面代价远高于房模（joeshouse 全树 173,876 瓦 = 278MB；本桥 30 瓦 = 722MB）。

## drill 键 vs bridge-edit-v1（跨会话字节确定性）

drill 30 键中 2 键与 bridge-edit-v1 重叠（`-b-2-0-{0,1}-0-1`——全桥取景粗瓦被两目标取景面重请求），
**逐键 sha256 同值**（跨会话确定性实证——与 M-H joeshouse defaultView 跨会话一致同型）。
联合域（bridge-edit-v1 ∪ 本 dump）= 本模型的回放消费面。

## 浏览深度界（供 Task 2 判据裁剪与用户预期管理）

1. **已采域**：全桥取景粗瓦（d2 ×2，bridge-edit-v1）+ 桥跨两点（x=500/1400）的 d2→d6 细节链。
   **未采域**：其他桥跨位置的 d≥3 细节瓦、已采位置的 d≥7 深瓦、放大链 ×4 以上键。
2. **深缩放行为**：DanQing 回放浏览到未采键时走参考同款缺瓦路径（父瓦 LOD 兜底显示 +
   hasMissingTiles 语义）——**不崩但细节止于已采域**；这是预算硬门（≤2GB/模型）下的如实界。
3. **量级依据**：d2 瓦 34-141 MB、d6 瓦 ~10-40 MB——全树 sweep 在 ≤2GB 门下不可行
   （每下潜一层数百 MB 起步）；用户在此模型上的可用浏览深度 ≈ 全桥轮廓 → 桥跨两端的
   d6 级细节（46m 视域），更深细节需按同配方扩采（新目录名）。

## 瓦特征盘点（30 瓦）

| 特征 | 命中 | DanQing 消费态 |
|---|---|---|
| 边缘 compact | **27/30 瓦** | ✅ 已消费（M-C）——本模型边缘几何的主要形态 |
| polylines(type=1) | 27/30 瓦 | ❌ 未消费（TD-20/TD-23 遗留） |
| pointString(type=2) | 5 图元 | ❌ 未消费 |
| 曲线 | 30/30（100%） | ✅ decode 已消费（M-F） |
| 纹理 / instances | 0 / 0 | 未命中 |
| 顶点表 | numRgba4=surface 图元数 / numRgba3=polylines+points；usesUnquantizedPositions=0 | LUT 直传已消费；TD-27 形态 0 命中 |

## 已知限制

1. 两目标覆盖（桥跨端部）——中段（x≈950）仅有全桥粗瓦（d2）覆盖，无细节链；扩采=新目录名。
2. 默认视图空域（坑 24）——本 dump 的瓦全部来自取景后的请求面（drill provenance 在案）。
3. 级间 waitSettle(10s) 对 >15s/瓦的生成节奏偏紧（坑 25）——深层瓦靠请求重叠兜回；
   provenance.drill.levelsReached 的 tilesAtEnd 已如实记录各级累积。

## manifest 契约

与 bridge-edit-v1/既有 dump 同。

## 大瓦本地持有（GitHub 单文件硬门——2026-09-30 M-K(2) push 裁决）

`files/8.imdl`（= `-b-2-0-0-0-1`，140,675,160 B = 134.2MB，sha256 `afe95030a655daa46848122fc3e1a61b8d3d22fff98063ef8f23ad895bb3d0b4`）
超 GitHub 单文件 100MB 硬门，**git 不入库、本地持有**（已入 `.gitignore`；`bridge-edit-drill-v1/files/8.imdl`
为同一字节[跨会话确定性]同裁）。manifest 的键/byteLength/sha256 记录不变（完整性门仍全量可验）。
回放影响：该瓦被请求时走参考同款 NotFound → 父瓦 LOD 兜底（与"未采域"同语义）；桥锁
`DumpOpenChain.OpensBridgeEditWithWorldContentFraming` 的请求面为 d3 键（`-b-3-0-0-0-1`，69MB 在库），
不依赖本瓦。重采/迁移经 Git LFS 或介质。
