# 阶段 4 M-U Timeline 实现计划（2026-10-10）

> 里程碑：M-U（Animation/Solar Timeline——大件 5/16）。
> 基线：M-T 收口（2026-10-09，2719 项全量门禁）。权威对照面：
> `docs/DTA功能对照统计-2026-10-07.md`（Animation/Timeline ❌ 行）+
> CLAUDE.md §15 余量序（Timeline 列首）。
> 用户指令：按照计划，继续完成与 DTA 有差异的功能。

## 0. 范围勘察结论（参考全读记录）

DTA 侧（权威工作面）：
- `test-apps/display-test-app/src/frontend/Timeline.ts`（328 行，全读）——
  工具栏 Animation 下拉（Viewer.ts:382-386，tooltip "Animation / solar time"）。
  4 provider 自动选择链（`createTimelineProvider` :115-126）：
  **Schedule**（`vp.displayStyle.scheduleScript` 在场，duration=duration.low/high，
  `vp.timePoint` 读写）> **Analysis**（`settings.analysisStyle` 在场，
  fraction 0..100 ↔ `settings.analysisFraction` 0..1）> **Solar**（is3d，
  `setSunTime(time)` + `_active` 门——**首次交互前 update 恒 no-op**，参考怪癖
  原样移植）> **NoOp**。面板本体：play/pause（字形 0xea32/0xea33）、
  Duration 数字输入（step 0.1、NaN/≤0 拒绝）、slider 0..1000、message 行、
  rAF 泵（elapsed/total 回绕）。
- 引擎消费链（core 侧，全读锚点）：
  - Viewport.ts:2621-2639 step 7/8：`target.analysisFraction` 直写；
    `scheduleScript` → `target.animationBranches = AnimationBranchStates.fromScript(script, timePoint ?? duration.low)`
    + `containsFeatureOverrides` → overridesNeeded + `containsTransform` → invalidateScene。
  - `AnimationBranchState.ts`（65 行全读）——fromScript：model 级
    `getClipVector(time)` → branchStates[`modelId`]={clip}；element 级
    `getVisibility(time)<=0` → branchStates[`modelId_Node_batchId`]={omit}；
    否则 element clip。`transformNodeIds=script.transformBatchIds`。
  - `RenderCommands.ts:352-394`——`getAnimationBranchState(branch)`（branch.animationId
    查 branchStates）→ omit 早退 / clip = PushClipCommand+PopClipCommand 夹
    PushBranchCommand。
  - `FeatureSymbology.ts:161` initFromView：`script.addSymbologyOverrides(this, timePoint ?? 0)`。
  - `RenderSchedule.ts`（1491 行，核心段全读）：ModelTimeline.addSymbologyOverrides
    （:974-981）= **模型级** `getFeatureAppearance(getVisibility(time), time)` →
    `overrides.override({modelId, appearance})`（**现有 dump 可 E2E 的面**）+
    逐 element timeline addSymbologyOverrides（:844-856：batchId 路径
    `setAnimationNodeNeverDrawn`/`overrideAnimationNode`——**需 script 烘焙瓦**
    [featureTable.animationNodeIds]，现有 dump 全缺，登记）。
  - `RenderPlan.ts:59/126-168`——plan.analysisStyle + analysisTexture
    （gradient texture——M-S 既有机制）。
  - `Target.ts:125/205-206/516`——`analysisStyle` 字段 changeRenderPlan 消费；
    `analysisFraction` get/set。
  - `glsl/Animation.ts`（305 行全读）——AnalysisStyle displacement 顶点着色器
    消费（u_animLUT/auxChannels）→ **需 AuxChannel 几何**（PolyfaceAuxData），
    归 Analysis Style Example 大件，本里程碑不落（登记）。
  - `SolarCalculate.ts`（229 行全读）——calculateSolarAngles/Direction/
    SunriseOrSunset 全家（NOAA solcalc 移植）。
  - `DisplayStyleSettings.ts:703-754/1339-1364`——scheduleScriptProps/timePoint/
    analysisStyle/analysisFraction（**getter 钳位 [0,1]**、setter equals 短路→
    **先 raise 后赋值**）/setSunTime（ecefLocation→Cartographic，缺席→
    Philadelphia -75.17035,39.954927）/clearSunTime/sunTime。

DanQing 侧（现状实证）：
- **已预置（ported-but-uncalled 族）**：dqApp DisplayStyle 事件全族
  （OnAnalysisFractionChanged/OnTimePointChanged/OnAnalysisStyleChanged/
  OnRenderTimelineChanged/OnScheduleScriptChanged 等——DisplayStyle.h:42-63）；
  Viewport 监听+validity 标志（Viewport.cpp:713-732）+plan 载体
  （Viewport.cpp:2222-2223，RenderPlan.h:56/59）；dqCommon FeatureOverrides
  完整动画节（animationNodeOverrides/neverDrawnAnimationNodes/
  overrideModel/overrideAnimationNode/setAnimationNodeNeverDrawn——
  FeatureOverrides.h:112-199）；dqBase CompressedId64Set；dqCommon Cartographic
  全家；RenderSystem::createClipVolume；LightSettings solar.timePoint 载体
  （M-M(1)，DumpIModelConnection 解析面 :204/:817）；TargetImpl::m_animationBranches
  **占位形**（vector<uint32_t>，TargetImpl.h:575-577——本里程碑升维）。
- **缺口**：RenderSchedule/AnalysisStyle/SolarCalculate 全家数据层；
  DisplayStyleSettings analysisStyle/scheduleScriptProps 成员 + analysisFraction
  钳位/timePoint 事件语义；DisplayStyle scheduleScript/timePoint/analysisStyle/
  sunTime 访问器；Viewport step 8 实装 + step 9 script 腿；TargetImpl
  AnimationBranchStates 升维 + analysisStyle/analysisFraction 存管；
  GraphicBranch::animationId + RenderCommands omit/clip 腿；Timeline 面板本体；
  DtaToolBars item 19 激活。

## 1. 任务分解（七件，逐件 RED→GREEN + 提交）

### U-a dqCommon RenderSchedule 数据层全量移植
RenderSchedule.ts 1491 行 → `dqCommon/PublicAPI/dqCommon/RenderSchedule.h` +
src。命名空间 RenderSchedule。组件：Interpolation；TimelineEntryProps 族
（Visibility/Color/CuttingPlane/TransformEntry + TransformComponents[position/
orientation/pivot/singleScale/scale]）；TimelineEntry（compareTo/equals）；
TimelineEntryList+Interval（findInterval 步进/线性插值门）；Timeline 基类
（visibility/color/transform/cuttingPlane 四条线的求值 + duration 扩展 +
getCuttingPlane 世界面 + getClipVector）；ElementTimeline（batchId/elementIds
双形态[array|compressed]/getVisibility.../addSymbologyOverrides 的 batchId 腿）；
ModelTimeline（elementTimelines/transformBatchIds/omitsElementIds/
getTimelineForElement[idLo,hi 映射]/discreteBatchIds/maxBatchId/
addSymbologyOverrides 的模型级腿）；Script（fromJSON[空→nullopt]/toJSON/
compareTo[缓存 EQUIVALENCE：JS WeakMap→C++ 直比]/equals/find/duration/
containsX 族/transformBatchIds/addSymbologyOverrides/modelRequiresBatching/
getBatchIdForFeature）；ScriptBuilder（测试用构脚本面，:1277-1491）。
测试：`RenderScheduleTest.cpp` ← RenderSchedule.test.ts（533 行全移植：
interpolates transforms/visibility、ScriptBuilder 两例、Entry/Timeline/Script
相等族 8 例）。

### U-b dqCommon AnalysisStyle + DisplayStyleSettings 数据面归位
AnalysisStyle.ts 275 行 → `dqCommon AnalysisStyle.h`（immutable + defaults +
legacy props 转换 + equals；thematic 段挂既有 ThematicGradientSettings/Gradient
——TD-8 解后 optional 值语义现成）。DisplayStyleSettings：m_analysisStyle
（成员+props+setter 事件序 :729-739）；scheduleScriptProps（props 透传
:703-709，事件无短路——照参考原样）；analysisFraction 钳位语义归位
（getter :744-747、setter :748-754——现 DanQing 裸 setter 是移植缺口）；
timePoint setter 事件序 :718-723。props 结构补 analysisStyle/scheduleScript
两键（DisplayStyleSettings.h:53-54 登记勾销）。测试：DisplayStyleTest.cpp
:318 两例 round-trip 启封（:446-461 input 全字段、:575-592 overrides 面）。

### U-c SolarCalculate 全家 + setSunTime 门面
SolarCalculate.h 增 10 个 file-scope 助手 + 3 public
（calculateSolarAngles/calculateSolarDirection/calculateSunriseOrSunset——
1:1 NOAA 链，JulianDay/eqTime/declination/hourAngle 公式逐项）。
DisplayStyle3dSettings::setSunTime(timePoint, Cartographic)
（lights = clone({solar:{direction,timePoint}})——LightSettings 值语义 clone
面补 solar 段）/clearSunTime/sunTime getter。dqApp DisplayStyle::setSunTime(time)
门面：GetIModel ecefLocation 面（I11 登记恒缺席）→ Philadelphia 默认
（EQUIVALENCE：参考 IModel 分支的 cartoCenter 回退原样）。
测试：SolarCalculateTest.cpp（SolarCalculate.test.ts 直移）+ DisplayStyle3dSettings
sunTime round-trip + DisplayStyle::setSunTime 缺省地理位置锁。

### U-d dqApp DisplayStyle/Viewport 链路面实装
DisplayStyle：getScheduleScript/setScheduleScript（settings.scheduleScriptProps
→ RenderSchedule::Script::fromJSON 缓存——参考 DisplayStyleState :377-403
scheduleScript getter/setter + loadScriptReferenceFromScript :131-133；
scriptProps 直设路径同设）；getTimePoint/setTimePoint（→settings.timePoint+
事件短路）；getAnalysisStyle/setAnalysisStyle；setSunTime/clearSunTime/
getSunTime（→U-c 门面）。Viewport：timePoint/analysisFraction 访问器归位
（参考 Viewport.ts:570-585 形态）；step 8 实装（Viewport.cpp:1766-1772
占位注释替换：script 在场 → setAnimationBranches(fromScript(script,
timePoint ?? duration.low)) + containsFeatureOverrides → overridesNeeded +
containsTransform → InvalidateScene）；step 9 的 script 腿
（Viewport.cpp:1782-1809 重建块内：script 在场 → addSymbologyOverrides(
m_featureOverrides, timePoint ?? 0)——模型级 override 直接生效，
element 级 batchId 路径随烘焙瓦登记）。

### U-e dqRender Target/RenderCommands 消费面
AnimationBranchState 移植（dqRender AnimationBranchState.h：
AnimationBranchState{RefPtr<ClipVolume> clip; bool omit}；
formatAnimationBranchId(modelId, branchId)——branchId<0→modelId；
fromScript 1:1——model 级 clip + element 级 omit/clip + transformNodeIds）。
TargetImpl：m_animationBranches 升维
（`std::map<std::string, AnimationBranchState>` + `std::set<uint32_t>
m_transformNodeIds`，替 vector 占位——setAnimationBranches 语义对参
考 :211-217）；analysisStyle 存管（`dqCommon::AnalysisStyle const*`，
changeRenderPlan 消费 plan.analysisStyle——RenderPlan 增 optional 载体+
equals 维）；analysisFraction 存管（changeRenderPlan 消费 plan 载体
——DanQing 屋型，参考 step 7 直写语义等价的 EQUIVALENCE 登记）。
GraphicBranch::animationId 字段 + TileDrawArgs graphics.animationId=tree.modelId
核对接线（M-H/M-K 面，缺则补）。RenderCommands：getAnimationBranchState +
pushAndPopBranchForPass 的 omit/clip 两腿（:352-394 形——omit 早退；
clip=PushClipCommand/PopClipCommand 夹 PushBranchCommand，空命令回收
推/弹对）。getAnimationTransformNodeId 移植（Target.ts:1168-1176——
holder 形；逐 batch 过滤消费随烘焙瓦登记）。

### U-f Timeline 面板（Animation 下拉激活）
Timeline.ts 328 行 → `samples/DisplayTestApp/src/Gui/TimelinePanel.{h,cpp}`：
ITimelineProvider 接口（duration/getDescription/update/getCurrentTime/
onInteraction）+ 4 provider 1:1（Schedule：duration=script.duration[秒 epoch]，
getCurrentTime=timePoint??low；Analysis：0..100↔fraction；Solar：构造算
sunrise/sunset（calculateSunriseOrSunset 当日+cartoCenter——非 geolocated
dump 走 Philadelphia 默认）、`_active` 门原样[onInteraction 才置 true]、
is3d 门；NoOp）。TimelinePanel QWidget：play/pause QToolButton
（setGlyphIcon 0xea32/0xea33）、Duration QDoubleSpinBox（0.1 步、参考
onchange 校验 NaN/≤0 拒绝）、QSlider 0..1000、message QLabel；QTimer 16ms
泵代 rAF（EQUIVALENCE：rAF 帧泵↔16ms 定时器）；onViewChanged 换 provider
（挂 Viewport 的 OnViewChanged 事件——参考 ToolBarDropDown.onViewChanged
契约 :218-227）。DtaToolBars item 19：SectionsPanel 同形接法
（QToolButton InstantPopup + openDropDown）。锁（Authored——DTA 无
Timeline 测试面，行为锚=Timeline.ts handler 体逐条）：provider 选择矩阵
四态、slider 拖动→elapsed/provider.update 映射、solar active 门两段
（未交互 update 不写/交互后写）、play/pause 状态机与按钮互斥、duration
非法拒绝、updateDuration 保 fraction、model 换 provider 重选。

### U-g E2E 像素锁 + 收口
- **Solar E2E**（Authored §5(g)）：instances60 saved 视图（solar 灯在位
  M-M(1)）→ 测试缝 setSunTime（正午 vs 昏刻）→ 受光面亮度分布位移像素锁
  + clearSunTime 恢复。判据：位置断言（明暗带 WHERE）+ 恢复逐位。
- **Schedule E2E**：测试缝注入 model 级 visibility script（instances60 单
  model，0% ↔ 100% 两档 Step 插值）→ step 8/9 链 → 内容塌缩/恢复像素锁
  （模型级 override 路径，U-d/U-e 的 E2E 直接证据）。
- **Analysis 锁**：fraction set/get round-trip + 事件恰一次 + 钳位
  [0,1] 边界（-0.5→0、1.5→1）。
- 收口：CLAUDE.md §1 增 M-U 段（七件全账+E2E 锁证据）+ §15 基线
  （大件 5/16、余量 11 续排 M-V）；统计台账 Animation 行 ❌→✅；
  全量门禁。

## 2. 数据面边界（EQUIVALENCE 预登记，§11.10 制度）

| # | 边界 | 处置 |
|---|---|---|
| E1 | element 级 schedule visibility/color/transform（batchId 路径）需 script 烘焙瓦（featureTable.animationNodeIds/imdl node 拆分——ParseImdlDocument.ts:360-460） | 数据层+override 接口全移植（FeatureOverrides 既有），渲染消费随烘焙瓦落地登记；E2E 走模型级路径 |
| E2 | AnalysisStyle displacement 视觉需 AuxChannel 几何（glsl/Animation.ts 全链） | 归 Analysis Style Example 大件；本里程碑 fraction 流+setter 语义+provider 链 |
| E3 | renderTimeline（RenderTimeline 元素 RPC 面） | dump 数据面缺席；scheduleScriptProps 直设面可用（参考同优先级语义 :682-684） |
| E4 | `vp.iModel.isGeoLocated`/`spatialToCartographicFromEcef` | I11 登记恒 false→Philadelphia 默认（参考回退原样） |
| E5 | rAF 泵 → QTimer 16ms | 帧泵语义等价登记 |
| E6 | RenderSchedule JS WeakMap 比较缓存 | C++ 直比（无 GC 弱引用语义，纯性能制品） |
| E7 | DanQing plan.analysisFraction/timePoint 载体 vs 参考 step 7/8 直写 | M-T AO 同型屋型（plan 载体→changeRenderPlan），语义等价登记 |

## 3. 门禁标准
- 每任务：定向测试 RED→GREEN 双向 + 受影响族回归绿。
- 里程碑收口：全量 ctest（Debug）零失败（环境态族按 TD-26/29 规程隔离归因）。
