# joeshouse-drill-v1 — JoesHouse.bim（视口请求面 drill 采集）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
rpc-dump.js + capture.mjs `--drill` 模式（`--drill '{"targets":[[x,y,z]],"zoomPerTarget":10,"skipFrame":true}'`）
+ imodel-rpc.js（iModelRpc 数据面）。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `JoesHouse.bim`（DTA assets 就位后 standalone 打开） |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core` @ `6f57040dc68a4c418dcabfe66cf61b036e144d16` |
| 树 | 10（imdl formatVersion 2424832 = major 37——与 joeshouse-v1 同批模型） |
| 采集方式 | 视口请求面：默认视图（内容质心=targets[0]=[7.302, 4.264, 4.496]）上 `viewport.zoom(undefined, 0.5)` 泵逐 ×2 下潜 10 级（skipFrame=true），wrapper 捕获 viewport 自然请求的瓦 |
| `provenance.drill` | targets/zoomPerTarget/framingApi=skipped/zoomInApi/windowSize=2000,1400/viewRect=1556×844/cameraOn=false/levelsReached=[{levels:10, tilesAtEnd:15}]——回放侧可安全忽略 |
| defaultView | 与 joeshouse-v1 全精度一致（跨会话确定性实证——origin/extents/rotation/viewRect 逐分量同值；记录于 manifest.provenance.defaultView） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（15 瓦 + 10 树 props，0 mismatch——不只抽检） |

## iModelRpc 数据面（imodel.json）

本 dump 会话独立采集一份（3 字段同源同值——`connection/views.defaultViewState/models` 与
joeshouse-v1 的 imodel.json 内容一致，`gaps=[]`）。清单见 joeshouse-v1/README.md 的 iModelRpc 节。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 10 |
| 瓦 | 15 |
| 总字节 | 125,516（0.12 MB） |
| 键域 | 4 棵小树各 1 键（0x41/0x45/0x47/0x49 的 `-b-2-0-0-0-1`——maxInitialTilesToSkip=3 跳级键；另 4 棵小树[0x26/0x3d/0x43/0x4b]根即叶、无跳级键）+ 两大树放大链：0x3f/0x4d 各 `-b-2-0-0-0-{1,2,4,8}` + 0x3f `-{10,20}` + 0x4d `-{10}` |
| 下潜终点 | 第 5 级（约）后零新瓦——SSE 在放大链顶饱和（10 级泵到 2cm 视高无 depth-3+ 细分请求） |

**逐键字节**（两大树中心支放大链）：0x3f: 7888/8320/8748/9604/10464/18908（×1..×32）；
0x4d: 5648/6932/9072/12068/16352（×1..×16）。

## drill 键 vs sweep 键域（坑 18 同族再现——交叉验证）

| 键 | 在 joeshouse-v1（sweep 173,876 键域）内？ |
|---|---|
| 12 键（4 小树 depth-2 + 两大树 ×1/×2/×4/×8） | ✅ 在 |
| `-b-2-0-0-0-10`（×16，0x3f/0x4d 各 1） | ❌ **不在**——sweep 放大 cap=×8 |
| `-b-2-0-0-0-20`（×32，0x3f） | ❌ **不在**——sweep 放大 cap=×8 |

结论与 M-G（instances60）同构：**"sweep 键域=视口请求面超集"在放大链顶不成立**（3 键域外），
原因明确（合成 BFS 的人为 cap=×8；真实视口 adjustedPixelSize 启发式放大到 ×16/×32——
非 2 幂 mult 段 "10"/"20" 为 hex）。联合域需 Task 2 多根合并。

**视口 LOD 形态实证**：10 级 zoom 泵（20.86m→2cm 视高）全程**放大链承载 LOD**——
无 depth-3+ 细分派生键、无兄弟分支键（与 instances60 坑 18 同族）；两大树中心支
(i,j,k)=(0,0,0) 放大至 ×32 后 SSE 自然饱和（第 5 级起零新瓦）。

## 坑 16 交叉取证（"取景杀 selection"模型相关性判定）

**JoesHouse 不复现**。对照实验（仓外 `dumps/joeshouse-drill-frametest/`——不入库）：
同目标 `vp.zoom(Point3d, factor)` 重定中心取景后，zoom 泵**继续产请求**（12→14 瓦，
dup=0——第 3 级 2 枚新瓦正常派生）。instances60 上同操作后全部视图变化零请求（5 次复现）。
**判定：坑 16 是模型相关现象（instances60 特异），非会话/视口普遍机制**——机制搜索面
收窄至 instances60 模型特征（深细分/undefined-sizeMultiplier 与重定中心的相互作用）。

## 已知限制

1. 三目标 aiming 未做（单目标中心支）——坑 16 在 JoesHouse 不复现，多目标取景补采
   **无机制障碍**（如需非中心支覆盖，新目录名重采）。
2. 窗口 2000×1400 请求下实际 viewRect=1556×844（Windows 屏幕钳制，provenance.drill 已记）。
3. 本 dump 不含 depth≥3 键（视口未请求——非采集缺失；sweep 域覆盖 depth≤10 全集）。

## manifest 契约

与 joeshouse-v1 同（trees/tiles/sha256 全量落盘；imodel.json 并列；V4 键含非 2 幂 mult）。
