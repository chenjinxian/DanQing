# instances60-drill-v1 — Properties_60InstancesWithUrl2.ibim（视口请求面 drill 采集）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
rpc-dump.js（`generateTileContent`/`requestTileTreeProps` 实例级包裹）+ capture.mjs 新增
`--drill` 模式（`--drill '{"targets":[[x,y,z],...],"zoomPerTarget":N,"radius":r,"skipFrame":bool}'`）。
**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `Properties_60InstancesWithUrl2.ibim`（60 实例）——DTA assets 就位后 standalone 打开 |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core`（采集时 HEAD 记于 manifest.provenance.itwinjsCommit） |
| 树 | `25_1d-E:6_0x1c`（1 树） |
| imdl formatVersion | 2424832（major 37——CurrentImdlVersion.Major 域，与 instances60-v1 同） |
| 采集方式 | 视口请求面：默认视图（内容质心对齐 targets[0]）上 `viewport.zoom(undefined, 0.5)` 泵逐 ×2 下潜 10 级（skipFrame=true），wrapper 捕获 viewport 自然请求的瓦 |
| `provenance.drill` | targets/zoomPerTarget/radius/framingApi=skipped/zoomInApi/windowSize=2000,1400/viewRect=1556×844/cameraOn=false/levelsReached——回放侧可安全忽略（DumpMount 只读 files+manifest） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（5 瓦 + 1 树 props，0 mismatch——不只抽检） |

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 5 |
| 总字节 | 289,572（0.28 MB） |
| 键域 | `-b-2-0-0-0-{1,2,4,8,10}`——单分支 (i,j,k)=(2,0,0) depth-0 的**放大链 ×1..×10** |
| 放大链混入率 | 5/5 键带 mult≥1 语义：1 键 mult=1（默认视图基础瓦）+ 4 键放大链（mult=2/4/8/10）——**无细分派生键、无兄弟分支键** |
| 下潜终点 | ×10 后 SSE 自然饱和（第 5..10 级零新瓦）——**真实视口的 LOD 需求在此模型上就是放大链，细分不经由视口触发** |

**逐瓦字节**（内容逐 mult 唯一——与 sweep 观察同）：20844 / 27084 / 44840 / 70828 / 125976。

## drill 键 vs sweep 键域（M-F artifact 裁定的交叉验证点）

| 键 | 在 instances60-v1（sweep 3587 键域）内？ |
|---|---|
| `-b-2-0-0-0-1/2/4/8` | ✅ 在 |
| `-b-2-0-0-0-10` | ❌ **不在**——sweep.js 的合成分域 cap=×8（sweep.js MAX_MAGNIFICATION=8，对齐 pitfall 14"实际请求 ≤×8"的假设），真实视口实测请求到 **×10** |

结论：**"sweep 键域=视口请求面超集"被实测推翻**（drill ⊄ sweep，1 键域外）——域差原因明确
（合成 BFS 的人为 cap，非参考机制分岔）。视口面的放大顶（×10，adjustedPixelSize 启发式，
非 2 幂）是 sweep 合成面接触不到的真实行为。两侧交集中的 4 键字节级一致（同源后端生成）。

## 已知限制（采集期 BLOCKED 项，如实登记）

1. **三目标 aiming 未达成**：任何重定中心取景（zoomToVolume / vp.zoom(Point3d,factor) / 真实滚轮）
   在本模型/会话上使后续全部视图变化的瓦请求静默归零（5 次 capture 复现；renderFrame/视锥/
   ViewingSpace/selection 调用全活着但返回空集，机制未定位）。取证链与死后解剖见
   `danqing-rpc-tools/README.md` 坑 16 与 drilldiag3/5/7/9/10/12/13。本 dump 因此只覆盖
   默认视图中心（=内容质心=计划 targets[0]）的下潜面；targets[1]/[2]（盒低角/高角近区）待机制
   定位后补采（新目录名）。
2. **细分派生链（-b-1-0-0-0-1 型）经视口不可得**：视口 SSE 在放大链 ×10 处饱和——计划预期的
   drill 细分键域非此模型真实视口行为（如实报告，不降格判据）。
3. 窗口 2000×1400 请求下实际 viewRect=1556×844（Windows 屏幕钳制，provenance.drill 已记）。

## manifest 契约

与 compatseed-v1/mirukuru-v1/instances60-v1 同（trees/tiles/sha256 全量落盘）。
`provenance.drill` 为附加元数据（回放忽略）。键格式 V4：`-b-{i}-{j}-{k}-{depth}-{mult}`
（mult=放大倍率；本 dump mult=10 键验证回放侧 ContentIdProvider 须接受非 2 幂 mult）。
