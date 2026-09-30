# baytown-v1 — Baytown.bim（sweep 全树 BFS 采集，depth≤10 + 字节预算 cap 1GB 登记）

**来源**：itwinjs-core display-test-app（**新检出 `D:\Github\itwinjs-core`** @
`7e57d0182ee375e34f3f1be840bbc623c6776e04`，electron + vite dev）经 CDP 注入 rpc-dump.js +
sweep.js（BFS 全树）+ imodel-rpc.js。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`
（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `Baytown.bim`（22,745,088 B——用户指定 `D:\test\Baytown.bim`，sha256 `52096041…` 拷入 DTA assets 后 standalone 打开；OpenPlant 工艺厂模型——imodel 名 "mybaytown"） |
| 参考仓 | `D:\Github\itwinjs-core` @ `7e57d0182ee375e34f3f1be840bbc623c6776e04`（M-K 指定新检出） |
| 采集时刻 | 2026-09-30T01:51:14Z（drill 会话 02:0x——同批） |
| iModel | "mybaytown"，guid `7ac072b1-7df8-492e-85da-163b42aac5ea`，changeset `096f51524e0ed3f088ea298a6833efd1a2285f88`（**非空 changeset**——源 bim 自带，与既有 dump 的 standalone 空串不同，如实记录） |
| projectExtents | low [393.716, 105.315, -0.152] / high [421.869, 140.094, 45.122]（米，对角 63.7） |
| 树 | **1 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:0_0x20000000002`（后缀=model id，与 modelSelector 一一对应；BisCore:PhysicalModel "ProcessPhysicalModel"） |
| sweep 完成信号 | `provenance.sweep.phase="done:budget-capped"`（**字节预算 cap=1000MB 触顶**——98,331 请求；errs=0，leaves=2,849 / branches=95,482，dup 拒收 17,064） |
| 预算裁决 | **sweep 有界采集**：cap=（tiles 120k，depth 10，bytes 1000MB，mag ×8）——触顶的是**字节 cap**；落盘 768.32 MB ≤ 计划硬门 2GB ✓。cap 值随 `provenance.sweep.caps` 落盘 |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（81,268 瓦 + 1 树 props + manifest/imodel.json，0 mismatch——不只抽检） |

### defaultView（应用后的默认取景——sweep 会话实录，与 drill 会话同值）

```
origin:  [405.0616464347499, 152.69810493474992, -6.519069270500294]
extents: [46.279390709685146, 25.102702929932043, 64.97169297126703]
rotation（行主序 Matrix3d.toJSON）——标准 iso 矩阵（√2/2、√6/6、√3/3 族）:
  [  0.7071067811865477, -0.7071067811865475,  2.7755575615628914e-16 ]
  [  0.40824829046386285, 0.4082482904638633,  0.816496580927726     ]
  [ -0.5773502691896257, -0.5773502691896257,  0.5773502691896258     ]
is3d: true  cameraOn: false  lens: 0  viewRect: 1556×844
```

（原始 ViewStateProps 见 imodel.json：BisCore:OrthographicViewDefinition，saved extents y=65.164 →
应用后 25.103——视口纵横比调整。）

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（4,307 B，与 manifest.json 并列；`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/changeset/projectExtents/rpcProps（iModelRpcProps 原样） |
| `views.list` | **5 视图**（全非私有）：`0x20000000006` "OpenPlant 3D"（OrthographicViewDefinition——默认）+ OPPID-01-SURGEFEED / OPPID-02-EXCHANGERS / OPPID-03-TOWER / OPPID-04-COOLERS（DrawingViewDefinition 族） |
| `views.defaultViewId` | `0x20000000006`（`queryDefaultViewId()` 命中覆写） |
| `views.defaultViewState` | **完整 ViewStateProps RPC 载荷原样**：viewDefinitionProps + modelSelectorProps[models×1] + categorySelectorProps[categories×36] + displayStyleProps |
| `models` | 1 个 ModelProps：0x20000000002 BisCore:PhysicalModel "ProcessPhysicalModel" |
| 缺口 | **无**（gaps=[]） |

回放对应关系：modelSelector.models 1 个 model（0x20000000002）⇔ 本 dump 1 棵树。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦（manifest 去重后） | **81,268** |
| 总字节 | 805,645,808（768.32 MB） |
| dup（collector 拒收） | 17,064 |
| 深度分布 | 0:1 / 1:1 / 2:1 / 3:5 / 4:34 / 5:207 / 6:964 / 7:4,080 / 8:14,684 / 9:45,534 / **10:15,757（字节 cap 触顶截断——d10 层不完整）** |
| mult 分布 | ×1:464 / ×2:19,527 / ×4:17,831 / ×8:43,446 |
| 根瓦 | `-b-0-0-0-0-1` 0.91MB / `-b-1-0-0-0-1` 1.06MB / `-b-2-0-0-0-1` 1.39MB（浅层肥、深层薄——d7 29KB 均值） |

**键域形态**：与 House_Model 同族（放大链 ×2/×4/×8 + ×8 细分递归）；增长 d4→d9
6.1×/4.7×/4.2×/4.3×/3.1×——d9 为峰层（45,534 瓦），d9→d10 被字节 cap 截断。

## 采集域界（预算裁决——如实登记）

1. **d10 层不完整**（字节 cap 触顶时 d10 已落 15,757 瓦）——d≥10 的深缩放键域**不在本 dump**。
   视口实际浏览面见 baytown-drill-v1：三目标取景 + 各 10 级 zoom 泵最深请求到 **depth 7**（2 瓦），
   sweep 域（≤d9 完整 + d10 部分）对视口浏览面是充分超集。
2. **瓦数 cap（120k）未触顶**（98,331 请求）——字节是本模型的界（768MB 落盘 vs 瓦数富余）。
3. drill 61 键**全部**在 sweep 域内且逐键 byteLength+sha256 同值（2026-09-30 全量对账）——本模型
   无"视口放大链顶超出 sweep cap 域"现象（drill 放大顶 ×8 = sweep cap，恰在界内）。

## 瓦特征盘点（消费对账表——引擎扩面判断的直接输入）

逐瓦 imdl header + 内嵌 glTF JSON 全量解析（工具 inventory.mjs；明细 inv.json 留仓外）：

| 特征 | 探针 | 命中 | DanQing 消费态 |
|---|---|---|---|
| 纹理（五探针） | materials[].texture / namedTextures / renderMaterials[].textureMapping / surface.uvParams / alwaysDisplayTexture | **0 / 81,268** | 未消费（TD-20 遗留）——**未命中**（与 House_Model 的 36 瓦命中对照） |
| instances | primitives[].instances | 105 瓦 / 108 图元 / 254 实例 | ✅ 已消费（TD-25） |
| **polylines(tesselated)** | primitives[].type=1 | **21,323 图元 / 19,507 瓦（24%）** | ❌ **未消费（TD-20/TD-23 遗留）——本模型大头命中（工艺管线骨架）** |
| **pointString** | primitives[].type=2 | **6,136 图元** | ❌ 未消费（参考形态在案、DanQing 无此消费链登记——厂模型仪表/管嘴点云） |
| 边缘四形态 | edges.* | 全 0 | ✅ 已消费（M-C——无命中） |
| 曲线 | header flags ContainsCurves | 78,419 / 81,268（**96.5%**） | ✅ decode 启发式已消费（M-F） |
| sizeMultiplier | header 解码 | ×1:464 / ×2:19,527 / ×4:17,831 / ×8:43,446 | ✅ 已消费（M-F） |
| 顶点表形态 | numRgbaPerVertex + usesUnquantizedPositions | **numRgba4: 62,844（=全部 surface 图元）/ numRgba3: 27,459（=polylines 21,323 + points 6,136 精确和——非 surface 图元的顶点表形态）**；usesUnquantizedPositions=0 | numRgba4 LUT 直传已消费（TD-20 U7）；**TD-27 unquantized-LUT（numRgba=5）0 命中**；numRgba3 与 polylines/points 消费缺口同域 |
| 多材质/材质数 | materials 节点 | 90,303（全部带 fillColor；图元:材质 1:1） | ✅ fillColor 已接线（TD-18） |
| materialAtlas / uniformColor | vertices | 0 / 80,227 图元 | ✅ 已消费（TD-20） |
| 多树/多 model | manifest trees | 1 树 1 model | ✅ 已装载 |
| auxChannels/areaPattern/patternSymbols/动画/multiModelFeatureTable | 各键 + flags | 0 | ❌ 未消费（无命中） |
| emptySubRangeMask | header emptySubRanges | disallowMagnification 77 / incomplete 24 瓦 | ✅ 已消费（M-F(2)） |

## 已知限制（如实登记）

1. **d10 层部分**（字节 cap 截断——见上"采集域界"）；d≥11 未采。
2. 非空 changeset 源（096f5152…）——回放侧按 dump 内逐瓦 changesetId 记录即可（standalone 打开
   语义下仅作元数据）。
3. OpenPlant 类模型的高 curve 瓦占比（96.5%）意味着深缩放时曲线瓦放大-细分循环（joeshouse 坑 23
   同族）是键域增长主因——d10 以下的键域未采（机制与增长率在案，外推 d10 全层 ~2×d9 ≈ 9 万瓦/~0.9GB）。

## manifest 契约

与 housemodel-v1/joeshouse-v1 同（trees/tiles/sha256 全量落盘；imodel.json 并列；V4 键）。
`provenance.sweep.caps` 为 M-K 新增。
