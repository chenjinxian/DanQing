# bridge-edit-v1 — 编辑大桥测试.bim（iModelRpc 数据面主产物 + 取景后粗瓦活性面）

**目录名映射**：`bridge-edit-v1` ⇔ 原模型文件 **`编辑大桥测试.bim`**（954,888,192 B，中文名——
dump 目录名按计划用 ASCII；中文文件名四层编码链验证见文末）。

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`** @
`7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入 rpc-dump.js +
imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `编辑大桥测试.bim`（954,888,192 B——用户指定 `D:\test\编辑大桥测试.bim`，sha256 `6732b4ee…` 拷入 DTA assets 后 standalone 打开；imodel 名 "TBD"，model 0x48 "大体量模型测试"——1.5 km 级桥梁，910 MB 源） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04`（M-K 指定新检出） |
| 采集时刻 | 2026-09-30T03:08:06Z |
| iModel | "TBD"，guid `dbd4826b-4f72-423c-b4fa-ff8205f2bae7`，changeset ""（standalone） |
| projectExtents | low [-10.881, -16.027, -10.099] / high [1714.207, 313.498, 110.815]（米——**含大空域**，见"默认视图空域"节） |
| 树 | **1 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:6_0x48`（后缀=model id 0x48；BisCore:PhysicalModel "大体量模型测试"）；location 平移 [+10052.37, +2087.70, +467.92]，contentRange（世界域）x 182..1714 / y -12.5..313.5 / z 45.2..110.8——1,532 m × 326 m × 65.6 m 桥梁 |
| 本 dump 角色 | **imodel.json 主产物**（4 视图/defaultView/models 全 JSON——DanQing 打开链回放源）+ 取景后粗瓦活性面（2 瓦 166.55 MB） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（2 瓦 + 1 树 props + manifest/imodel.json，0 mismatch）；瓦头形态自检（"iMdl" magic + major≤37）通过 |

## 默认视图空域（坑 24——本模型的关键事实）

**bim 保存的默认视图 0x99（"3D Imperial Design - View 1"）不含几何**：origin [13.22, 14.60, -14.44]、
extents [23.41, 15.80, 40.00] 的 40 m 视域指向原点附近空域，而桥梁几何（经 location 变换）在
x 182..1714 / z 45..110——**视锥与几何完全不相交**。参考 DTA 打开本模型即白屏（合法零请求）：
CDP 实证 `numRequestedTiles=0` + tileAdmin statistics 全零 + electron 0% CPU + rAF 1ms 活性 +
tile cache 零增长（工具 README 坑 24 取证链）。

**因此本 dump 的 2 瓦非"初始视口请求面"**，而是采集会话内 `viewport.zoomToVolume(世界域 contentRange)`
取景后的粗瓦面（应用后视域 ext=[1366.4, 899.0, 1155.0] / org=[615.0, 783.5, -622.4]）。
**DanQing 打开链连带**：defaultView 回放=空视图，Task 2 接入须备"取景到几何域"路径
（zoomToVolume 世界域对象 {low:{x,y,z},high:{x,y,z}}）——否则三入口打开后白屏。

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（4,279 B，与 manifest.json 并列；`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（iModelRpcProps 原样——rpcProps.key 为**中文路径原样**） |
| `views.list` | **4 视图**（全非私有）：0x99 "3D Imperial Design - View 1"（SpatialViewDefinition——默认）+ 3 项 |
| `views.defaultViewId` | `0x99`（`queryDefaultViewId()` 命中覆写）——**空域视图，见上节** |
| `views.defaultViewState` | **完整 ViewStateProps RPC 载荷原样**：viewDefinitionProps[origin/extents/angles{pitch -35.26, roll -45.00, yaw 30.00}/cameraOn=false] + modelSelectorProps[models×1] + categorySelectorProps[categories×16] + displayStyleProps |
| `models` | 1 个 ModelProps：0x48 BisCore:PhysicalModel "大体量模型测试"（jsonProperties.formatter=英制 ft/in） |
| 缺口 | **无**（gaps=[]） |

回放对应关系：modelSelector.models 1 个 model（0x48）⇔ 本 dump 1 棵树（树 id 后缀=model id）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦 | 2（`-b-2-0-1-0-1` 33.96 MB + `-b-2-0-0-0-1` 140.68 MB——d2 跳级键，maxInitialTilesToSkip=3） |
| 总字节 | 174,636,796（166.55 MB） |
| 瓦头 flags | ContainsCurves=2/2、**Incomplete=2/2**（粗瓦标记不完整——后端对大体量模型的粗瓦域）、DisallowMagnification=2/2 |

**大瓦体量（M-K 预算实证——坑 25）**：d2 单瓦 34/141 MB（粗瓦即全桥几何）——sweep 全树在
≤2GB 硬门下不可行（d2 两瓦已 175 MB；drill 实测 d3 瓦 68.6 MB、d4 瓦 41.7 MB——深度下潜瓦数×
体量双爆炸）。**裁决：本模型不做 sweep**（预登记风险落地）；tiles 面 = 本 dump（全桥取景粗瓦）
+ bridge-edit-drill-v1（跨段细节面），联合域即回放消费面。

## 瓦特征盘点（消费对账表——2 粗瓦样本）

| 特征 | 探针 | 命中 | DanQing 消费态 |
|---|---|---|---|
| 纹理（五探针） | materials[].texture 等 | **0 / 2** | 未命中 |
| instances | primitives[].instances | 0 | — |
| **边缘 compact** | edges.compact | **4 图元（2/2 瓦）** | ✅ 已消费（M-C）——**joeshouse 之后首个 compact 边命中模型** |
| **polylines** | primitives[].type=1 | 8 图元 | ❌ 未消费（TD-20/TD-23 遗留） |
| **pointString** | primitives[].type=2 | 2 图元 | ❌ 未消费（同 Baytown——厂/桥模型点要素） |
| 曲线 | header flags ContainsCurves | 2 / 2（100%） | ✅ decode 启发式已消费（M-F） |
| 顶点表形态 | numRgbaPerVertex | numRgba4: 4（=surface 图元数）/ numRgba3: 10（=polylines+points）；usesUnquantizedPositions=0 | LUT 直传已消费；TD-27 形态 0 命中 |
| 多树/多 model | manifest trees | 1 树 1 model | ✅ |

（drill 30 瓦的扩展盘点：edges 27/30 瓦、polylines 27/30 瓦、curves 30/30、纹理 0、instances 0——
与本表同型；明细 bridge-edit-drill-v1/README.md。）

## 中文文件名四层编码链（全链实测通过——采集配方存档）

| 层 | 验证 |
|---|---|
| ① 脚本传参 | bash→node argv/env UTF-8→UTF-16 无损（hex 比对 `e7bc96…`[编] 起始正确）+ spawn 子进程 roundtrip（capture.mjs 同路径） |
| ② assets 拷贝 | `fs.copyFileSync` 910 MB 拷 3.5s，双端 sha256 `6732b4ee…` 同值 |
| ③ DTA 打开 | `IMJS_STANDALONE_FILENAME` 传中文路径，DTA 正常打开（imodel.json 4 视图全采）；后端 tile cache `编辑大桥测试.bim.Tiles` 同名落盘 |
| ④ collector 落盘 | `provenance.seed="编辑大桥测试.bim"` 原样入 manifest（JSON UTF-8）——本文件即证据 |

**dump 目录名用 ASCII**（`bridge-edit-v1` / `bridge-edit-drill-v1`）——本节即"目录名 ⇔ 原中文名"映射。

## 已知限制（如实登记）

1. **默认视图空域**（见上——坑 24）：defaultView 回放为白屏；取景路径由 Task 2 补。
2. **无 sweep**：全树键域未采（预算裁决——d2 瓦体量实证）；深缩放键域缺失，
   浏览深度界见 bridge-edit-drill-v1/README.md。
3. 粗瓦 Incomplete=2/2：粗瓦域本身标记不完整（后端语义），非采集缺失。

## manifest 契约

与既有 dump 同（trees/tiles/sha256 全量落盘；imodel.json 并列；V4 键）。
