# 阶段 P-3：compatseed / mirukuru 全量采集与 Start 页集成 — 实现计划（2026-10-23）

> 模板 = P-2 newBridge（fdc66e2496 / 收口 2c671ee24a）。约束：§0 来源铁律、§5 测试保真
>（Authored 锁）、§8.2 引擎零网络（采集器仓外合法）、§11.8/§11.11（入库只读、重采新目录名）。

## 背景与动机

compatseed-v1（7 瓦）/ mirukuru-v1（1 瓦）是 M-D（2026-09-27）早期**视口 zoom-out 采集**：
compatseed 只覆盖 d0-d6 祖先链（子树完全未采）、mirukuru 只有树 0x1c 的根瓦，且两根均**无
imodel.json**（打开链硬需求）——因此两模型从未接入 Start 页，只供 RpcDumpRender 两回归锁消费。
用户指令（2026-10-23）：把 compatseed/mirukuru 采集完整并接入 Start 页。

勘察实证（P3-0）：
- 种子 = itwinjs-core 仓内资产 `CompatibilityTestSeed.bim`（1,761,280 B）/ `mirukuru.ibim`
  （1,220,608 B），现成在盘（`test-apps/display-test-app/assets/`），standalone 免认证免网络。
- 采集工具链（仓外 `D:\Github\danqing-rpc-tools`）+ itwinjs @`88da8fb9` 前置全部在位。
- Start 卡片命名用户裁决 = 英文原名（"Compatibility Test Seed" / "Mirukuru"——House_Model/
  Baytown ASCII 先例）。

## 执行实录

### P3-a 采集（仓外，两模型串行，分钟级收口）

```bash
node capture.mjs --mode launch --seed ".../CompatibilityTestSeed.bim" --out dumps/compatseed-sweep-v1 \
  --sweep --sweep-max-tiles 400000 --sweep-max-depth 10 --sweep-max-bytes-mb 1536 --sweep-max-mag 8 --wait 3600
node capture.mjs --mode launch --seed ".../mirukuru.ibim" --out dumps/mirukuru-sweep-v1 \
  --sweep --sweep-max-tiles 400000 --sweep-max-depth 10 --sweep-max-bytes-mb 1536 --sweep-max-mag 8 --wait 3600
```

- **compatseed-sweep-v1**：1 树 `25_1d-E:6_0x1c`，sweep **done 自然收敛**（请求 253 / 入库 212
  [dup 42] / errs=0 / magnifierCapped 43），20.43MB，深度 d0-d10（d10 层 81 瓦全叶化）。
  imodel.json 3,391B（3 视图 / default 0x41 = **BisCore:OrthographicViewDefinition** / 1 model，
  gaps=0）。
- **mirukuru-sweep-v1**：2 树（`0x1c` 内容根 1,652B + `0x28` 空叶根 424B），**双树根即叶——
  全树 = 2 瓦为模型固有形态**（leaves=2/branches=0/errs=0，非预算截断）。imodel.json 3,703B
  （1 视图 / default 0x24 SpatialViewDefinition / **2 models [0x1c,0x28]**，gaps=0）。

### P3-b 验收 + 入库

- frontier 枚举：compatseed d11 missing=0/parents=0（全树完整）；mirukuru d1 missing=0（根即叶）。
- curate.mjs 只读封存分支：两树根内**全量 sha256+byteLength 一致**（212/212、2/2）。
- 整根拷入 `third_party/tile-sample-assets/rpc-dumps/`（files/ + manifest.json + imodel.json +
  per-dump README——provenance/caps/默认视图裁决/复现命令全录）；小体量全量 git 直收，
  **.gitignore 零改动**（既有全具名模式不吞新根）；rpc-dumps/README.md 加 P-3 目录两小节 +
  两旧根各行补重采指针；仓外 dumps/README.md 补录 P-3 节。
- 旧 compatseed-v1/mirukuru-v1 按 §11.11 只读保留（RpcDumpRenderTest.cpp:236/405 显式路径继续
  消费，零干扰）。
- **frameToWorldContent 双裁决 = false**（以新 imodel.json 实测）：compatseed 正交 iso 视图
  （origin [0.133,2.733,-2.033] + ext [3.677,4.246,4.503]）与世界内容域 ≈[-0.25,2.25]³ 相交；
  mirukuru identity 视域 x∈[288444,289588]/y∈[3803583,3804137] 完整包含内容域
  （projectExtents [288874..289161]×[3803761..3803959]）。

### P3-c 仓内接线（6 站点，全编辑既有文件，CMake 零改动）

1. `DumpOpenHelper.cpp` dumpPackageForModel：两分支单根包（imodelRoot==tileRoot）+ 头部映射表
   注释两行。
2. 同文件 modelIdForIModelKey：`compatibilitytestseed.bim→compatseed`、`mirukuru.ibim→mirukuru`
   （basename 小写不敏既有机制；key 真值 = 两新根 imodel.json rpcProps.key 实测）。
3. `main.cpp` windowTitleForModel：`"Compatibility Test Seed"` / `"Mirukuru"`。
4. `StartView.cpp` configureModelButtons：两 NewFileButton（0xe9cc 同款图标）+ connect →
   requestOpenDumpModel + addWidget（序尾）。
5. `StartView.h:64` 信号注释尾追加 P-3 两键。
6. `IModelFileToolsTest.cpp`：DumpPackageForModelTableIntact 加两节（imodelRoot 后缀 /
   tileRoots.size()==1 / frameToWorldContent=false 对账）+ ModelIdForIModelKeyMapsDumpKeys 加
   两键（正/反斜杠双形态）。

### P3-d 打开链锁 ×2（DumpOpenChainTest.cpp 尾追加；baytown/newbridge 模板）

**OpensCompatSeedWithSavedView**（窗 = 采集窗 1263×831）：
- ② 树装载钉：恰 1 树 `25_1d-E:6_0x1c`。
- ① saved 视图六轴 + rotation 九元素钉：org [0.1333,2.7333,-2.0333] / ext [3.6770,4.2458,4.5033]
  / cameraOn=false / rotation = saved angles[yaw30/pitch-35.264/roll-45] 的 YPR 矩阵——**参考公式
  rZ(yaw)·rY(-pitch)·rX(roll) 离线复算与采集器 provenance.defaultView.rotation 逐位吻合
  （双源互证）**。
- ③ 请求面对账：union=212 键，恰 1 枚 `-b-5-0-0-0-1` Completed（root..d4 canSkip 穿透
  [maxInitialTilesToSkip=6] 后首 Visible——M-D 原采集窗较小首请求为 -b-6，同机制不同窗档），
  notFound=0 / error=0 / 零重复；ready=1/6（root→…→d5 六瓦结构）。
- ④ 像素锚：首绿 204,422px=帧 4.869%@2526×1662（dpr2），bbox (1020,551)-(1505,1110) 中央区块，
  质心 (1262,831) 对帧心偏移 (1,0)；钉 0.2×=40,000 + 质心 ±20% 带。**瓦携 instances 修饰
  （TD-25 链）活体上屏实证**。

**OpensMirukuruWithSavedView**（窗 = 采集窗 1263×831）：
- ② 树装载钉：恰 2 树 `25_1d-E:6_0x1c` + `25_1d-E:6_0x28`（modelSelector 序——打开链多树
  装载用例）。
- ① saved 视图六轴 + identity rotation 钉：org [288444.035,3803583.339,-219.297] /
  ext [1144.207,553.629,787.849] / cameraOn=false（**location 大坐标平移 [289793,3804398,0]
  消费链用例**）。
- ③ 请求面对账：union=2 键，恰 2 枚双树根 Completed（0x1c 内容根 + 0x28 空叶根——叶根无
  canSkip 穿透对象）；多树集合断言 `treeId ∈ {0x1c,0x28}`（未照抄单树 EQ）；ready=1/10
  （0x28 空叶无 graphics）。
- ④ 像素锚：首绿 699,123px=帧 16.653%，bbox 满幅展布（平地投影特性），质心 (1270,835) 偏移
  (7,4)；钉 0.2×=139,000 + 质心 ±20% 带。

**RED 实证**（P-2 ④ 先例）：stash 四接线文件（DumpOpenHelper/main/StartView.cpp+h，测试文件
不动）重建后，两新锁 + TableIntact + ModelIdForIModelKeyMapsDumpKeys **四项全 RED**；
unstash 重建后 4/4 GREEN——双向验证闭环。

### P3-e 门禁与收口

- 目标面：DumpOpenChain 9/9 + RpcDumpRender（旧两锁零回归）+ DumpBrowse + PickDumpScene +
  JoesHouse 族 + IModelFileTools 全绿（全族轮 JoesHouseHover.FlashBrightens* 一败，
  **隔离复跑 ×2 全绿**——TD-29 满载抖动族归因维持）。
- 全量 Debug ctest：**2956 项 = 2928 通过 + 28 跳过 + 0 失败**（基线 P-2 收口 2954 + 2 新锁；
  exit code 0；LastTest.log 2928 passed + 28 skipped 自洽对账）。
- Release 冒烟：`build/rel-smoke7.ps1`（rel-smoke5 原地扩两键）七模型全绿——
  Compatibility Test Seed[白色六边形内容中央上屏；MDI "[ 1 ] SpatialViewDefinition0 <0x41> (3d)"；
  Models 面板 1 模型 1 树；Categories 0x14；状态条 "Opened compatseed (1 tile trees) — saved view
  rendered"；chain-done 451ms] + Mirukuru[灰色平地足印中央上屏；MDI "[ 1 ] 3D Metric Design -
  View 1 <0x24> (3d)"；Models 面板 mirukuru+Medium_3SM 双模型、Tile Trees 25_1d-E:6_0x1c+0x28 双树；
  Categories 0x17；"Opened mirukuru (2 tile trees)"；chain-done 438ms]；七份 stderr 零 NotFound/
  零 SEH。截图 build/rel-smoke-{compatibility,mirukuru}.png。

## 提交

- 提交 1（P3-ab 数据资产）：两新根整根 + rpc-dumps/README.md。
- 提交 2（P3-cde 接线+锁+门禁实录）：DumpOpenHelper/main/StartView(.cpp+.h)/两测试文件 +
  CLAUDE.md §1 + 本文档。
