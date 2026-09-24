# 参考对齐审计报告（2026-09-13）

**范围**：初始化 / 打开 blank connection 初始视口 / Fit / 缩放 / 旋转 / 平移 / resize。
**方法**：全部结论来自实读参考源码（行号真实），对照 DanQing 现状。禁止症状推理（CLAUDE.md §11.8/§12.8）。
**背景差异（合法，非缺陷）**：Qt 窗口 vs HTML canvas；OpenGL vs WebGL。这两层的语义等价映射是适配层职责，不构成行为差异的借口。

---

## 1. resize 链路 —— 差异最大，已确认行为近似等价但性能差 100 倍

**参考机制**（Target.ts，实读）：
| 步骤 | 参考实现 | 位置 |
|---|---|---|
| 尺寸检测 | renderFrame step7 `target.updateViewRect()`：只更新 `renderRect`；canvas 尺寸变化由浏览器自动重分配 drawing buffer | Target.ts:1330-1336 |
| 失效 | `onResized()` = `disposeFbo()`：销毁 **Target 自己的 1 个合成 FBO**（+ `_dcAssigned=false`） | Target.ts:1441-1443, 315-330 |
| 惰性重建 | 下一帧 `drawFrame` 入口 `assignDC()`→`_assignDC()`=disposeFbo+allocateFbo（按当前 viewRect）+ 重建 blitGeom（CopyColorNoAlpha quad）+ `uniforms.viewRect.update` | Target.ts:751-766, 1315-1328, 290-313, 546-549 |
| compositor | preDraw 检测尺寸变化自行重建纹理/FBO | SceneCompositor.ts |

**DanQing 现状**：`Viewport::resizeEvent` → `delete m_renderTarget + createRenderTarget`（销毁**整只** TargetImpl：compositor/commands/全部 16 FBO/OIT）。`OpenGLRenderTarget::updateViewRect` 是返回 false 的空壳（移植缺口）。
**影响**：行为近似等价（视觉一致），但 `CompositorFrameBuffers::dispose` 的 glDelete 驱动同步等待实测 80-110ms → resize 卡顿。
**修复方向**：1:1 补齐 updateViewRect（更新 m_rect）/ onResized（disposeFbo 语义：销毁 TargetImpl 自身 FBO+pick，惰性标志）/ drawFrame 入口 allocateFbo 惰性重建。**前置条件：ResizePixelConsistencyTest 已建（绿）**。

## 2. 初始化链路 —— 基本对齐

| 项 | 参考 | DanQing | 状态 |
|---|---|---|---|
| 事件循环启动时机 | ViewManager.addViewport 首个视口 → startEventLoop（ViewManager.ts:297-298） | AddViewport → StartEventLoop | ✅ |
| 每帧顺序 | processEvent → renderLoop → tileAdmin.process（IModelApp.ts:614-630） | 同序（Application.cpp EventLoop） | ✅ |
| renderSystem WebGL 参数 | 唯一显式 `powerPreference:"high-performance"`（System.ts:376-388）；antialias 经 antialiasSamples 在 render target 层 | RHI 适配层 | ✅（等价映射） |
| 默认工具 | SVTSelectionTool（"SVTSelect"）；startDefaultTool 在首次 setSelectedView | SelectionTool/IdleTool 体系 | ✅（近似） |
| window resize | `window.addEventListener("resize", requestNextAnimation)` + 1s 兜底 interval | resizeEvent → InvalidateController → RequestRedraw | ✅（近似，Qt 等价） |

## 3. 打开 blank connection 初始视口 —— 需逐字段比对 manufactureSpatialView

**参考**（ViewPicker.ts:149-169，实读）：
- `createBlank(iModel, ext.low, ext.high-ext.low, undefined)`：origin=ext.low、extents=对角线、**rotation=undefined→单位阵（Top 视图）**
- viewFlags 仅覆盖 3 项：`backgroundMap:true, lighting:true, renderMode:SmoothShade`（**grid/acsTriad 保持默认 false**）
- `backgroundColor = white`；`environment.withDisplay({sky:true})`
- **camera 保持 OFF**（props.cameraOn undefined→false）
- 视锥最终值经 `fixAspectRatio(viewRect.aspect)`（Viewport.ts:2037-2044）——用 **dock 之前**的 contentDiv 尺寸；dock 后仅 requestRedraw 不再 fix
- **打开后无自动 Fit / 无动画**（ScreenViewport.create 只 changeView，首次 doAnimate=false）

**DanQing**：ViewList::create→getDefaultView 已有对应移植（ViewPicker.cpp）——**待办：逐字段核对**（特别是 rotation、viewFlags 3 项、白背景、sky、fixAspectRatio 时机）。

## 4. Fit —— 基本对齐，2 个待核对项

**参考**（ViewTool.ts 实读）：
- 链：`FitViewTool.doFit` → `fitViewWithGlobeAnimation` → `fitView`：`computeFitRange`（computeViewRange ∩ clipVolume）→ `lookAtVolume(range, viewRect.aspect, options)` → `synchWithView({animateFrustumChange:doAnimate})` → `viewCmdTargetCenter=undefined`（ViewTool.ts:823-851）
- 无 options 时 **margin 默认膨胀 1.04**（ViewState.ts:1151-1155）；相机开启的 3D 视图不加 margin（:1110-1113）
- 动画：`{animateFrustumChange:doAnimate}`，doAnimate 默认 true → FrustumAnimator 默认 **1000ms**/Cubic.Out/cancelOnAbort=false（ViewAnimation.ts:36-45 + FrustumAnimator.ts:62）

**DanQing**：View3DInventor::viewAll → LookAtVolume + synchWithView（无动画选项）。
**待核对**：① Fit 的动画（参考 doAnimate=true 默认应动画，DanQing 现无）② `computeViewRange ∩ clipVolume`（DanQing 用 projectExtents 直传）。

## 5. 缩放（滚轮）—— 已对齐（本会话完成）

doZoom 定点缩放 + adjustViewDelta clamp（min=0.001，慢滚 ~30 格到底）+ 500ms/Cubic.Out/cancelOnAbort=true 动画 + coalesceWheelEvents（Chromium 帧合并等价层）。extents.y 的 fixAspectRatio 差异已修（skipAspectFix）。

## 6. 旋转 —— 需逐项核对

**参考**（ViewTool.ts:1180-1298，实读）：
- 角度：**θx = π·dx/width（拖满整幅屏宽 = 180°）**、θy = π·dy/height；屏幕 x 绕"preserveWorldUp 时世界 Up（有 depthPoint 用 getUpVector(depthPoint) 否则 unitZ）否则视图 Y 轴"、屏幕 y 绕视图 X 轴；合成取轴角后**取负**
- 原点保持：**绕 tool.targetCenterWorld 定点**，对保存的**基准 frustum** 施加 fixed-point 变换后 setupFromFrustum（**非增量旋转**）
- 外部 frustum 变化防御：currentFrustum≠activeFrustum 时采纳新基准（:1223-1237）
- 拖拽中无动画（每事件直接 setupFromView）；旋转期间滚轮中心改到 targetCenterWorld（:1290-1298）

**DanQing**：ViewRotate 已有移植——**待办：角度系数/轴向/定点语义逐项核对**。

## 7. 平移 —— 需逐项核对

**参考**（ViewTool.ts:1109-1167，实读）：
- 换算链：`dist = world(lastNpc) − world(thisNpc)`（**两次 NPC→世界投影之差**，比例隐含在 npcToWorld）→ `view.setOrigin(origin + dist)`；相机开启走 `moveCameraWorld(dist)`（eye+target 同加，再 lookAt）
- 起点 NPC 的 z：相机开启取深度点否则**焦点平面**（getFocusPlaneNpc：targetPoint 的 npc.z，出 [0,1] 取 0.5）
- `HandleWithInertia.doManipulation` 强制 thisPtNpc.z == lastPtNpc.z（约束在拾取平面内平移）
- 松手惯性：500ms、damping 默认 0.96（clamp [.75,.999]）

**DanQing**：ViewPan 已有移植——**待办：NPC 投影差值链/焦点平面/惯性参数逐项核对**。

---

## 修复顺序（每项先建/确认像素级回归再动）

**项 2-4 逐字段比对已完成（2026-09-13）**，最终差异清单：

### 确认 1:1 对齐（无需动）✅

- **初始视口**（manufactureSpatialView）：origin/extents/rotation（默认 identity=Top，ViewState.h:530）/viewFlags 仅 3 项覆盖/白背景/sky on/camera off/fixAspectRatio 时机（Create 时一次 + resize 后 InvalidateController→下帧 SetupFromView，与参考 renderFrame resized 分支同构）——**全绿**
- **旋转**：θx=π·dx/w、θy=π·dy/h（ViewTool.cpp:1390/1396）、xAxis 的 UnitZ 等价性已核实（getUpVector 对 extents 内的点/无 geo 返回 UnitZ，ViewState.ts:1246-1248；blank connection 无深度拾取点 → 双侧一致）、yAxis=RowX、yR·xR 取负、绕 targetCenterWorld 定点、对基准 frustum 非增量变换、onWheel 路由——**全绿**（globe 分支 TODO）
- **平移**：NPC 投影差值链（ViewPan::perform :1220-1222）、焦点平面 z、MoveCameraWorld/setOrigin 分支、HandleWithInertia 的 z 约束（:945）/惯性向量/dampen clamp [.75,.999]、惯性常量（true/0.96/500ms，ViewTool.h:225-227 vs ToolSettings.ts:72-77）——**全绿**

### 确认差异（动手清单）——**四项已全部修复（2026-09-13）**

| # | 差异 | 参考 | 修复 | 验证 |
|---|---|---|---|---|
| ① | resize | updateViewRect→onResized(销毁 1 FBO)→下帧 assignDC 惰性重建（Target.ts:1330/1441/290-313/751-766） | `TargetImpl::setViewRect`（updateViewRect+onResized 合并语义）+ `drawFrame` 入口 `allocateFbo` 惰性 + **preDraw 重建含 OIT**（对齐参考 `_fbos.init` 覆盖 translucent，SceneCompositor.ts:290-291——上次错乱的真根因） | resize 88.9ms→**6.2ms**；ResizePixelConsistencyTest + 全量 2279 绿 |
| ② | Fit aspect | `lookAtVolume(range, viewRect.aspect, options)`（ViewTool.ts:827） | `viewAll` 传 `viewRect().aspect()` | 全量回归 |
| ③ | Fit 动画 | `synchWithView({animateFrustumChange:doAnimate=true})` → 1000ms Cubic.Out | `viewAll` 传 `{animateFrustumChange:true}`（其余默认=参考：1000ms/CubicOut/cancelOnAbort=false） | 全量回归 |
| ④ | 惯性尾部 | `vp.setAnimator(this)` 驱动松手滑行（ViewTool.ts:1072） | `Viewport::setAnimatorRef`（非拥有引用——参考 GC 语义的 C++ 等价，ViewHandleArray 持有句柄）+ RenderFrame Step2/17 驱动非拥有槽 + `beginAnimation` 接线 + 惯性结束 `saveViewUndo`（过时 TODO 已修） | 全量回归 |

### 已登记 TODO（当前 blank 场景无行为影响，随子系统移植）

- computeFitRange 的 clipVolume 相交（clipVolume flag 关时 no-op）
- viewCmdTargetCenter 字段（ViewTool.cpp:719 已登记）
- globe 分支：fitViewWithGlobeAnimation/animateFlyoverToGlobalLocation/viewingGlobe/getUpVector 椭球法向（拉远到太空才触发）
- zoomToAlwaysDrawnExclusive（无 alwaysDrawn 集合时空集 false 兜底——语义等价 ✅）

### 动手顺序（每项前置回归保护）——已完成

1. ✅ ①（resize 1:1）——像素回归保护下完成
2. ✅ ②+③（Fit 的 aspect + 动画）
3. ✅ ④（惯性 setAnimatorRef 非拥有等价）
