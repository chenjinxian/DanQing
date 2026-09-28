# joeshouse-v1 — JoesHouse.bim（sweep 全树 BFS 采集，depth≤10 登记 cap）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
rpc-dump.js（`generateTileContent`/`requestTileTreeProps` 实例级包裹）+ sweep.js（BFS 全树：
合成 iModelTree 鸭子类型请求全部 contentId，页内 computeChildTileProps 派生子瓦）+ imodel-rpc.js
（iModelRpc 数据面）。**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `JoesHouse.bim`（1,404,928 B——DTA assets 就位后 standalone 打开；源 `test-apps/display-test-app/test-models/`） |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core` @ `6f57040dc68a4c418dcabfe66cf61b036e144d16` |
| 采集时刻 | 2026-09-28T14:02:49Z（第 9 轮采集——前 8 轮暴露并清偿坑 19/20/22/23，见工具 README） |
| iModel | "Joe's house.bim"，guid `2b382042-4a92-46a4-a3cf-db4b77085489`，changeset ""（standalone） |
| projectExtents | low [-7.488, -9.009, -0.643] / high [27.762, 18.753, 11.693]（米） |
| 树 | **10 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:6_0x{26,3d,3f,41,43,45,47,49,4b,4d}`（后缀=model id，与 modelSelector 一一对应） |
| sweep 完成信号 | `provenance.sweep.phase="done"`（自然收敛至 cap 界——非 budget-capped；tiles=194,612[含二访 dup] / leaves=342 / errs=0） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（173,876 瓦 + 10 树 props，0 mismatch，49.2s——不只抽检） |

### defaultView（应用后的默认取景——与 drill 会话全精度一致）

```
origin:  [-0.3322686917484541, 23.82680528315059, -14.244668121301645]
extents: [38.46216332077429, 20.862510181705332, 35.41381746319353]
rotation（行主序 Matrix3d.toJSON）:
  [ 0.7071067811865478, -0.7071067811865472, -1.1102230246251565e-16 ]
  [ 0.4082482904638621,  0.4082482904638622,  0.8164965809277269     ]
  [-0.5773502691896262, -0.5773502691896266,  0.5773502691896245    ]
is3d: true  cameraOn: false  lens: 0  viewRect: 1556×844
```

记录的是**视口应用后**的状态（DTA 加载 bim 保存的 ViewState 后按 viewRect 纵横比调整
extents.y——原始 ViewStateProps 见 imodel.json：saved extents [38.462, 20.161, 35.414] →
应用后 y 20.161→20.863）。DanQing 打开时应直接应用本记录值（等轴测视角：rotation 为
标准 iso 矩阵——√2/2、√6/6、√3/3 族）。

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（7,144 B，与 manifest.json 并列；采集=本 dump 同一 sweep 会话，`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（iModelRpcProps 原样） |
| `views.list` | **1 视图**：`0x4e` "3D Imperial Design - View 1"（BisCore:SpatialViewDefinition，非私有） |
| `views.defaultViewId` | `0x4e`（判定=ViewPicker.populate 逻辑：非私有首项；`queryDefaultViewId()`=0x4e 命中覆写——两路同值） |
| `views.defaultViewState` | **完整 ViewStateProps RPC 载荷原样**（IModelReadRpcInterface.getViewStateData——views.load 内部同参）：viewDefinitionProps[origin/extents/angles/camera{eye,focusDist,lens}/cameraOn/categorySelectorId/displayStyleId] + modelSelectorProps[models×10] + categorySelectorProps[categories×8] + displayStyleProps[environment/hline 等全样式] |
| `models` | 10 个 ModelProps（全 BisCore:PhysicalModel）：0x26 Joe's house / 0x3d Struct master / 0x3f Struct 2 / 0x41 Struct 1 / 0x43 Arch master / 0x45 Arch 1 / 0x47 Arch 2 / 0x49 Arch 3 / 0x4b MEP master / 0x4d MEP 1 |
| 缺口 | **无**（gaps=[]——defaultViewStateSource="IModelReadRpcInterface.getViewStateData（RPC 载荷原样）"） |

回放对应关系：modelSelector.models 10 个 model ⇔ 本 dump 10 棵树（树 id 后缀=model id）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 10 |
| 瓦（manifest 去重后） | **173,876** |
| 总字节 | 291,514,784（278.0 MB） |
| dup（collector 拒收） | 20,742（视口自然请求重叠 ~7.3k + 坑 23 二访派生波 ~13.5k——见下） |
| 深度分布 | 0:16 / 1:6 / 2:12 / 3:15 / 4:48 / 5:122 / 6:447 / 7:1,872 / 8:9,067 / 9:50,791 / 10:111,480 |
| mult 分布 | ×1:16（根）/ ×2:20,744 / ×4:20,744 / ×8:132,372 |
| 逐树 | 0x3f(Struct 2)=34,551 瓦 123MB；0x4d(MEP 1)=139,313 瓦 162MB；其余 8 树各 1-2 瓦（根叶或根+depth-2） |

**键域形态**（与 instances60 同族但更极端）：每体素列 = 放大链 ×2/×4/×8（3 键）+ ×8 细分
（子代继承父 mult=8——TileMetadata.ts:841-846）；8 棵小树为浅叶（根即叶或根+一跳 depth-2，
maxInitialTilesToSkip=3 的视口跳级键）。叶=342（仅小树与真空体素——曲线瓦永不 isLeaf，见 cap 登记）。

## 深度 cap 登记（M-H Task 1 判定的采集域界）

**MAX_DEPTH=10**（3.7cm 体素界 = 38m 项目域/2^10）。机制：曲线瓦（MEP 管/结构弧面，
ContainsCurves=173,530/173,876 瓦[99.8%]）decode 永不 isLeaf → sizeMultiplier=1 →
放大链 ×2/×4/×8（cap=×8，真实视口界）→ ×8 强制细分 → 子代同型递归（坑 14 类
放大-细分循环，**永不自然收敛**）。实测增长 @depth 8→10（0x3f：2,164→10,426→20,922
[4.8×/2.0×]；0x4d：6,903→40,365→90,558 [5.8×/2.2×]——峰速在 d8→d9，d9→d10 收敛
~2×）；按 ~2×/层外推 depth≤12 全量 ~85 万+ 瓦/GB 级资产——采集与体积不可行。视口浏览房模实际下潜
≤depth 10（drill 实测 10 级泵仅放大链 ×32、无 depth-3+ 细分请求——joeshouse-drill-v1 交叉验证）。
depth-10 界瓦在 manifest 中计为 branch（342 叶不含界瓦——界内全键字节真实有效）。

## 瓦特征盘点（消费对账表——M-H 引擎规模判定的直接输入）

逐瓦 imdl header + 内嵌 glTF JSON 全量解析（工具 inventory.mjs；明细 joeshouse-v1-inventory.json 留仓外）：

| 特征 | 探针 | 命中 | DanQing 消费态 |
|---|---|---|---|
| **纹理** | materials[].texture / namedTextures / renderMaterials[].textureMapping / surface.uvParams / alwaysDisplayTexture | **0 / 173,876** | 未消费（TD-20 遗留 textured 变体）——**未命中，引擎本里程碑不扩面** |
| instances | primitives[].instances | 67 瓦 / 81 图元 / 795 实例 | ✅ 已消费（TD-25） |
| 边缘 compact | edges.compact | 75,336 瓦 | ✅ 已消费（M-C） |
| 边缘 segments/silhouettes/indexed | edges.* | 0 | ✅ 已消费（M-C——无命中） |
| 曲线 | header flags ContainsCurves | 173,530 / 173,876（99.8%） | ✅ decode 启发式已消费（M-F）；几何经 mesh 面消费（曲线已三角化入 surface 图元——polylines/pointString 图元 0 命中） |
| sizeMultiplier | header 解码（TileMetadata.ts:907-934 复算） | ×1:16 / ×2/×4:各 20,744 / ×8:132,372 | ✅ 已消费（M-F setContent 升门控） |
| 多材质/材质数 | materials 节点 | 77,166（全部带 fillColor；图元:材质 ≈ 1:1） | ✅ fillColor 已接线（TD-18）；per-图元材质关联已解析 |
| 顶点色/法线 | vertices.uniformColor | 77,131 / 77,166 图元 | ✅ LUT 直传已消费（TD-20 U7；uniformColor 在顶点表内） |
| 多树/多 model | manifest trees | 10 树 | ✅ 多树已装载（M-E） |
| polylines(tesselated)/auxChannels/areaPattern/patternSymbols/动画 | JSON 各键 | 0 | ❌ 未消费（TD-20/TD-23 遗留——无命中，不阻塞） |
| emptySubRangeMask | header emptySubRanges | 见 manifest（非零） | ✅ 已消费（M-F(2) 最后一跳） |

## 已知限制（如实登记）

1. **depth≥11 未采**（cap 登记见上——机制、增长率、投影在案；drill 证明视口面在此界内）。
2. **视口放大链顶超出 sweep 域**（坑 18 同族）：sweep 放大 cap=×8，视口实测请求 ×16/×32
   （joeshouse-drill-v1 的 3 键 `-b-2-0-0-0-{10,20}` 在本 sweep 域外）——联合域需
   Task 2 多根合并（sweep ∪ drill）。
3. leaves=342 的浅叶域：8 棵小树全树即 1-2 瓦（非缺失——模型本身浅）。
4. 采集九轮方的过程：坑 19（budget cap settle 死锁/drain 链静默死亡）、坑 20（POST 无背压
   丢瓦 33%）、坑 22（collector 无声死亡）、坑 23（派生收敛指数重复循环 + visited 语境键）
   全部暴露并清偿——过程证据在 `danqing-rpc-tools/capture-sweep-joeshouse{1..9}.log` 与
   工具 README 坑清单。

## manifest 契约

与 compatseed-v1/mirukuru-v1/instances60-v1 同（trees/tiles/sha256 全量落盘）。
`provenance.sweep`/`provenance.defaultView` 为附加元数据（回放可忽略——DumpMount 只读
files+manifest；defaultView 供 M-H Task 3 初始取景应用）。键格式 V4：
`-b-{depth}-{i}-{j}-{k}-{mult}`（全 hex——mult "10"=0x10=16，回放侧 ContentIdProvider
须接受非 2 幂 mult；imodel.json 为 iModelRpc 面回放源）。
