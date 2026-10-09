# 阶段 4 M-Q：Rendering Style 14 预设（display-test-app ViewAttributes 渲染样式族）

**立项**：用户 2026-10-06 选定（DTA 对照矩阵大件 15 余量中第 2 件——`docs/DTA功能对照与专业化分析-2026-09-30.md` §2.4 行 "Rendering Style 14 预设｜❌[引擎]（overrideDisplayStyle/envJSON 链未移植）｜大件"）。

**工作流**：勘察（本文 §1-§3）→ 逐件 RED→GREEN + 回归 + 逐件提交 → 收口（全量门禁 + CLAUDE.md §1 增 M-Q 段 + §15 基线 + 完成实录）。纪律：§0 来源铁律 + §11.8 开工前实读 + §5 RED 先行 + §6 五维对照。

---

## 1. 参考实证账（锚）

- **预设表**：`test-apps/display-test-app/src/frontend/ViewAttributes.ts:27-268`——
  `RenderingStyle = DisplayStyle3dSettingsProps & {name}`；`renderingStyleViewFlags`
  基底（:31-42——10 位：noCameraLights/noSourceLights/noSolarLight/visEdges/
  hidEdges/shadows/monochrome/ambientOcclusion/thematicDisplay/renderMode=SmoothShade）
  + 14 项：None/Default/Ambient/Illustration/Sun-dappled/Comic Book/Outdoorsy/
  Schematic/Soft/Moonlit/Thematic:Height/Thematic:Slope/Gloss/Atmosphere
  （逐字段：environment[sky/ground 色值 tbgr]/backgroundColor/viewflags 覆写位/
  lights[solar dir+intensity+alwaysEnabled/ambient/hemisphere 上下色+强度/
  portrait/specularIntensity/numCels/fresnel]/hline[visible/hidden 色·pattern·
  width + transThreshold]/monochromeMode/monochromeColor/ao[Soft 全 8 叁]/
  thematic[Height: axis+z 阶梯渐变；Slope: displayMode+range[0,90]+自定义双色键]）。
- **应用链**：`ViewAttributes.applyRenderingStyle`（:283-286——`name !== "None"` 才
  `vp.overrideDisplayStyle(style)`）→ `Viewport.overrideDisplayStyle`
  （Viewport.ts:657-659——`displayStyle.settings.applyOverrides(overrides)`）→
  `DisplayStyleSettings.applyOverrides`（common/DisplayStyleSettings.ts:1003-1007
  ——`_applyOverrides` + `onOverridesApplied` 事件）：
  - base `_applyOverrides`（:1009-1076）：**viewflags 合并语义**
    （`ViewFlags.fromJSON({...this.viewFlags.toJSON(), ...overrides.viewflags})`
    ——缺席位保持现值）；backgroundColor/monochromeColor/monochromeMode/
    backgroundMap/mapImagery/timePoint/analysisStyle/whiteOnWhiteReversal/
    analysisFraction/scheduleScript/renderTimeline/subCategoryOvr/modelOvr/
    realityModelDisplay/excludedElements。
  - 3d `_applyOverrides` 覆写（:1186-1215）：environment（`Environment.fromJSON` 整替）/
    hline（HiddenLine.Settings.fromJSON）/ao/solarShadows/**lights**
    （LightSettings.fromJSON）/planProjections/thematic/contours。
  - 事件消费：`DisplayStyleState.registerSettingsEventListeners`
    （DisplayStyleState.ts:1006-1016——thematic Height 无 range 时以
    projectExtents.z 为默认域）；设置变更 → ChangeFlags.displayStyle →
    invalidateRenderPlan + onDisplayStyleChanged（Viewport.ts:2710-2712）。
- **UI**：`addRenderingStyles`（ViewAttributes.ts:261-281——"Rendering Style: " 下拉、
  entries=名称×index、value=0、handler=`applyRenderingStyle(renderingStyles[index])`；
  **3d only**：`view.is3d() ? "" : "none"`）。
- **测试**：`core/common/src/test/DisplayStyle.test.ts:554-588`
  "overrides selected settings"——应用键输出=overrides 值、缺席键=原值
  （merge 语义直锁；§5 优先级 2 直移）。:519-552 "creates selective overrides"
  （toOverrides 选择性导出）**范围外**（renderingStyle 面无消费者——登记）。

## 2. DanQing 现状断点清单

**已在**（勘察实证）：
- `DisplayStyleSettings::applyOverrides` + `DisplayStyle3dSettings::applyOverrides3d`
  （DisplayStyleSettings.cpp:81-140）：viewflags/bg/mono×2/analysis×2/renderTimeline/
  timePoint/clipStyle/wowR + environment/thematic/hline/ao/**lights**——覆盖面齐。
- **灯光全链 1:1 已通**（M-M(1) 面，勘察再证）：LightSettings 7 字段 fromJSON
  （LightSettings.cpp:131-142）→ LightingUniforms 16 槽 pack（含 hemisphere 上下色
  [5,8]/specularIntensity[13]/numCels[14]/fresnel[15] 负号=反转）→ u_lightSettings[0]
  上传（ShaderBindings.cpp:46-58）→ shader 消费（LightingShaders.h：hemisphere :76/
  fresnel :101-112/**cel 着色 :116-122**）→ RenderPlan.lights（:2206 填充 +
  equals 失效）。
- monochrome（M-O(1)：mode 枚举 + u_monoRgb）/hline 编辑器（M-O(4) P6）/
  environment 编辑器（P7：sky 渐变重建链）/Display Style 下拉壳（P5——单条）。

**断点**：
1. **viewflags 替换非合并**：`applyOverrides` 的
   `m_viewFlags = ViewFlags::fromJSON(&*props.viewflags)`（:82）——缺席位回
   `asBool(value_or(false))` 默认；参考合并保持现值。**后果**：应用任一预设会把
   grid/acsTriad/constructions/styles/weights/textures/materials/fill/transparency
   等不在 renderingStyleViewFlags 的位重置（用户开着的 grid 会被关）——真语义缺口。
2. **Viewport::overrideDisplayStyle 缺席**：props→apply→失效链无入口；且
   applyOverrides 直改成员**绕过 per-section setter 事件**（OnEnvironmentChanged
   不发 → 天空球不重建；render plan 经 equals 比对可刷新 [viewflags/bg/lights/mono
   在 plan 内] 但 environment/sky 不在 plan——需按 props 在场段发对应失效）。
3. **14 预设表缺席**（Gui 侧）。
4. **Rendering Style 下拉缺席**（ViewSettingsPanel——P5 下拉下方插入位；3d 门）。
5. `toJSON3d` 的 lights 段 TODO（DisplayStyleSettings.cpp:126——props 断言需要值
   断言或补 toJSON；Lights toJSON 已在 LightSettings.cpp:144+ ——接线即可）。
6. solarShadows/planProjections/contours 字段未解析（applyOverrides3d 尾 TODO）。

## 3. 范围裁决

- **in**：14 预设全量数据表 + viewflags 合并语义 + Viewport::overrideDisplayStyle
  （含失效链）+ ViewSettingsPanel 下拉 + DisplayStyle.test.ts merge 测试移植 +
  代表性预设 E2E 像素锁。
- **out/登记**：
  - AO/Thematic/Atmosphere 的**视觉面**（Ambient/Soft 的 ambientOcclusion、
    Thematic:Height/Slope 的主题染色、Atmosphere 的 ground/atmosphere 装饰）——
    对应渲染 pass 未移植（各自为独立大件）；本件数据面 1:1 写入（viewflags 位 +
    ao/thematic props），视觉面部分无效——EQUIVALENCE 登记 + 数据面锁。
  - solarShadows（Sun-dappled 的 shadows viewflag 数据面写入、阴影 pass 缺）、
    planProjections、contours——字段解析随消费面立项（登记）。
  - toOverrides 选择性导出——renderingStyle 面无消费者（登记，非缺口）。

## 4. 分件

### Q-a 引擎：applyOverrides 合并语义 + Viewport::overrideDisplayStyle

- **落点**：`DisplayStyleSettings.cpp`（viewflags 合并：`fromJSON` 前以现值填充
  缺席位——参考 spread 语义）+ `LightSettings toJSON 接入 toJSON3d`（:126 TODO）+
  `Viewport.{h,cpp}` `overrideDisplayStyle(DisplayStyle3dSettingsProps const&)`
  （applyOverrides3d + 按 props 在场段失效：viewflags/bg/lights/mono 在场 →
  InvalidateRenderPlan[plan.equals 失效自然]；environment 在场 → 天空重建失效
  [复用 P7 链的等价失效面——开工实读 OnEnvironmentChanged 消费者]；hline 在场 →
  边变体失效[树 Id/edgeOptions 面——P6 同款]）。
- **RED 锁**：DisplayStyle.test.ts:554-588 "overrides selected settings" 直移
  （**RED 实证断点 1**：viewflags 缺席位被重置——grid=true 应用预设后回 false）+
  overrideDisplayStyle 失效链锁（环境在场 → 天空重建计数；lights 在场 → 渲染计划
  版本变化）。
- **开工前实读**：DisplayStyleSettings.ts:1003-1076 全文 + DisplayStyle.test.ts
  :554-588 + DanQing P7 环境失效链/P6 边失效面。

### Q-b 预设表：RenderingStyles（Gui）

- **落点**：`samples/DisplayTestApp/src/Gui/RenderingStyles.{h,cpp}`——14 项
  逐字段 1:1 表（ViewAttributes.ts:44-268；tbgr 色值原样）+
  `applyRenderingStyle(Viewport&, index)`（None no-op :283-286）。
- **RED 锁**：表完整性（名称序 14 + 代表字段精确值：Illustration hline visible
  color=0 pattern=0 width=1 + noCamera/noSource/noSolar/visEdges；Moonlit
  monochromeColor=7897479 + solar intensity=3 alwaysEnabled + hemisphere
  lowerColor (83,100,87)；Comic Book numCels=2 + solar intensity 1.95 +
  visible width=3；Soft ao 8 参；Thematic:Height axis=(0,0,1)+
  SteppedWithDelimiter；Thematic:Slope range[0,90]+Custom 双色键 0x404040/
  0xffffff；Sun-dappled shadows=true + solar dir[0.939,…,-0.328]）+
  applyRenderingStyle None no-op。
- **开工前实读**：ViewAttributes.ts:31-268 逐字段。

### Q-c UI：ViewSettingsPanel Rendering Style 下拉

- **落点**：ViewSettingsPanel（Display Style 下拉块下方——参考 ViewAttributes 构造
  序 :292 addRenderingStyles 位于 addRenderMode 前；DanQing 面板序以 P5 块为锚）：
  "Rendering Style: " QComboBox（14 项名称；handler=applyRenderingStyle[viewport]；
  3d only 显隐——面板本身 3d 场景，门随视图态）。
- **RED 锁**：wiring（下拉项数/名称序/选中触发 viewport 样式变化——Illustration
  后 viewflags.visEdges==true + noCameraLights 等；None 不改）。
- **开工前实读**：ViewAttributes.ts:261-281 + ViewSettingsPanel.cpp 现结构。

### Q-d E2E 像素锁（DtaTest）

- **判据草案**（minimal-solid 三盒 + 黑背景 + 逐预设；§5(g)/§11.11 WHERE）：
  - **Schematic**：backgroundColor=16777215（白）→ 背景像素变白 + visEdges 黑边
    （背景色判据 + 边存在性）；
  - **Illustration**：noCamera/noSource/noSolar → 无方向光（面板亮度塌缩 vs 基线）
    + visEdges 黑边像素出现（hline visible color=0 width=1）；
  - **Moonlit**：monochrome 位 + monochromeColor 7897479 → 全帧单色灰绿族
    （内容像素 r≈g≈b 收敛判据）；
  - **Comic Book**：numCels=2 cel 量化 → 亮度阶梯（直方图离散簇 vs 基线连续）；
  - **grid 保位（merge 语义 E2E）**：开 grid → 应用 Default → grid 像素仍在。
- **开工前实读**：DtaTest 既有像素锁样板（TileTreeRenderTest/JoesHouseEdge）。

## 5. EQUIVALENCE / 裁决预登记

| # | 面 | 预期发散 | 验证法 |
|---|---|---|---|
| E1 | viewflags 合并 | 无（1:1 补齐——本件核心） | merge 测试（RED 起点） |
| E2 | AO/Thematic/Atmosphere 视觉面 | 数据面写入、视觉 pass 缺 | 数据面锁（viewflags 位 + props round-trip）+ 登记注；**Thematic 半勾销（2026-10-09 M-S 清偿——Thematic 视觉面全链上屏[E2E 五锁]）+ AO 半勾销（2026-10-09 M-T 清偿——HBAO 屏空间遮蔽全链上屏[E2E 交角锁]）；仅 Atmosphere 维持登记** |
| E3 | solarShadows/planProjections/contours 字段 | 未解析（消费面未立项） | Sun-dappled shadows 位数据面断言 + 登记注 |
| E4 | 2d 显隐门 | DanQing 面板无 2d 场景（门按参考落、无 2d 测试面） | 代码面登记 |
| E5 | envJSON 采集面（矩阵行原文"envJSON 链"） | 预设经 C++ 表非 JSON 加载——等价承载（数据同源） | 表完整性锁 |

## 6. 门禁与收口

- 每件：目标面回归（新增锁 + 相邻：DisplayStyleSettings/ViewFlags/LightSettings/
  DisplayStyleSwitch[P5]/EnvironmentEditor[P7]/HiddenLineOverride[P6] 族）+ 增量提交。
- Q-a 触渲染核心：既有像素回归族抽跑（TileTreeRender/RpcDumpRender/DumpOpenChain
  + JoesHouse×3——applyOverrides 为既有路径的字段级修正，merge 语义只影响
  override 调用者[当前零调用者=纯新增面，零回归预期]）。
- 收口：全量 ctest（TD-26/TD-29 规程）+ CLAUDE.md §1 增 M-Q 段 + §15 基线 +
  本文档完成实录。

---

## 完成实录（收口时回填）

四件全清（Q-a..Q-d，2026-10-06，提交 4 笔）：

| 件 | 提交 | 锁 | 门禁 |
|---|---|---|---|
| Q-a 合并语义 + overrideDisplayStyle | d15962b44d | OverridesSelectedSettings[DisplayStyle.test.ts:554-588 直移——RED 实证 grid/weights 五断言红] + OverrideDisplayStyleMergesAndInvalidates[viewport 层] | dqCommonTest 476/476 + DisplayStyleSwitch 3/3 + 相邻族 |
| Q-b 14 预设表 | 62b6ffafbd | RenderingStylesTest 6 锁（名称序/hline 族/Moonlit/灯光 rig/Thematic+Atmosphere+Default/apply） | 表 6/6 + AcsDisc 复绿 |
| Q-c UI 下拉 | b18c85cbf4 | PanelComboAppliesSelectedStyle（14 项+应用+None no-op） | RenderingStylesTest 7/7 + ViewSettingsPanel 7/7 |
| Q-d E2E + wantLighting 修复 | e3b2c8e5c9 | RenderingStylePresetsPixelLock（五判据） | dqRenderTest 740 + dqAppTest 445 + DtaTest 复跑 1 环境态 |

**裁决落实**：E1 合并语义 1:1 补齐（Q-a 核心）；E2 AO/Thematic/Atmosphere 视觉
面数据面写入+EQUIVALENCE 注（RenderingStylesTest 数据锁）；E3 solarShadows
位数据面（Sun-dappled 表锁）；E4 3d 门按参考落（面板即 3d 场景）；E5 C++ 表
承载（表完整性锁）。

**引擎顺带修复**：①SurfaceGeometry ApplyLighting 视图级门（wantLighting =
SmoothShade && vf.lighting——SurfaceGeometry.ts:35-37/331；此前仅几何级
isLit，三灯全关不熄光照的移植缺口）；②toJSON3d lights 段接线。

**收口门禁**（2026-10-06，Debug，全量 ctest）：**2687 项 = 2657 通过 + 27 跳过 +
3 失败**——三分支全隔离复跑绿归因（MaximizeKeepsGridVisible=真窗口挂起被手杀
[桌面态污染族]；MinimizeRestore=TD-29 族 ×2 绿；GltfDecorationTool=M-O(3) 已录
flake）。

**取证/插曲实录**：①内核态僵尸进程 saga（昨夜卡滞轮残留锁 exe——用户任务
管理器手杀清障）；②**TD-26 归因直接实证**：同二进制 ghost 在→step-0 呈现
分裂败、ghost 清→AcsDisc 通过——僵尸进程污染 CAPTUREBLT 屏幕捕获层实锤
（CLAUDE.md TD-26 行随 M-Q 收口更新）；③E2E 判据三次订正实录（Illustration
变亮=反照率语义非变暗/Moonlit 参考不置 monochrome 位/黑边=资产无边表——
判据设计先核参考语义的再教训）。
