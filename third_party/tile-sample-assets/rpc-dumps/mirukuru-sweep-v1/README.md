# mirukuru-sweep-v1 — mirukuru.ibim（sweep 全树采集——双树根即叶，2 瓦即全树）

**来源**：itwinjs-core display-test-app（`D:\Github\itwinjs-core` @ `88da8fb98647d37632a5cef4d6bb6e2e5c007855`，
electron + vite dev）经 CDP 注入 rpc-dump.js + sweep.js（BFS 全树）+ imodel-rpc.js（iModelRpc 数据面）。
**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

> 本根是 M-D 早期 `mirukuru-v1`（1 瓦——2026-09-27 视口采集，只有树 0x1c 的根瓦，
> 无 imodel.json）的**全量重采**。旧根按 §11.11 保持只读、继续供
> RpcDumpRenderTest.MirukuruRendersRealBackendTile 消费；Start 页打开链走本根。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `mirukuru.ibim`（1,220,608 B——itwinjs-core 仓内资产原样：`core/backend/lib/cjs/test/assets/mirukuru.ibim`，经 `test-apps/display-test-app/assets/` standalone 打开） |
| 参考仓 | `D:\Github\itwinjs-core` @ `88da8fb98647d37632a5cef4d6bb6e2e5c007855` |
| 采集时刻 | 2026-10-23T02:36:04Z |
| iModel | "DgnV8Bridge"（V8 桥转换模型），guid `20f0f4a0-64bb-4b2b-b060-11c25024d986`，changeset ""（standalone） |
| projectExtents | low [288874.117, 3803761.189, -0.5] / high [289160.842, 3803959.119, 0.5]（米——日本平面坐标系，287×199 m 平地，z 跨 1 m） |
| 树 | **2 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:6_0x1c`（model 0x1c，props=files/5.json 713 B，maxInitialTilesToSkip=2，rootTile.range ±919×±637×±0.003）+ `25_1d-E:6_0x28`（model 0x28，props=files/4.json 531 B，**isLeaf / maximumSize=0 / 空树**） |
| sweep 完成信号 | `provenance.sweep.phase="done"`（自然收敛；请求 3 瓦 / 入库 2 瓦[dup 1]，leaves=2 / branches=0，errs=0） |
| 完整性判据 | **双树根即叶**（0x1c 根 1,652 B 含全部内容几何；0x28 根 424 B 空叶）——无子层可枚举（前沿枚举 d1：missing=0）。**全树 = 2 瓦为模型固有形态**，非预算截断 |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（2 瓦 + 2 树 props，curate.mjs 只读封存分支，0 mismatch） |

### defaultView（应用后的默认取景——sweep 会话实录）

```
is3d: true  cameraOn: false  viewRect: 1263×831
```

保存视图（imodel.json defaultViewState.viewDefinitionProps，回放源）：
class=BisCore:SpatialViewDefinition，id=0x24（"3D Metric Design - View 1"），
origin [288444.035, 3803583.339, -219.297]，extents [1144.207, 553.629, 787.849]，
rotation=identity，cameraOn=false。
**非坑 24**：视域 x∈[288444, 289588] / y∈[3803583, 3804137] **完整包含**世界内容域
（contentRange × location ≈ projectExtents [288874..289161]×[3803761..3803959]）
→ saved 视图直渲，`frameToWorldContent=false`。

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（3,703 B，与 manifest.json 并列；`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（key=`D:\Github\itwinjs-core\test-apps\display-test-app\assets\mirukuru.ibim`） |
| `views.list` | 1 视图，defaultViewId=`0x24` |
| `views.defaultViewState` | 完整 ViewStateProps RPC 载荷原样：viewDefinitionProps（上节）+ modelSelectorProps[**models=["0x1c","0x28"]**——双模型双树] + categorySelectorProps[categories=["0x17"]] + displayStyleProps（styles: environment/hline/sceneLights/viewflags） |
| `models` | 2 个 ModelProps：0x1c、0x28 |

回放对应关系：modelSelector.models 2 个 model ⇔ 本 dump 2 棵树（树 id 后缀=model id；
0x28 空树请求面 = 根瓦一次即 Completed）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 2 |
| 瓦（manifest 去重后） | **2**（2,076 B：0x1c 根 1,652 B + 0x28 根 424 B） |
| 树 props | 2（713 B + 531 B） |

## 接入

`dumpPackageForModel("mirukuru")`（DumpOpenHelper.cpp）：主根=本根（imodel.json+树 props+瓦
单根齐备，无 fallback）。Start 页卡片 "Mirukuru"；MDI 标题同名。
锁：`DumpOpenChain.OpensMirukuru*`（DumpOpenChainTest.cpp）；映射钉值
`SurfaceFileTools.DumpPackageForModelTableIntact` / `ModelIdForIModelKeyMapsDumpKeys`
（IModelFileToolsTest.cpp）。

## 复现命令

```bash
cd /d/Github/danqing-rpc-tools
node capture.mjs --mode launch \
  --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/mirukuru.ibim" \
  --out dumps/mirukuru-sweep-v1 \
  --sweep --sweep-max-tiles 400000 --sweep-max-depth 10 \
  --sweep-max-bytes-mb 1536 --sweep-max-mag 8 --wait 3600
```
