# DisplayTestApp view 页面功能差距矩阵（DTA ↔ DanQing）

> 2026-09-10。左侧 = itwinjs display-test-app open blank connection 后 view 页面的全部功能（清单出处见《DisplayTestApp-openBlankConnection-执行路径分析.md》§5 前的调研）；右侧 = DanQing DisplayTestApp 现状（FreeCAD shell + dqApp/dqRender）。
> 状态图例：✅ 已实现且行为对齐　🔶 部分实现/等价替代/存根　❌ 未实现　➖ blank connection 下 DTA 侧也为空/不可达

## 1. 窗口框架

| DTA（Window.ts 浮动窗口） | DanQing（FreeCAD MDI） | 状态 | 说明 |
|---|---|---|---|
| 标题栏 `[ id ] viewName <viewId> (3d)`（blank→UNNAMED） | MDI 子窗口标题固定 "3D View" | 🔶 | UI 拓扑差异（预期）；无逐视口 id/viewName |
| 拖动标题栏移动、手柄调整大小 | MDI 子窗口拖移动/调整 | ✅ | 等价（Qt MDI 原生） |
| 双击标题栏 全屏⇄还原 | MDI 最大化/还原 | ✅ | 等价 |
| 关闭按钮 | MDI 关闭（closeEvent→GL 管线同步拆除） | ✅ | |
| Pin 图钉 | 无 | ❌ | |
| Dock 位掩码停靠（Full/Top/Bottom/Left/Right/四角） | MDI 标签/最大化；Windows 菜单 Tile/Cascade | 🔶 | 模型不同：MDI vs 浮动停靠 |
| Ctrl+\ 克隆 / Ctrl+\| 关闭 / Ctrl+I/M/H/L/K/J 停靠 / Ctrl+[ ] 切换 | Std_ViewCreate（克隆）、Std_CmdCloseActiveWindow 等经菜单/Windows 菜单；**无快捷键绑定** | 🔶 | 功能有、快捷键未接 |

## 2. 视口交互（ScreenViewport + IdleTool）

| DTA（IdleTool.ts:37-103） | DanQing（dqApp Viewport Qt→ToolAdmin 桥 + IdleTool） | 状态 | 说明 |
|---|---|---|---|
| 中键拖拽 = Pan | ✅ View.Pan | ✅ | EventDispatchTest 锁定 |
| Shift+中键 = Rotate | ✅ View.Rotate | ✅ | |
| Ctrl+中键 = Look（3D） | ✅ View.Look（ViewLook handle + LookViewTool，IdleTool Ctrl+中键分支恢复 allow3dManipulations?Look:Scroll） | ✅ | LookToolTest 锁定（2026-09-11，W2） |
| 滚轮 = 光标中心缩放 | ✅ processWheelEvent → doZoom | ✅ | 遗留：缩放时 triad 双影/跳跃（独立排查中） |
| 左键 = 活动工具（默认 Select：点选/框选） | ✅ SelectionTool + PickAtPoint（像素拾取） | ✅ | blank 下无可选元素（两侧一致） |
| 无活动工具时左键=Rotate / 右键=Pan | 有（IdleTool 分支已移植）；但首视口后 Select 常开 → 分支实际不可达 | ✅ | 与 DTA 一致（SVTSelect 常开） |
| 触摸（单指拖动/捏合/单击/双击 Fit） | 无触摸层 | ❌ | 桌面端低优先 |
| 视图 undo/redo 栈（20 步） | ✅ Viewport ViewPose 栈（savePose/applyPose + 防抖，ViewUndo/ViewRedo 经 Analysis 工具栏触发） | ✅ | |
| ACS triad / grid 装饰（viewFlags 驱动） | ✅ AcsTriadDecorator + PlanarGrid（createPlanarGrid(frustum)） | ✅ | RenderSmokeTest 像素锁定 |
| tooltip div / HTML overlay / Bentley logo | 无（Qt 桌面无 DOM 覆盖层） | ❌ | 平台差异 |

## 3. Viewer 工具栏（26 项逐项）

> 工具栏已按 DTA 重组（2026-09-10 起）——26 项全部上架，未实现置灰；5 条可停靠 QToolBar（Views/Selection/View Settings/View Tools/Analysis），无视口时整体置灰。2026-09-11：Surface 页 5 项（Open iModel/Blank Connection/3 示例）自工具栏移至 Start 页卡片（DTA 语义：打开前入口），直接用 DTA 图标字体字形。

| # | DTA 按钮 | DanQing 对应 | 状态 |
|---|---|---|---|
| 1 | Debug info（DiagnosticsPanel） | 无诊断窗 | ❌ |
| 2 | Open iModel from disk | Start 页 DTA 卡 "Open iModel from disk"（置灰，DTA 字形图标；无数据后端（仓外）） | 🔶 |
| 3 | Open iModel from hub | 无（无 hub/登录） | ❌ |
| 4 | 视图下拉 ViewPicker | Views 工具栏 "Views" 下拉（ViewList 重建，blank 下合成条目 "Spatial View"） | ✅ |
| 5 | Models 下拉 | 无 | ➖（blank 空） |
| 6 | Categories 下拉 | 无 | ➖（blank 空） |
| 7 | External saved views | 无 | ➖ |
| 8 | Saved camera paths | 无 | ➖ |
| 9 | Element selection（SVTSelect） | Select 工具（默认工具，首视口选择时启动） | ✅ |
| 10 | Measure distance | Std_Measure（FreeCAD 存根，activated 空） | 🔶 存根 |
| 11 | View settings 面板 | View Settings 工具栏弹出面板（ViewSettingsPanel：Render Mode + 13 viewFlags + Camera） | ✅ 面板落地（见 §4） |
| 12 | Fit view | Std_ViewFitAll + View Tools "Fit"（FitViewTool/LookAtVolume） | ✅ |
| 13 | Window area 框选缩放 | ✅ View.WindowArea（WindowAreaTool 全量移植：十字线 CanvasDecoration 2D 层 + 橡皮筋 WorldOverlay + 正交缩放路径 + undo 接线；View Tools "Window Area" 真按钮） | ✅ | WindowAreaTest 锁定（2026-09-11，W4）；相机路径 TODO（lookAt/determineVisibleDepthRange 未移植，blank 正交不走） |
| 14 | Rotate | View Tools "Rotate"（RotateViewTool） | ✅ |
| 15 | Standard rotations 8 向 | View Tools 8 按钮 + View 菜单 stdviews + 右键 Standard Views 子菜单 | ✅ |
| 16 | Walk（View.LookAndMove） | 无；已上架置灰（View Tools 工具栏 "Walk"，未实现） | ❌ |
| 17/18 | View undo / redo | Analysis 工具栏 "Undo"/"Redo" 真功能：ViewUndoTool/ViewRedoTool → Viewport ViewPose 栈（取景位姿撤销/重做，0.5s 防抖合并） | ✅ | ViewUndoTest 锁定 |
| 19 | Animation / solar time | 无 | ➖（blank 无 schedule） |
| 20 | Sectioning tools | Std_ToggleClipPlane 存根（isActive=false） | 🔶 存根 |
| 21 | Spatial Classification | 无 | ➖ |
| 22 | Override feature symbology | 无 UI（引擎层 FeatureOverrides + hilite pass 已验证） | ➖（blank 无特征） |
| 23 | Point cloud settings | 无 | ➖ |
| 24 | Contour display | 无 UI（ContourTechnique 引擎层在） | ❌ |
| 25 | Load Format Set from JSON | 无（QuantityFormatter 引擎层在） | ❌ |
| 26 | Google Maps | 无 | ➖（默认不出现） |

## 4. View settings 面板（ViewAttributes）逐项

| DTA ViewAttributes | DanQing | 状态 | 说明 |
|---|---|---|---|
| Display Style 下拉 | 无 | ➖ | blank 单样式 |
| Render Mode 下拉（6 种） | 面板 RenderMode 下拉（Wireframe/Solid Fill/Hidden Line/Smooth Shade，对齐 ViewAttributes.ts:425-428 的 4 项；CrossingEdges/HiddenLineVisibleEdges 引擎未移植） | ✅ | 面板落地 |
| Rendering Styles 预设（Illustration/Gloss/Atmosphere） | 无 | ❌ | |
| View Flags：ACS Triad | View Settings 工具栏 "ACS" 开关 | ✅ | |
| View Flags：Grid | "Grid" 开关 | ✅ | |
| View Flags：Fill/Materials/Textures/Constructions/Transparency/LineWeights/LineStyles/ClipVolume/ForceSurfaceDiscard/WhiteOnWhite/Monochrome | 面板 13 项复选组全部落地（写入 DisplayStyle viewFlags） | ✅ | 面板落地 |
| Camera 开关（+Scaled） | View 菜单 Std_OrthographicCamera/Std_PerspectiveCamera（onMsg→TurnCameraOff/EnableCamera）；另：View Settings 面板含 Camera 复选（面板落地） | ✅ | 无 Scaled 子项 |
| Environment editor（sky/ground） | "Sky" 开关（toggleSkyBox）；无编辑器 | 🔶 | 开关 ✅、编辑器 ❌ |
| Background Map 区（8 项开关） | backgroundMap viewFlag 引擎在（manufacture 置 ON）；无 UI/无网络瓦片 | 🔶 | blank 无网络——DTA 侧同样无图 |
| Edge Display（Visible/Hidden/Smooth） | 无（边渲染引擎未接 UI） | ❌ | |
| Ambient Occlusion | 无 | ❌ | |
| Thematic Display | 引擎层 ThematicUniforms 在；无 UI | ❌ | |

## 5. Surface 级框架

| DTA | DanQing | 状态 | 说明 |
|---|---|---|---|
| 顶部工具栏（Open iModel / Blank / 3 个 Example） | Start 页 DTA 卡区（2026-09-11 起，New File 最前 5 卡，DTA 图标字体字形）：Open Blank Connection ✅ 真功能；Open iModel from disk + 3 示例置灰 | 🔶 | Blank ✅；Example/iModel 入口置灰待后端 |
| Keyin 输入框（` 聚焦，工具 parseAndRun） | 无 keyin UI | ❌ | dqApp 有 ToolAdmin 工具注册表，缺 keyin 前端 |
| Snap modes 下拉 | AccuSnap 引擎在（NearestKeypoint 默认）；无 UI | 🔶 | |
| Tile load indicator / FPS monitor | 无 | ❌ | |
| Notifications 浮动窗 | NotificationManager → 主窗口状态栏 | 🔶 | 等价替代（状态栏 vs 浮动窗） |
| 打开文件（.bim） | 无（无数据后端（仓外）） | ❌ | 依赖仓外数据/内核路线 |
| glTF 导入 | View3DInventor::loadGltf（DanQing 独有，DTA 无此工具栏入口） | ✅ | DanQing 超出项 |

## 6. blank 初始状态不变量（已对齐，执行路径分析 §2）

| 不变量 | DTA | DanQing | 状态 |
|---|---|---|---|
| 取景 origin/extents/Top 正交 | (-1000,-1000,-100)/(2000,2000,200) | 同（ViewList→manufactureSpatialView→CreateBlank） | ✅ |
| viewFlags（backgroundMap/lighting/SmoothShade/grid OFF/acsTriad OFF） | — | 同 | ✅ |
| 白底 + 天空开 | ColorDef.white 0x00FFFFFF | 同（本轮修复 alpha） | ✅ |
| 标题 UNNAMED | `[ 1 ] UNNAMED <0x0> (3d)` | "3D View"（MDI 固定标题） | 🔶 UI 差异 |

## 汇总

> 统计口径：按本矩阵 §1–§6 表格行实点（共 65 行；§3 的 #17/18 undo/redo 合并行计 1 行），每行按状态列唯一计数。

- ✅ 已对齐：28 项（视口核心交互、标准视图、Fit、Window area 框选缩放、Ctrl+中键 Look、相机开关、View Settings 面板、Grid/Sky/ACS 开关、选择工具、视图 undo/redo 栈、blank 不变量 3 项等）
- 🔶 部分/存根/等价：12 项（窗口模型/标题、Open/Measure/剖切存根、Environment/BackgroundMap 半落地、Snap/通知替代等）
- ❌ 未实现：15 项（Walk、keyin、诊断/FPS/瓦片指示、Edge Display UI、AO/Thematic UI、Rendering Styles、触摸、Pin、hub 等）
- ➖ blank 下不可达：10 项（Models/Categories/SavedViews/相机路径/动画/分类/特征覆盖/点云/Google Maps/Display Style）

**建议优先级（view 页面平价所需、且 blank 可观测）**：
1. ~~View undo/redo 栈（ViewPose 栈，Viewport.ts:3164-3168）~~ ✅ 已完成（2026-09-10，view-undo-redo 计划 U1-U4：Viewport ViewPose 栈 + ViewUndo/ViewRedo 工具 + Analysis 工具栏 Undo/Redo 真功能）
2. ~~View.WindowArea（框选缩放）+ View.Look（Ctrl+中键）——补全 IdleTool 映射~~ ✅ 已完成（2026-09-11，windowarea-look 计划 W1-W4：ViewStatus/extentLimits/adjustViewDelta + ViewLook/LookViewTool + ToolAdmin 装饰器通路 + CanvasDecoration 2D 层机制 + WindowAreaTool 全量；QPainter overlay 的屏幕上合成可见性待人工实操确认，GL 后端兜底预案已备）
3. View settings 面板扩展（Render Mode + 其余 viewFlags 开关）——引擎已就绪，纯 UI
4. Keyin 输入框（`）——打开全部已注册工具的测试通道
5. 诊断/FPS 指示——调试基建
