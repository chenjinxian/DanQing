# housemodel-v1 — House_Model.bim（sweep 全树 BFS 采集，depth≤10 + 瓦数预算 cap 120k 登记）

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`**——与 M-D..MI 的
tiangong-kaiwu 检出同 commit `7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入
rpc-dump.js（`generateTileContent`/`requestTileTreeProps` 实例级包裹）+ sweep.js（BFS 全树）+ imodel-rpc.js
（iModelRpc 数据面）。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `House_Model.bim`（24,535,040 B——用户指定 `D:\test\House_Model.bim`，sha256 `cb33899f…` 拷入 DTA assets 后 standalone 打开） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04`（M-K 指定新检出；适配冒烟 2026-09-30 全链通过——坑 3 兜底 `/@fs/` 模块发现在新检出同型工作） |
| 采集时刻 | 2026-09-30T01:33:17Z |
| iModel | "House_Model.bim"，guid `36decf30-645f-4687-8cf1-040c92d6eda1`，changeset ""（standalone） |
| projectExtents | low [-7.128, -5.718, -0.610] / high [19.155, 19.228, 11.810]（米，对角 38.3） |
| 树 | **1 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:0_0x26`（后缀=model id 0x26，与 modelSelector 一一对应；BisCore:PhysicalModel "House_Model"） |
| sweep 完成信号 | `provenance.sweep.phase="done:budget-capped"`（**瓦数预算 cap=120,000 请求触顶**——非自然收敛；errs=0，leaves=5,901 / branches=114,099，dup 拒收 13,344） |
| 预算裁决 | **sweep 有界采集**：cap=（tiles 120k，depth 10，bytes 1000MB，mag ×8）——触顶的是**瓦数 cap**；落盘 489.91 MB ≤ 计划硬门 2GB ✓。cap 值随 `provenance.sweep.caps` 落盘（capture.mjs `--sweep-max-*` 注入） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（106,658 瓦 + 1 树 props + manifest/imodel.json，0 mismatch——不只抽检） |

### defaultView（应用后的默认取景——sweep 会话实录，与 drill 会话同值）

```
origin:  [-7.697224741842015, 32.67749740127916, -4.62285029714193]
extents: [44.82454296160068, 24.313569575572604, 36.87868026189712]
rotation（行主序 Matrix3d.toJSON）:
  [  0.9227168738896339, -0.3854783659809479,  1.3877787807814457e-17 ]
  [ -0.04769422846475421, -0.11416534175552427, 0.992316247631266    ]
  [ -0.38251644567324616, -0.9156269459242136, -0.12372738050651448  ]
is3d: true  cameraOn: **true**  lens: 1.0892212059510455 rad  viewRect: 1556×844
```

**注意与 joeshouse 的差别**：本模型默认视图**开透视相机**（cameraOn=true，lens 62.4°）且为
非标准旋转（屋顶斜视角）——DanQing 打开时须按本记录应用（原始 ViewStateProps 见 imodel.json：
saved extents y=18.485 → 应用后 24.314，视口纵横比调整）。

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（8,890 B，与 manifest.json 并列；`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（iModelRpcProps 原样） |
| `views.list` | **42 视图**（34 非私有）：Plan-Second/First Floor ×4、Section-Left/Back、Elevation-Right/Back 等 DrawingViewDefinition 族 + 默认 3D 视图 |
| `views.defaultViewId` | `0xdb`（判定=ViewPicker.populate 逻辑：`queryDefaultViewId()`=0xdb 命中覆写）——BisCore:SpatialViewDefinition，**cameraOn=true** |
| `views.defaultViewState` | **完整 ViewStateProps RPC 载荷原样**（IModelReadRpcInterface.getViewStateData）：viewDefinitionProps[origin/extents/angles{pitch -22.49, roll -97.70, yaw -2.96}/camera{eye,focusDist,lens}/cameraOn=true] + modelSelectorProps[models×1] + categorySelectorProps[categories×25] + displayStyleProps |
| `models` | 1 个 ModelProps：0x26 BisCore:PhysicalModel "House_Model" |
| 缺口 | **无**（gaps=[]——defaultViewStateSource="IModelReadRpcInterface.getViewStateData（RPC 载荷原样）"） |

回放对应关系：modelSelector.models 1 个 model（0x26）⇔ 本 dump 1 棵树（树 id 后缀=model id）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦（manifest 去重后） | **106,658** |
| 总字节 | 513,708,984（489.91 MB） |
| dup（collector 拒收） | 13,344（放大链二访派生波——坑 23 语境键去重后的正常重合） |
| 深度分布 | 0:1 / 1:1 / 2:1 / 3:2 / 4:14 / 5:146 / 6:797 / 7:4,634 / 8:26,974 / 9:59,296 / **10:14,792（瓦数 cap 触顶截断——d10 层不完整）** |
| mult 分布 | ×1:451 / ×2:28,451 / ×4:28,449 / ×8:49,307 |
| 逐树 | 0x26 = 106,658 瓦 490MB（单树模型） |
| 根瓦 | `-b-0-0-0-0-1` 3.19MB / `-b-1-0-0-0-1` 3.73MB / `-b-2-0-0-0-1` 3.99MB / d3 两瓦 4.38MB（浅层瓦极肥——d4 起 370KB→d7 28KB 逐层变薄） |

**键域形态**：depth 0-3 全链真实存在（与 joeshouse 小树"根即叶/跳级键"形态不同）——放大链
×2/×4/×8 + ×8 细分递归（TileMetadata.ts:841-846 子代继承父 mult）；d4→d9 增长 10.4×/5.5×/5.8×/5.8×/2.2×
（d6→d7→d8 仍有 ~5.8×/层——曲线瓦放大-细分循环的 joeshouse 同族形态），d9→d10 被瓦数 cap 截断。

## 采集域界（预算裁决——如实登记）

1. **d10 层不完整**（cap 触顶时 d10 已落 14,792 瓦、队列未清空）——d≥10 的深缩放键域**不在本 dump**。
   视口实际浏览面见 housemodel-drill-v1：默认视图 + 10 级 zoom 泵最深只请求到 **depth 6**（38m→0.62m
   视域），sweep 域（≤d9 完整 + d10 部分）对视口浏览面是充分超集。
2. **bytes cap（1000MB）未触顶**（落盘 490MB）——体积不是本模型的界，瓦数 cap 是。
3. 视口放大链顶 ×4（drill 实测）≤ sweep 放大 cap ×8——本模型**无**坑 18 族"drill 键在 sweep 域外"
   （drill 61 键全在 sweep 域内，逐键对账见 housemodel-drill-v1/README.md）。

## 瓦特征盘点（消费对账表——引擎扩面判断的直接输入）

逐瓦 imdl header + 内嵌 glTF JSON 全量解析（工具 inventory.mjs；明细 inv.json 留仓外）：

| 特征 | 探针 | 命中 | DanQing 消费态 |
|---|---|---|---|
| **纹理** | materials[].texture / namedTextures / renderMaterials[].textureMapping / surface.uvParams / alwaysDisplayTexture | **36 瓦命中**（materialsWithTexture=36、namedTextures=36、uvParams=36；textureMapping=0、alwaysDisplayTexture=0） | ❌ **未消费（TD-20 遗留 textured 变体）——本模型首次出现命中，引擎扩面判断的直接输入** |
| instances | primitives[].instances | 214 瓦 / 288 图元 / 3,984 实例 | ✅ 已消费（TD-25） |
| **polylines(tesselated)** | primitives[].type=1（MeshPrimitiveType.Polyline） | **1,514 图元 / 1,514 瓦** | ❌ **未消费（TD-20/TD-23 遗留）——inventory 探针 2026-09-30 修正（type 数字枚举）后首次现形** |
| pointString | primitives[].type=2 | 0 | 未消费（无命中） |
| 边缘四形态 | edges.segments/silhouettes/indexed/compact | **全 0**（与 joeshouse 75,336 瓦 compact 相反——本模型无边缘几何） | ✅ 已消费（M-C——无命中） |
| 曲线 | header flags ContainsCurves | 100,667 / 106,658（94.4%） | ✅ decode 启发式已消费（M-F）；几何经 mesh 面消费 |
| sizeMultiplier | header 解码（TileMetadata.ts:907-934 复算） | ×1:451 / ×2:28,451 / ×4:28,449 / ×8:49,307 | ✅ 已消费（M-F setContent 升门控） |
| 顶点表形态 | vertices.numRgbaPerVertex + usesUnquantizedPositions | **numRgba4（量化 LUT）73,327 图元 / numRgba3 1,765 图元；usesUnquantizedPositions=0** | ✅ numRgba4 LUT 直传已消费（TD-20 U7）；**TD-27 的 unquantized-LUT（numRgba=5）形态在本模型 0 命中** |
| 多材质/材质数 | materials 节点 | 75,092（全部带 fillColor；图元:材质 1:1） | ✅ fillColor 已接线（TD-18） |
| materialAtlas / uniformColor | vertices | 27,663 / 54,009 图元 | ✅ 已消费（TD-20） |
| 多树/多 model | manifest trees | 1 树 1 model | ✅ 已装载（M-E 多树链的单树特例） |
| auxChannels/areaPattern/patternSymbols/动画/multiModelFeatureTable | JSON 各键 + header flags | 0 | ❌ 未消费（无命中，不阻塞） |
| emptySubRangeMask | header emptySubRanges | 非零（disallowMagnification 64 瓦/incomplete 142 瓦） | ✅ 已消费（M-F(2)） |

## 已知限制（如实登记）

1. **d10 层部分**（瓦数 cap 截断——见上"采集域界"）；d≥11 未采。
2. 纹理命中 36 瓦：瓦 JSON 含 materials[].texture/namedTextures/uvParams 引用，但**纹理字节本体不在
   imdl 瓦内**（externalTextures 走独立 RPC——本 dump 与既有 dump 同界，不含纹理图源）。
3. defaultView 透视相机（cameraOn=true）——DanQing 侧 M-H 的初始取景应用链按 cameraOff 等轴测
   验证过；本模型是首个 cameraOn 用例（Task 2 接入时需核对相机路径）。

## manifest 契约

与 joeshouse-v1 同（trees/tiles/sha256 全量落盘；imodel.json 并列；V4 键 `-b-{d}-{i}-{j}-{k}-{mult}`）。
`provenance.sweep.caps` 为 M-K 新增（预算 cap 值随 provenance 落盘）。
