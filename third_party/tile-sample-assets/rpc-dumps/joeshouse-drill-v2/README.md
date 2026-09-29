# joeshouse-drill-v2 — JoesHouse.bim（M-I(5) P2c 补采：saved-view 面 + 缩远态 LOD 面）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
rpc-dump.js + capture.mjs `--drill` 模式（`--drill '{"targets":[[2,9,8.5]],"zoomPerTarget":3}'`，无
skipFrame——JoesHouse 取景不复现坑 16）+ imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`
（§8.2 零网络约束）。采集目的：M-I Task 5 P2c——女儿墙区"采集域混杂"定性的补采归一尝试。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `JoesHouse.bim`（DTA assets standalone 打开，tile cache 已暖——重采高速路径） |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core` @ `e44b94a1913e056971951f2354afc8ee0dc487ba`（**注意**：与 joeshouse-v1/drill-v1 的 `6f57040d` 不同——检出已前进；imdl formatVersion 同为 2424832=major 37，字节域兼容） |
| 树 | 10（与 v1/drill-v1 同批模型同树 id 集） |
| 采集方式 | 默认视图立起（saved-view 面先落）→ `viewport.zoom(Point3d(2,9,8.5), factor)` 取景 → `zoom(undefined,0.5)` 泵 3 级；wrapper 捕获 viewport 自然请求 |
| `provenance.drill` | targets=[[2,9,8.5]]/zoomPerTarget=3/framingApi=viewport.zoom(center,factor)/radiusUsed=173.231（=props=10.json root range 对角线一半——见"已知限制"1）/viewRect=1556×844/levelsReached=[{levels:3, tilesAtEnd:20}] |
| defaultView | 与 joeshouse-v1 全精度一致（跨会话确定性再证——origin/extents/rotation/viewRect 逐分量同值；记录于 manifest.provenance.defaultView） |
| 校验 | capture 收尾自动校验：sha256 抽检 3 条复算一致 + byteLength/formatVersion 门全过（"capture OK"） |

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 10 |
| 瓦 | 20 |
| 总字节 | 81,996（0.08 MB） |
| 键域 | 6 棵非平凡树（0x3f/0x4d/0x41/0x45/0x47/0x49）各 3-4 键；4 棵空瓦小树（0x26/0x3d/0x43/0x4b）本会话零 content 请求（根即叶，与既往会话同形） |

**逐批键域（按落盘 mtime 分期——精确到请求态归属）**：

| 批 | mtime | 请求态 | 键 | 域归属 |
|---|---|---|---|---|
| n21-26（6 瓦） | 21:55:05（视口立起同秒） | **saved-view 面** | 6 树各 `-b-2-0-0-0-1`（3,112-7,888B） | 全在 v1∪drill-v1（drill-v1 同键同字节域） |
| n27-32（6 瓦） | 21:55:18（取景 zoom-OUT ×16.6 后） | 缩远态面 | 6 树各 `-b-0-0-0-0-1`（depth-0 m1） | 全在 v1（sweep 下潜域覆盖 depth-0） |
| n33-38（6 瓦） | 21:55:18（同上批） | 缩远态面 | 6 树各 `-b-1-0-0-0-1`（depth-1 首层子瓦） | **6 键全不在 v1∪drill-v1**——本 dump 独有 |
| n39-40（2 瓦） | 21:55:52（第 3 级泵后） | 缩远态面 | 0x3f/0x4d 各 `-b-0-0-0-0-2`（depth-0 m2） | 在 v1（放大 ×2 ≤ sweep cap ×8） |

## 与既有域的关系（P2c 补采的实际增量）

1. **saved-view 面零增量**：本会话 saved 视图（ext 38.5×20.9×35.4）请求面 = 6×`-b-2-0-0-0-1`——
   与 drill-v1 首层完全同键（DumpOpenChain/DumpBrowse 既证 19 键全连通），**回放侧无缺口**。
2. **缩远态面 6 键增量**（`-b-1-0-0-0-1` × 6 树，全为 v1∪drill-v1 域外；同批 `-b-0-0-0-0-1`/
   `-b-0-0-0-0-2` 8 键均在 v1 域内）：sweep BFS（下潜域）与 drill-v1（saved 起点下潜域）都
   **从不缩远**——depth-1 首层子瓦只在视域放大（zoom-out）时被 SSE 选中。这是"视口请求面"的
   一个此前未采集的方向（粗端）。
3. **女儿墙深钻未达成**（如实登记）：取景 radius 缺省=root range 对角线一半（173m，props=10.json
   的根域）→ 取景 factor=(2×173)/20.9≈**16.6=zoom-OUT**（视域 20.9m→346m）——3 级泵只回到 43m，
   未达女儿墙尺度。若需深钻：`--drill` 显式给 `radius`（如 6——factor≈0.57=zoom-IN）。**P2c 归一
   结论改由回放侧对拍承担**：saved-view 面零增量 ⇒ 现行 v1∪drill-v1 回放域即 DTA 活体同面，
   女儿墙区对拍 = 渲染对齐问题（M-I(3)/(4) 已接线的色表/边线），非域缺口。

## 已知限制

1. 取景 radius 用了工具缺省（root range 对角线/2）——对本模型产生 zoom-OUT 而非取景zoom-IN；
   深钻女儿墙未实现（补救配方见上）。
2. 窗口 2000×1400 实际 viewRect=1556×844（同 drill-v1——Windows 屏幕钳制）。
3. 4 棵空瓦小树本会话零 content 请求（根即叶）——其字节在 v1 域内。

## manifest 契约

与 joeshouse-v1 同（trees/tiles/sha256 全量落盘；imodel.json 并列——本会话独立采集，`gaps=[]`，
defaultViewId=0x4e、10 models 与 v1 同值）。
