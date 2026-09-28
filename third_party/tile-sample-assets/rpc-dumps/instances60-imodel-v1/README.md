# instances60-imodel-v1 — Properties_60InstancesWithUrl2.ibim（iModelRpc 数据面采集）

**来源**：itwinjs-core display-test-app（tiangong-kaiwu 检出，electron + vite dev）经 CDP 注入
rpc-dump.js + imodel-rpc.js（M-H 需求升级：iModelRpc 面——打开连接/视图清单/默认 ViewState/ModelProps
全量落盘，DanQing 按 itwinjs 完全一致流程回放的参数源）。tiles 不重复采集（instances60 已有
sweep/drill 资产：`instances60-v1`/`instances60-drill-v1`——本目录只为 iModelRpc 面自包含；
manifest 内 1 瓦为默认视图自然请求，顺带证明会话活性）。
**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `Properties_60InstancesWithUrl2.ibim`（DTA assets 就位后 standalone 打开） |
| 参考仓 | `D:\Github\tiangong-kaiwu\itwinjs-core` @ `6f57040dc68a4c418dcabfe66cf61b036e144d16` |
| iModel | "DgnV8Bridge"，guid `dfac2750-4c3e-41de-afe7-5f4d97375006`，changeset ""（standalone） |
| 树 | 1（`25_1d-E:6_0x1c`——imdl formatVersion 2424832 = major 37，与 instances60-v1 同） |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（1 瓦 + 1 树 props，0 mismatch） |

### defaultView（应用后的默认取景——manifest.provenance.defaultView）

视口应用后的 origin/extents/rotation/is3d/cameraOn/viewRect 全量记录（形态同 joeshouse-v1
README——此处不重复粘贴；DTA 应用 saved ViewState 后按 viewRect 纵横比调整 extents.y）。
原始 ViewStateProps 见 imodel.json。

## iModelRpc 数据面（imodel.json——本目录的主产物）

`imodel.json`（3,744 B；`gaps=[]`，defaultViewStateSource="IModelReadRpcInterface.getViewStateData（RPC 载荷原样）"）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（iModelRpcProps 原样） |
| `views.list` | **4 视图**（全 BisCore:SpatialViewDefinition，全非私有）：0x25 "Default - View 1" / 0x2a "Default - View 2" / 0x2f "Default - View 3" / 0x34 "Default - View 4" |
| `views.defaultViewId` | `0x25`（ViewPicker.populate 逻辑：非私有首项；`queryDefaultViewId()`=0x25 命中覆写——两路同值） |
| `views.defaultViewState` | 完整 ViewStateProps RPC 载荷原样：viewDefinitionProps[origin [16.460, -6.778, -8.143] / extents [19.326, 10.798, 19.557] / cameraOn=false / camera / categorySelectorId / displayStyleId] + modelSelectorProps[models=["0x1c"]] + categorySelectorProps[categories×1] + displayStyleProps[全样式] |
| `models` | 1 个 ModelProps：0x1c BisCore:PhysicalModel "Properties_60InstancesWithUrl2" |
| 缺口 | **无**（gaps=[]） |

回放对应关系：modelSelector.models=["0x1c"] ⇔ instances60-v1 的 1 棵树（树 id 后缀 0x1c）。

## 已知限制

1. 本目录不承载 tiles 覆盖目标（tiles 资产=instances60-v1[3,587 瓦 sweep]+instances60-drill-v1
   [5 瓦视口面]）；manifest 内 1 瓦 `-b-2-0-0-0-1` 为默认视图自然请求（会话活性证据，
   字节与 instances60-v1 同键一致[同源后端生成]——如需严格零瓦形态可剔除该文件，保留以实证）。
2. 默认视图 4 选 1 的判定已按 DTA ViewPicker.populate 逻辑固化（首非私有 + queryDefaultViewId
   覆写——两路同值 0x25，无歧义）。

## manifest 契约

与既有 dump 同（trees/tiles/sha256 全量落盘；imodel.json 并列）。V4 键格式。
