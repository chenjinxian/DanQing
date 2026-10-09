# CLAUDE.md

本文件是 DanQing 项目的开发准则，每次会话自动加载。执行任何代码或文档变更前，先读完本文件。
**规则优先于默认行为。违反规则是错误，不是风格偏好。**

> **规则严重度标签**（每条规则标注）：
> - `[P0 硬约束]` — 不可妥协的架构/契约红线，违反即错误，不得合入主干
> - `[P1 强规则]` — 必须遵守，例外需显式论证，否则评审打回
> - `[P2 指南]` — 推荐做法，非阻塞
>
> （未标 `[P?]` 的章节为描述性/过程性内容，无规则可评级；§14 技术债务表按行标注严重度）
>
> **文档层级**：本文件（CLAUDE.md）是开发规则的**唯一权威**。`docs/DanQing-C++代码规范.md` 为从属格式参考（include 顺序、clang-format、Doxygen 等），不得与本文件矛盾；冲突时以 CLAUDE.md 为准。

---

## 0. 首要指令 (The Prime Directive) `[P0 硬约束]`

DanQing 是参考实现（reference implementation），不是原创设计。所有代码、命名、测试必须严格对齐参考项目（itwinjs-core / imodel-native / filament / FreeCAD）。

**三条铁律：**

1. **来源铁律**：所有【代码与测试】= 参考项目源码的 C++ 改写——**含修 bug：行为修复前先读参考的对应实现，照机制修，不做症状推理补丁**（§11 第 8 条，案例 §12.8、§12.9）。
   - 代码：参考未覆盖的功能标 TODO + 参考来源，不得自创算法/接口/数据结构/常量。
   - 测试：测试场景、边界条件、断言值全部来自参考项目，不得自行构造。优先级 imodel-native C++ 测试 > itwinjs-core TS 测试。仅当两处参考均无对应测试时才可自写，且必须标注 `// Authored: no reference test exists in <project> for <feature>`。
   - `// Ported from:` 行号必须对应**真实读过**的参考实现——用行号装点自创代码是双重违规（§12.8 教训 2）。
   - 参考项目的代码与测试是"规范"，不是"可借鉴的资源"。

2. **命名铁律**：所有标识符（文件/类/枚举/变量/方法/函数）1:1 对齐参考项目（详见 §3）；方法/函数按参考语言原样（TS→camelCase，C++ 参考→PascalCase）。可追溯性由每符号 `// Ported from:` 注释保证。

3. **规范铁律**：严格遵守 C++ 编码规范（§9）。CLAUDE.md 是规则唯一权威，`docs/DanQing-C++代码规范.md` 为从属格式参考，冲突时以 CLAUDE.md 为准。

违反 `[P0]` 是错误，不是风格偏好，不得合入主干。

---

## 1. 项目定义与总目标

丹青（DanQing）是**纯客户端图形引擎**（不是 CAD 应用、不是几何内核）：itwinjs-core 数字孪生大体量渲染能力 × Filament 高质量实时渲染与全平台能力的共同体。目标用户是图形/CAD 应用开发者。在天工开物平台家族中的分工：渲染层——几何内核归真形（私有仓，自主研发），CAD 应用归鲁班CAD，数据/事务归 imodel-native。

5 个已实现模块：

| 模块 | 命名空间 | 状态 | 职责 |
|------|---------|------|------|
| **dqBase** | `dqBase` | ✅ 已实现 | 共享基础（智能指针、Result、容器、事件），零领域知识 |
| **dqGeom** | `dqGeom` | ✅ 已实现 | 纯数学几何（曲线/曲面/网格/拓扑/裁剪/BSpline），零固体内核依赖 |
| **dqCommon** | `dqCommon` | ✅ 已实现 | 共享类型（ColorDef、ViewFlags、Frustum、FeatureTable、GeometryStream、Cartographic），对应 itwinjs-core @itwin/core-common |
| **dqRender** | `dqRender` | ✅ 已实现 | 渲染引擎（RHI/Tile/多Pass/Shader），对应 itwinjs core/frontend + filament |
| **dqApp** | `dqApp` | ✅ 已实现 | 渲染宿主层（Application/ViewManager/Viewport 24 步管线/ToolAdmin；Qt 仅作宿主窗口桥接） |

> **改名完成说明（2026-09-24）**：项目级与代码标识符改名均已落地——`dq*` 命名空间、`DQ_*` 宏、`DANQING_*` 环境变量、`danqing_*` CMake 目标、`Dq*` 基础设施类型前缀。
> 旧定位遗留（CNC 底座 / 文生 3D 平台 / zoBRep·zoData·zoPlatform 规划）已于 2026-09-24 随定位重写废弃，对应文档已删除。

**当前进度基准（2026-09-28）**：Windows 呈现链路已端到端打通并经真实 app 验证——WGL 平台层对齐 filament（swapchain 自持 DC/统一像素格式/表面自愈），resize=表面过期重建交换链，OIT 合成程序生命周期修复；真实 app 配方（空白连接→Grid+ACS→最大化→深度缩放）网格与三轴全程稳定。像素级回归 harness 在树（View3DResizeTest/ZoomBlackBoxSeqTest/ResizePixelConsistencyTest 等）。**阶段 0 归位完成（M-A/M-B/M-C）**：imdl 消费链（格式层/LUT 直传/readContent 链接）+ 选择壳（draw-from-selected/root markUsed/hasMissingTiles）+ 调度收口（TileRequest users 共享门/swapPending/forgetUser 撤单）+ imdl 边缘四形态消费（见 §14 TD-18~TD-23）。全量门禁（2026-09-27，Debug）：2473 项测试 2472 通过 + 27 跳过 + 1 环境态失败（View3DResize.MinimizeRestoreKeepsScreenAlive——"基线即黑"环境守卫触发，隔离复跑 ×2 全绿，故障随满载桌面时序而非代码，按登记环境态归因）；TileTreeRender 像素锁 10/10（TD-11~TD-21 清偿；TD-22 登记、TD-23 M-C 清偿，见 §14）。 **阶段 1 M-D 完成（RPC 真实数据回放建立）**：仓外 collector 采集真实后端 imdl/树 props（compatseed-v1/mirukuru-v1 入库 `third_party/tile-sample-assets/rpc-dumps/`，只读）；DumpTileFetcher/DumpTileTreeProps 本地 manifest 回放（零网络——TD-24 清退 + ZeroNetworkByConstruction 全仓扫描锁）；ContentIdProvider（TileMetadata.ts:596-713，V1/V2/V4 请求键方案——V4 前缀 flags 0xb 与采集键域逐位吻合）；RpcDumpRender 像素锁（mirukuru 真后端瓦上屏：20.45% 内容、质心对帧心 ±2px）+ compatseed 7 瓦 LOD 链消费锁（请求键覆写→字节回放→≥2 瓦 graphics 提交）；瓦内容 instances 修饰未消费暴露并登记 TD-25。全量门禁（2026-09-27，Debug）：2487 项测试 100% 通过 0 失败（上轮环境态 View3DResize.MinimizeRestoreKeepsScreenAlive 本轮自然通过）；TileTreeRender 10/10 + RpcDumpRender 2/2。**阶段 1 M-E 完成（2026-09-28，完整 iModel+Tiles 加载 + TD-25 实例化）**：instances60-v1 全树采集入库（Properties_60InstancesWithUrl2.ibim：1 树 3587 瓦/289.67MB，65 瓦 instances 证据——2a4b9c9）；多树全量装载 + iModel 元数据消费（DumpMount 入口，0cb3c00）；TD-25 清偿（46e743b——ImdlInstances→InstanceBuffers→InstancedGeometry 实例化上屏：60 实例像素锁 colored=123069/质心偏移 407px）+ TD-22 同任务清偿（~Tile 三 feed 清扫）；**完整 iModel+Tiles 加载（instances60 全树前缀）E2E 对账锁 Instances60FullLoadReconcilesManifestKeys**——DumpTileFetcher requestLog 全部请求键 vs manifest 3587 键集合逐一对账（M-E(4)：单相 fit 冷启动驱动；M-F(2) 升级**两相驱动**——相 2 确定性钻取 kZoom=3 使细分子转 TooCoarse 下潜，mult=1 派生子键 "-b-2-0-0-0-1" 入链通集[派生子瓦请求→字节回放→graphics 的 E2E 直接证据]，覆盖率 1→2/3587 实测）。全量门禁（2026-09-28，Debug）：2494 项 2467 通过 + 27 跳过 + 0 失败；TileTreeRender 10/10 + RpcDumpRender 4/4。**阶段 1 M-F 完成（2026-09-28，子瓦 contentId 派生链收口）**：①setContent 语义归位（f179f8d——sizeMultiplier 升门控+contentId 覆写 IModelTile.ts:134-148 逐行 + readContent 全字段接线 + TileAdmin drain 前置 + desc 解码 0 门归位）；②emptySubRangeMask 消费最后一跳（loadChildren 构造 parent 携 mask——IModelTile.ts:156 传 this 语义，原跳过逻辑永不触发；LoadChildrenSkipsSubVolumesMarkedEmptyByParentMask 锁）；③**取证结论：instances60 采集域是 sweep BFS 面产物，非视口请求面**（仓外 sweep.js：合成 generateTileContent 逐键请求 + 页内 computeChildTileProps 派生 + 放大 cap=×8 + BFS 未完成前缀）——depth-0 放大键 "-b-0-0-0-0-{2,4,8}" 任何视口驱动都不会请求（放大子与父同范围[TileMetadata.ts:785-799]→父 TooCoarse 时同尺寸永不 Visible；hasSizeMultiplier 使 isNotReady 假[IModelTile.ts:272]免跳级预算→无请求穿透），残差 miss "-b-1-0-0-0-1"/"-b-3-0-0-0-1" 为采集面 artifact（冷启动根首 loadChildren 内容前细分[两实现同构]+:148≤1 门保留唯一幸存子+fit 下 Visible 被请求；sweep 从未细分 mult=1 父→域外），两相锁内实钉；引擎侧 computeChildTileProps/setContent/selectTiles 与参考逐行同构（选择轨迹 [SEL] 取证实证）；TD-25 遗留"子瓦 contentId 派生链差异"项勾销（§14）。全量门禁（2026-09-28 M-F 收尾，Debug）：2497 项 = 2469 通过 + 27 跳过 + 1 环境态失败（View3DResize.MinimizeRestoreKeepsScreenAlive——"基线即黑"环境守卫族，隔离复跑 ×2 全绿，故障随满载桌面时序而非代码，按登记环境态归因）；TileTreeRender 10/10 + RpcDumpRender 4/4 + ImdlTileTreeTest 6/6（新增 mask 锁）。**阶段 1 M-G 完成（2026-09-28，zoom-drill dump 采集 + drill 回放 E2E 同构锁）**：①instances60-drill-v1 入库（ee3ec7e——真实视口请求面采集：默认视图 zoom 泵下潜 10 级、wrapper 捕获视口自然请求；1 树 5 瓦，键域=depth-2 放大链 `-b-2-0-0-0-{1,2,4,8,10}`[V4 全 hex——mult 段 "10"=0x10=16，SSE 在 ×16 饱和]；**"drill ⊆ sweep"被实测推翻**（×16 键域外——sweep 合成 cap=×8，M-F artifact 裁定交叉验证成立）；三目标 aiming 未达成 + root/-b-1 缺字节[wrapper 竞态+参考跳过语义]登记 dump 已知限制）；②**drill 回放 E2E 同构锁 `Instances60DrillReplaysViewportChain`**（"视口请求面=回放可达面"同构证据——drill **5/5 键全部 Completed**、最深键 -b-2-0-0-0-10 实钉、miss 集实钉 3 枚[-b-1-0-0-0-1 采集域外 / 根键 :299-301 NotFound 回退 / 过冲放大子 -b-2-0-0-0-20]、树侧零在途 + Completed→graphics 对账 + 像素锚 kZoom=8 锚定视图 content≥450000[首绿 1197243=帧 42.76%]+质心中央带；泵形 kZoom=2^n 迭代 6 级——m1@2 以 Visible 干净派生[无 -b-3 细分污染]，m16@32 连通）；③**引擎修复（RED 取证→§11.8 移植缺口，非参考同构——未走 merged-mount 备案）**：`maxInitialTilesToSkip` props 载体接线（TileProps.ts:63 → IModelTileTree.ts:51/:76/:390 `?? 0`——ImdlTreeMetadata 增载体 + ImdlTileTree 构造器消费 + DumpTileTreeProps 解析；修复前 DanQing 硬编码 0 → 根 NotFound 时 :265/:269 canSkip 恒假 → 子代永不遍历、树被根阻断[RED 实测链={根 NotFound}、0/5 键]，参考语义下预算=3 使根/d1/d2 恒 canSkip、根 NotFound 不阻下潜——活体采集即在该语义下产链）；④**强制选择帧泵**（NotFound 交付不触发失效级联，:299-301 回退在突发帧停后饥饿[M-F(2) 相 1 实测 log={-b-1} 停 20s]——泵每迭代 InvalidateController 把请求面推到不动点，键级幂等不改请求面；drill/M-F(2)/M-E/CompatSeed 四锁泵统一，M-F(2)/M-E 原钉值全部保持[根经回退路径 Completed——同键同字节；M-E 像素 123069→139796 确定性漂移（终审复测 ×2，阈值余量充足，漂移源未归因，锁头补复测钉值登记]]）；⑤**CompatSeed 锁配方订正**（maxInitial=6 忠实化：根恒 canSkip 不被钻取视图请求[参考同构]——相序订正"钻取先于 fit"（d6 细分只能在 d5 内容到达前派生[:282 用当前元数据；fit 先行则 d5 就绪后子代恒放大、d6 永不可达——两形态日志在案]），判据演化为 d6+d5 两瓦就绪 + 最深在域键 -b-6 实钉 + 放大过冲 miss 两枚[-b-6-0-0-0-{4,2}] + Completed→graphics 对账）。覆盖率：**sweep 2/3587 + drill 5/5 键视口链同构（M-G）**。全量门禁（2026-09-28 M-G 收尾，Debug）：2498 项 = 2470 通过 + 27 跳过 + 1 环境态失败（StandardViewAnimationTest.RotationSwitchAnimatesAndCompletes——0xc000041d 窗口回调异常，隔离复跑 ×2 全绿，窗口环境态族归因；同会话前轮全量的 WheelZoomCoalesceTest.AcsDiscSurvivesDeepZoomPixels 呈现分裂失败经 stash 双态对照实锤净基线同败、本轮自然通过）；TileTreeRender 10/10 + RpcDumpRender 5/5。**阶段 1 M-H 完成（2026-09-29，交互式双模型打开 + 全 LOD 浏览）**：①**完整数据面采集**（19b3160/150d36d0——joeshouse-v1[10 树 173,876 瓦/278MB] + joeshouse-drill-v1[10 树 15 瓦] + instances60-imodel-v1 入库；imodel.json=**iModelRpc 数据面**[连接/views 清单/defaultView 全 JSON/models——DTA 页内直读语义与 RPC 同面]三 dump 零缺口；JoesHouse 特征盘点：纹理 0/173,876 未命中[textured 不扩面依据]、instances 67 瓦/795 实例、compact 边缘 75,336 瓦、曲线 99.8%）；②**DumpTileFetcher 多根合并**（a72f893——fallback 有序只读备根清单，sweep∪drill 联合域，命中源记 requestLog.hitRoot）；③**dqApp 打开链五环**（dbc1848f——DumpIModelConnection[imodel.json 回放]→ViewList::RpcHooks→CreateFromProps 全字段应用→changeView→modelSelector 逐 model treeId 派生装载+location 消费；**引擎两修复**：location 消费链[DumpTreeProps.location 解析+setIModelTransform+readContent 创建时 Branch 包裹 EQUIVALENCE 登记]+frustum 剔除域错位[computeVisibility 的 range/球经 treeToWorld 变换——Tile.ts:405-411 移植缺口，[SEL] 取证三段定位]；**双同构锁** DumpOpenChain：instances60 saved 请求面恰 1 键 `-b-2-0-0-0-1`[==drill 采集首键，同键同字节] + joeshouse 10 model⇔10 树 drill 15/15 键全连通[每树内部请求子序与 drill 序同构]）；④**Start Views 清理**（552bdd0——FreeCAD-only 13 项裁决全清[New/Open File 卡片/recent-files 卡片面 7 源文件/FirstStart 向导/theme/general settings/ShowOnStartup/postStart/3 置灰 DTA 占位卡] + 双模型入口卡片→`dumpPackageForModel` 一处集中映射[与测试同构包定义]）；⑤**浏览零缺失锁**（DumpBrowse 双模型全绿——打开链进入→zoom 泵 kZoom=2^n 绕视域中心至 drill 键全连通+1 过冲级→×0.5 回退 2 级→平移两方向；全程 requestLog 对账：**域内键零 NotFound**[instances60 Completed=5=drill 5 键全通/joeshouse Completed=19=drill 15 键+4 叶根全通]、零 Error/零重复、**域外白名单=过冲放大子逐键钉死**[instances60 恰 1 枚 m32/joeshouse 恰 3 枚 0x4d-m32+0x3f/0x4d-m64]（细分/兄弟支域外键零出现）、**回退+平移零新增请求**[放大链瓦覆盖全树范围]、Completed→graphics 对账=裁决白名单逐键钉死[空瓦叶根×4+TD-27 形态键×1]；双像素锚 4 帧全钉[saved 410953/907420 px + deepest 2367075 px@16×/943933 px@64×，质心位置断言]；泵形钉值 instances60 饱和@kZoom=16/joeshouse@kZoom=64）；⑥**真实 app 交互验证**（mh5-smoke.ps1 桌面注入——双入口各一会话：saved 初始帧[全屋/60 实例球]→分级深缩放 LOD 逐级细化[满幅墙面/实例球面]→回退内容恢复→拖拽平移跟手；22 帧 build/mh5-*.png 目检 + stderr 620KB 轨迹日志零 NotFound/零 SEH；joeshouse 打开链>20s 实测登记[45s 等待实钉]）；⑦**TD-26/TD-27 新登记**（AcsDisc 间歇失败留痕调查 + numRgba=5 unquantized-LUT 顶点表形态未消费——参考消费链在案、shader 分支已预移植而解析/接受/变体三维度缺口，TD-20 同族）。全量门禁（2026-09-29 M-H 收尾，Debug）：2516 项 = 2488 通过 + 27 跳过 + 1 环境态失败（View3DResize.MinimizeRestoreKeepsScreenAlive——"基线即黑"环境守卫族，隔离复跑 ×2 全绿，故障随满载桌面时序而非代码，按登记环境态归因；TD-26 的 AcsDisc 本轮自然通过[间歇态又一实录]）；TileTreeRender 10/10 + RpcDumpRender 5/5 + DumpOpenChain 2/2 + DumpBrowse 2/2。**阶段 1 M-I 完成（2026-09-29，打开性能 + DTA 渲染对齐 + 拾取高亮——用户三问题清偿）**：①**M-I(1) 打开性能**（60c908e9c）：joeshouse manifest 58MB 双解析（DumpOpenHelper props+fetcher 各一遍，Debug 18.7s）→ **manifest 单次解析共享**（DumpManifest 移入通道）+ 哈希索引（多根首命中序/requestLog/byteLength 校验语义不变）+ 解析器瘦身（memchr 批扫/from_chars 去 substr+strtod）——Debug 打开 18.7s→**实测 2.87s**（真实 app stderr 桩 `DANQING_OPEN_TRACE` 取证；instances60 0.42-0.48s）；②**M-I(2) 拾取贯通**（95f6a7ea7）：`TargetImpl::setSceneContainer` **ported-but-uncalled 清偿**（§11.10 又一例）——drawFrame 把 scene 喂 pick 视图 graphics（参考 Target.readPixels 重画当前场景语义）+ PickDumpScene 像素锁 3 项（PickAtPoint>0→SelectionSet 命中→hilite 上屏/未命中角落不变——修复前 PickAtPoint 恒 0 RED）；③**M-I(3) 色表接线**（3be6f004a）：非均匀顶点色（35 prim `numColors:2` 色表形态）强制白改 `ColorInfo.createNonUniform` 链 + glsl Color.ts:16-26 色表采样——JoesHouseColor 像素锁（红带 41,324px@右下/蓝带 21,767px@右上，修复前管线 `#FFFFFF`）；④**M-I(4) 边线两段**（662d5e43b，终审 Approved）：qpos 协议移植修复（ShaderBuilder.ts:757-774 三缺口——函数调用式 vertex main 未赋 qpos→indexed 边零碎片，[EDGE] 692 dispatch/0 碎片根因）+ hline→EdgeSettings 边色覆盖六跳链（styles.hline 黑边）——JoesHouseEdge 锁 1,540 黑线像素/457 行/870 列/均值 26.4（终审复跑 1,540/456/869 AA 抖动）；⑤**M-I(5) 收口**：P2c 补采 **joeshouse-drill-v2** 入库（20 瓦/82KB——saved-view 面零增量[=6×`-b-2-0-0-0-1` 与 drill-v1 同键] + **缩远态面 6 键增量**[`-b-1-0-0-0-1`×6 树在 v1∪drill-v1 域外——sweep/drill 都从不缩远；第三 fallback 根接线]；女儿墙深钻受取景 radius artifact[缺省=root range 对角线/2→zoom-OUT ×16.6]未达成——如实登记 dump README）→ 归一结论（域侧无缺口 ⇒ 女儿墙差异归渲染对齐、已由③④清偿；真实 app saved 视图对拍 mi2-dta-joe.png 同形[管线有色/面板黑边/女儿墙区实心单色]）+ **真实 app 终验**（mi5-smoke 桌面注入：joeshouse 2.87s/instances60 0.42s 打开、点选高亮[joeshouse 高亮区 bbox(1288,378)-(2780,1192) 空点清选/instances60 单球白青高亮→清选后帧差≈110px 角斑]、缩放 LOD 3 级细化+回退无回归；stderr 归档零 NotFound/零 CRASH）。TD 表：TD-23"indexed 窗口像素锁未建"勾销 + qpos/hline 追加登记；**TD-28 新登记**（非阻断忠实度五项台账——hasFeatures 未覆写/elementId 64→32 截断/空表 override LUT 归属/addAnimation 门/ComputeQuantizedPosition 门控）。全量门禁（2026-09-29 M-I 收尾，Debug）：**2524 项 = 2497 通过 + 27 跳过 + 0 失败**（本收获轮：环境态族[MinimizeRestore/AcsDisc]本轮全数自然通过）；TileTreeRender 10/10 + RpcDumpRender 5/5 + DumpOpenChain 2/2 + DumpBrowse 2/2 + PickDumpScene 3/3 + JoesHouseColor/JoesHouseEdge。**阶段 1 M-J 完成（2026-09-30，两渲染回归清偿 + 取证仪器修正）**：①**M-J(1) instances60 实例球透明回归修复**（86fadb67e）——根因双机制叠加：`RenderCommands::addBatch` 缺参考透明覆盖门（RenderCommands.ts:668-677 的 `viewFlags.transparency || overrides.anyViewIndependentTranslucent` 门——任何带覆盖 LUT 的 batch 被无条件双份绘制进 Translucent pass）× M-I(2) 接通 pick/hilite 使 FeatureOverrideLUT 懒建面首次全量激活（潜伏缺口显形——M-H 实心→M-I 透明的时间线吻合）；放大机制：Translucency.ts addTranslucency 的 OIT 双输出未移植，单输出预乘色写 accum + blendFuncSeparate 把 accum.a 乘 0 → saved 视图 6 行球重叠洗白呈"透明"（单覆盖处替换色≈原色——解释 joeshouse 无症状的分叉）。修复 1:1 移植该门 + FeatureOverrideLUT 补 anyOpaque/anyTranslucent/anyViewIndependentTranslucent；像素锁 `Instances60SavedViewSpheresRenderSolid`（洗白淡彩像素 RED 2043→GREEN 58 双向验证）；既有 content 锚登记为对透明不敏感（洗白/实心双双过阈——判据水点补强）。②**M-J(2) hover 高亮构件颜色丢失修复**（fe7ed85d3）——像素症状未在当前资产面复现（桌面注入 sweep×12 腿+grid 逐点 hover+测试 harness flash 爬升：全部单元素语义正确）但**机制缺口实锤**并按参考修复：无 feature 表 batch 也被建 override LUT（hasFeatureOverrides 以 LUT 对象存在为准）→ Overrides 变体采 unit 7 上**别的 batch 的 LUT**（参考 BatchUniforms._setCurrentBatch:74-89 以 overrides.anyOverridden 活跃门 + 表存在性语义）+ `anyOverridden()` 只扫低字节（选摘高亮 flags16 被误判无覆盖）；三机制锁 RED→GREEN（anyOverriddenCoversHighByte/TablelessBatchHasNoOverrideLut/SetCurrentBatchActiveOverridesRequireAnyOverridden）+ 像素锁防跨构件污染门（flash 增亮目标/红蓝绿三类非悬停构件逐通道不变/清 flash 全帧还原）；TD-28③ 清偿。③**M-J(3) 实例球行色"发散"证伪 + 取证仪器修正**（5a903dfbf）——M-J(1) 发现的"行色排布发散"经 60 球色对照表取证（dump 字节 ground truth：6 行×10 球单色行[绿/紫/蓝/黄/橙/红]）+ 互换指纹分析（蓝红计数相等 8468≈8462/黄→teal/橙→天蓝）判定为 **dumpBmp 仪器伪影**（RGBA 内存帧直写 BI_RGB BGRX BMP → R/B 全帧互换），**引擎从未坏**（真彩反证链：桌面截图无互换六行全对/当前构建双路径+真实 app 复跑与 DTA 同构）；引擎零改动，产出 = 行色布局锁 `Instances60RowColorLayout`（六色族量值+WHERE 行色↔深度链+**仪器自检判据**[R/B 互换副本过同一分类器必须判红]）+ **dumpBmp 5 处 R/B 修正**（此后 dump 真彩；历史 dump 目视结论需按此重新解读——M-J(1) side-by-side 即伪影）。全量门禁（2026-09-30 M-J 收尾，Debug）：2531 项 = 2530 通过 + 27 跳过 + 1 失败（AcsDiscSurvivesDeepZoomPixels——TD-26 四态对照净基线同败实锤后再实录[本轮隔离 ×3 同败]，预存真缺陷疑似信号增强，留痕调查）。**阶段 1 M-L 完成（2026-09-30，DTA 功能对照与示例专业化收口——终审 Approved 无 Critical）**：①**M-L(1) 分析报告**（docs/DTA功能对照与专业化分析-2026-09-30.md，M-K(2) 收口入库）：参考 itwinjs-core display-test-app frontend 75 文件全读 × 本仓 DisplayTestApp+dqApp 公开 API 对照——**85 行矩阵**（✅25/🔶13/❌37/➖9；❌37 分层=[UI] 接线级 12、[引擎] 移植级 9、[引擎] 大件 16），逐项带参考源锚。②**M-L(2) 清理**（179969baf）：与 DTA 无关功能三档删除**净 -7703 行**（A 档死文件/死树：Translator/NavigationDialog 桩、StdWorkbench 死工具栏树、WorkbenchSelector 死路径；B 档死命令域+死 chrome：Macro/Structure/Tools 三域、Help 12 个生态链接、RecentFiles/Macros 空菜单、File/Edit/View 存根族——注册命令 ~110→24、菜单栏 7→4；裁决档：无文档后端恒空的 Model 停靠面板改造为 **Models/瓦树实用面板**（TileTreePanel——数据源 = 打开产物注册表 registerOpenedDump/forgetOpenedDump/findOpenedDump 自 main.cpp 迁入 DumpOpenHelper））；测试同步删/改。③**M-L(3) 接线级补齐**（528222ac2，每项先读 DTA 对应源码）：①Keyin 输入链（ToolRegistry keyin 面[getToolList/parseKeyin/parseAndRun/tokenize——Tool.ts 逐行] + ParseArgs（frontend-devtools parseArgs.ts 全量）+ KeyinField 宿主件[历史/补全/`聚焦]——ToolRegistryKeyin 10 锁移植 ToolRegistry.test.ts）；②诊断三小件（TileLoadIndicator 状态栏瓦装载汇总 + FpsMonitor/RecordFpsTool + SaveImageTool[readImageBuffer 生产化析出]）；③Models 选择器演示版（DumpOpenTreeProvider 逐模型可见性位 + TileTreePanel 复选/Show All/Hide All/Invert + ModelPicker 单步隔离 6 键——IdPicker.ts 子集，缺数据面登记）；④Edge Display 开关（ViewSettingsPanel Visible/Hidden Edges——viewFlags 位）；⑤视口同步（ViewportSync ported-but-uncalled 清偿：onViewChanged 订阅/disconnect 令牌/synchronizeViewportFrusta 实装 savePose/applyPose + SyncViewportsTool/SyncViewportFrustaTool 宿主）；⑥ToolAssistance 接线（InputHintWidget ported-but-uncalled 清偿——ViewTool.ts:628-655 host 半边）；⑦NotificationManager.OutputMessage 补 OnMessageOutput.Raise（通知链从不可达→live）。Task B **ReadMe 展示页**（Start 页第三分组卡片→12 条已锁能力滚动只读页）。跳过登记（超出接线级，归后续里程碑）：ZoomToSelectedElements/Saved Views 下拉/Snap modes/Categories 选择器/Macro/Grid/Monochrome/Display Style 运行时切换。④**M-L(3) 终审（2026-09-30，Approved 无 Critical）**：2 Important 随 M-K(2) 收口清偿（provider 可见性位"状态携带"发散登记[DumpOpenHelper.h/TileTreePanel.h EQUIVALENCE 注——参考随 ViewState.modelSelector 迁移、本仓挂 viewport provider，换视图不携带，TODO 转正] + **TD-29 新登记**）；Minor ③-⑧ 清偿（ToolAdmin/ParseArgs 参考行号 renumber 至 @7e57d018 实测行、NotificationManager "abstract"措辞订正[参考基类实为 no-op 具体方法 NotificationManager.ts:204]、SaveImage waitForSceneCompletion 前置登记+PNG 写失败消息区分[Failed to read image/Failed to produce PNG 两段]、ReadMe 第 11 条措辞订正[视口同步=keyin 工具非状态栏件]、视口同步三偏差登记[size<2 早退/frusta 同步未传 noSaveInUndo/重入门只罩 Raise]、**分析报告文档入库**）。门禁（M-L(3) 提交时）：dqAppTest 380=379+1跳+0败；DisplayTestAppTest 146/146；全量 2552/2552 通过 0 失败（27 跳过）。**阶段 2 M-M 完成（2026-09-30，五模型渲染要素对齐 + DTA 功能续接——六提交全录）**：①**M-M(2) polyline/pointString 图元消费清偿**（1e3f1fe53——TD-20/TD-23 遗留）：TilesetJson 增 primType（MeshPrimitive.ts:13-17）+ polyline 三视图（TesselatedPolyline 顶层 indices/prevIndices/nextIndicesAndParams）+ point indices；ImdlGraphics 两图元消费（12B 量化 LUT[SimpleBuilder 布局：qpos 6B+colorIndex 2B+feature 4B] + 后端预细分角点流字节原样入 BO 零 CPU 重排 → PolylineGeometry[增 usesQuantized ctor 形参 + setNonUniformColor]；point → PointStringGeometry LUT 形态[setLut/setLutPrimitive]）；shader：Polyline 量化变体色路径归位全路径 getComputeElementColor（复用 SurfaceCommon::addColor 量化分支）+ PointString 增量化 LUT 变体（1:1 PointString.ts:38-66 组合：gl_PointSize=fudge(lineWeight)+addColor 量化+roundCorners+CheckForEarlyDiscard；VBO 属性形态[triad]保持）；分派：PointString LUT uniform 组 + Polyline 量化组（u_qOrigin/u_qScale + 每 draw u_shaderFlags）。锁：baytown saved 瓦 2 polyline+2 point 图元色表 ground truth（黄/红/绿/chocolate+品红——线点无光照平色精确匹配）——RED 零出现→GREEN 2078-8380 sampled px（DTA 同视 8240）。②**M-M(3) textured 嵌入纹理消费**（3f4ab5d57——TD-20 遗留）：housemodel 纹理形态实证=**DisplayParams 级引用**（materials[k].texture{name,params{weight}}——非 surface.textureMapping）+ namedTextures 嵌入 bufferView（gta0=2048×64 PNG glyph atlas 8876B 瓦内字节——**无需采集**，用户"不限制存储"指令下核实全嵌入）+ surface.type 2/3+uvParams（QParams2d）；消费链：createEmbeddedTexture（loadNamedTexture :70-80 bufferView 分支——ImageSource→DecodeImage[stb §8.3]→SAMPLEABLE 纹理）+ hasTextures/setTexture/setTexCoordParams（xy=decodedMin、zw=(max-min)/65535）+ Textured=SurfaceType::Unknown（无光照——Material4 ignoreLighting=true 同构；TexturedLit 本采面 0 命中）+ 分派 u_qTexCoordParams（kComputeTexCoordQuantized 预移植直用）。锁：TexturedSurfaceAttachesEmbeddedTexture（type=2 prim→hasTextures+纹理句柄+UV 参数对账）。视觉：roof 从平灰转平铺纹理图案（与 DTA 同构）。遗留 TODO：params.transform（textureMat2x3 u/v swap——图案取向差异面）与 textureWeight。③**M-M(4) TD-27 unquantized-LUT 清偿**（同提交）：解析层 usesUnquantizedPositions 字段 + 接受门 numRgba=4|(5&&unquant) + **变体轴 isLutUnquantized**（§3.4 偏差簿记——TechniqueFlags 新字段+equals+compositor 双点；CachedGeometry::usesVertexLut 虚接口+setUsesUnquantizedLut）+ createCommon/addNormal/addColor 增 lutUnquant 分支（kComputeUnquantizedPositionFromLUT+kPreReadVertexDataUnquantized+色源 g_vertLutData4.xy[UnquantizedInstanced 常量本已预移植]+法线 NonQuantizedPrelude）+ 分派 LUT 供给门改 usesVertexLut。锁：UnquantizedLutTableProducesGraphics（真 dump 瓦 28.imdl——2 prim numRgba=5→graphics 产出+变体位对账）+ DumpBrowse 白名单移除该键（36+6 instances 瓦转正）。④**M-M(6) 接线级两项**（b7eb678f8）：**ViewState.ToProps**（保存方向——CreateFromProps 对偶：angles 反解移植 YawPitchRollAngles::CreateFromMatrix3d 全文体[atan2+A/B 对 maxAbsRadians×0.95/sumSquaredRadians 择优+gimbal-lock+round-trip sanity]+三 selector 回写+displayStyle 三段 toJSON 对偶；round-trip 锁=instances60 saved 视图复活逐项）+ **Snap modes keyin**（`dta snapmode <mode>`——App.ts:486-489 setActiveSnapMode 引擎通道；AccuSnap::setActiveSnapMode[clear 联动]+九模式名 1:1+无参恢复默认；锁=解析面/keyin 设置/恢复/未知名不改模式）。⑤**取证教训第三录**：MSVC 增量依赖再失灵（exe 21:13<src 21:26，cmake 报成功未重链——**删 obj 强制重编**为标准处置）；DtaTest 与 app 双目标的 cpp 需双注册（SnapModeTool.cpp 链接符号缺失实录）。⑥**M-M(5) bridge-edit 全树 sweep 收口入库**（bca5215d29——用户解除存储限制指令）：**380,738 瓦 / 4,955.79MB**（done:budget-capped——400k 瓦 cap 触顶，errs=0；dup 19,262 跳过）入库 `bridge-edit-sweep-v1`（sha256 入库后抽验 ×3 OK；树 props 仍自主根单源）；接入 = bridge-edit fallback 第三位（v1→drill-v1→sweep-v1——**浏览深度界从 drill 30 瓦域扩至全树域**）；大瓦 `files/12.imdl`（-b-2-0-0-0-1，134.2MB，与既有两根 8.imdl 同一字节）超 GitHub 100MB 硬门本地持有（.gitignore——回放缺瓦走父瓦兜底；最大非忽略瓦 13.imdl 68.6MB）；域扩后 bridge 锁复跑全绿（framed-view content=769,368px——请求面不变实证）。README 含 provenance/caps/复现命令。**阶段 3 M-N 完成（2026-10-01，接线级残余两项清偿——Categories 选择器 + ZoomToSelectedElements）**：①**M-N(1) Categories 引擎通道+面板**（b59d7441/47968ffe）：Viewport::SetInvisibleSubCategories → RenderTarget/TargetImpl 集合+版本链 → RenderCommands::addBatch 惰性重算 Batch::applySubCategoryVisibility（特征表逐行 subCategoryId 对不可见集合清 LUT Visibility 位）→ 分片 override discard。**三修复**：anyOverridden 补隐藏行腿（DanQing flags16.Visibility 反极性——隐藏行标记被清原不触发激活门）；**allHidden 死锁**（addBatch 的 allHidden 跳过门在惰性重算之前→全隐 batch 不入命令表→un-hide apply 永不运行——重算前置到 addBatch）；Polyline-Overrides 变体编译修复（首次激活暴露旧槽体无 return+读错 texel 通道→归位 surface 槽体 1:1）。像素锁：housemodel 全 24 subCategory 隐藏→**内容塌缩 0px**（基线 445224）/清空→**精确恢复**。CategoriesPanel（ViewState.categorySelector 数据源+imodel-native 默认子类规则 cat+1 映射——**EQUIVALENCE 发散登记**：无 iModel.subcategories RPC 表，baytown cat+2 非默认子类形态不随 category 隐藏，转正=采集该数据面）。[CAT] 探针（DANQING_CAT_TRACE）。②**M-N(2) ZoomToSelectedElements 全链**（f9a9a039）：placements.js 注入（--placements——getPlacements 的 ECSQL 原文全集）+ collector /placements 端点 + instances60-placements-v1 入库（62 行）；DumpIModelConnection 随 open 装载 placements.json（可缺席）+ findPlacement；ZoomToSelectedElementsTool（keyin "dta zoom selected"——选集→Placement 八角世界域→LookAtVolume）；锁=装载 62 行+0x38 球钉值+六轴世界域 1e-6+多元素并集+空选集。门禁：**全量 2569 项 100% 通过**（MinimizeRestore 环境态族本轮自然通过）。**阶段 4 M-O(1) 完成（2026-10-01，核销账 + 删除侧收尾 + 接线级六件——双向任务首会话，提交 5 笔）**：①**核销账入库**（`docs/阶段4-MO-…-实现计划-2026-10-01.md`——三路勘察实证矩阵 vs 仓库现状：删除侧真实余量远小于 §3.4 预估[M-L(2) 已超预期清完菜单树/存根命令]，实现侧 12 接线级实证落地 + 余量重排 I1-I11/P1-P10；用户三决策：➖9 架构外结案 / 大件首立项 Sectioning=M-P / 本会话全案；矩阵 Measure 参考锚笔误更正 core/frontend/src/tools/MeasureTool.ts:185）。②**删除侧收尾 D1-D5**：qrc+icons 死图标 **161→74**（双形态匹配 KEEP 74/DELETE 87，qrc↔磁盘双向校验零缺失零孤儿；images_classic 43 全为 qss 视觉基线引用保留）+ sequencerBar/notificationArea 死 chrome 删（StatusBarTest 断言同步 4→3）+ **D2 裁决零改动**（Window 域 5 sPixmap=FreeCAD VERBATIM 元数据[MDIChromeTest 锁定]、svg 从未入库——登记结案）+ IntegrationTest 头注更正。③**接线级六件**（每件 RED→GREEN 双向验证）：**Monochrome Color/Scaled**（含引擎链四缺口归位：RenderPlan.monochromeMode bool→枚举[RenderPlan.ts:114] + DisplayStyle mode 通道/独立事件 + StyleUniforms→u_monoRgb 真实绑定[此前无喂数方] + Common.ts:57 位 0 置位[此前从不置——monochrome 渲染整链 render-dead] + Monochrome.ts:46-50 u_mixMonoColor 逐 draw 语义[此前自造常量 0] + SurfaceGeometry.wantMixMonochromeColor[isGlyph EQUIVALENCE]；像素锁红 2,127,508px/绿 0）；**Snap modes 下拉**（AccuSnap 数组通道 App.ts:95-99 + 8 项下拉 Multi-snap=-1→7 模式位掩码逐项序）；**hover 装饰 tooltip**（NotificationManager tooltip 面 ported-but-uncalled 清偿[§11.10 又一例] + Viewport hover locate 驱动 + QToolTip 宿主半边[+15/-20]）；**Grid 设置 keyin**（"dta grid settings" s/r/g/o/l 全参 + ToolAdmin.gridLock 字段随工具移植[AccuDraw 消费面未移植先立]）；**快捷键族 MDI 子集**（Ctrl+[ ] 焦点轮换/Ctrl+\ 克隆/Ctrl+| 关闭；图钉/停靠 8 键浮窗专属 EQUIVALENCE➖）；**Models/Categories 工具栏按钮**（空下拉→dock 面板 toggle[面板宿主 EQUIVALENCE]；CategoriesPanel.cpp 补入 DtaTest 目标——此前零测试编入）。④**溢出如实登记转 M-O(2)**：3d OutputShaders（勘察修正：需引擎 shader 源注册面 debugControl.debugShaderFiles 等价物，非纯接线）/3f glTF instances/3h 视口标题/3i ToolAssistance mid-tool（引擎逐工具实装超接线级）；**MeshGeometry[glTF/Polyface] 路径无 monochrome 组件**登记（参考对所有 Surface 技巧适用）。门禁：全量 ctest（TD-26/TD-29 环境态族规程隔离复跑 ×2 全绿归因；**OpensBaytownOrthographicSavedView=TD-29 满载抖动族新实录**——满载轮败/隔离 ×3 绿[content=456000px/16.286%/质心 (872,290) 正常域]）。**阶段 4 M-O(2) 完成（2026-10-04，DTA 接线级余量七件全清——提交 5 笔[I9+3d 因提交失误合并一笔]）**：①**3h 视口标题**（Viewer.updateTitle :453-461 `[ vpId ] viewName <id> (3d)` 格式落地 MDI 子窗 + Title.ts:9-15 app 标题面中文名映射迁主窗——引擎面：ViewState EntityProps 就地承载[GetId/getCodeValue/SetId/SetCodeValue + is2d 基类虚] + CreateFromProps/ToProps round-trip + 三处 Clone 拷贝[clone 链缺拷贝曾致 getDefaultView clone 丢字段——RED 实锤后修] + OnChangeView 订阅覆盖全部 changeView 路径；RED 双锁：引擎[revived id=0x25/codeValue=Default - View 1] + 宿主[DumpOpenChain caption 正则 `^\[ \d+ \] .+ <.+> \(3d\)$`]）。②**3i ToolAssistance mid-tool**（ViewManip.provideToolAssistance keyed 主指令+mouse/touch 双段 :630-657 + WindowAreaTool FirstPoint→NextPoint 跃迁 :3563-3582 + FitViewTool Accept 面 :3218-3234 三体落地 + ViewTool::translate en 内嵌表 + NotificationManager.setToolAssistance→OnToolAssistance 事件[参考 no-op 基类+appui 消费] + 宿主 DtaTools 换事件驱动[install-time 表删除]；引擎三锁 + DtaToolsWiring 15/15）。③**3f glTF 实例化开关**（GltfDecorationTool keyin "dta gltf" i>/s>/c>/r>/f> 参数面[URL 形态 §8.2 不取数 EQUIVALENCE] + createGltfInstanceTransform :41-67 + buildInstancedScene :69-98 CPU 展开烘顶点[GPU RenderInstances 通道未移植 EQUIVALENCE]；锁三件：变换域[origin=t+P−sR·P 界 4.5×maxExtent——初版自创 2.5 界被参考数学实锤纠正]/七色循环+两两分离/像素锁 BoxTexturedDots×5 seed=42 簇分离——RED 实锤 computeBounds 缺失致 LookAtVolume(null) 零内容）。④**I10 FeatureOverrides 面板**（per-viewport Provider Viewport.ts:1570-1615 1:1——替换非参考 ViewManager 全局面 + Step 9 重建归位 :2619-2645[修复前 m_featureOverridesDirty 仅首帧 true——provider 变更永不触发重建] + RenderTarget overrideFeatureSymbology/getFeatureOverrides/getFeatureOverridesVersion[对象同一性→版本计数 EQUIVALENCE] + OpenGLRenderTarget 桥转发[组合壳虚调用曾落基类 no-op] + PushBatch appearance 惰性重算[webgl FeatureOverrides.update :412-441 ovrsUpdated 分支同构——读-保-写全字段] + **Surface-Overrides 变体 rgb/alpha 值源归位**[computeFeatureOverrides :642-651 + getSecondFeatureRgba :111-117——覆盖色取第 2 texel；此前直读 texel0 flags 字节致命中元素呈 (flags,flags16,lineCode)/255 近黑——PPM 眼验+簇判据双实锤] + 宿主 FeatureOverridesPanel 全套[I9 recall 消费面 toJSON/overrideElementsByArray] + Overrides 按钮 live；像素锁[命中元素可视簇向覆盖色移动 1607px/簇域内容 bbox 内/邻近拾取点/far 不变——多 batch 拷贝下可视覆盖实例偏移=高亮锁同域登记]）。⑤**I9 Saved Views**（引擎缝 serialize/deserializeViewStatePropsJson[与 parseViewStateProps 消费面互为闭偶——optional 缺席不写出=fromJSON 缺省守恒，%.17g round-trip] + NamedVSPS 载体[NamedViews.ts:11-106 1:1——SortedArray→vector+lower_bound] + SavedViewPicker[Create 同名/空名拒绝/Recall=deserialize→code.value=name→ApplySavedView 缝→override+selectionSet 恢复/Update=delete+save 同名/Delete 钮+键] + 持久化本地文件 EQUIVALENCE[env DANQING_SAVED_VIEWS_DIR] + Views 工具栏转弹出；锁：JSON round-trip[instances60 真实载荷全字段]+缺省守恒+链式[recall 复原+二次幂等 1e-6——首定心偏移=参考 ViewState3d ctor :1506-1508 centerEyePoint 相机视图重定心语义+持久化 round-trip+Update/Delete]+选择集与 override 双恢复[selectedElements=参考 Id64 字串数组形态]；夹具教训：DumpOpenResult 须夹具成员持有[局部变量 setup 返回即析构连接→imodel 悬空 SEH]）。⑥**3d OutputShaders**（DebugShaderFile 升公共载体 + RenderSystemDebugControl 扩 compileAllShaders/debugShaderFiles + 收集门 env DANQING_DEBUG_SHADERS[=IMJS_DEBUG_SHADERS 等价] + ShaderProgram saveShaderCode :355-374[desc 剥 //!V!//!F!+归一+noname-N+_VS/_FS.glsl，compile 记录失败也记]/use 标 isUsed :337-353 + Techniques::compileAllShaders :1009-1015 + **懒建模型矩阵枚举**[SurfaceTechnique ctor :318-348 守卫枚举 1:1——不动懒建语义/启动零成本，各技巧矩阵 EQUIVALENCE 以 Surface 矩阵为通用面] + OutputShadersTool keyin "dta output shaders"[c 编译通知分流 + u/n/v/f/g/h 六向 :289-293 + d= 补尾 + _makeShadeBat 逐字常量{fxc 工件 GL-only 不执行} + writeExternalFile→本地 ofstream §8.2]；四锁：六向谓词/注册表[首绘 118 条+命名形+isUsed]/compileAllShaders 严格增长/落盘对账[行数=条目数+文件数=唯一名数{预建表同 flags 多槽 PointString 5x 重复=参考同形}+v 过滤全 _VS]）。⑦**I11 第二 iModel 叠加**（TiledGraphics.ts 1:1 宿主面[Reference 委托包装+Provider create=defaultViewState modelSelector 逐 model 树装配——宿主打开链 ④ 同源+toggle per-viewport 注册表+ToggleSecondaryIModelTool keyin "dta tiled graphics"] + 引擎两面[DumpTileFetcher::addFallbackRoot ×2 形态——运行期追加根进同一查找域，索引只为新根建；DumpIModelConnection ecefLocation 面——数据面缺席恒 false，EcefLocation 类型随 geolocated 数据面落地登记]；EQUIVALENCE 四条[取数面单 fetcher 追加根 vs 两连接后端——验证=多根命中锁/per-ref symbology overrides 未移植——DanQing 缺省无 category 隐藏面=参考 ovrs 效果面等价/ecef 恒等分支/getTransformFromIModel 无消费者]；像素锁[并域取景{空瓦叶树倒置 null rootTile.range 守卫——DumpBrowse 白名单同族} 基线 3542→叠加 22769/差集 22155/新增簇 19388+WHERE[新增簇 bbox 69% 在主 bbox 外——两世界域不重叠但屏幕投影部分交叠的实测形态]/toggle off 回落] + 多根命中[追加根 hitRoot=2 上 Completed 10 键]）。另：**陈旧测试源三修**（dqRenderTest 二进制自 API 变更后未重编的增量依赖陷阱再实录：ThematicDisplaySensor 双命名空间歧义限定 + RenderPlan monochromeMode bool→枚举断言[M-O(1) 遗留] + u_mixMonoColor ProgramUniform 断言→源声明断言[M-O(1) 起 graphic 语义]）；取证仪器教训：PS 5.1 文本管道改源=编码损坏（Get-Content ANSI 读+BOM 写——Write 工具整写恢复）。全量门禁（2026-10-04 M-O(2) 收口，Debug）：**2596 项 = 2570 通过 + 26 跳过 + 0 失败**（跳过 = 既有 headless-GL 编译族 + 2 环境跳过；TD-26/TD-29 环境态族本轮自然通过——0 环境态失败）。**阶段 4 M-O(3) 完成（2026-10-04，移植级首批三件——提交 4 笔）**：①**P1 Walk/Fly/LookAndMove**（NavigateMotion :1747-1921 全量 1:1[init/takeElevator/modifyPitchAngleToPreventInversion ±85° 限位/generateMouseLook+Rotation+Translation 三变换/moveAndMouseLook·moveAndLook/pan/travel/look/resetToLevel] + ViewNavigate :1924-2003[getNavigateMode 修饰键三模态/animate 逐帧 frustum×transform→setupViewFromFrustum/onReinitialize walk 相机归位 75.6°/drawHandle 锚点圆环] + ViewWalk/ViewFly :2951-3032[三模态合成——Walk Travel=角率+前进 XY 约束/Fly=角率×2 前进沿视角] + ViewLookAndMove :2006-2645 核心[testHandleForHit 恒命中 Medium/deadZone=pixelsFromInches(0.5)²/walkVelocityChange 倍率 clamp±10 负倒数/键盘+拖拽双输入源/onKeyTransition WASD/箭头/QE/PgUp·Dn/C·Z/+-= 全键位/enableKeyStart 视线中心起拖/onWheel 动态中滚轮变速；EQUIVALENCE 三条=pointer lock 无锁路径/touch 无输入面/collision 轮廓 readPixels 通道延后[设置面 1:1 toggles]] + 三工具+ToolAssistance 键盘指令族[createKeyboardInstruction/createModifierKeyInstruction/arrowKeyboardInfo ⯅⯇⯈⯆/shift·ctrl·altKey——:141-292 随消费面落地] + ViewManip 三面[onKeyTransition focusHandle 路由/setCameraLensAngle 保眼/enforceZUp 轴角组合] + Viewport::TurnCameraOn[cameraOff→NpcToWorld 四角中点取景+validateLensAngle 1°..170°；determineVisibleDepthRange 未移植→{0,1} 回退域 EQUIVALENCE]；RED→GREEN NavigateTest 8 锁[含 KeyAccumulation 双态实测——非动态下 +/- 早退/VelocityMultiplier 3×/TurnCameraOnFromOrtho/SetCameraLensAngle 保眼 1e-6]）；②**P2 Measure distance**（MeasureDistanceTool :185-729 核心链[收点+ctrl 分流=位置累积 vs 立即接受/Reset=Restart/Undo location 优先弹/段数学 3-4-5+垂直 slope=π/updateTotals 累计+标签/Distance vs Cumulative 分流/getSnapPoints 段端去重/createDecorations 双层线+marker 圆点+标签] + MeasureLabel[setPosition worldToView+0.44" 上偏+视域门/黑 0.4 底白边框——fillRect 经路径组合 EQUIVALENCE] + keyin measure distance；EQUIVALENCE 七条=格式化恒米制 4 位/adjustPoint 恒等/refAxes 恒 identity/Marker 视觉等价[canvas 无 pick 面选中态不可达]/transientIds 恒 0/getDecorationGeometry 不实现/tooltip 并消息面；RED→GREEN MeasureToolTest 7 锁）；③**P3 框选/ctrl 增选**（selectByPoints 族 :287-439 + Viewport::PickAtRect[getAreaSelectionCandidates :637-721 等价——**Box 收缩带** :676-701 outline=四边缘 2-device-px 带读并集/inside=contents-outline，五读差集 EQUIVALENCE] + Line 跨线[沿线采样近似] + 点击面 ctrl Invert 归位 :414 + **decorate 虚面归位 InteractiveTool**[Tool.ts:537-539 历史误置 ViewTool——PrimitiveTool 装饰通路开面] + onResetButtonUp 框选清理分支 :472-477；EQUIVALENCE=hit-cycling 延后[pick 单值无命中列表]/体积选择恒 area/虚线 dash 面缺；测试缝 SetPickRectHandlerForTest 逐读回调[收缩带差异注入直接可测]；RED→GREEN SelectBoxTest 7 锁[含收缩带五读 1+4 计数实钉+inside={0x1,0x3} 差集]）；另：**TD-26 信号增强实录**（AcsDiscSurvivesDeepZoomPixels 本轮连续隔离 ×2 同败+stash 双态对照 P2 净基线同败——预存真缺陷与 M-O(3) 改动面无关，§14 TD-26 行更新）；DtaToolsWiring.GltfDecoration 偶发 flake 一录[3f 随机采样贴界——隔离复跑绿]。全量门禁（2026-10-04 M-O(3) 收口，Debug）：**2618 项 = 2592 通过 + 26 跳过 + 0 失败**（跳过 = 既有 headless-GL 编译族 + 2 环境跳过；AcsDisc 本轮自然通过——TD-26 间歇态与收口前复现轮交替，§14 行更新）。**阶段 4 M-O(4) 完成（2026-10-04，移植级 P4-P9 六件——提交 6 笔）**：①**P4 RenderMode 2 值 = 勘误结案**（勘察实锤：参考 RenderMode 恰 4 值[ViewFlags.ts:18-43——Wireframe=0/HiddenLine=3/SolidFill=4/SmoothShade=6]，CrossingEdges/HiddenLineVisibleEdges 在 core+display-test-app **全仓零命中**——对照矩阵 :234 行前提不成立；矩阵行勘误订正 + ViewSettingsPanel 陈旧注释删，零实现）；②**P9 Macro 播放器**（MacroTool :8-55 1:1——readExternalFile→本地 ifstream EQUIVALENCE[§8.2]/
 剔除+
 分行+空行剔除/空文件 no-content 告警/逐行 parseAndRun 三分支文本 1:1+序列不中断/openMessageBox→OutputMessage EQUIVALENCE；keyin dta macro；MacroToolTest 5 锁）；③**P5 Display Style 运行时切换**（ViewState::SetDisplayStyle——ViewState.ts:644 setter 1:1 + EQUIVALENCE[DisplayStyle 持不可拷贝成员→DisplayStyle3dSettings 值拷贝承载整替换]；视口侧既有监听面直接消费；宿主下拉激活[EQUIVALENCE：参考 ECSQL 枚举 displayStyle 元素表，dump 数据面无该表→当前样式单条+采集面登记]；DisplayStyleSwitchTest 2 锁[全字段面+事件恰一次+视口失效]）；④**P6 hline 边样式编辑器**（HiddenLineSettings::override 聚合[HiddenLine.ts:210-219 段级合并 1:1] + ViewSettingsPanel Edge Display 分区[Transparency Threshold slider/Smooth 复选/visible+hidden 两编辑器 Color·Weight·Pattern] + **TileAdmin::edgeOptions 权威源**[TileAdmin.ts:86——树 Id 派生两处接线 DumpOpenHelper/TiledGraphics，缺省值不变捕获键域稳定]；HiddenLineOverrideTest 3 锁）；⑤**P7 Environment 编辑器**（ViewSettingsPanel Environment 分区[EnvironmentEditor.ts:69-329——Sky Box/Ground Plane 复选/Background Color/2-4 色 radio/四色钮/双 Exponent slider/Reset] + updateSkyEnvironment 段合并/resetEnvironment/setEnvironmentDisplay 三可测槽；EnvironmentEditorTest 3 锁[段合并缺席保持/Reset 默认四色/事件恰一次]）；⑥**P8 Cesium 陈列馆**（CesiumDecorator 8 族 1:1[EmptyExample.ts:50-496——point/lineString/shape/arc/path/loop/polyface/solidPrimitive 每族 1-4 例三挂载形，projectExtents.center 基 ±50000..280000 米级布局+xOffset -220000] + EQUIVALENCE 三条[createCircularStartMiddleEnd→XY 共面三分点外接圆构造/createScaledXYColumns→FromVectors/xOffset 保留] + builder 半边 computeChordTolerance 接线[PrimitiveBuilder.finish 的 LOD 门——缺 closure 恒 null 首跑全族 0 实钉] + CesiumExampleTool keyin dta cesium example toggle；CesiumGalleryTest 2 锁[三分点回代 ≤1e-9+8 族色相域分类器像素锁——光照合法缩放主导通道判别+path vs loop r−b 分离，首绿 shape 36222/loop 24899/polyface 2062802+WHERE 展布]；分类器调试两录[maxAll 含 g 自身恒假分支/±30 精确窗对光照填充色不成立——绿三角实渲染 (1,177,1)]）。全量门禁（2026-10-04 M-O(4) 收口，Debug）：**2633 项 = 2607 通过 + 26 跳过 + 0 失败**（跳过 = 既有 headless-GL 编译族 + 2 环境跳过；AcsDisc 本轮自然通过——TD-26 间歇态实录）。**阶段 4 M-P 完成（2026-10-05/06，Sectioning 剖切大件首立项——七件全清，提交 7 笔）**：①**P-A dqGeom 几何三件**（e223802——UnionOfConvexClipPlaneSets 全新 1:1[union 语义族+addOutsideZClipSets 参考 truthiness 怪癖 zLow=0 跳过 1:1+takeConvexSets 尾取序] + ClipPrimitive/ClipShape[asClipShape 虚槽=instanceof 的 no-RTTI 适配 + ensurePlaneSets const mutable 懒建 + parseClipPlanes 三径：linear 退化/凸内域/凸外域 fan+角平分线；凹多边形与 mask 洞 parsePolygonPlanes TODO=Triangulator/HalfEdgeGraph 未移植，行为=参考三角化不可用失败形态，视图剖切四定义不触及] + ClipVector[classify 族/extractBoundaryLoops/toCompactString std::to_chars 最短往返] + ClipUtilsLoops 拆文件[loops/range/does 族]；12 新锁 RED→GREEN）。②**P-B ViewState clipVector 存储**（cb7db46——ViewDetails.ts:137-166 双态语义 1:1 就地承载[惰性物化 getter+setter 六步+empty≡no-clip no-op]，OnClipVectorChanged 全仓首次 Raise 接通既有 Viewport 监听；Clone 三点+CreateFromProps ×2+ToProps 对偶；DisplayStyle clipStyle 门面；宿主 parseClipVectorProps 深解析；ViewClipStateTest 6 锁=ViewDetails.test.ts 六例 1:1）。③**P-C RenderPlan→ClipStack/ClipVolume 参考语义整体重写**（e7fb93——ClipVolume 视坐标编码[哨兵行(0,0,0,0)/(2,2,2,0)+界行 j 索引怪癖 1:1] + ClipStack setViewClip 六步/三虚槽 §3.4 driver 注入 + RenderPlan clip/clipStyle + TargetImpl clipStack 视矩阵 EQUIVALENCE[branch 栈底 mv·localToWorld⁻¹] + compositor push/pop 配对；ClipVolumeStackTest 10 锁=webgl 测试移植）。④**P-D shader 片元裁剪全链**（9fbc20——Clipping.ts addClipping 1:1[u_clipParams[0] 数组后缀=RHI location 根因] + RHI 三修复[RGBA32F 映射/NEAREST setTextureFilters 新虚接口/clientType 格式族推导] + 变体 numClipPlanes 维 + **TileTreeRender.ViewClipPlaneDiscardsHalfspace E2E**[红盒塌缩+bbox 回退+旗标语义+可逆]；五层根因链全录）。⑤**P-E ViewClip 工具族**（843f40——五工具+工厂面全量，getPlaneInwardNormal 六世界朝向 EQUIVALENCE[AccuDraw 上下文旋转缺席→世界轴/视图 Z 列承载]，ByElement placements 既有数据面；ClipViewToolTest 10 锁）。⑥**P-F ViewClipDecoration+EditManipulator**（efa8b2——HandleProvider/HandleUtils/HandleTool[选集直挂 Synch EQUIVALENCE] + ViewClipDecoration 全交互[getClipData/createControls 选集门/shape 六手柄/plane 质心箭头/negate/clear/orientView/zExtents/Decorate 双 builder 箭头] + Provider 四事件；**引擎修复三处**：SelectionSet 变更门[no-op 不触发——Synch↔clearControls 无限往复根因] + Viewport::computeViewRange 移植 + ComputeFitRange 补 ref union[装饰 viewRange 被空连接宽松 extents 推出内容域]；**C++ 生命周期安全面**[clear() 重入守卫+选集事件栈延迟 delete]；ClipDecorationTest 11 锁 + ViewClipDecorationOutlineAndNegatePixelLock E2E[手柄箭头簇 WHERE：水平细条贴面迹线列/帧高中带+negate 像素双向]）。⑦**P-G SectionsPanel 宿主 UI**（003e48——Clip type 下拉四项[Geometry 项 DTA 专有未移植➖] + Define/Edit/Clear 三钮[tools.run provider ctor 形参直构承载] + Analysis 工具栏 Sectioning 激活；**引擎补口**：PrimitiveTool::targetView 公有字段[PrimitiveTool.ts:27] + ViewClipClearTool 安装即清闭环 + ToolAdmin::onButtonDown 移 public；SectionsPanelTest 4 锁含 **Instances60ToolEntryClipsContent E2E**[面板 Define→单点接受→Face 取向锁+NewPlane 事件+像素半空间削减 405628→256628+旗标单调+Clear 恢复]；**teardown join 死锁 saga 取证**[14 线程全 Wait——DqEventScope 悬垂退订根因，阶段二分五轮定位]）。EQUIVALENCE E1-E8 预登记全部核实落实；新探针 DANQING_CLIP_TRACE/DANQING_CLIPDECO_TRACE（§13.1）。全量门禁（2026-10-06 M-P 收口，Debug）：**2679 项 = 2652 通过 + 26 跳过 + 1 失败**（AcsDisc——TD-26 归因：同一二进制在本会话 P-G 收口轮全量自然通过[直接非代码证据]；本轮新形态=step-0 呈现分裂[FBO 圆盘 ✓/屏幕 BitBlt 0]，与桌面残留的不可杀僵尸测试进程污染 CAPTUREBLT 屏幕捕获层相关疑——桌面态相关零代码相关；隔离复跑 ×3 同败实录；其余全绿含 TD-29 满载族本轮自然通过）。**阶段 4 M-Q 完成（2026-10-06，Rendering Style 14 预设——大件 2/16，四件全清，提交 5 笔）**：①**Q-a applyOverrides 合并语义 + Viewport::overrideDisplayStyle**（d15962b——viewflags 合并 1:1 `_applyOverrides` 的 spread 语义[现值 toJSON 只携偏离位 + 覆写仅覆盖在场位——此前整替使缺席位回默认=应用预设会重置用户 grid/weights 等 19 位的真语义缺口] + toJSON3d 接线 lights 段 + Viewport::overrideDisplayStyle[Viewport.ts:657-659——applyOverrides3d + 按 props 在场段发等价失效：viewflags→FeatureOverridesDirty+RenderPlan / bg·mono·lights→RenderPlan / environment→Decorations[天空指纹重建] / hline→Scene]；DisplayStyle.test.ts:554-588 直移 RED 实证[grid/weights 五断言红]）。②**Q-b 14 预设表**（62b6ff——RenderingStyles.{h,cpp} 逐字段 1:1[ViewAttributes.ts:31-268：十四项 environment tbgr 色值/viewflags 覆写位/lights 全七字段/hline/monochrome×2/ao 8 参/thematic Height·Slope] + applyRenderingStyle[None no-op]；表完整性 6 锁）。③**Q-c UI 下拉**（b18c85——ViewSettingsPanel "Rendering Style: " combo 14 项[Display Style 与 Render Mode 之间]；wiring 锁）。④**Q-d E2E + wantLighting 修复**（e3b2c8——**引擎修复：SurfaceGeometry ApplyLighting 视图级门**[1:1 SurfaceGeometry.ts:35-37/331 的 wantLighting=SmoothShade&&vf.lighting——此前仅几何级 isLit，三灯全关位[Illustration 族]不熄光照的移植缺口]；RenderingStylePresetsPixelLock 五判据[Schematic 白底 89% 帧/Illustration 反照率亮度 59.7→85.0——修复前 RED 恒零变化/Moonlit 夜空 corner (0,30,53)/Comic cel 量化+顺序合并语义实录——Comic 覆写不含三灯位保持 Illustration 关灯态=参考 spread 合并 1:1/merge grid 保位]；hline 黑边判据不落=资产无边表如实登记[JoesHouseEdge 锁钉]）。EQUIVALENCE E1-E5 全核实落实（AO/Thematic/Atmosphere/solarShadows 视觉面数据面 1:1+E2-E3 登记；2d 门 E4；C++ 表 E5）。插曲：内核态僵尸进程锁 exe（用户手杀清障）+ **TD-26 归因直接实证**（同二进制 ghost 在→呈现分裂败/ghost 清→过——僵尸污染 CAPTUREBLT 实锤）+ E2E 判据三次订正实录（先核参考语义再设判据）。全量门禁（2026-10-06 M-Q 收口，Debug）：**2687 项 = 2657 通过 + 27 跳过 + 3 失败**（三分支全隔离复跑绿归因：MaximizeKeepsGridVisible=真窗口最大化挂起 2h 被用户手杀[桌面态污染族——与 M-P 僵尸 saga 同型]；MinimizeRestoreKeepsScreenAlive=TD-29"基线即黑"族隔离 ×2 绿；GltfDecorationToolCreateTransformDomains=M-O(3) 已录 flake 隔离绿）。

**阶段 4 M-R 完成（2026-10-06，DTA UI 布局与交互全面对齐——用户指令"按 DTA 布局与功能全面调整、区别只是平台技术、代码与交互逻辑一致" + 三结构裁决[留 Start 页/留菜单栏/留 MDI]，提交 2 笔）**：①**R-a+R-b 双工具栏换位 + 26 项主工具栏 + 交互合同**（fb271c7——DtaToolBars 重构：5+1 分域工具栏→**appToolBar[Surface.createToolBar :122-178 五项：Open disk **激活**[QFileDialog 选 dump 包根→openDumpIModel]/Open Blank/Analysis 置灰/Deco/Cesium keyin] + mainToolBar[Viewer.ts:238-446 **26 项严格序**]**；Surface.ts:103-119 焦点换位 1:1[OnSelectedViewportChanged→app/main 显隐换位——DTA appendChild 语义]；ToolBar.ts:122-197 交互合同[openDropDown 单开互斥/closeOpenDropDowns/destroyed 摘除/only3d 四项 is3d 显隐]；**Measure/Walk 按钮激活**[引擎 M-O(3) P1/P2 已移植而按钮置灰的遗留——本件清偿]；Google Maps config 关态零出现；DtaToolBarsTest 重写 7 锁）。②**R-c 顶状态条 + 底部双 span**（setupDtaStatusBar 重构：keyin/FPS/tileLoad/snap 八项下拉迁 DTA.StatusBar 顶条 QToolBar[index.html #status-bar 位置/子件序 1:1]；installDtaOutputSpans 底部 showstatus/showerror 双 QLabel[Utils.ts:8-15 等价] + showStatus 双写分流[Pane→status/Err→error]；OutputSpansReceiveStatusAndError 锁 + StatusBarAssembly 订正）。EQUIVALENCE E1-E7 全落实（置灰族五项/CameraPaths·Animation 可点空下拉=DTA blank 语义/spinner 不落 tileLoadIndicator 承载）。全量门禁（2026-10-06 M-R 收口，Debug）：**2686 项 = 2658 通过 + 27 跳过 + 1 失败**（AcsDisc——TD-26 族[step-0 呈现分裂再实录：本轮桌面无僵尸但用户并行窗口满载[Windows Terminal 对拍作业/Edge/VS Code]——CAPTUREBLT 屏幕捕获层桌面态敏感族，隔离 ×3 同败实录；同会话 M-Q 收口轮同二进制全绿=非代码证据]）。

**阶段 1 M-K 完成（2026-09-30，三模型数据面采集 + 三入口接入——Baytown / House_Model / 编辑大桥测试）**：①**M-K(1) 采集入库**（fc2d8437d，仓外 danqing-rpc-tools × 新检出 D:/Github/itwinjs-core @7e57d018——适配冒烟/中文文件名四层编码链/全量 sha256 复算 0 mismatch；六 dump README+预算 cap 随 provenance.sweep.caps 落盘；**大瓦本地持有裁决（M-K(2) push 时）**：bridge-edit 两 dump 的 `files/8.imdl`（`-b-2-0-0-0-1`，134.2MB 同一字节[跨会话确定性]）超 GitHub 单文件 100MB 硬门 → git 不入库本地持有[.gitignore+两 README"大瓦本地持有"节登记，manifest sha256 不变；回放缺瓦走父瓦兜底，桥锁请求面为 d3 键不依赖]）：**housemodel**-v1[1 树 106,658 瓦/489.91MB，sweep 瓦数 cap 120k 触顶 d10 截断] + drill[61 瓦/23.56MB，d1-d6——61/61 键在 sweep 域内且逐键同字节，无 M-G 放大越域现象]；**baytown**-v1[1 树 81,268 瓦/768.32MB，sweep 字节 cap 1GB 触顶 d10 截断] + drill[61 瓦三目标/8.03MB]；**bridge-edit**（编辑大桥测试.bim——ASCII 目录名映射）-v1[**sweep 跳过**：d2 瓦 34/141MB 实证超 2GB 硬门——坑 25；imodel.json 主产物 + zoomToVolume 取景粗瓦 2 瓦/166.55MB] + drill[30 瓦/722.02MB，桥跨两端 d2→d6=46m 视域]；**坑 24**：bridge-edit 默认视图 0x99 指向空域（40m 视域不含几何——参考 DTA 同白屏零请求）；特征盘点：**textured 36 瓦首命中**（housemodel——materials/namedTextures/uvParams 三探针，纹理字节本体不在 imdl 瓦内）、**polylines/pointString 未消费形态大头**（baytown polylines 21,323 图元/19,507 瓦=24% + pointString 6,136；housemodel polylines 1,514；bridge compact 边缘 27/30 瓦命中[joeshouse 后首个 compact 命中模型]）、instances 214/105 瓦命中（已消费 TD-25）、曲线 94-100%、baytown changeset 非空（源 bim 自带）；**坑 16 确证**：三模型取景后 zoom 泵全部继续产请求——坑 16 = instances60 特异现象（工具 README M-K 跨模型判定在案）。②**M-K(2) 三入口接入**（本任务）：`dumpPackageForModel` 三映射 + Start 页 Models 分组五入口（bridge-edit 卡片标注中文名映射）+ MDI 标题映射；**引擎两补口（RED 取证→§11.8 移植缺口）**：①`Tile` 构造器包围球半径取 max(half-extent) 而非参考 Tile.ts:141 的**对角线全长一半**——球偏小 √3 倍 → FrustumPlanes cheap sphere test 把视锥远处大域瓦误剔 OutsideFrustum（housemodel 透视 saved 视图零请求 RED——[SEL] 新插桩实钉根瓦 r=118.4 被越距剔除 vs 参考 173.2 同场景 Partial；诊断开关 DANQING_SEL_TRACE 入 §13.1）；②**OrthographicViewState 1:1 移植**（SpatialViewState.ts:290-296——SpatialViewState 子类唯一覆写 supportsCamera(): false + CreateFromProps/Clone 类保持）+ convertViewStatePropsToViewState 类选择面（IModelConnection.ts:1553-1565 findClassFor 语义）——baytown 默认视图类 = BisCore:OrthographicViewDefinition，原类门只认 SpatialViewDefinition → load 失败 → 参考 ViewPicker.ts:39-44 catch 分支的空白视图（modelSelector 空 → 零树打开失败 RED）；③**坑 24 取景路径**：DumpOpenPackage.frameToWorldContent → LookAtVolume(逐树 contentRange × location 世界域并集, viewRect.aspect) + synchWithView（Viewport.ts:2316-2319 zoomToVolume 语义；saved 空域视图装载面原样、取景只改 viewport 面）。**三同构锁**（DumpOpenChain 增 3 项=6——判据按实际采集面裁剪）：**housemodel**（首个 cameraOn=true 透视用例：装载面 origin/extents/cameraOn/lens 1.0892 rad/focusDist/rotation 逐项 + viewport 面 eye==saved camera.eye 的 centerEyePoint 自洽机制确证）请求面恰 1 键 `-b-2-0-0-0-1`（root/d1 canSkip 穿透 maxInitial=3——M-H instances60 同形；树侧 root→d1→d2 对账）+ 像素 473,953 px（帧 16.93%）质心 (1011,680)；**baytown**（OrthographicViewState 首用例）请求面恰 1 键 `-b-1-0-0-0-1`（maxInitial=2 → d1 首请求）+ 像素 516,871 px（18.46%）质心 (869,301)——saved 取景中心≠厂区中心的数据属性（view center z 6.1 vs 厂区 22.5），y 绝对带 [150,550] 翻转侦测；**bridge-edit**（坑 24 取景路径）取景面 org/ext 与 README"应用后视域"**六轴吻合**（1366.37/899.01/1155.03 + 615.02/783.51/-622.44 vs README 1366.4/899.0/1155.0 + 615.0/783.5/-622.4——1 位小数舍入内）+ 请求面恰 1 键 `-b-3-0-0-0-1` **经 fallback drill 根跨根命中**（多根 fetcher 首个跨根实录；与采集 d2 面的分级差 = mpp=extents.x/窗宽 的 SSE 定量结果——d2 pixelSize 2334>maxSize 2048 TooCoarse@1263 窗[SEL] 实测，采集面 d2 Visible ⇒ 采集取景窗宽 ≤~1108 的 provenance nuance[README 已知限制坑 25 同域]）+ 像素 767,195 px（18.27%）质心对帧心偏移 (-11,-28)（取景居中语义 ±5% 强锚）。**可见性影响与浏览深度界（预期管理）**：baytown 工艺管线骨架（polylines 21,323）+ 仪表点要素（pointString 6,136）不渲染（TD-20/TD-23 遗留，TD-20 行更新）；housemodel 36 瓦纹理不贴（TD-20 遗留 textured 变体首命中）；bridge-edit 浏览深度界 = 全桥轮廓 → 桥跨两端 d6（46m 视域），未采位置/深度走参考同款父瓦 LOD 兜底（hasMissingTiles 语义——drill README"浏览深度界"）；housemodel/baytown sweep d≥10 截断（视口实测最深 d6/d7——sweep 域充分超集）。④**真实 app 冒烟**（桌面注入三轮，build/mk2-*/mk2b-*/mk2c-*/mk2d-*.png）：Start 页五卡片陈列 → House_Model 全程（初始帧 = 透视屋顶斜视角全屋[首个透视 dump 上屏]→缩放 2 级 LOD 细化）→ Baytown（初始帧 = iso 全厂[塔器/管线/管架/换热器——管线为 sweep 实体管，未消费的 polylines/pointString 是中心线/点要素层]→缩放 2 级到阀组细节）→ Bridge Edit（初始帧 = 全桥轮廓取景居中 + grid/ACS[坑 24 取景路径 live]；打开链 ms=357[DANQING_OPEN_TRACE 桩]；终端被用户并行会话抢占致缩放帧未采——初始帧与锁内证据已足）；Models 面板逐模型/瓦树实数据陈列（ProcessPhysicalModel/大体量模型测试/25_1d-E:6_0x48 等）；app-stderr 零 NotFound/零 SEH。取证教训登记：宽窗 FlowLayout 的两张卡片曾排出可视区外——坐标点击 miss（UIA Button 定位修复）；MDI Tab 坐标点击不可靠（UIA SelectionItemPattern 替代）。⑤**门禁（M-K(2) 收尾，Debug）**：**2555 项 = 2554 通过 + 27 跳过 + 1 环境态失败（View3DResize.MinimizeRestoreKeepsScreenAlive——"基线即黑"环境守卫族，隔离复跑 ×2 全绿，故障随满载桌面时序而非代码）**；目标面选择器 43/43 全绿（DumpOpenChain 6[增 3]/DumpBrowse 2/JoesHouse×3/PickDumpScene 3/RpcDump 6/TileTreeRender 10/ToolRegistryKeyin 10/ModelsPanel）。**阶段 2 M-M(1) 完成（2026-09-30，光照链归位——方向光缺失 saga 根治 + imdl 材质消费 + lights/sceneLights 解析）**：①**A/B 对拍仪器**（capture.mjs `--shot` 模式：DTA 侧四模型 saved-view 整窗 PNG + viewRect sidecar——ab-inst60/joeshouse/housemodel/baytown 四 dump；DanQing 侧=open-chain-*.bmp 帧转换对拍）。**对拍实锤初始缺口：DanQing 全模型表面≈纯 ambient 0.2×基色（instances60 球面 48/240、joeshouse meanL=64 平坦），DTA 球面=分面着色低多边形（maxL 104/spread 90）**——即"两个光照亮点"的机制=portrait 灯（视图空间常量 (-0.7071,0,0.7071)×0.3）+ solar 视图空间默认向 ((0.272166,0.680414,0.680414)×1.0) + 默认材质镜面（0.4 白/exponent 13.5，Material.default≡shader defaults (26265,65535,65535,13.5) 双保险）。②**根因链（§11.9 插桩逐层定位）**：GPU uniform 读回探针（[LIGHT-post]，DANQING_LIGHT_TRACE）证明 u_lightSettings/u_materialParams/u_sunDir 全部就位、片元级 debug 变体证明 mat_weights=(0.6,0.4)/dot(n,sun)=0.71 健康——**断点=u_sunDir 的 legacy 名值双写**：SceneCompositorImpl renderOpaque 的 `m_frameParams.setVec3("u_sunDir", m_target.getSunDir())` 在 `shader->draw`（ProgramUniform 正确视图空间值）之后经 `uploadUniforms` 把片元侧覆写为遗留世界向 (0.3,0.5,0.8)——**已拆除**（参考唯一路径=TargetUniforms.SunDirection；修复后 u_sunDir=(0.2722,0.6804,0.6804) 与 DTA 逐位一致，球面分面着色出现）。③**材质链归位**：`RenderMaterialInternal::defaultMaterial` 从错误值 (1.0,0,黑) 归位 Material.default（fromParams 1:1 参考 ctor：diffuse 0.6/specular 0.4/镜面白/exponent 13.5/rgba=-1 哨兵）；compositor uniform 上传加 wantMaterials 门（Surface.ts:206-220：viewFlags.materials && SmoothShade，atlas 落 Material.default）；`computeSurfaceFlags` 的 IgnoreMaterial 从"materialInfo 空"归位视图级门（SurfaceGeometry.ts:326-328）；**imdl surface 材质消费**（此前 materialFillColor 解析后无消费方）：TilesetJson 解析 renderMaterials 表全字段 + surface.material 两形态（串键/内联 SurfaceMaterialParams）→ resolveSurfaceMaterial → RenderMaterialParams → SurfaceGeometry::setMaterialInfo/getMaterialInfo（housemodel 0x40 实测：diffuse 0/specular 0/exponent 0.3861 权重全零材质上屏）。④**灯光 JSON 链**：styles.lights（新版）解析（DumpIModelConnection→ViewStateProps→ViewState.cpp setLights→applyOverrides3d）+ **styles.sceneLights 旧格式回退**（DisplayStyleSettings.ts:1109-1116——只取 sunDir，MicroStation 遗留强度全忽略；五模型 dump 实态均携带 sceneLights）；SunDirection.bind 补 frustum 重同步腿（参考 sync(uniforms.frustum,this)——此前世界向下相机转动滞留旧方向）。⑤**对拍结果**：housemodel 修复帧与 DTA 同 saved 透视光照结构高度吻合（暖墙面/屋檐阴影/屋面梯度）；instances60 球面分面着色上屏。**取证教训两条**：(a) **增量构建陈旧 obj 陷阱**——shader 头（LightingShaders.h 等）编辑后三次"重建"产出逐字节相同的错误帧（294038B×3）+ 探针缺失——MSVC 增量依赖未触发；行为未随源码变时先 `touch` 强制重编验证（本 saga 中段多个错误结论由此产生并复盘作废）；(b) **取证分析器的二进制基址**——node 侧 imdl 解析最初把 binary 基址 +8（BIN 块头误判）导致顶点表字节结论全错（7 轴顶点/伪布局），正确=v1 式 JSON 后 4 字节对齐直连（pad(e,4)）；GPU 侧 LUTDUMP 回读与 tile 逐字节对拍终审定音。**参考检出漂移登记**：D:/Github/itwinjs-core 现处 7e57d018 的后代 commit 88da8fb9（某并行会话前移），光照/材质/imdl 路径 `git diff 7e57d018..HEAD` 为空（零保真影响）。⑥**测试**：新增 MaterialTest（默认值/打包/缺省保持）+ ImdlMaterialTest（renderMaterials 表/两形态解析/缺席语义，值取自 housemodel 实测）+ SunDirection×2（视图空间默认/世界向 frustum 重同步）+ DisplayStyle3dSettings lights 应用/缺席（移植 DisplayStyle.test.ts lights 用例）；**JoesHouseEdge 黑边锁重钉**（1540→319：光照修复→面板分面亮度重排→±2 亮邻门在暗面失效；边链完好=噪声 30 的 10×/均值 36.9 黑/行列铺开，阈值 0.45×）+ SurfaceFlagsShaderTest IgnoreMaterial 期望 0（视图级门语义）。门禁：全量 2560 项除 2 项判据更新外全绿（27 跳过为既有环境跳过族）。**⑦实例球光照收口（用户对拍反馈：圆柱/圆锥已一致、60 球仍暗）**：RED 锁先行（紫行=实例 override 独有色 (135,0,135)，缺陷态 meanR=62.5/maxR=118 vs DTA 实测 103.7/209——均值/峰值双判据 80/150）→ **根因=实例变换旋转未进法线矩阵**（instances60 tile 实例矩阵实测=绕 X 90°：local +Z→world -Y；DanQing 的 CPU `u_normalMatrix` 只含 branch mv，而实例矩阵仅存于 GPU 逐实例属性——CPU 永远看不见）；**修复 1:1 参考语义**（Vertex.ts:162-166：`g_nmx = transpose(inverse(mat3(MAT_MV)))` **在 shader 内**从含实例矩阵的 g_mv 算——addNormal 增 instanced 形参，SurfaceVariantCompiler 传入；非实例路径保持 §3.4 CPU 上传；frustumScale 对角项省略=(1,1) 3D 世界分支 EQUIVALENCE 与 CPU 路径一致）→ 修复后紫行 **meanR=103.2/maxR=209 vs DTA 103.7/209（0.5%/0 精确对齐）**；位置链本就含实例矩阵（g_mv=u_instanced_modelView×g_modelMatrixRTC，SurfaceCommon.h）——法线链为唯一缺口。回归 33/33（DumpOpenChain/JoesHouse×3/PickDumpScene/RpcDump/TileTreeRender/DumpBrowse/ModelsPanel）。

架构详情：`docs/DanQing-图形引擎架构设计.md`
C++ 规范：`docs/DanQing-C++代码规范.md`（从属格式参考）

---

## 15. 阶段 4 M-O 指令（2026-10-01 用户指令，新会话首个里程碑）

**双向任务**：①补充实现 DTA 中**全部**功能；②删除 DTA 中没有的功能。权威工作面 =
`docs/DTA功能对照与专业化分析-2026-09-30.md`（85 行矩阵 §2 + 清理面三档 §3——逐项带
参考源锚与裁决）。

**实现侧现状基线**（M-R 收口，2026-10-06——权威账在
`docs/阶段4-MR-DTA布局对齐-实现计划-2026-10-06.md`）：
- **大件 2/16 清偿**（Sectioning M-P + Rendering Style M-Q——详账见各自计划文档）。
- **UI 布局对齐（M-R，2026-10-06）**：DTA 双工具栏换位[app 5 项/主 26 项严格序]
  + 焦点换位 + 下拉交互合同[单开互斥/only3d 显隐] + 顶状态条[keyin/FPS/tile/snap]
  + 底部 showstatus/showerror 双 span；Measure/Walk/Open-disk 按钮激活（引擎既在）。
  **UI 层与 DTA 布局/交互逻辑一致；平台壳差异（菜单栏/Start 页/MDI）按用户裁决保留。**
  **余量 = 大件 14**（M-S 起续排）。P-A dqGeom 几何三件
  （ClipVector/ClipPrimitive·ClipShape/UnionOfConvexClipPlaneSets + ClipUtilsLoops）
  → P-B ViewState clipVector 存储 + clipStyle 门面 → P-C RenderPlan→ClipStack/
  ClipVolume 参考语义重写 → P-D shader 片元裁剪全链（RHI 三修复）→ P-E ViewClip
  五工具族 → P-F ViewClipDecoration+EditManipulator（引擎顺带修复：SelectionSet
  变更门/computeViewRange/ComputeFitRange ref union）→ P-G SectionsPanel 宿主 UI
  + 工具入口 E2E（权威详账见 M-P 计划文档完成实录）。
- **➖9 项已结案**（用户 2026-10-01 确认：Hub/OIDC/Push·Pull/编辑写入族/
  CreateSectionDrawing/Reality data 网络瓦/地图 provider/GenerateTileContent/
  InvokeFrontendIpc——登记架构外不做引擎实现，后续里程碑不再排期）。
- 执行纪律不变：§0 来源铁律 + §11.8（每项先完整读参考源码再动手）+ §5 RED 锁先行 +
  §6 五维一致性对照；大件逐里程碑提交。
- **➖9 项已结案**（用户 2026-10-01 确认：Hub/OIDC/Push·Pull/编辑写入族/
  CreateSectionDrawing/Reality data 网络瓦/地图 provider/GenerateTileContent/
  InvokeFrontendIpc——登记架构外不做引擎实现，后续里程碑不再排期）。
- 执行纪律不变：§0 来源铁律 + §11.8（每项先完整读参考源码再动手）+ §5 RED 锁先行 +
  §6 五维一致性对照；大件逐里程碑提交。

**删除侧现状基线**（M-O(1) 核销账重盘，2026-10-01——权威账在
`docs/阶段4-MO-…-实现计划-2026-10-01.md` 核销账节）：
- M-L(2) 已超预期清完：菜单树 4 顶级菜单/13 存根命令全删/命令注册 24——§3.4 的
  "中风险段"（菜单裁剪+"可点无效"命令）**已清**。M-O(1) 收尾余量 D1-D5 亦清：
  qrc+icons 死图标 161→74、sequencerBar/notificationArea 死 chrome、悬空 sPixmap
  裁决零改动（VERBATIM 元数据）。**删除侧至此无已知余量**。
- B 档（FreeCAD 壳骨架/标准视图/窗口域）与 C 档（测试基建/qss）**不动**
  （§3.2/§3.3 裁决在案——"去 FreeCAD 功能"非"去壳"，用户 2026-09-28 指令）。
- 删除执行纪律：每删一项同步改/删对应测试断言；全量门禁零失败为收口门。

---

## 2. 参考实现定位

DanQing 是参考实现（reference implementation），不是原创设计。所有代码、测试、接口必须严格对齐参考项目，确保行为兼容性。

| 参考项目 | 语言 | 角色与覆盖范围 |
|---------|------|---------|
| **itwinjs-core** | TypeScript | **渲染栈主参考**：渲染管线、前端 API、Tile 系统、glTF |
| **filament** | C++ | RHI 驱动、OpenGL 后端、**平台层（Windows WGL 平台层的权威参考——PlatformWGL.cpp 结构：swapchain 自持 DC、dummy 承载窗口、像素格式匹配）**、高质量实时渲染与全平台后端抽象 |
| **imodel-native** | C++ | dqGeom 几何参考 |
| **FreeCAD** | C++ | 仅测试宿主（samples/DisplayTestApp）Gui 命令框架参考；Start 页已去 FreeCAD 无关功能（2026-09-28 用户指令——New/Open File、recent-files、FirstStart 向导、theme/general settings 等 FreeCAD-only 件全清，M-H(4)） |

参考项目为仓外检出，只读参考，不是本仓库的一部分。
权重变化只影响未来开发优先级与模块规划；§0 来源铁律、§5 测试优先级（imodel-native > itwinjs-core）等规则原文不变。

---

## 3. 命名对齐规则 `[P0 硬约束]`

### 3.1 总原则

> **所有标识符（文件名、类名、枚举名、枚举值、变量名、方法名、函数名）1:1 对齐参考项目。方法/函数名按参考语言原样保留（TS→camelCase；C++ 参考→PascalCase），不归一。**

可追溯性由每符号 `// Ported from:` 注释保证，不依赖方法名拼写一致。

### 3.2 命名主决策表

| 标识符 | 规则 | TS 参考（itwinjs） | C++ 结果 |
|---|---|---|---|
| 文件名 | 1:1 对齐参考类/文件 | `FrustumUniforms.ts` | `FrustumUniforms.h/.cpp` |
| 类/struct 名 | 1:1（PascalCase） | `BranchUniforms` | `BranchUniforms` |
| 枚举类型名 | 1:1 | `FrustumUniformType` | `FrustumUniformType` |
| 枚举值 | 1:1（PascalCase） | `TwoDee`/`Orthographic` | `TwoDee`/`Orthographic` |
| **方法名** | **1:1（TS→camelCase）** | `changeFrustum` | `changeFrustum` |
| **自由函数名** | **1:1（TS→camelCase）** | `normalizedDifference` | `normalizedDifference` |
| 局部变量 | 1:1（camelCase） | `viewX`/`cameraPosition` | `viewX`/`cameraPosition` |
| 参数 | 1:1（camelCase） | `isViewCoords` | `isViewCoords` |
| 成员变量 | `m_` + 参考基名（camelCase） | `_viewClipEnabled` | `m_viewClipEnabled` |
| 静态成员 | `s_` + camelCase | — | `s_defaultTimeout` |
| 常量 | `k` + PascalCase | — | `kMaxTileDepth` |
| 命名空间 | 模块名 | — | `dqRender` |
| 宏 | UPPER_SNAKE + 模块前缀 | — | `DQ_RENDER_EXPORT` |

> 方法/函数名按参考语言原样：TS 移植→camelCase（`changeFrustum`、`bindProjectionMatrix`），C++ 参考（imodel-native/filament）→PascalCase。两种参考都 1:1，无归一、无碰撞负担。

**如何验证：** clang-tidy 命名检查；code review 比对参考标识符；`// Ported from:` 注释覆盖率。

### 3.3 方法/函数名对齐规则 `[P1 强规则]`

- 方法名/函数名 1:1 按参考语言原样保留，**不归一**：TS 移植→camelCase（`changeFrustum`、`bindProjectionMatrix`、`normalizedDifference`）；C++ 参考（imodel-native/filament）→PascalCase。
- 这与结构性标识符（类/枚举/变量）的 1:1 原则一致；不因 C++ 规范对 TS 方法做 PascalCase 归一（实测会产生大量类型名碰撞 + 跨模块 virtual 不一致，见 §12.6）。
- TS `get xxx` 属性 → C++ 访问器保留参考命名（camelCase）；查找型访问器的动词约定见 `docs/DanQing-C++代码规范.md` §3.4。

### 3.4 C++ 强制偏差表（允许的适配） `[P1 强规则]`

移植时这些偏差是 C++ 必要的，必须按下表映射，不得自创：

| 参考构造 | C++ 适配 |
|---|---|
| TS `_field` 私有 | `m_field`（前缀 `_`→`m_`，基名 1:1） |
| TS GC 引用 / `extends RefCounted` | `RefCounted<T>` CRTP + `RefPtr<T>`（禁 `shared_ptr`） |
| TS `const enum`/union/string-literal | `enum class Name : <type>`（显式底层类型） |
| TS 模块路径 | `dqXxx` 命名空间（单数 PascalCase） |
| TS `T \| undefined` | `std::optional<T>` 或 `RefPtr<T>`(null) |
| TS `throw E` | `Result<T,E>` 返回（核心引擎禁异常） |
| TS `interface`/abstract class | 纯虚 `IXxx`（I 前缀，规范 §3.2） |
| TS `as X` / 类型断言 | `static_cast<X>(...)`（禁 C 风格） |
| TS `class Foo<T>` | `template<typename T> class Foo` |
| TS `get width()` 只读属性 | `Width() const noexcept`（无 Get 前缀） |
| 参考原名与同作用域已有类型/符号冲突 | 保留最小区分后缀（如 `Projection`）+ `// Ported from:` 记录参考原名 | 碰撞例外，如 TS `frustum()`→`frustumProjection()` 避让 `Frustum` 类型 |

### 3.5 禁止 `[P0 硬约束]`

- 自创参考中不存在的名字（重命名 `FrustumUniforms`、加模块前缀 `RenderFrustumUniforms`）。
- 缩写/简写参考标识符（`BindProjMat`）。
- 翻译成中文命名；必须保留参考英文标识符。
- 方法/函数名脱离参考语言自创大小写（TS 移植改成 PascalCase，或 C++ 参考改成 camelCase）。
- 成员变量用 `m`+PascalCase（`mProjection`）或无前缀；必须 `m_`+camelCase。

---

## 4. 代码溯源（Code Provenance） `[P0 硬约束]`

每个源文件必须有明确出处，可追溯到参考项目之一。

- 每个文件顶部标注：
  ```cpp
  // Ported from: <project> <relative-path-to-source-file>
  ```
- 出处必须指向具体源文件，不得使用模糊描述。
- 无出处的代码视为未验证实现，不得合入主干。

**如何验证：** CI 检查每个 `.h`/`.cpp` 顶部含 `// Ported from:` 或 `// Authored:` 行；缺失则构建失败。

---

## 5. 测试保真（Test Fidelity） `[P0 硬约束]`

测试用例是行为兼容性的契约。所有 `TEST()` 必须从参考项目移植，不得臆造。

**规则：**

(a) **来源优先级**：
1. `imodel-native/iModelCore/<模块>/Tests/NonPublished/*.cpp`（C++，直接移植）
2. `itwinjs-core/core/<包>/src/test/*.test.ts`（TS，翻译为 C++/GoogleTest）

(b) **可追溯**：每个 `TEST()` 顶部必须注释出处：
```cpp
// Ported from: <project> <test-file-path>
//              TEST(SuiteName, CaseName)
TEST(MySuite, MyCase) { ... }
```

(c) **不发明场景**：测试场景、边界条件、断言值全部来自参考实现。DanQing 类型与参考不同时，仅调整断言适配类型，场景不变。

(d) **RED-GREEN**：参考测试揭示缺失 API 时，先移植测试（RED），再补实现（GREEN）。

(e) **TS→C++ 映射**：`describe/it` → `TEST(Suite,Case)`；`expect().to.throw(E)` → `EXPECT_THROW(fn,E)`；`@ts-expect-error` 块忽略。

(f) **例外**：仅当 imodel-native 与 itwinjs-core 均无对应测试时方可自写，必须标注 `// Authored: no reference test exists in <project> for <X>`。

(g) **渲染/窗口行为的回归测试授权**：窗口系统/呈现层行为（resize、最大化、swapchain 生命周期）在参考项目（浏览器 WebGL）中不存在对应测试——此类场景允许按 §12.9 的取证结论自写**像素级**回归（readPixels 断言内容存活，不是只断言"不崩"），标注 `// Authored:` 并在注释中记录复现配方与证据链。此类回归还须满足 §11.11 的判据有效性要求：至少一个**位置断言**（内容在哪个象限/哪侧/朝向），需要不对称标记时**新建专用测试资产**、禁止原地突变既有资产。

**如何验证：** CI grep 每个 `TEST(` 上方 5 行内含 `Ported from` 或 `Authored` 注释；CI 统计 `// Authored:` 占比并报警（异常增长→复查是否绕过移植）。

---

## 6. 一致性验证（Conformance Verification） `[P1 强规则]`

每完成一个开发任务，必须对照参考项目进行一致性验证。

| 维度 | 验证内容 |
|------|---------|
| 接口签名 | 方法名、参数类型、返回类型与参考一致 |
| 类型命名 | 遵循 §3 命名对齐规则 |
| 数值精度 | 容差常量、默认值与参考精确值一致 |
| 行为语义 | 算法流程、分支逻辑、错误处理与参考一致 |
| 调用链接线 | 参考侧调用者/数据来源 → DanQing 侧调用者逐条对照（**ported-but-uncalled = 移植未完成**，见 §11.10） |
| 测试覆盖 | 移植的测试全部通过 |

**如何验证：** 每个任务 PR 附五维度一致性对照表（checklist 勾选）；执行时机为每个任务完成后、提交前。

---

## 7. 禁止事项（Prohibited Practices） `[P0 硬约束]`

| 类别 | 禁止内容 |
|------|---------|
| 算法 | 自行设计排序、查找、几何运算等算法 |
| 接口 | 自行定义方法名、参数、返回值 |
| 数据结构 | 自行设计成员变量、内存布局 |
| 常量 | 自行设定容差、默认值、阈值 |
| 测试 | 自行构造测试场景、边界条件、断言值 |
| 架构 | 自行引入设计模式、抽象层次、扩展点 |

若参考项目未覆盖某个功能，标记为待实现（TODO + 参考来源说明），而非自行补全。

**如何验证：** code review 强制核对；clang-tidy 规则覆盖可机械检查项。

---

## 8. 架构约束（硬约束） `[P0 硬约束]`

### 8.1 依赖方向

```
dqApp → { dqRender, dqCommon, dqGeom }
dqRender → { dqGeom, dqCommon } → dqBase
dqCommon → { dqBase, dqGeom }
dqGeom → dqBase
```

- 依赖只许向下，永不反向
- **dqCommon ⊥ dqRender/dqApp**：共享类型不知道上层
- **dqRender → dqGeom** 是唯一允许的引擎间边
- 被禁止的横向依赖或反向依赖是 P0 错误

### 8.2 SDK 独立性与零网络协议 `[P0 硬约束]`

引擎模块（dqBase、dqGeom、dqCommon、dqRender）必须作为独立 SDK 提供给 CAD 应用开发者，**不得依赖 Qt**。

**零网络协议（2026-09-27 用户指令）**：DanQing 全模块（含 dqApp/samples）**不得有任何通过 RPC 或 HTTP 网络协议请求数据的操作**。DanQing 替换的是 itwinjs-core 中**请求获取数据之后**的加载、解析、渲染部分；请求获取（RPC/HTTP/fetch）归宿主层（生产=TS 宿主，测试=本地文件）。数据经 `ITileFetcher`/`ITileTreePropsProvider` 类 DI 缝隙以字节进入引擎。存量 `QtTileRequestFetcher`（dqApp HTTP 拉瓦）已随 TD-24 清退（f1ae49f）。

| 模块 | Qt 依赖 | 说明 |
|------|---------|------|
| **dqBase** | **零** | 所有 Qt 类型由 thin wrapper 替代（DqVector/DqString/DqMutex 等） |
| **dqGeom** | **零** | 容器用 std::vector，字符串用 std::string |
| **dqCommon** | **零** | Export.h 用平台原生宏 |
| **dqRender** | 公开 API **零**；src/ 内部允许 | TileRequester 用依赖注入接口，GL context 用原生 API；**窗口系统交互只经原生句柄（void* nativeWindow），窗口生命周期归应用层——WGL/平台层不得链接/调用 Qt（2026-09-14 用户指令：渲染与相机视口操作全走图形 API，不绑死 Qt，未来可换其他图形 API）** |
| **dqApp** | **允许** | 应用层，不是 SDK，可以使用 Qt（Qt ↔ GL 的唯一桥接点是 winId()/WinIdChange → `Swapchain::rebind`） |

dqBase 的 thin wrapper 类型对齐 imodel-native 的 bvector/Utf8String 模式：
- `DqVector<T>` = `std::vector<T>`（DqTypes.h）
- `DqString` = `std::string`（DqTypes.h）
- `DqMutex` = `std::mutex`（DqSync.h）
- `DqGuid` = 自定义 128-bit GUID（DqGuid.h）

### 8.3 第三方库集成

参考项目的第三方库直接集成到 dqBase 源码中（不作为外部依赖）：

| 库 | 来源 | 许可证 | 位置 |
|---|------|-------|------|
| **btree** | Google cpp-btree | Apache 2.0 | `dqBase/PublicAPI/dqBase/btree/` |
| **bpool** | Boost.Pool | BSL 1.0 | `dqBase/PublicAPI/dqBase/bpool.h` |
| **stb_image** | nothings/stb | Public Domain / MIT | `third_party/stb_image/` |
| **cgltf** | jkuhlmann/cgltf | MIT | `third_party/cgltf/` |

不得用 std::map/std::set 替代 btree，不得用 std::malloc 替代 bpool——这些是性能关键路径，不是风格偏好。

> **`std::unordered_*` 的处理（§0 逐文件参考保真）**：本条只禁 `std::map`/`std::set`（有序）。`std::unordered_map`/`std::unordered_set` 的去留由**被移植文件的参考源**决定，而非 imodel-native 的容器风格：移植自 itwinjs-core 且参考用 TS `Map<K,V>`/`Set<V>`（哈希 + 插入序）的文件，`std::unordered_*` 是**忠实**实现，保留；移植自 imodel-native C++（`bmap`/`bset`）的文件用 `bmap`/`bset`。即"imodel-native 无哈希容器"不等于"TS-ref 文件里的 unordered 是偏差"。例外（技术必要，登记 TD-9/TD-10）：imodel-native `bpool.h` 本身用 `std::set<void*>`（1:1 保留）；`LRUTileList` 用 `std::map<Tile*,LRUNode>`（mapped-value 地址稳定性，`bmap` 不可替代）。

> **cgltf（GltfReader 批准偏差，架构师签字 2026-07-10，audit F2b）**：`GltfReader.h` 用 cgltf（jkuhlmann/cgltf，C99 glTF 2.0 spec 解析器，MIT，`third_party/cgltf/`）替代逐行移植 itwinjs `GltfReader.ts`（2975 行）的 Bentley glTF 栈。理由：glTF 2.0 是 Khronos 开放标准（即真正"参考"），`GltfReader.ts` 仅是其一种实现；cgltf 是成熟 spec-conformant 解析器，行为等价、无 Bentley 专有算法/数据结构。登记为**批准的第三方解析器替换**（非 §0 违规）；`GltfReader.h` 仍引 §0 参考 + 标注 APPROVED DEVIATION，mesh 几何提取（→ `IndexedPolyface`）行为对齐参考。

> **stb_image（GltfReader baseColor 纹理解码，批准偏差，架构师签字 2026-09-15）**：dqCommon/dqRender 公开 API 零 Qt（§8.2），glTF `baseColorTexture` 的 PNG/JPEG 解码用 stb_image（nothings/stb，单头，PD/MIT）替代自写解码器（§7 禁自创算法）。图像解码是成熟开放标准，stb_image 是广泛使用的 spec-conformant 解码器，无 Bentley 专有算法。登记为批准的第三方解码器替换（非 §0 违规）。

### 8.4 SDK 边界

- 公开 API 在 `PublicAPI/<模块名>/`，唯一可被跨模块 `#include` 的目录
- 实现细节在 `src/`，绝不暴露到 `PublicAPI/`
- 跨模块通信只通过 SDK 接口（抽象基类、回调、句柄）

**如何验证（§8 全节）：** (a) include 依赖图工具检查无反向/横向依赖；(b) 引擎模块公开头 `grep -rE 'QString|QVector|QHash|QMap|<Q' PublicAPI/` 不得命中 Qt 符号。

---

## 9. C++ 编码规范纲要 `[P0/P1 混合]`

完整格式细节见 `docs/DanQing-C++代码规范.md`（从属格式参考，不得与本节矛盾）。

- **C++20**（对齐 imodel-native），编译标志 `-std=c++20 -Wall -Wextra -Werror -fno-exceptions -fno-rtti`
- **命名**：严格遵循 §3（类型 PascalCase；方法/函数 1:1 对齐参考；前缀 `m_` 成员 / `s_` 静态 / `k` 常量 / `I` 纯虚接口）
- **禁止（硬约束，DanQing 架构选择，与参考一致）**：异常、RTTI、裸指针、`shared_ptr`、`NULL`
- **格式偏好（从属于 §0/§3 1:1 参考对齐）**：`typedef`→`using`、C 风格转换→`static_cast`、运算符重载——**仅当参考项目（imodel-native/filament C++）未使用时对新代码强制**；参考已用则 1:1 保留（证据见 §12.7、§14 TD-7）
- **允许**：STL 容器头（公开头）—— 对齐 imodel-native Bentley/PublicAPI 和 filament include/ 的实际做法
- **必须**：`enum class` + 显式底层类型、`override`/`final`、`noexcept`、`explicit`、`#pragma once`、`using` 别名
- **所有权**：`RefCounted<T>` CRTP + `RefPtr<T>`（禁 `shared_ptr`）
- **错误**：`Result<T,E>`（核心引擎禁 `throw`；仅模块边界捕获第三方异常转 `Result`）
- **公开头文件**：Qt-free（见 §8.2）；字符串/集合用 `std::string`/`std::vector` 或 dqBase thin wrapper（`DqString`/`DqVector`）

---

## 10. 渲染架构

DanQing 的渲染管线严格对齐 itwinjs-core 的 `core/frontend/src/render/` 架构；Windows 平台层（WGL）严格对齐 filament 的 `PlatformWGL.cpp`。

### 10.1 核心组件

| 组件 | itwinjs-core | DanQing | 说明 |
|------|-------------|-------|------|
| RenderSystem | `RenderSystem.ts` | `dqRender/RenderSystem.h` | 全局单例，工厂模式 |
| RenderTarget | `RenderTarget.ts` | `dqRender/RenderTarget.h` | 声明式接口 |
| Scene | `Scene.ts` | `dqRender/Scene.h` | foreground/background/overlay |
| Decorations | `Decorations.ts` | `dqRender/Decorations.h` | 装饰器图形容器 |
| DecorateContext | `ViewContext.ts` | `dqApp/DecorateContext.h` | 装饰器收集上下文 |
| ChangeFlags | `ChangeFlags.ts` | `dqApp/ChangeFlags.h` | 变更标志位掩码 |
| GraphicBranch | `GraphicBranch.ts` | `dqRender/GraphicBranch.h` | 场景图节点 |
| RenderGraphicOwner | `RenderGraphic.ts` | `dqRender/RenderGraphic.h` | 防止自动释放 |

### 10.2 渲染管线

`Viewport::renderFrame()` 对齐 itwinjs-core 的 **24 步**管线（`Viewport.ts:2546-2703`，源码逐行核对）：

```
1.  帧统计开始 (beginFrame)           13. Feature Symbology Overrides
2.  ChangeFlags 快照与重置            14. 场景创建 (createScene→changeScene) ← 最重
3.  缓存 view/target                 15. 渲染计划验证 (validateRenderPlan)
4.  StopWatch 计时                    16. 装饰收集 (addDecorations→changeDecorations)
5.  动画执行 (animate)                17. Flash 处理 (processFlash→setFlashed)
6.  isRedrawNeeded 初始化             18. Pre-render hook (onBeforeRender)
7.  尺寸变化检测 (updateViewRect)     19. 计时结束
8.  控制器同步 (setupFromView,条件)   20. 实际绘制 (drawFrame, 仅 isRedrawNeeded) ← GPU 提交
9.  选择集更新 (setHiliteSet)         21. 帧统计结束 (endFrame)
10. overridesNeeded 计算              22. 尺寸事件 (onResized)
11. 分析分数 (setAnalysisFraction)    23. 变更事件分发 (onViewportChanged 族)
12. 时间点 / ScheduleScript           24. 持续渲染请求 (requestNextAnimation)
```

关键约束（详见分析文档 §3）：
- **isRedrawNeeded=false 时跳过 GPU 提交**（置位来源共 10 处）
- **步骤 24 条件不含 missing tiles**——瓦片重绘走 `invalidateScene()` 级联
- **四级失效级联**：invalidateController → invalidateRenderPlan → invalidateScene → invalidateDecorations（每级主动 requestNextAnimation）
- 步骤 12 的 `containsTransform` 会**本帧内** invalidateScene（变换动画每帧重建场景）

### 10.3 SceneCompositor 多 Pass 渲染

```
SceneCompositor::Draw(commands)                    // SceneCompositor.ts:1410-1519
  ├─ ClearOpaque()                                 // 3-MRT 清屏（color/featureId/depthAndOrder）
  ├─ RenderBackground() / RenderSkyBox() / RenderBackgroundMap()
  ├─ pushViewClip()
  ├─ RenderVolumeClassification()
  ├─ RenderLayers(OpaqueLayers)
  ├─ onRenderOpaque 事件                            // GPU 厂商扩展点
  ├─ RenderPointClouds()                            // 可选 EDL
  ├─ RenderOpaque()    → DrawPass(OpaqueLinear/Planar 写 pick; OpaqueGeneral 不写)
  ├─ RenderLayers(TranslucentLayers)
  ├─ IF needComposite:                             // CompositeFlags=None 时整段跳过
  │    ClearTranslucent() → RenderTranslucent()    // WBOIT 加权混合 OIT（无需排序）
  │    → RenderHilite() → Composite()              // 7 变体全屏合成
  └─ RenderLayers(OverlayLayers) + popViewClip()
```

> 渲染管线全流程、WebGL 后端内部实现（DrawCommand 构建/着色器变体/Uniform 与 GL 状态/几何与 Imdl 解码/Tile 系统/拾取路径）与性能设计原理详见 `docs/itwinjs-core-渲染系统执行流程分析.md`。

### 10.4 Windows 呈现层（WGL，filament 对齐） `[P1 强规则]`

Windows 呈现链路的结构规则（每条都有真实事故背书，案例 §12.9）：

- **结构对齐 filament PlatformWGL**：swapchain（WglSwapChain）自持 DC（create 时 GetDC 一次/destroy 时 ReleaseDC）；dummy 承载窗口创建 GL 上下文；所有窗口共用同一 `m_pfd`（HDC 与 HGLRC 像素格式必须匹配）；`destroySwapChain` 只销毁 headless 自建窗口（**永不 DestroyWindow 应用窗口**）。
- **resize = 表面过期**：GL 子窗口表面 extent 变化（最大化/还原/拖拽）必须原地重建平台交换链（Vulkan `VK_ERROR_OUT_OF_DATE_KHR` 的 WGL 对应物）。"OpenGL 无需重建交换链"是错误假设——SwapBuffers 可逐帧成功而屏幕冻结旧帧。
- **窗口句柄生命周期归应用层**：Qt 在状态跃迁时可销毁重建原生子窗口，**Windows 会回收复用同一 HWND 地址**——`rebind` 不得做句柄相等短路；WinIdChange 即"旧表面已死"语义，无条件重建。
- **句柄失效必须显式传播**：销毁 GL 对象时必须同步失效所有缓存它的层（编译状态/句柄成员/状态跟踪器）；销毁后继续使用 = use-after-destroy，且下游静默跳过（如 `useProgram` 的 `if (prog && prog->isValid())`）会把故障放大成错误输出而非报错。
- **可见性跃迁（最小化/恢复）分两层分锅：表面层 ≠ 触发层**（2026-09-14 最小化黑屏 saga）：屏幕黑 ≠ 表面坏——先用 present 计数器 + "窗口完全可见后显式帧能否上屏"区分。两个陷阱：①恢复窗口期内的**第一帧会 present 进虚空**（原生窗口尚未完成映射，Win11 恢复动画期内），`m_redrawPending` 已消费则再无第二帧；②`QEvent::Paint` 是**单次触发语义**（Qt validate 后不再补发 WM_PAINT），唯一一次 Paint 也可能落在映射前。修复范式 = Show→rebind（表面层）+ Paint→RequestRedraw（触发层）+ Show 时延迟补帧（150ms/400ms，有界事件驱动，保证至少一帧落在完全可见之后）。

---

## 11. 工作流程

1. **先完整阅读参考项目源码**，建立完整的功能清单（每个文件、每个类、每个函数）
2. 对照清单逐项移植，不跳过任何一项
3. 确认模块归属，是否违反 §8 依赖方向
4. 看现有代码，匹配周围风格（命名遵循 §3）
5. 先写测试（来自参考，§5），再写实现
6. 完成后对照 §6 一致性验证
7. 文档反映实际状态，不是理想状态
8. **修 bug 同样适用第 1 条** `[P1 强规则]`：任何行为修复前，先定位并**完整阅读**参考项目的对应实现（不是只查行号），确认参考在该场景下的真实机制后再改。参考没有该机制时（如某 GL 适配细节），先验证差异是否源于移植缺口（补齐移植），而不是发明替代方案。症状推理（"大概是 GPU 同步"→延迟销毁/"重建太频繁"→防抖）是自创实现的入口，禁止（案例见 §12.8）。
9. **调试取证纪律** `[P1 强规则]`（案例 §12.9）：
   - **先要精确复现配方**：用户的操作序列（从哪个初始状态、哪些开关、什么动作顺序）是唯一 ground truth；拿到配方前不做复现尝试——初始状态差异（如 Grid 默认关闭）会让数小时的自主探索全部无效。
   - **证据来自插桩轨迹，不来自假设**：在层间边界（窗口事件/swapchain 生命周期/GL 状态/FBO 回读）放 env 门控探针，让轨迹说话；轨迹与假设矛盾时信轨迹。
   - **症状重叠 = 可能多根因**：修好一条根因后症状仍在，不是"修复无效"，是还有一条——每次修复后必须复核全部证据是否闭环。
   - **渲染/视觉行为改动，像素级回归先行**：先建 readPixels 断言（内容存活/基准帧对比），再动代码；"不崩"不是"画对了"。
   - **真实窗口取证三件套**：①每击前台守卫（`GetForegroundWindow` == 目标）+ 按钮 `WM_NCHITTEST` 实时探测，不满足即中止，绝不盲点；②坐标不推算——裁剪目标区域截图亲眼量，点击后再截图确认；③取证工具先自测（stderr 重定向是全缓冲、.cmd 必须纯 ASCII、env 门控要验证真的生效）。

10. **移植完整性以调用链为单位** `[P1 强规则]`（案例 §12.10）：
    - "完成"的度量单位是**调用链**，不是类/函数：移植任何组件时，先在参考侧检索它的全部**调用者与数据喂入者**，DanQing 侧必须有等价接线。**组件忠实但无人调用（ported-but-uncalled）= 移植未完成**——忠实的组件放在断裂的链上平时不可见，直到角案（符号/纵横比/时序/坐标系）引爆。
    - 上游缺数据时**禁止从相邻数据自创旁路合成**（症状：两实现"看起来等价"却在某维度反号）——必须补齐参考的上游链（调用者/喂入者），这与第 8 条同源：旁路是自创实现的入口。
    - **等价替换登记制**（唯一例外通道）：确需用相邻数据合成参考从别处取得的值时，必须 (a) 注释 `EQUIVALENCE: 参考源=<project path:line>；发散=<清单 | 未发现，验证法=...>`；(b) 每条已知/潜在发散配一个能抓住它的回归测试。
    - **等价性是全定义域命题**：宣称等价前枚举输出的全部维度（数值、符号、边界、时序、坐标系），逐维度验证；抽查常见路径不算验证。

11. **判据与仪器的有效性纪律** `[P1 强规则]`（案例 §12.9/§12.10）：
    - **位置断言制度**：视觉特性的回归至少一个 WHERE 断言（内容在哪个象限/哪侧/朝向），断言信息量 ≥ 失败模式自由度——面交换类缺陷有一个自由度，断言就必须钉住一个朝向标记（几何缺口/纹理色点）。需要不对称标记时新建专用测试资产（BoxTexturedDots 模式），**禁止原地突变既有资产**（突变令历史测量不可复现）。
    - **仪器自检**：新取证工具首次使用前用已知答案校准（合成图过同一写入器 / 已知状态的程序过同一探针）；输出一律**绝对路径**，读回前验证文件时间戳；env 门控验证真的生效；机器视觉/模型的方位结论（左右/上下）必须**字节级复核**后才可采信。
    - **测前重 dump**：每次测量前重新确认实验对象状态——资产内容（哈希）、exe 构建时间（库改动后 app 必须重建）、双实现加载的是同一文件；上一轮的 dump 不代表这一轮。
    - **A/B 对比协议**：双实现对比必须同输入（打印哈希）、同视图/状态、干净初始态，缺一即结论无效；合成驱动（CDP/模拟点击）建立的状态与用户真实流程不等价时，其观测只作线索不作结论；破坏用户会话状态的动作（reload/重启用户的应用）先征得同意。

12. **多根因排除与交付前全扫** `[P1 强规则]`（案例 §12.8/§12.9/§12.10）：
    - 每次修复后执行**全量复扫**：全部标准视图 × 双实现矩阵 + 复核全部既有证据闭环；症状消失 ≠ 根因清零。
    - "修复无效"的第一解释是**还有一条根因**，不是"修复错了"——除非复扫证据直接推翻修复本身；禁止在未复核证据前回滚或叠加新补丁（§12.8 三层补丁全回滚的原案）。
    - 交付用户验证前：全量重建所有受影响目标并核对 exe 时间戳——"库改了没重建 app"会制造假"修复无效"。

---

## 12. 经验教训（反模式）

### 12.1 不要先设计再对照，要先读再写
**错误**：先设计 API，再对照参考项目检查遗漏。
**正确**：先完整阅读参考项目全部源码，建立功能清单，再逐项移植。
**教训**：dqBase 第一版只有 22 个头文件（覆盖率 27%），因为没有先读参考项目就动手写代码。

### 12.2 不要用"优先级"代替"完整性"
**错误**：把功能分为高/中/低优先级，低优先级的推迟或跳过。
**正确**：参考项目做了什么，就做什么。没有"可以不实现"的选项。
**教训**：btree/bpool 被标记为"不实现"，实际上是性能关键路径和公开 API 的一部分。

### 12.3 不要自己判断"够不够用"
**错误**：认为 std::map 可以替代 btree，std::malloc 可以替代 bpool。
**正确**：参考项目选择 btree/bpool 有其原因，不要用自己的判断替代参考项目的判断。
**教训**：btree 比 std::map 快 3-10x，bpool 是固定大小块分配器——这些不是"风格偏好"，是架构决策。

### 12.4 不要假设，要验证
**错误**：假设 Qt 是稳定的跨模块 ABI 就直接植入。
**正确**：从 §8 架构约束推导——引擎模块必须独立，因此不能依赖 Qt。
**教训**：Qt-first 设计导致整个 dqBase 需要重写。

### 12.5 参考项目的代码是"规范"，不是"参考"
**错误**：把参考项目当作"可以借鉴的资源"。
**正确**：DanQing 是参考实现（reference implementation），参考项目的代码就是规范。
**教训**：§0 已经明确写了这个原则，但执行时没有严格遵守。

### 12.6 方法名归一不可行——让规则让步于参考一致性
**错误**：要求 TS 移植的方法名统一 PascalCase（"统一 C++ 风格"），用 clang-tidy 批量改写。
**正确**：方法名 1:1 按参考语言（TS→camelCase），与结构性标识符一致；规则反转，TD-1 消解。
**教训**：PascalCase 方法归一在本代码库不可行——众多 getter 以类型命名（`shaderLanguage()`→`ShaderLanguage()` 与枚举同名，132 处碰撞）；且 dqRender 重写 dqCommon 虚函数，跨模块归一会破坏多态（17 处 virtual 隐藏）。命名规则应让步于参考一致性，而非强求语言统一。

### 12.7 格式规则让步于参考一致性——typedef / C 风格转换 / 运算符重载
**错误**：把 C++ 规范 §2.2（禁 `typedef`）、§2.3（禁 C 风格转换）、§9（禁运算符重载）当作机械红线，用 clang-tidy 批量改写存量代码。
**正确**：这三项是**格式偏好**，从属于 §0（参考是规范）与 §3（1:1 对齐）。当参考项目（imodel-native/filament C++）本身使用 `typedef`/C 风格转换/比较·哈希运算符时，DanQing 移植代码 1:1 保留；仅参考未涉及的新代码才强制 `using`/`static_cast`/避免运算符重载。
**教训**：审计确认存量"违规"全部是逐字移植：`DqTime.h` 的 `(double)`/`(uint32_t)`/`(double)(int64_t)` 转换 = imodel-native `BeTimeUtilities.h:58/69/111/179`；`BeThreadLocalStorage.h:32`/`LocalState.h:68`/`PTypesU.h`/`Version.h` 的 `typedef` = imodel-native 同名行；`DqGuid`/`DqId`/`Version`/`DqTime`/`ScopedArray` 的比较/哈希/下标运算符是 `std::map`/`set`/`unordered_map` 键所需且对齐参考。强行归一会引入 P0 违规（破坏 1:1）。与 §12.6 同构：格式规则让步于参考一致性。登记见 §14 TD-7。

### 12.8 修 bug 不是自创许可——症状推理补丁 vs 参考机制移植（2026-09-13 resize 事故）
**错误**：用户报告"缩放后 resize 卡顿"，在**从未打开参考项目对应实现**的情况下，连续三层症状推理补丁：①"glDelete 驱动同步等待 80-110ms"→ 自创延迟销毁（retireAll/flushRetired）；②"Target 整只销毁太重"→ 自创 `setViewRect`/`resetForResize`（还在注释里写 `Ported from: Viewport.ts:2604-2608` 装门面，但从未读 Target.ts 的 updateViewRect 实现）；③"连续 resize 每帧重建"→ 自创 150ms 防抖 timer。三次全部引入视觉回归（Grid/ACS 位置错乱 → Fit 视口下 Grid 不可见），靠用户肉眼发现，最终整段回滚。
**正确**：修复前先完整阅读参考的 resize 链路——`Viewport.ts:2604` `resized → target.updateViewRect()` 之后 Target.ts/OnScreenTarget 里 updateViewRect/onResized 到底做什么（它如何重分配 GL 资源、哪些状态失效），照机制移植；若 DanQing 当初的 `delete+createRenderTarget` 本身就是移植缺口（updateViewRect 空壳），修复方向是**补齐参考的 updateViewRect 实现**，而不是在自创路径外面再包三层自创补丁。
**教训**：
1. **修 bug 是 §0 暴露面最大的场景**——移植新功能时"先读参考"是自然动作，调试时的紧迫感让人退回"合理推理+快速补丁"习惯，而渲染管线的症状（卡顿/闪烁）推理出的"病因"十有八九不是参考机制的真实结构。
2. **`// Ported from:` 行号引用必须对应真实读过的实现**。引用行号装点自创代码是双重违规：既自创了实现，又污染了溯源体系（§0 的可追溯性机制被当作遮羞布）。写下行号前自问：这个文件的这个函数，我打开过吗？
3. **渲染/视觉行为的改动必须有"画对了"的验证，不能只验证"不崩"**。View3DResizeTest 只断言进程存活，三次视觉回归全部漏网。改动渲染管线资源生命周期前，先建像素级回归（readPixels 对比基准帧/断言装饰图元屏幕位置），再动代码。
4. **用户等待压力不是降低流程标准的理由**——"猜测→让用户验证→又错"的循环比"先读参考再修"浪费的时间多得多（本案例三次返工 + 用户三次无效验证）。

### 12.9 黑方块 saga——多根因、句柄生命周期与真实窗口取证（2026-09-14）
**事故**：用户报告"最大化后 Grid 消失、滚轮放大后出现黑色大方块"。历经：看门狗两版（症状补丁，皆弃）→ 桌面注入复现（被终端遮挡吃掉全部点击）→ 自主探索数小时无效（Grid 默认关闭，复现配方错误）→ 用户给出精确配方后立刻复现。
**最终双根因**：
1. **呈现层**：`OpenGLSwapchain::resize` 假设"OpenGL 无需重建交换链"——GL 子窗口表面 extent 变化（最大化）后 WGL 呈现关联失效，SwapBuffers 逐帧成功但屏幕停在按新尺寸缩放的旧帧（旧帧网格平面区域=用户看到的"黑色大方块"）。修复=resize 即表面过期、原地重建平台交换链（Vulkan OUT_OF_DATE 的 WGL 对应物）。
2. **内容层**：resize 重建 OIT 资源时**销毁了合成 shader 的 GL 程序**（参考 dispose(_fbos) 只毁 FBO/纹理、程序随上下文存活），但 `ShaderProgram` 编译缓存仍 Success、句柄悬空 → `use()` 把死句柄交给 `driver.useProgram` → `if (prog && prog->isValid())` **静默跳过 glUseProgram** → 合成 quad 被残留 program 画黑。
**教训**：
1. **症状重叠 = 可能多根因**。修好呈现层后网格仍消失——不是修复无效，是还有第二条。每次修复后必须复核全部证据是否闭环（本次判据：OIT 三纹理回读全对而 composite 输出中心纯黑 → 损失在合成层 → [COMPDIAG] 探针抓 curProg 165→3 实锤）。
2. **销毁 GL 对象必须同步失效所有缓存层**（编译状态/句柄成员/状态跟踪器）。下游对失效句柄的"防御性静默跳过"会把 use-after-destroy 放大成错误输出——失效传播是设计义务，不是可选项。
3. **HWND 会被回收复用同地址**。"句柄相等"不等于"同一对象还活着"；表面过期语义无条件重建，不看句柄值。
4. **先要精确复现配方再动手**。用户的操作序列（初始状态+开关+动作序）是唯一 ground truth；我自主探索数小时（各种最大化循环/缩放风暴）全部无效，因为 Grid 默认关闭、配方从根上就错了。
5. **真实窗口取证**：桌面注入点击必须前台守卫 + hit-test 实时探测（无守卫的 12 轮循环全打在被最大化的终端上，还误关了它）；坐标不推算（analyze_image 两次给错、窗口每次记住不同几何）——裁剪截图亲眼量、点击后再截图确认；取证工具先自测（.cmd 中文注释编码让 set 静默失效、stderr 文件重定向全缓冲 4KB 不落盘、env 门控要验证真生效）。
6. **像素级回归先行**：`MaximizeKeepsGridVisible`（readPixels 断言）在修复前就是红色锁定、修复后转绿——比真实 app 反复点鼠标可靠一个数量级。


### 12.10 深度反转 saga——旁路参考投影源、隐性面切换与“层层忠实却结果矛盾”（2026-09-16）
**事故**：用户报告 glTF 立方体贴图与 DTA 左右相反（F 朝向不同）。取证中每一层单独验证都“忠实”（文件 UV→polyface→CPU 顶点→GPU VBO 字节→attrib 指针→着色器源码→纹理对象回读），渲染结果却呈 u 镜像——逻辑死局。
**根因**：`Viewport::renderFrame` 把 `worldToNdc`（视域盒→NDC 线性直通，m22 恒正）经 `setViewportTransform→changeProjectionMatrix` 当投影，**旁路了参考唯一的投影来源**（`changeRenderPlan(plan.frustum) → FrustumUniforms.changeFrustum → lookIn+ortho(0,depth)`，m22=−2/depth）。正值 m22 + LEQUAL = 最远面获胜 → 正交 Top 视图渲染的是立方体**底面**——底面 UV 与顶面左右相反 → 假性“贴图 u 镜像”（v 不受影响，故只有左右反）。
**修复**：RenderPlan 补 `frustum/fraction/is3d`（RenderPlan.ts:48/66/67）→ `changeRenderPlan` 调 `changeFrustum`（Target.ts:534 顺序：在 updateRenderPlan 前）→ `ValidateRenderPlan` 从 ViewingSpace 填充 → `setViewportTransform` 推送 lookIn/ortho 一致对。DanQing 的 `FrustumUniforms::changeFrustum` 本就是忠实移植——只是无人调用。
**教训**：
1. **逐层验证全部通过却结果矛盾时，怀疑隐性面切换**。深度反转不改变 x/y 投影：几何轮廓、贴图存在性、甚至像素内容都“看起来对”，只有 UV 场的梯度方向与数据对不上。解法：着色器输出 `v_texCoord` 为颜色（DANQING_UV_DEBUG）+ 面内大样本回归梯度，一次定位是哪个面的场。
2. **`[MVP]` m22 符号是深度反转的一击必杀探针**：正 = 反转（LEQUAL 下最远获胜），负 = 参考惯例（近平面 depth 0）。
3. **不对称标记物**：几何用缺口角（bbox 四角内容存在性），纹理用色点；字形质心有歧义（臂/stem 分布不均）勿用。
4. **测试 exe 的文件输出路径相对 CWD**——必须从仓库根跑，否则写失败读旧帧（本 saga 两次假观测的直接来源；探针文件路径建议绝对化或先自测）。
5. CDP 驱动参考 app（DTA）的坑：`Page.reload` 丢 blank connection 视口；合成点击开的视口 `renderTarget:none`（画布零内容，采集不可信）；rAF 被遮挡挂死（用 setTimeout 泵）；大数组 returnByValue 极慢（页内分析/`Page.captureScreenshot`+canvas rect 裁剪替代）。
6. **像素级回归锁**：`GltfStandardView.TopViewRendersTopFaceNotBottom`（BoxTexturedDots 色点资产，断言 Top 视图蓝点在面右/红点在左 = +Z 面忠实场）——修复前 RED、修复后 GREEN，已双向验证。

---

## 13. 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- **开发一律 Debug**（用户指令）。
- Qt 路径：`third_party/qt/windows/6.11.1/msvc2022_64`（Windows，当前开发平台）；`third_party/qt/mac/6.11.1/macos`（仅 dqApp 使用，引擎 SDK 不依赖）。GoogleTest：`third_party/googletest`。
- 常用测试目标：`DisplayTestAppDtaTest`（真窗口 harness，View3DResize/ZoomBlackBox 等像素级回归）、`DisplayTestAppTest`、`dqAppTest`、`dqGeomTest` 等；ctest 选择器如 `-R "View3DResize|ZoomBlackBox"`。
- DtaTest 全量为回归门（TD-16 于 2026-09-23 清偿：73 通过 + 2 跳过 + 0 失败；此前「不跑全量」的临时指令 2026-09-21 随修复失效）。

### 13.1 渲染/窗口诊断开关（env 门控，默认零开销）

| 环境变量 | 作用 |
|---|---|
| `DANQING_GL_TRACE=1` | 窗口事件轨迹（[VPEVT] WinIdChange/Show/Resize）、swapchain 生命周期（[WGL] create/destroy/makeCurrent 失败+自愈/SwapBuffers 失败、[SC] rebind/resize 重建） |
| `DANQING_NM_TRACE=1` | 法线贴图链取证（[NMDIAG]）：GltfDecoration 的 normalMapTexture 解析结果（尺寸）+ 绘制点门控状态（surfTex/normalTex 句柄、HasNormalMap 位、displayNormalMaps、renderMode、textures、applyLighting）——2026-09-17 法线贴图接线 saga 所加 |
| `DANQING_OIT_DUMP=1` | 合成器帧尾 dump（accum/revealage/opaqueSnapshot 回读、mainRT pre/postComposite/endDraw、translucent 命令清单）+ [PRES] present 计数与尺寸 |
| `DANQING_SV_TRACE=1` | ViewingSpace.adjustZPlanes 分支轨迹（[SVADJ]：进入的 org/delta/grid/bgMap/extents、StronglyOutside 门的平面数与分类结果、depthRange 与 delta.z 决策）——2026-09-15 Front/Back 甩位 saga 取证所加 |
| `DANQING_UV_DEBUG=1` | Surface 片元着色器 sampleSurfaceTexture 改为输出 `v_texCoord`（R=u,G=v）——贴图镜像/UV 链取证的活体场可视化（2026-09-16 深度反转 saga） |
| `DANQING_MVP_TRACE=1` | 每图元 u_mvp 16 元素 + 行列式（**m22 符号 = 深度反转探针**：正=最远面获胜） |
| `DANQING_UV_TRACE=1` / `DANQING_VAO_TRACE=1` / `DANQING_DRAW_TRACE=1` | PolyfaceGraphic CPU 顶点 dump（[PGV]）/ draw 时 VBO 字节+attrib 绑定回读（[VAO]/[DRAW3]，含 GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING） |
| `DANQING_TEX_DUMP=1` / `DANQING_SHADER_DUMP=1` / `DANQING_ATTR_TRACE=1` / `DANQING_WRAP_TRACE=1` | 256×256 纹理上传后 glGetTexImage 回读（[TEXDUMP]）/ 着色器源码落盘 build/v(f)shader-*.glsl / activateProgram 时 a_texCoord location+attrib3 状态（[ATTR]）/ 绘制时 wrap 状态（[WRAP]） |
| `DANQING_CURSOR_TRACE=1` | 光标/光圈链取证：ToolAdmin::Decorate 光圈守卫（[CURSOR]）+ Viewport::leaveEvent（[LEAVE]，fflush 落盘）+ drawCanvasDecorations 的 flush/输出目标块回读（写 C:/Windows/Temp/danqing_cursor_flush.log——GUI 进程 stderr 不落盘的旁路）——2026-09-19 leave 取证所加 |
| `DANQING_DP_TRACE=1` | 深度预览链轨迹（[DP]）：ViewManip::previewDepthPoint 门控状态（preview/inDyn/nPts）+ CollectDecorations 运行 + drawFrame/drawOverlays 的 WorldOverlay 命令数——2026-09-19 Rotate 锚定态取证所加 |
| `DANQING_INST_TRACE=1` | instances 消费链轨迹（[INST]）：createImdlLutGraphics 的实例化门控状态（count/viewsOk/symOk→InstanceBuffers 指针）——2026-09-28 TD-25 清偿取证所加 |
| `DANQING_SEL_TRACE=1` | 瓦选择/剔除链轨迹（[SEL]）：ImdlTileTree::computeVisibility 的 sphere-cull 判定（worldRange/球心/半径 + cull/pass）+ SSE 判定量值（radius/pixelSize/maxSize → Visible/TooCoarse）——2026-09-30 M-K(2) housemodel 透视 saved 视图零请求取证所加（根因 = Tile 包围球半径移植缺口） |
| `DANQING_LIGHT_TRACE=1` | 光照 uniform 读回探针（[LIGHT-post]）：draw 后 glGetUniformfv 读回 GL 程序实际收到的 u_sunDir/u_lightSettings[0,4,12]/u_materialParams/u_materialColor——2026-09-30 M-M(1) 方向光缺失 saga（上传链断点定位；u_materialParams 曾恒 0 的双写取证） |
| `DANQING_LUT_DUMP=1` | 顶点 LUT 纹理上传回读（[LUTDUMP]）：宽>256 且高=1 的 2D 纹理（顶点表形态）上传后 glGetTexImage 回读首 32 字节——与 tile 的 bvVertex 字节对拍（M-M(1) 上传链验证；同 [TEXDUMP] 先例） |
| `DANQING_NO_LIGHTING=1` / `DANQING_LIGHT_DEBUG=1` / `DANQING_NO_FACEFLIP=1` | TEMP-DIAG 判别实验族（M-M(1) saga）：NO_LIGHTING=applyLighting 整段透传（二分 dimming 源）；LIGHT_DEBUG=片元级取证色输出（v_materialParams/mat_weights/dot 项/u_lightSettings 逐轮换）；NO_FACEFLIP=finalizeNormal 去 gl_FrontFacing 翻转（绕序假设判别——实测无效证伪） |
| `DANQING_CAT_TRACE=1` | category 可见性探针（[CAT]，M-N(1)）：Batch::applySubCategoryVisibility 逐 batch 打印特征数/不可见集大小/特征表 distinct subCategoryId 集——+1 规则映射与特征表实态的对账面 |
| `DANQING_TEXCreation_TRACE=1` | 嵌入纹理解码探针（[TEXC]，M-M(3)）：createEmbeddedTexture 的名字/format/字节量 + 解码后尺寸 |
| `DANQING_OPEN_TRACE=1` | dump 模型打开链计时桩（[OPEN] model entry / chain-done ms= / trees=——stderr 落盘，GUI 进程须 run-traced.cmd 类启动器+优雅退出才落盘）：M-I(1) 打开性能与 M-I(5) 真实 app 终验取证所加 |
| `DANQING_DUMP_OUT=1` | canvas 帧的输出目标全帧 PPM 落盘 build/outtarget-%03d.ppm（有界 40 帧，绝对路径）——亲眼定位 canvas 装饰在输出目标中的实际落点（2026-09-19） |
| `DANQING_AUTO_OPEN_DECO=1` | DisplayTestApp 启动 3.5s 后自动打开 Decoration Geometry Example（合成点击在 Start 页卡片上不生效的绕路——真实 app 桌面注入取证用） |
| `DANQING_CLIP_TRACE=1` | 剖切链取证（[CLIPDUMP]，M-P P-D）：ClipStack 纹理上传回读（行数/编码串）——shader 片元裁剪 saga（RGBA32F 映射/NEAREST/u_clipParams[0] 后缀三根因链）所加 |
| `DANQING_CLIPDECO_TRACE=1` | 剖切装饰链取证（[CLIPDECO-DEC]/[CLIPDECO-DRAW]，M-P P-F）：ViewClipDecoration::Decorate 进入态（clipId/suspend/loops/shape）+ drawClipPlanesLoops 的 loop 域与 finish 产出——装饰上屏排障 |
| `DANQING_THM_TRACE=1` | thematic 链取证（[THM]，M-S S-d）：TargetImpl changeRenderPlan 的 plan.thematic/wantThematic/渐变纹理态 + ThematicUniforms 渐变纹理首/中/尾 texel +（S-e 增）ThematicSensors 传感器纹理上传后 GPU 回读 vs CPU 打包对拍（CLIPDUMP 先例）——断链定位 saga（空槽形态/双 BranchUniforms 副本/observer 门恒跳三根因链）与 IDW 传感器上传链验证所加 |
| `DANQING_THM_FRAGDBG=1..10` | thematic 片元级取证（M-S S-d 起——DANQING_UV_DEBUG 同族）：变体构建期把 applyThematicDisplay 槽体换为诊断输出（1=g_normal 视化/2=ndx 灰度[含 IDW 片元循环后]/3=u_thematicAxis 视化/4=u_thematicRange/5=rawPosition.z 顶点侧/6=v_thematicIndex 直读/7=getSensor(0) 位置/8=getSensor(1) 位置/9=双传感器值通道/10=v_eyeSpace 帧直读）——S-d Slope 混帧 saga 与 S-e IDW 帧判别（v_eyeSpace=视空间实证定音）所加 |

真实 app 取证启动器（stderr 落盘）：`build/run-traced.cmd`（纯 ASCII；调试会话可按需重建）。桌面注入取证脚本模板：`build/leave-forensics.ps1`（前台守卫 + 物理光标 SetCursorPos + 截图对拍 + 优雅退出保 stderr 落盘；**先杀僵尸 DisplayTestApp.exe 再跑，否则前台/测试全污染**）。

---

## 14. 现有命名技术债务

以下存量违规是历史代码积累，**不阻塞新规则落地**，但必须显式登记、按严重度排期清理。新代码必须零违规。

| 编号 | 存量违规 | 范围 | 修正 | 严重度 |
|---|---|---|---|---|
| ~~TD-1~~ ✅ 已解决（规则反转） | dqRender 方法名 camelCase | dqRender 全模块移植代码 | §3 规则反转：方法/函数名 1:1 按参考语言（TS→camelCase），不再 PascalCase 归一。现有 camelCase 代码已合规，无需重命名。证据见 §12.6 | — |
| ~~TD-2~~ ✅ 已解决 | 成员变量 `m`+PascalCase | 全模块 | 全模块清理完成：dqBase/dqGeom/dqCommon（15）+ dqRender（~557）+ dqApp；word-boundary perl | — |
| ~~TD-3~~ ✅ 已解决 | 自由函数 `frustumProjection`/`orthoProjection` 归一为 `Frustum`/`Ortho` 会与 `dqCommon::Frustum` 类型同名冲突 | `FrustumUniforms.{h,cpp}` | 按 §3.4 碰撞例外保留后缀（非违规） | — |
| ~~TD-4~~ ✅ 已解决 | dqRender 链接 Qt6（`TileRequestFetcher` + 死代码 `QtContextBridge`）违反 §8.2 | `dqRender/src/tile/`、`dqRender/src/platform/` | Qt 实现移至 dqApp（`QtTileRequestFetcher`）经 DI 注入；`TileAdmin` 默认改为 Qt-free `NullTileFetcher`；删除死代码 `QtContextBridge.mm`；dqRender 零 Qt 链接 | — |
| ~~TD-5~~ ✅ 已解决 | 公开头 Qt 类型审计 | 引擎模块 PublicAPI | 已审计：公开头代码零 Qt（命中均为"// 替代 Qt 容器"说明性注释，保留）；`ITileFetcher.h` 已 Qt-free | — |
| ~~TD-6~~ ✅ 已解决 | 现有 1783 个测试中未标注 `// Ported from:`/`// Authored:` 的 `TEST()` | 全模块 tests/ | 全部补齐：文件级出处传播到每个 `TEST()`（脚本 `scripts/annotate_test_provenance.pl`）；0 个未标注 | — |
| ~~TD-7~~ ✅ 已解决（规则澄清） | `typedef` / C 风格转换 / 运算符重载 被误判为 §2.2/§2.3/§9 违规 | dqBase 移植代码（DqTime、BeThreadLocalStorage、LocalState、PTypesU、Version、DqGuid、DqId、ScopedArray） | §9 澄清：这三项是**格式偏好**，从属于 §0/§3 1:1 参考对齐；存量均为 imodel-native 逐字移植，1:1 保留合归。新代码仍优先 `using`/`static_cast`/避免运算符重载。证据见 §12.7 | — |
| ~~TD-8~~ ✅ 已解决 | `GradientSymb`/`GradientSymbProps` 用 `std::shared_ptr<ThematicGradientSettings(+Props)>` 违反 §9（指针仅用于打断 `Gradient.h ↔ ThematicDisplay.h` 循环包含） | `dqCommon/Gradient.h`、`ThematicDisplay.h`、`Gradient.cpp`、`GradientThematicTest.cpp`、新增 `GradientKeyColor.h` | 解决方案 B1：**保持 `ThematicGradientSettings` 为值类型**（忠实 itwinjs 值对象语义，避免 `RefCounted` 删拷贝经 `optional<GradientSymb>`→`GraphicParams::Clone()` 的级联）。将 `GradientKeyColor`/`GradientKeyColorProps` 抽到独立 `GradientKeyColor.h`，使 `ThematicDisplay.h` 仅依赖它（不再 include `Gradient.h`），从而打断循环；`Gradient.h` 反向 include `ThematicDisplay.h`，`thematicSettings` 字段由 `shared_ptr` 改为 `std::optional`（可空 + 可拷贝）。引擎公开头 `shared_ptr`/`weak_ptr` 归零；1783 测试全绿 | — |
| TD-9（保留，非违规） | `bpool.h` 用 `std::set<void*>`（free_list/used_list）看似违反 §8.3 | `dqBase/PublicAPI/dqBase/bpool.h`（L873/883/888/971） | **1:1 移植 imodel-native `bpool.h`**（参考源码 L867/877/882/971 同样用 `std::set<void*>`）。§0/§12.7 要求 C++ 参考 1:1 保真；参考本身选 `std::set<void*>`，DanQing 逐字保留合归，**不得**改 `bset`（否则偏离 imodel-native）。同 TD-7/§12.7 性质：格式/容器规则让步于参考一致性 | — |
| TD-10（保留，非违规） | `LRUTileList` 用 `std::map<Tile*, LRUNode>` 而非 `bmap` 看似违反 §8.3 | `dqRender/src/tile/LRUTileList.{h,cpp}` | **技术必要性**：侵入式双向链表持有指向 mapped value 的裸 `LRUNode*`（previous/next）。`std::map` 保证 mapped-value 地址稳定（node-based 堆分配）；`bmap`（B-tree）插入时节点分裂会重定位 value，导致这些指针悬空（UB，实测 `FreeMemoryEvictsAfterClearUsed` 失败）。TS 参考 `LRUTileList.ts` 把节点嵌进 Tile；本 C++ port 用 map 避免 Tile 循环头依赖。如要消除需重构为 `bmap<Tile*, unique_ptr<LRUNode>>`（独立 pass）。已就地注释说明 | — |
| ~~TD-11~~ ✅ 已解决（2026-09-23） | dqApp ToolAdmin ViewTool 所有权 | dqApp ToolAdmin.{h,cpp} + ViewTool.cpp + DtaToolBars.cpp + Viewport.cpp | **修复（adopt/run 分离，架构师选型）**：①`adoptViewTool`/`disownViewTool` 接管点——生产 `runViewTool(new ...)` 先 adopt（登记 m_ownedViewTools）再 run，失败 disown+caller 删；②`startViewTool` **不登记**（栈对象/裸 run() 测试路径零删除——参考 GC 语义的 C++ 表达）；③自退出工具（ViewUndoTool::onPostInstall→exitTool 在 run() 调用链内）同步 delete this 是 UB——exitViewTool 改 **deferred 队列**，`flushDeferredViewToolDeletes` 在 RenderFrameGuard 析构（帧边界，参考 async run() 微任务恢复语义 ViewTool.ts:100-112）执行；④installViewTool 统一 dispose 旧 owned（含防悬空 scrub）。**教训**：startViewTool 清槽后重查槽值会误建 SuspendedToolState 快照（参考 if/else 在置空前判断 ToolAdmin.ts:1820-1827；快照到已变更 toolState → exit 恢复错态） | — |
| ~~TD-12~~ ✅ 已解决（2026-09-23） | dqAppTest 7 项跨测试失败 | — | 根因：AcsTriadDecorator 缓存 graphic 属于创建时的 RenderSystem——跨视口（前测 driver 已死）时 disposeGraphic 触死 driver（MeshGraphic::~MeshGraphic → SEH 0xc0000005 符号化栈实锤）。修复：缓存记录 m_creatingSystem，Decorate 时 system 变更则指针置空不 dispose（死 driver GL 资源随上下文消亡）。连带修 TileTreeRegistry.DropSupplier 指针同一性断言（堆回收复用地址→改状态判定）。结果：dqAppTest 353/353 | — |
| ~~TD-13~~ ✅ 已解决（2026-09-23） | IndexedPolyface::IsAlmostEqual 通道缺口 | dqGeom/src/polyface/IndexedPolyface.cpp | **修复**：对齐参考 PolyfaceData.ts:184-214 全通道面——points/normals/params 按 tol、point/normal/color/paramIndex 精确、colors 精确、edgeVisible、twoSided、expectedClosure。**锁**：`IsAlmostEqualCoversAllDataChannels`（Authored：参考无 GeometryQuery 级 IsAlmostEqual 对应物——10 通道差异检出 RED→GREEN） | — |
| ~~TD-14~~ ✅ 已解决（2026-09-23） | GltfDecoration 纹理不去重 | dqApp/src/GltfDecoration.cpp + GltfDecoration.h + PolyfaceGraphic + RenderPipeline/Viewport(external 透传) | **修复**：①resolvedTextures 成员级缓存（1:1 GltfReader._resolvedTextures GltfReader.ts:533——共享图一次解码+上传；**生命周期=decoration 成员**，~GltfDecoration 在 disposeGraphic **之后**销毁——局部缓存在 BuildGraphic 尾销毁会让纹理死于渲染前，glTF 像素回归抓过）；②PolyfaceGraphic `setTextureExternal/setNormalMapTextureExternal`（CreateTextureArgs.ownership="external" 语义 CreateTextureArgs.ts:50-52——graphic 不删共享句柄）；③createGraphicFromPolyface 三层透传 external 默认 false 零行为变化 | — |
| ~~TD-15~~ ✅ 已解决（2026-09-23） | 绑定派发缺口 | dqRender/src/render/UniformHandle.h + ShaderProgramImpl.{h,cpp} + SurfaceNormal.h + SceneCompositorImpl.cpp + GlLoader.{h,cpp} | **根因**：UniformHandle 是纯缓存（set* 只写 m_data 不触 GL），use()/draw() 的绑定派发在跑但值从未上屏——参考 UniformHandle.ts:90-138 每个 dirty set* 直接 context.uniform*，DanQing 移植丢了 GL 派发半截。**修复**（1:1 参考机制）：①`UniformHandle::create(glProgram, name)`（UniformHandle.ts:41-55）解析 location 存 handle，-1 = 缺失静默跳过（参考 null-location no-op）；②每个 set* dirty 后直接 glUniform*；③`Uniform::compile` 经 `ShaderProgram::getGlProgram()`（link 时缓存的裸 GL 句柄，参考 prog.glProgram）解析；④SurfaceNormal 的 u_normalMatrix 从 nullptr 注册补为 wireNormalMatrix 绑定（surface 侧漏注册——参考算 g_nmx 于 shader 内，DanQing §3.4 偏差走 CPU 上传）；⑤拆除 SceneCompositorImpl 4 处 legacy 名值上传（u_sunDir/u_lightSettings/u_surfaceFlags/u_normalMatrix）。**锁**：`UniformBindingDispatch.ProgramUniform/GraphicUniformBindingDispatchesToGl`（真 GL 探针：绑定→改值→重绑→readPixels 断言红通道 64→255；RED→GREEN 双向验证）。**注意**：glUniform4fv 写 mat4 location 是 INVALID_OPERATION no-op——wireModelViewMatrix 的 u_mv 走 setUniform4fv(mv,4) 与名值路径同值双写，无害但应改 setMatrix4（后续清理） | — |
| ~~TD-16~~ ✅ 已解决（2026-09-23） | DtaTest 全量 37 项失败 | — | ①DtaToolBars 两断言测试漂移修正（工具栏 5→6 加 Deco Example；Debug 点亮）②跨测试污染同 TD-12 根因修复。结果：DtaTest 73 PASSED + 2 SKIPPED + 0 FAILED，全量恢复为回归门 | — |
| ~~TD-17~~ ✅ 已解决（2026-09-23） | 无纹理纯色路径缺陷 | dqRender/src/render/PolyfaceGraphic | **根因**：`buildFromPolyface` 的 defaultColor 解包布局与调用方打包错位——打包 `(a<<24)|(b<<16)|(g<<8)|r`（GL RGBA 字节序），解包却按 `(r<<24)|(g<<16)|(b<<8)|a` → 通道错位（绿→品红/蓝→黄/alpha 落低位→透明→黑），纹理路径（纹理供色）掩盖。**修复**：解包对齐打包布局（defaultColor 与 per-vertex color 两处）。**锁**：`TileTreeRender.SolidBaseColorFactorRendersMaterialColors`（minimal-solid 资产，三色像素断言） | — |

| ~~TD-18~~ ✅ 已解决（2026-09-23 两轮收尾） | imdl 消费栈 | dqRender/src/tile | **已完成**：格式层（ImdlHeader/GltfHeader 变体/ImdlDocument）+ 量化顶点 CPU 解码（`decodeImdlGraphics`，TD-19 修复 1-based 索引后窗口端到端绿）+ **material fillColor 接线**（0x00BBGGRR→DanQing RGBA 打包，去硬编码绿）+ **FeatureTable 解析**（12B 头+3×u32/feature packed words→`dqCommon::FeatureTable`，PackedFeatureTable.ts:139-141 布局）+ `computeImdlChildTileProps` + `ImdlTile/ImdlTileTree` + `PrimaryTileTreeSupplier` + SpatialRefs 默认工厂。**测试**：ImdlHeader 5 + ImdlDocument 5 + ImdlGraphics 3 + ImdlTileTree 8 + 窗口端到端 1（全绿）。**后续增强**（独立登记，非通路缺口）：per-vertex 色/法线（oct-encoded）/纹理 UV、meshopt 压缩（v37 夹具在 ImdlParser.test.ts:284-443）、多材质/gradient、requestTileTreeProps/generateTileContent RPC 化（**闭环 2026-09-27**：M-D RPC dump 回放建立——树 props/瓦字节经 DumpTileTreeProps/DumpTileFetcher 本地回放，ContentIdProvider 请求键接线；**instances 修饰消费闭环 2026-09-28**：TD-25 于 M-E 清偿，60 实例像素锁 + 完整加载 E2E 对账锁） | — |

| ~~TD-19~~ ✅ 已解决（2026-09-23） | imdl 渲染端未上屏 | dqRender/src/tile/ImdlGraphics.cpp | **根因**：24-bit surface 索引 0-based 直传 `AddPointIndex`——IndexedPolyface 是 **1-based**（对照组 GltfReader.cpp:343 "cgltf indices are 0-based" 明确 +1；索引 0 是 buildFromPolyface 的跳过哨兵 PolyfaceGraphic.cpp:83）→ 每 facet 全部 corner 被跳过 → 0 顶点 → draw 空提交（graphics=1 但空、[MVP] 无 tile 矩阵、三视图不上屏——全部症状吻合）。**修复**：+1 转换（与 glTF reader 同惯例）。**锁**：`TileTreeRender.ImdlTilesetRendersRecordedFixture`（录制夹具端到端——绿色矩形 212 万像素上屏） | — |
| ~~TD-20~~ ✅ 已解决（2026-09-25，M-A 里程碑） | imdl 顶点消费形态偏差：CPU f64 逐顶点解码 + fan 展开 VBO（偏离参考的 LUT 直传主路径）；FeatureTable 不链接（readContent 丢弃） | dqRender/src/tile（ImdlGraphics.cpp/ImdlTileTree.cpp/RealityTile.cpp）+ dqCommon PackedFeatureTable | **U8 归位**（d337cf0/16a8c45）：readContent 链接线上 packed words→PackedFeatureTable.ts:35-58 构造→TileContent.featureTable + Batch 包裹（ImdlReader.ts:110-123）。**U7 归位**（16b1598/578b7cd/4dbe099+本行提交）：imdl 顶点表 LUT 直传主路径 `createImdlLutGraphics`——线上 RGBA8 顶点表（=LUT texel 布局，Quantized.LitMeshBuilder VertexTableBuilder.ts:342-398）按 JSON width/height 原样上传（ParseImdlDocument.ts:1005-1008/1029-1042 + VertexLUT.ts:93-99），零 CPU 逐顶点解码，shader 侧采样解量化（glsl/Vertex.ts computeVertexPosition）；Surface 量化变体全通道 LUT 读取 + LUT 形态 SurfaceGeometry（drawArrays + a_qPosition 24-bit UBYTE3 流）。**EQUIVALENCE 登记**（§11.10）在 ImdlTileTree/RealityTile 两处切换点（CPU f64→shader f32 解量化 ≤1ulp）。**锁**：`ImdlGraphicsTest.LutPathUploadsVertexTableVerbatim`（字节级直传）+ `LutPathUsesLessGpuMemoryThanVboPath`（16B/顶点 vs 52B/角）+ TileTreeRender 8 项像素锁全绿。计划与取证：docs/阶段0归位-MA-imdl消费链-实现计划-2026-09-25.md + docs/itwinjs-tile-vertexLUT机制深挖-2026-09-24.md。**遗留 TODO**（后续里程碑；M-K(1) 盘点更新 2026-09-30——两项缺口从 0 命中理论面变为实测命中面）：①textured 变体（surface.uvParams 等）——**housemodel 36 瓦首命中**（materials/namedTextures/uvParams 三探针；纹理字节本体不在 imdl 瓦内[externalTextures 走独立 RPC，dump 与既有同界]）；②polylines（tesselated）+ pointString 消费缺口——**baytown 21,323 polylines（24% 瓦）+ 6,136 pointString / housemodel 1,514 polylines / bridge-edit d2 8+2 与 drill 27/30 瓦**（numRgba3 顶点表形态与该缺口同域：baytown 27,459 = polylines+points 精确和）；可见性影响 = baytown 工艺管线骨架/仪表点要素、bridge 桥面细节线不渲染（M-K(2) 预期管理登记）。其余：meshopt 压缩顶点表、hasTranslucency 路由、uniformFeatureID 语义、maxTextureSize 接线、12B SimpleBuilder（无光照）网格拒绝入 LUT（unlit 接线待 textured 变体一起做）、FeatureTable 所有权（TD-21 已于 M-B 清偿） | — |
| ~~TD-21~~ ✅ 已解决（2026-09-26，M-B 里程碑） | `FeatureTable::m_array` 裸 owning 指针无析构（`new IndexedFeature[maxFeatures]` 无 `delete[]`）——存量泄漏被 TD-20 的 U8 链接激活为每 tile 热路径（每 tile 泄漏 2 张表：TileContent + Batch 拷贝） | `dqCommon/PublicAPI/dqCommon/FeatureTable.h:154` + `dqCommon/src/FeatureTable.cpp:21` | **修复**：rule-of-5 补齐（析构+移动+深拷贝，commit 6038dd5，值语义测试 FeatureTableTest.ValueSemanticsMoveAndCopy） | — |
| ~~TD-22~~ ✅ 已解决（2026-09-28，M-E Task 3） | 持久喂入集合的陈旧 Tile\* 窗口——TileAdmin m_requestedTiles[user] 持 Tile\* 至 requestTiles 替换/forgetUser，Tile 销毁后 processRequestsForUser 解引用悬空（参考为 JS 安全僵尸对象） | dqRender/src/tile/TileAdmin.cpp（onTileContentDisposed）+ ~Tile | **修复**：按登记方向落地——~Tile 既有钩子 onTileContentDisposed 扩为"LRU drop + m_requestedTiles/m_selectedTiles/m_readyTiles 三 feed 裸 Tile\* 清扫"（参考 Tile[Symbol.dispose] 无此义务——JS GC 语义，C++ 显式所有权适配）。**触发实案（RpcDumpRender.Instances60RendersAllInstances 拆解）**：子瓦请求键派生与 manifest 不符（-b-1-0-0-0-1 vs 采集键域 -b-2-0-0-0-1，id 派生链差异另登记）→ fetch NotFound → 瓦片残留 feed → 先拆树后拆视口时 ~Viewport forgetUser 遍历 feed 解引用死 Tile → SEH 0xc0000005。修复前安全性侥幸依赖"请求全部完成"（mirukuru/compatseed 全成功 hence 不爆），任何悬挂请求（失败/取消未投递）+ 先拆树即爆，与拆树/拆视口顺序无关 | — |
| ~~TD-23~~ ✅ 已解决（2026-09-27，M-C 里程碑） | 阶段 0 三块尾款：①draw 从 ready 集而非协议 selected 取图形（多层 imdl 树过渡期空白——M-B 终审登记的迁移项）；②调度缺口（TileRequest 无 users/共享门——同瓦多 user 重建请求；单 pending 队列——无 swapPending/users-empty 取消；forgetUser 不撤请求）；③imdl 边缘体系零消费（segments/silhouettes/indexed/compact 四形态解析与绘制全缺） | dqRender/src/tile（TileTree.cpp/TileAdmin.cpp/TileRequest.cpp/TileRequestChannel.cpp/ImdlGraphics.cpp/TilesetJson.h/CompactEdges.cpp/ImdlDocument.h） | **①选择壳**（bcddb59）：TileTree::selectTiles 从 selected 绘制 + root markUsed（IModelTileTree.ts:435-449）+ hasMissingTiles 归位；**②调度收口**（94c31ea/59101e2）：TileRequest.users/addUser/isCanceled（TileRequest.ts:25-77）+ 共享请求门（TileAdmin.ts:897-925）+ swapPending 双缓冲（TileRequestChannel.ts:215-219）+ users-empty 取消（:242-253）+ forgetUser 独占撤单（TileAdmin.ts:928-940）；**③U11 边缘四形态**（97af309/4a9ba7b/d6c0373）：JSON 解析层（ParseImdlDocument.ts:665-727）+ segments/silhouettes 字节直传 BO（Mesh.ts:47-53 + EdgeGeometry/SilhouetteEdgeGeometry）+ indexed EdgeLUT+IndexedEdgeGeometry（IndexedEdgeGeometry.ts + EdgeLUT.create）+ compact 兜底展开（CompactEdges.ts）。**锁**：SelectTilesProtocolTest.DrawCollectsProtocolSelectedTransitionalChildren + TileRequestUsers 3 项（SharedRequestAddUserInsteadOfRebuild/LoadingStateOverridesEmptyUsers/QueuedUserlessRequestReenqueuedNotDuplicated）+ TileRequestChannelTest 取消 3 项（UsersEmptyCancelsPendingAndActive/ReenqueueMovesRequestFromPreviouslyPending/CanceledQueuedRequestIsNotDispatched）+ ImdlEdges/CompactEdges 族 + TileTreeRender.ImdlEdgesRenderContrastingRingInSolidFill + ImdlSilhouetteEdgesRenderAtExtremesAndCulledOnFace 像素锁（环带+silhouette）。**边缘遗留**（登记，归后续）：u_lineCode 未上传（仅实线——EdgeSettings lineCode 覆盖半接线）；computeEdgePass 仅 SmoothShade 门（isDrawingShadowMap/wireframe+translucent 分支未移植）；WoW 反转/monochrome（wireframe 家族）未移植；~~indexed 窗口像素锁未建~~ ✅ 已清偿（M-I(4) JoesHouseEdge 真窗口像素锁——1,540 黑线像素/457 行/870 列，compact+indexed 边真窗口上屏）；maxEdgeTableDimension=2048 占位（GL maxTextureSize 接线 TODO）；target 级 FrustumUniforms 过期结构性登记（Task 5）；polylines（tesselated）仍缺（见 TD-20 清单）；持久喂入的陈旧 Tile\* 窗口（见 TD-22）、u_renderOrder 的 drawingBackgroundForReadPixels 分支（概念未移植）。**M-I(4) 追加清偿**（662d5e43b，终审 Approved）：qpos 协议移植缺口修复（参考 ShaderBuilder.ts:757-774 三缺口——DanQing 函数调用式 vertex main 未赋 qpos → indexed 边 6 顶点退化零碎片、[EDGE] 692 dispatch/0 碎片根因）+ hline→EdgeSettings 边色覆盖六跳链（styles.hline.visible.color 进 ViewState→computeEdgeColor 等价物→EdgeSettings——黑边）。计划与取证：docs/阶段0归位-MC-选择壳调度与边缘-实现计划-2026-09-26.md + docs/阶段1-MI-打开性能渲染对齐与拾取-实现计划-2026-09-29.md | — |
| ~~TD-24~~ ✅ 已解决（2026-09-27，M-D 里程碑） | `QtTileRequestFetcher` 用 QNetworkAccessManager 做 HTTP 拉瓦（3D Tiles tileset 内容），经 Application.cpp:82 注入——违反 §8.2 零网络协议（2026-09-27 用户指令：DanQing 只做请求获取之后的加载/解析/渲染，请求归宿主层） | `dqApp/src/QtTileRequestFetcher.{h,cpp}`（整删）+ `dqApp/src/FileTileFetcher.{h,cpp}`（删 http(s) 回退分支）+ `dqApp/src/Application.cpp:80`（FileTileFetcher 直用） | **清退**（f1ae49f）：删 QtTileRequestFetcher 与 FileTileFetcher 网络回退、dqApp 去链 Qt6::Network——取数唯一路径 = 本地文件/manifest 回放（数据经 ITileFetcher DI 字节进入）。**锁**：DumpTileFetcherTest.ZeroNetworkByConstruction（六代码目录 13 网络原语令牌递归扫描零命中 + 扫描覆盖数>100 仪器自检门，常驻回归）；RPC 捕获工具（danqing-rpc-tools）留仓外（§8.2）；M-D(3) 端到端 = RpcDumpRender 两锁（真后端 imdl 上屏） | — |
| ~~TD-25~~ ✅ 已解决（2026-09-28，M-E Task 3） | 瓦内容的 **instances 修饰未消费**（每 primitive 的 `instances`{count/transforms/transformCenter/featureIds}——按实例放置共享几何）。真后端 compatseed/instances60 dump 瓦全部携带；未消费时几何落在原点附近的局部盒 → 取景域外不上屏 | dqRender/src/tile（TilesetJson.h ImdlInstancesProps + ImdlGraphics.cpp createImdlLutGraphics 实例接线）+ dqRender/src/render（MeshGraphic::addInstancedSurface / InstanceBuffers / InstancedGeometry.draw divisor 路径 / SurfaceCommon+SurfaceVariantCompiler 实例化变体 / SceneCompositorImpl instanced dispatch）+ rhi Driver（bindInstanceBuffer 元素类型形参 + disableVertexAttribArray） | **清偿**（参考全链逐步落地）：①解析层 ImdlInstancesProps（ImdlSchema.ts:160-166 + parseInstances :1045-1087 归零语义）；②InstanceBuffers::create 三缓冲区 BO 直传（修正 featureIds 3B/实例 + symbology 8B/实例的存量字节语义错误）；③InstancedGeometry 包裹 LUT SurfaceGeometry（Mesh.ts:123-143——带 instances primitive 的 edges 共享实例缓冲 TODO 登记，本资产 numVisible=0 未触发）；④shader 实例化变体（glsl/Instancing.ts addInstancedModelMatrixRTC + Vertex.ts:147-154 g_mv = u_instanced_modelView × g_modelMatrixRTC + Color.ts:32-35 applyInstanceColor；旧 u_instanced_modelView[64] uniform 阵 + gl_InstanceID 偏差形态移除）；⑤dispatch（flags.isInstanced 逐图元 DrawCommand.ts:211 + shader 缓存键补 isInstanced/positionType 维 + u_instanced_modelView=branchMv×rtcOnly、u_proj=mvp×mv⁻¹——edge 分支同款 EQUIVALENCE 登记）；⑥a_featureId 逐实例 feature 链（FeatureSymbology.ts:61-77，Overrides/Pick 两变体）。**锁**：ImdlInstancesTest.ParsesInstanceFieldsFromDump（dump 驱动解析锁）+ LutPathUploadsInstanceBuffersVerbatim（三缓冲区字节级 verbatim）+ SurfaceShaderVariant.InstancedVariantConsumesInstanceAttributes（变体源码锁）+ **RpcDumpRender.Instances60RendersAllInstances 60 实例像素锁**（instances60-v1 真后端 dump：着色像素 ≥20000[首绿 123069 的 0.16×] + bbox 展布 ≥400×150 + 着色/绿质心偏移 >80[首绿 407] + 角落探针——修复前 colored=0 RED 双向验证）+ **RpcDumpRender.Instances60FullLoadReconcilesManifestKeys 完整加载 E2E 对账锁**（M-E Task 4 建；M-F(2) 升级两相驱动：DumpTileFetcher requestLog 全部请求键 vs manifest 键集合逐一对账——链通集=根键+"-b-2-0-0-0-1"[mult=1 派生子键，相 2 钻取消费]、差异集=域外 miss 实钉两枚["-b-1-0-0-0-1"/"-b-3-0-0-0-1"——采集面 artifact，锁头取证结论在案]、树侧零在途终态 + 像素锚[钻取视图内容 93.7% 帧/质心对帧心 ±2px]；不盲目 ready==tiles.size——采集域是 sweep BFS 面产物）。**同任务清偿 TD-22**（悬挂请求 + 先拆树的 forgetUser SEH）。**遗留登记**：~~子瓦 contentId 派生与采集键域不符~~ ✅ **M-F(2) 勾销**（2026-09-28 取证实证：采集域=仓外 sweep.js 合成 BFS 面[generateTileContent 鸭子请求+放大 cap×8+细分 mult 继承父键 TileMetadata.ts:841-846+BFS 未完成]——depth-0 放大键视口面永不可请求[同范围+hasSizeMultiplier 免预算穿透]，mult-1 深度键仅 "-b-2-0-0-0-1" 在域；引擎派生与参考逐行同构[[SEL] 选择轨迹实证]，残差 miss=采集面 artifact 实钉两相锁；覆盖率 1→2/3587 实测上升，锁头有完整两相驱动说明）；instances primitive 的 edges 实例化（MeshGraphic.create 全图元共享 _instances）；u_instanced_rtc/g_instancedRtcMatrix（Vertex.ts:120-139）仅 Contours/Thematic/Classifier 消费——随那些特性落地；compatseed 像素判据回加（本锁以 instances60 为直接输入，compatseed 链锁不变）；InstanceBuffers::create 不算 m_range（getRange 现无调用方——未来剔除/range 消费时补算）；instanced+非量化+featureMode≠None 变体 decodeUInt24 无定义（当前不可达——createImdlLutGraphics 恒量化） | — |
| TD-26（登记 2026-09-29，M-H(5)；M-P 收口增实录 2026-10-06） | `WheelZoomCoalesceTest.AcsDiscSurvivesDeepZoomPixels` 间歇失败——**净基线可复现**（信号强于经典环境态族[窗口回调崩溃类]）：main 基线两轮复现在案（M-G(2) 终审[stash 双态对照实锤净基线同败、本轮自然通过] + M-H(3) 全量[main 基线复现同败、改动面零交集]）；M-H(5) 全量自然通过[间歇态再实录]；M-J 收尾[隔离 ×3 同败——预存真缺陷疑似信号增强]；**M-O(3) 收口[开发中连续隔离 ×2 同败 + stash 双态对照（P1 净基线同败、与 P2/P3 改动面无关）；全量门禁自然通过——间歇态与复现轮交替实录]**。失败形态=深缩放像素断言（Acs 圆盘像素存活）——时序敏感的预存真缺陷疑似（滚轮合并[coalesce]窗口 × 桌面负载竞态？）；**M-P 收口新形态（2026-10-06）**：step-0 即呈现分裂[DISC trace：fboBlues=48/screenBlues=0 每步——FBO 有圆盘而屏幕 BitBlt 无]且同会话前轮全量自然通过[同二进制——直接非代码证据]，与桌面残留不可杀僵尸测试进程（1 线程 Responding、606MB、无主窗口）污染 CAPTUREBLT 捕获层相关疑；隔离 ×3 同败实录，留痕调查（复跑策略：失败即隔离复跑 ×2 记录；下次复现时抓 [Wheel] 轨迹与帧时序取证 + 排查僵尸进程）；**M-Q 期间归因直接实证（2026-10-06）**：僵尸进程清除后同二进制 AcsDisc 即过——CAPTUREBLT 污染实锤，桌面态相关零代码相关定案；**M-S S-e 收口轮再实录（2026-10-09）**：呈现分裂形态复发[step-0 起 fboBlues=48/screenBlues=0 每步——FBO 内容正确（本伦改动面=BranchStack 单栈统一+传感器链，FBO 侧 48 蓝像素逐位存活=引擎渲染无损伤直接证据），屏幕 BitBlt 捕获层空]、隔离复跑 ×2 同败、无僵尸进程在册——桌面捕获层环境态族归因维持 | `dqApp`（WheelZoomCoalesceTest.cpp——测试域零改动，仅登记） | 环境态族注释（各进度段"环境态失败"归因）遇到本测试时改指 TD-26 |  |
| TD-27（登记 2026-09-29，M-H(5) 浏览锁取证发现） | imdl 顶点表 **unquantized-LUT 形态未消费**（`numRgbaPerVertex=5` + `usesUnquantizedPositions=true`——20B/顶点：位置=4 texel 转置重组 f32[glsl Vertex.ts:44-54 computeUnquantizedPosition——每 texel .w 装 featureAndMaterial 字节]+第 5 texel 照明块；`createImdlLutGraphics` 的 `numRgba!=4` 拒绝门使整瓦 Completed 后零 graphic[Ready+空 graphic]，父瓦 LOD 兜底显示） | dqRender/src/tile/ImdlGraphics.cpp:208（接受门）+ dqRender/src/tile/TilesetJson.h（解析层无 usesUnquantizedPositions 字段）+ dqRender/src/render/SurfaceCommon.h:118-126（变体标志一维化——quantized=false 走非 LUT attribute 路径，LUT 几何↔16-bit 解码两维度需拆分）+ SurfaceNormal.h/SurfaceTexture.h（unquantized texel 源分支——参考 Surface.ts:397-398/:461） | **清偿方向**（§11.8 已核读参考）：解析层补字段（ParseImdlDocument.ts:1031）→ createImdlLutGraphics 接受 numRgba=4/5 + usesQuantizedPositions 透传（VertexLUT.ts:99 `!vt.usesUnquantizedPositions`）→ 变体标志拆两维[LUT|decode]（shader 解码/pre-read 分支已 1:1 预移植在 VertexTableShaders.h——kComputeUnquantizedPositionFromLUT/kPreReadVertexDataUnquantized/addVertexTable(quantized) 选择器，缺口仅在变体键与 texel 源切换[Color.ts:17 colorIndex 源 g_vertLutData4.xy]）→ §5(g) 像素锁先行。**触发面**：joeshouse-drill-v1 采集最深键 `0x3f/-b-2-0-0-0-20`（18,908B，42 元素 36+6 instances——DumpBrowse 锁零 graphics 白名单实钉）+ instances60 过冲键同形；与 TD-20 遗留"12B SimpleBuilder 拒绝入 LUT"同函数同形态先例 |  |
| TD-28（登记 2026-09-29，M-I 终审忠实度台账——**非阻断五项**；③已于 2026-09-30 M-J(2) 清偿 fe7ed85d3） | ①`MeshGeometry::hasFeatures` 未覆写（RenderCommands 的 Linear/Planar→Opaque pass 重映射偏离参考 Primitive.hasFeatures 语义）；②拾取链 elementId 64→32 截断（回放资产与引擎双端同截自洽——与参考 64-bit elementId 语义发散，当前资产键域内无碰撞实害）；~~③无 feature 表 batch 的 override 采样归属~~ ✅ **M-J(2) 清偿**（BatchUniforms._setCurrentBatch:74-89 参考语义——表存在门 + anyOverridden 全 16 位活跃门 + 空表 batch 不建 override LUT；三机制锁 + hover 像素锁）；④indexed 边 addAnimation 无条件调用（参考 Edge.ts:274-277 门 isAnimated——DanQing 恒调，行为中性零位移）；⑤`has(ComputeQuantizedPosition)` 变体门控 vs 参考恒跑（Vertex.ts 恒跑该函数，DanQing 以变体位门控——补齐非量化 LUT[TD-27]时须回参考形，与 SurfaceCommon.h:124-131 既有登记同源） | dqRender/src/render（RenderCommands/MeshGeometry/ShaderBuilder/EdgeShaderBuilder/SurfaceCommon 各对应处） | 非阻断台账：逐项在触及对应文件时按 §0/§11.8 回参考形；TD-27 清偿时第⑤项随之回形 |  |
| TD-29（登记 2026-09-30，M-L(3) 终审 Important-2；M-O(1) 增实录 2026-10-01） | 重像素锁**满载抖动族**（信号强于单例 TD-26——同构但面更大）：`RpcDumpRender.MirukuruRendersRealBackendTile` / `RpcDumpRender.Instances60RendersAllInstances` 的角落渗漏断言（cornerContentRatio 四角探针）+ `TileTreeRender.ImdlSilhouetteEdgesRenderAtExtremesAndCulledOnFace` 左右边位断言 + **`DumpOpenChain.OpensBaytownOrthographicSavedView`（M-O(1) 新实录——满载轮败、隔离复跑 ×3 全绿[content=456000px/16.286%/质心 (872,290) 正常域]）**——满载轮变动失败集、逐例隔离复跑全绿、内容指标恒在基线（Mirukuru 20.475%/质心 (998,698)）——时序敏感的像素边界判据 × 桌面负载竞态（TD-26 的 AcsDisc 为同族单例） | `samples/DisplayTestApp/tests`（RpcDumpRenderTest.cpp/TileTreeRenderTest.cpp/DumpOpenChainTest.cpp——测试域零改动，仅登记） | 留痕调查：失败即隔离复跑 ×2 记录（与 TD-26 同策略）；下次复现时抓帧时序/[SEL] 轨迹取证后收紧判据或加确定性渲染屏障；各进度段"环境态失败"注释遇本族测试时改指 TD-29 |  |
| TD-30（登记 2026-10-08，M-S S-d tallbox 取证发现——**未归因实证**） | `PrimitiveBuilder.addPolyface` → GeometryAccumulator → MeshBuilder 网格路径把装饰盒的顶点 **z 压平**（`ThematicDisplayE2ETest` 的 TallBoxDecorator 2×2×10 盒[PolyfaceBuilder 六面源数据 z∈[0,10] 实测 8 点齐]——VBO 顶点 rawPosition.z 恒 0[v_thematicIndex 全程≈0 实钉]；**法线三维正确**[侧面 slope=90° 归白端]而位置 z 失——位置特定压平；同路径的 Cesium 陈列馆 3D 多面体却呈现三维[未被同法压平的并存实证]）。与 thematic 正交（网格装配路径缺陷/用法差异未定）。**S-e 复读实锤**（盒改挂 GraphicType::Scene 后 Height 段 blueAll=1078000/1078000 全域纯蓝=ndx 恒 0——压平仍在；Slope 段侧面归白端 539008 再证法线无损） | `dqRender/src/render/GeometryAccumulator.cpp` + `MeshBuilder`（网格装配链） | 留痕调查：下次触及 GeometryAccumulator/MeshBuilder 时先复现小盒用例再归因（PolyfaceBuilder 六面盒 → addPolyface → VBO 顶点 z 恒 0 的链路——chordTolerance 焊接/居中重排/2d 判定三疑点）；暂以测试注记如实登记 |  |
| TD-31（登记+清偿 2026-10-09，M-S S-e 全二进制取证发现） | **TileAdmin 全局 fetcher 的跨测试污染**（全二进制模式专属，per-test ctest 门禁不可见）：dump 族测试把 `TileAdmin::instance()` 的 fetcher 换成 DumpTileFetcher 且不恢复（DumpOpenHelper.cpp:71——app 会话内多模型共享 dump 根的**刻意语义**，非缺陷）；后续走 `RealityTile::loadContent` 全局 fetcher 面的文件 tileset 测试（minimal-solid 等）取数命中 dump manifest NotFound → 内容永不达（baseline 全黑实证：[THM] panels (0,0,0)）。**潜伏史**：M-D 引入 dump 回放起即存在，但全二进制注册序中文件 tileset 测试（TileTreeRender）恒在 dump 套件之前，从未被触发——直到 ThematicDisplayE2E 注册到 JoesHouseHover 之后 | `samples/DisplayTestApp/tests/ThematicDisplayE2ETest.cpp`（MinimalSolidBox 重挂 FileTileFetcher）+ 同源风险面：TileTreeRenderTest（注册序侥幸在 dump 前） | **已清偿**（消费侧测试隔离：MinimalSolidBox 装载前重挂 FileTileFetcher=Application::Startup 初始面，污染邻接对拍转绿）。**教训**：全局 DI 缝隙的替换方须被每个后续消费者显式重设——新测试用文件 tileset 且注册序可能落在 dump 套件后时，必须自带 fetcher 重挂（勿依赖注册序侥幸） |  |
> 清理属独立后续 pass，不在本规则重写范围内。
