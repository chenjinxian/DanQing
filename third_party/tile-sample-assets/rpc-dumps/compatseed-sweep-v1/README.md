# compatseed-sweep-v1 — CompatibilityTestSeed.bim（sweep 全树 BFS 采集，depth≤10 自然收敛）

**来源**：itwinjs-core display-test-app（`D:\Github\itwinjs-core` @ `88da8fb98647d37632a5cef4d6bb6e2e5c007855`，
electron + vite dev）经 CDP 注入 rpc-dump.js + sweep.js（BFS 全树）+ imodel-rpc.js（iModelRpc 数据面）。
**采集工具在仓外** `D:\Github\danqing-rpc-tools\`（§8.2 零网络约束）。

**§11.11**：入库后只读；重采=新目录名（不得覆盖/突变本目录）。

> 本根是 M-D 早期 `compatseed-v1`（7 瓦——2026-09-27 视口 zoom-out 采集，只覆盖 d0-d6
> 祖先链，无 imodel.json）的**全量重采**。旧根按 §11.11 保持只读、继续供
> RpcDumpRenderTest.CompatSeedReplaysLodChainToGraphicsReady 消费；Start 页打开链走本根。

## provenance

| 项 | 值 |
|---|---|
| 模型 | `CompatibilityTestSeed.bim`（1,761,280 B——itwinjs-core 仓内资产原样：`core/backend/lib/cjs/test/assets/CompatibilityTestSeed.bim`，经 `test-apps/display-test-app/assets/` standalone 打开） |
| 参考仓 | `D:\Github\itwinjs-core` @ `88da8fb98647d37632a5cef4d6bb6e2e5c007855`（与 bridge-edit-sweep/newbridge 各根同 commit） |
| 采集时刻 | 2026-10-23T02:34:54Z |
| iModel | "DgnDbTestUtils"，guid `048a431f-0398-4091-acbd-c92657680797`，changeset ""（standalone） |
| projectExtents | low [-50, -50, -10] / high [50, 50, 30]（米） |
| 树 | **1 树**（imdl formatVersion 2424832 = major 37）——`25_1d-E:6_0x1c`（model 0x1c）；props=files/11.json（688 B，maxInitialTilesToSkip=6，location 平移 +99.755 各轴） |
| sweep 完成信号 | `provenance.sweep.phase="done"`（**自然收敛**——非 budget-capped；请求 253 瓦 / 入库 212 瓦[dup 42]，leaves=0 / branches=253，errs=0，magnifierCapped=43） |
| 预算裁决 | cap=（tiles 400k，depth 10，bytes 1536MB，mag ×8）——**无一触顶**；落盘 20.43 MB。cap 值随 `provenance.sweep.caps` 落盘 |
| 完整性 | 前沿枚举（enumerate_frontier.py d11）：**missing=0 / parents=0 / m2 放大子=0**——d10 层全部叶化，全树无缺口 |
| 校验 | 入库前 **全量** sha256+byteLength 复算一致（212 瓦 + 1 树 props，curate.mjs 只读封存分支，0 mismatch——不只抽检） |

### defaultView（应用后的默认取景——sweep 会话实录）

```
origin:  [0.5061666699876851, 3.106166669987686, -1.2876666600246287]
extents: [3.6769552621700488, 2.419279353019248, 4.503332099679081]
rotation（行主序 Matrix3d.toJSON）:
  [  0.7071067811865476, -0.7071067811865475,  2.7755575615628914e-16 ]
  [  0.4082482904638628,  0.40824829046386324, 0.816496580927726     ]
  [ -0.5773502691896258, -0.5773502691896256, 0.5773502691896257    ]
is3d: true  cameraOn: false  viewRect: 1263×831
```

保存视图（imodel.json defaultViewState.viewDefinitionProps，回放源）：
class=**BisCore:OrthographicViewDefinition**（正交——baytown 同型，打开链类选择面消费）,
id=0x41，origin [0.1333, 2.7333, -2.0333]，extents [3.6770, 4.2458, 4.5033]，
angles yaw=30°/pitch=-35.26°/roll=-45°（iso 取向），cameraOn=false。
**非坑 24**：视域与世界内容域相交（内容 = 2.5m 立方，local [-100.005,-97.505]³ ×
location +99.755 → 世界 ≈[-0.25, 2.25]³）→ saved 视图直渲，`frameToWorldContent=false`。

## iModelRpc 数据面（imodel.json——打开流程回放源）

`imodel.json`（3,391 B，与 manifest.json 并列；`gaps=[]`）：

| 字段 | 内容 |
|---|---|
| `connection` | name/guid/projectExtents/rpcProps（key=`D:\Github\itwinjs-core\test-apps\display-test-app\assets\CompatibilityTestSeed.bim`） |
| `views.list` | 3 视图，defaultViewId=`0x41` |
| `views.defaultViewState` | 完整 ViewStateProps RPC 载荷原样：viewDefinitionProps（上节）+ modelSelectorProps[models=["0x1c"]] + categorySelectorProps[categories=["0x14"]] + displayStyleProps（styles: environment/viewflags） |
| `models` | 1 个 ModelProps：0x1c |

回放对应关系：modelSelector.models 1 个 model（0x1c）⇔ 本 dump 1 棵树（树 id 后缀=model id）。

## 统计（实测——如实报告）

| 项 | 值 |
|---|---|
| 树 | 1 |
| 瓦（manifest 去重后） | **212**（20.43 MB；深度分布 d0:4 d1-d6:3-4 d7:6 d8:30 d9:72 d10:81） |
| 树 props | 1（688 B） |

## 接入

`dumpPackageForModel("compatseed")`（DumpOpenHelper.cpp）：主根=本根（imodel.json+树 props+瓦
单根齐备，无 fallback）。Start 页卡片 "Compatibility Test Seed"；MDI 标题同名。
锁：`DumpOpenChain.OpensCompatSeed*`（DumpOpenChainTest.cpp）；映射钉值
`SurfaceFileTools.DumpPackageForModelTableIntact` / `ModelIdForIModelKeyMapsDumpKeys`
（IModelFileToolsTest.cpp）。

## 复现命令

```bash
cd /d/Github/danqing-rpc-tools
node capture.mjs --mode launch \
  --seed "D:/Github/itwinjs-core/test-apps/display-test-app/assets/CompatibilityTestSeed.bim" \
  --out dumps/compatseed-sweep-v1 \
  --sweep --sweep-max-tiles 400000 --sweep-max-depth 10 \
  --sweep-max-bytes-mb 1536 --sweep-max-mag 8 --wait 3600
```
