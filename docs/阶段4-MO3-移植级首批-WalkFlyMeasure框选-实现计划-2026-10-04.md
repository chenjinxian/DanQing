# 阶段 4 M-O(3) 实现计划：移植级首批（P1 Walk/Fly/LookAndMove + P2 Measure + P3 框选/ctrl/hit-cycling）

日期：2026-10-04 · 状态：勘察完成待执行 · 权威账：`docs/阶段4-MO-DTA全功能补齐与非DTA删除-实现计划-2026-10-01.md` 核销账 P1-P3（M-O(3) 去向）

**Goal:** 清偿核销账 M-O(3) 三件：P1 Walk/Fly/LookAndMove 视图工具 + handle 子类、
P2 Measure distance、P3 框选/ctrl 增选/hit-cycling。

**Tech Stack:** C++20 / Qt6（仅宿主层）/ GoogleTest / 参考仓 itwinjs-core @ 88da8fb9。

**Spec:** CLAUDE.md §15 + §0/§11.8/§5/§11.11 纪律不变（每项先完整读参考源码；
RED 锁先行；视觉特性 WHERE 断言）。

---

## 核销账（参考锚实读 + DanQing 现状勘察，2026-10-04）

| # | 项 | 参考锚（已实读） | DanQing 现状 | 缺口 |
|---|---|---|---|---|
| P1 | Walk/Fly/LookAndMove | `ViewTool.ts`：**NavigateMotion** :1747-1921（takeElevator/modifyPitchAngleToPreventInversion/generateMouseLookTransform/generateRotationTransform/generateTranslationTransform + pan/look/travel 合成）/ **ViewNavigate** :1924-2003（abstract——getNavigateMotion + animate 逐帧 frustum 乘 transform + onReinitialize 相机归位[lensAngleMatches/setCameraLensAngle/enforceZUp] + drawHandle 圆环光标锚）/ **ViewLookAndMove** :2006-2100（输入向量 + _lastCollision + touch sticks）/ **ViewWalk** :2952-2990 / **ViewFly** :2993-3032 / 工具 **LookAndMoveTool** :3107-3156（focusHome + 键盘指令面）/ **WalkViewTool** :3161-3178 / **FlyViewTool** :3183-3199 + ViewHandleType.Walk/Fly/LookAndMove 位 + ToolSettings.walkVelocity/walkCameraAngle/walkEnforceZUp | **AnimatedHandle/ViewHandleArray/ViewManip 完整在树**（Task 10——ViewPan/Rotate/Scroll/Look 同构；animate 逐帧链[Animator 接口] + ViewManipPriority + startHandleDrag + provideToolAssistance keyed 面[M-O(2) 3i]）；InputState isShiftDown/isControlDown 在；Viewport setupViewFromFrustum/getFrustum 在 | NavigateMotion + ViewNavigate + 三 handle + 三工具 + **InputState 键位跟踪面**（WASD/QE/+- 的 keyDown 集——onKeyTransition 管道在，InputState 无键集）+ ToolSettings 三参 + focusHome（键盘焦点——Qt 宿主缝）+ ViewHandleType 三位 |
| P2 | Measure distance | `MeasureTool.ts:185-733` MeasureDistanceTool（PrimitiveTool：onPostInstall/showPrompt + setupAndPromptForNextAction + getSnapPoints + displayDynamicDistance[动态线段+文本] + displayDelta + createDecorations[终段标记] + onMouseMotion + reportMeasurements + updateTotals + getMarkerToolTip[HTMLElement→Qt tooltip 缝] + onDataButtonDown 收点/onResetButtonUp 接受/onUndoPreviousStep 撤销 + getReferenceAxes）+ Translate en 串 | PrimitiveTool/DecorateContext/canvas 装饰面（drawCanvasDecorations——ACS triad 先例）/AccuSnap（snap modes M-M(6)）/QToolTip 宿主（M-O(1) hover tooltip）在 | 工具全链 + **canvas 文本装饰**（距离标签——Canvas2d 面 text 绘制）+ Translate 串 + GraphicLine 装饰（reference 用 builder 直线——DanQing canvas 直线先例在） |
| P3 | 框选/ctrl/hit-cycling | `SelectTool.ts`：selectByPointsDecorate :287-328（矩形/交叉线 canvas 装饰）/ selectByPointsProcess :329-357（SelectionMethod Box/Line + ctrl→Invert 分流）/ selectByPointsStart :359-369 / selectByPointsEnd :371-391 / onMouseStartDrag→start / onMouseEndDrag→end :428-439 / processHit :408-426（ctrl→Invert）/ **hit-cycling :480-502**（currHit + doLocate 轮转）+ `ElementSetTool.getAreaSelectionCandidates` :637-721（**readPixels[Feature] 矩形遍历**——Box: 2px inset outline 带 inside/outside 分流[allowOverlaps 选完全在内]；Line: 投影 fraction 距离 <1.5px）+ getAreaOrVolumeSelectionCandidates :726-744（volume 分支——ToolSettings.enableVolumeSelection 关时恒 area） | **onMouseStartDrag/onMouseEndDrag 路由在**（ToolAdmin Task 7——InteractiveTool 虚面 + dispatch 全链）；SelectionTool Step 3 完整（stub 地图自注 :77-228：selectByPoints 恒 false + hit-cycling TODO）；pick pass（featureId 回读）在（M-I(2) PickAtPoint 链） | selectByPoints 四方法 + **rect-pick 原语**（pick pass 矩形回读→featureId 集合——PickAtPoint 的矩形扩展）+ SelectionMethod/SelectionProcessing ctrl 分流 + 矩形 canvas 装饰 + hit-cycling（LocateManager.currHit 面——DanQing 无 LocateManager：hit 列表经 pick buffer 深度序，cycling=同像素第 N 深？参考=AccuSnap currHit 轮转；**执行时定等价面**） |

---

### Task 1: P1 Walk/Fly/LookAndMove

**Files:**
- Modify: `dqApp/PublicAPI/dqApp/ViewTool.h` + `dqApp/src/ViewTool.cpp`（NavigateMotion + ViewNavigate + ViewWalk/ViewFly/ViewLookAndMove + 三工具 + ViewHandleType 三位）
- Modify: `dqApp/PublicAPI/dqApp/ToolAdmin.h/.cpp`（InputState 键位跟踪集——onKeyTransition 落键集；ViewNavigate.getInputVector 消费）
- Test: dqAppTest（NavigateMotion 数学锁：mouseLook/rotation/translation 变换逐值[合成视图] + ViewNavigate 输入向量→motion 分流 + 工具注册/toolAssistance）+ DtaTest 可选像素锁（walk 一帧位移断言）

**Steps:**
- [x] ①读参考全文：ViewTool.ts:1747-2100（NavigateMotion + ViewNavigate + LookAndMove）+ :2952-3032（Walk/Fly）+ :3107-3199（三工具）+ ToolSettings walk* 三参 + focusHome :82。
- [x] ②RED：NavigateMotion 数学锁（构造视图 + 变换逐项）+ 输入向量锁（键集→向量）。
- [x] ③GREEN：全链落地（handle animate 逐帧 → frustum 乘 transform → setupViewFromFrustum——AnimatedHandle 先例）。
- [x] ④回归：ViewTool/LookTool/EventDispatch 族 + ToolAssistance 面。
- [x] ⑤门禁 + Commit。

### Task 2: P2 Measure distance

**Files:**
- Create: `samples/DisplayTestApp/src/Gui/MeasureDistanceToolHost.{h,cpp}`（或直接入 dqApp tools——MeasureTool.ts 在 core/frontend，归 dqApp；执行时按 SelectionTool 先例定）
- Test: 锁 = 距离计算/接受链（合成点列）+ DtaTest 像素锁（动态线段装饰出现/终段标记）+ tooltip 面

**Steps:**
- [x] ①读参考全文：MeasureTool.ts:185-733 + CoreTools Translate 串（Measure.*）。
- [x] ②RED：接受链锁（点列→segments→distance 数学）。
- [x] ③GREEN：工具 + 装饰 + tooltip。
- [x] ④门禁 + Commit。

### Task 3: P3 框选/ctrl/hit-cycling

**Files:**
- Modify: `dqApp/src/SelectionTool.cpp`（Step 3 stub 地图清偿——selectByPoints 族 + ctrl 分流 + hit-cycling）
- Modify: `dqApp/PublicAPI/dqApp/Viewport.h/.cpp`（PickAtRect——pick pass 矩形 featureId 回读原语）
- Test: DtaTest 像素锁（instances60 框选球行→选集=行内 ids + ctrl 反选 + 跨线模式）+ 引擎锁（rect-pick 原语对账）

**Steps:**
- [x] ①读参考全文：SelectTool.ts:281-502 + ElementSetTool.ts:637-744 + SelectionMethod/SelectionProcessing 枚举。
- [x] ②RED：rect-pick 原语锁 + 框选像素锁。
- [x] ③GREEN：原语 + selectByPoints 链 + 装饰 + ctrl + cycling。
- [x] ④门禁 + Commit。

### Task 4: 收口

- [x] 全量门禁（环境态族按 TD-26/TD-29 规程）+ CLAUDE.md §1 M-O(3) 段 + §15 更新 + 本文档实录脚注。

## 验证

- 每任务：对应选择器 ctest + 受影响面回归；RED→GREEN 双向留痕（§5）。
- 像素锁：§11.11 WHERE 断言（P1 位移方向 / P2 线段位置 / P3 框选集合域）。

## 决策点（执行时终裁）

1. **P1 键盘输入面**：InputState 键集（onKeyTransition 写入）vs 事件直驱 handle——倾向键集（参考 currentInputState 语义 1:1）。
2. **P2 文本装饰**：Canvas2d text（QPainter 路径）vs 仅图形 + 状态栏数字——倾向 canvas text（参考距离标签为 UI 主体）。
3. **P3 hit-cycling 等价面**：参考 LocateManager.currHit + doLocate 轮转；DanQing pick buffer 单深度——候选=pick pass 多深度回读（depthAndOrder 已有）取同像素深度序轮转；执行时按参考 doLocate(hitIndex) 语义定。
4. **P2 工具归属**：dqApp tools（core/frontend 对应）——与 SelectionTool 同位。

---

## 完成实录（2026-10-04 收口）

三件全清，提交 4 笔（计划入库 + P1/P2/P3 各一）。逐项实录：

| # | 项 | 决策落定 | 超计划面 |
|---|---|---|---|
| P1 | Walk/Fly/LookAndMove | 决策 1 落定：**事件直驱 handle.onKeyTransition**（非键集——参考 :615-618 ViewManip 路由 focusHandle 的 1:1；InputState 键集面不需要） | ToolAssistance 键盘指令族（:141-292 createKeyboardInstruction/createModifierKeyInstruction/arrowKeyboardInfo/shift·ctrl·altKey——此前登记未移植随消费面落地）；Viewport::TurnCameraOn（determineVisibleDepthRange 未移植→{0,1} 回退域 EQUIVALENCE）；enforceZUp 的 createRotationVectorToVector 轴角组合（dqGeom 无该工厂——同向恒等/反向 false=参考 undefined 语义） |
| P2 | Measure distance | 决策 2 落定：**框+白边占位**（CanvasContext 无 text/measureText 面——fillText 缺席登记；恒米制 4 位格式化 EQUIVALENCE）；决策 4 落定：dqApp（SelectionTool 同位） | EQUIVALENCE 七条登记（头文件）；reportMeasurements 的 Distance/Cumulative 分流 + keyin "measure distance" |
| P3 | 框选/ctrl | 决策 3 落定：**hit-cycling 延后**（pick 缓冲深度决胜单值无命中列表——depthAndOrder 多深度轮转需 pick 多命中面，登记） | **decorate 虚面归位 InteractiveTool**（Tool.ts:537-539——历史误置 ViewTool，ToolAdmin::Decorate 转 activeTool 全类转发，PrimitiveTool 装饰通路开面）；Box 收缩带五读差集（全域+四边缘 2-device-px 带）；Line 跨线沿线采样近似；测试缝 SetPickRectHandlerForTest 逐读回调 |

**收口登记**：
- TD-26 信号增强实录：AcsDiscSurvivesDeepZoomPixels 开发中连续隔离 ×2 同败 +
  stash 双态对照（P2 stash 后 P1 净基线同败）——预存真缺陷与 M-O(3) 改动面
  无关；**全量门禁自然通过**（间歇态与复现轮交替实录，§14 TD-26 行更新）。
- DtaToolsWiring.GltfDecoration 偶发 flake 一录：3f 随机采样贴界
  （createGltfInstanceTransform 随机 P+sR 组合贴 4.5×maxExtent 界）——
  隔离复跑绿；后续如再现可收紧界或固定种子采样集。
- ViewTool.cpp 曾因 PS 5.1 文本管道改源导致中文注释编码损坏——git checkout
  还原后经 Edit 工具重做（取证仪器教训同 M-O(2) 收口登记）。
- 全量门禁（2026-10-04，Debug）：**2618 项 = 2592 通过 + 26 跳过 + 0 失败**。
