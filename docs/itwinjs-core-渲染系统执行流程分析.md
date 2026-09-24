# itwinjs-core 渲染系统执行流程完整分析

本文档全面分析 itwinjs-core 的渲染管线架构，作为 DanQing 渲染模块的参考实现对齐依据。

**分析范围**: 启动初始化 → 帧循环 → 渲染管线 → 子系统详解

---

## 一、整体架构概览

### 1.1 核心组件关系

```
┌─────────────────────────────────────────────────────────────────┐
│                        IModelApp                                │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐    │
│  │  ToolAdmin   │  │ ViewManager  │  │    TileAdmin        │    │
│  │  (输入处理)   │  │ (视口管理)    │  │  (瓦片加载调度)     │    │
│  └──────┬───────┘  └──────┬───────┘  └─────────────────────┘    │
│         │                 │                                      │
│         ▼                 ▼                                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              requestNextAnimation()                      │    │
│  │         (requestAnimationFrame 心跳)                     │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Viewport (每个视口)                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  ViewState   │  │ ChangeFlags  │  │   RenderTarget       │  │
│  │  (视图状态)   │  │ (变更标志)    │  │  (渲染目标/后端)     │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    RenderSystem (全局单例)                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │  Techniques  │  │  IdMap 缓存   │  │   WebGL Context      │  │
│  │  (着色器管理)  │  │ (纹理/材质)   │  │   (GL 状态管理)      │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 文件位置映射

| itwinjs-core 文件 | DanQing 对应 | 说明 |
|---|---|---|
| `core/frontend/src/Viewport.ts` | `dqApp/Viewport.h/cpp` | 视口，渲染管线协调器 |
| `core/frontend/src/ViewManager.ts` | `dqApp/ViewManager.h/cpp` | 多视口管理 |
| `core/frontend/src/ChangeFlags.ts` | `dqApp/ChangeFlags.h` | 变更标志位 |
| `core/frontend/src/render/RenderSystem.ts` | `dqRender/RenderSystem.h` | 渲染系统抽象 |
| `core/frontend/src/internal/render/webgl/System.ts` | `dqRender/src/gl/GLSystem.cpp` | WebGL 实现 |
| `core/frontend/src/render/RenderTarget.ts` | `dqRender/RenderTarget.h` | 渲染目标抽象 |
| `core/frontend/src/internal/render/webgl/Target.ts` | `dqRender/src/gl/Target.cpp` | WebGL 渲染目标 |
| `core/frontend/src/render/Scene.ts` | `dqRender/Scene.h` | 场景容器 |
| `core/frontend/src/render/Decorations.ts` | `dqRender/Decorations.h` | 装饰容器 |
| `core/frontend/src/render/GraphicBranch.ts` | `dqRender/GraphicBranch.h` | 场景图节点 |
| `core/frontend/src/ViewContext.ts` | `dqApp/ViewContext.h` | 上下文类族 |
| `core/frontend/src/render/RenderGraphic.ts` | `dqRender/RenderGraphic.h` | 渲染图形抽象 |

---

## 二、动画帧循环（Heartbeat）

### 2.1 三层调度链

```
requestAnimationFrame (浏览器)
  └─> IModelApp.eventLoop()
        ├─> ToolAdmin.processEvent()      // 1. 输入/工具处理
        ├─> ViewManager.renderLoop()      // 2. 视口渲染
        └─> TileAdmin.process()           // 3. 瓦片加载
```

**`IModelApp.requestNextAnimation()`** 是整个渲染系统的唯一心跳入口：

```typescript
// IModelApp.ts:513
public static requestNextAnimation() {
  if (IModelApp._noRender) return;
  if (!IModelApp._animationRequested) {
    IModelApp._animationRequested = true;
    requestAnimationFrame(() => IModelApp.eventLoop());
  }
}
```

- 使用 `_animationRequested` 标志防止重复请求
- 需求驱动：无人请求时循环自然停止
- 可通过 `animationInterval` 设置周期性定时器保持循环

### 2.2 ViewManager.renderLoop()

```typescript
// ViewManager.ts:399
public renderLoop(): void {
  if (0 === this._viewports.length) return;

  // 全局场景失效处理
  if (this._skipSceneCreation)
    this.validateViewportScenes();
  else if (this._invalidateScenes)
    this.invalidateViewportScenes();

  this._invalidateScenes = false;
  this.onBeginRender.raiseEvent();

  // 遍历所有视口执行渲染
  for (const vp of this._viewports)
    vp.renderFrame();

  this.onFinishRender.raiseEvent();
}
```

---

## 三、Viewport.renderFrame() — 核心管线

这是每个视口每帧的唯一入口，共约 160 行代码（L2546-2703），执行 **约 24 个逻辑步骤**：

### 3.1 完整步骤分解（源码行号对照）

```
Viewport::renderFrame()  (L2546-2703)
  │
  ├── [1]  帧统计开始: _frameStatsCollector.beginFrame()              (L2547)
  ├── [2]  ChangeFlags 快照与重置                                      (L2549-2551)
  ├── [3]  缓存 view 和 target 引用                                    (L2553-2554)
  ├── [4]  创建 StopWatch 计时器                                       (L2557-2558)
  ├── [5]  动画执行: animate()                                         (L2560-2563)
  ├── [6]  初始化 isRedrawNeeded = _redrawPending || continuousRendering (L2565-2566)
  ├── [7]  尺寸变化检测: target.updateViewRect()                       (L2568-2572)
  ├── [8]  控制器同步: setupFromView()                                  (L2574-2575)
  ├── [9]  选择集更新: setHiliteSet()                                  (L2577-2581)
  ├── [10] 计算 overridesNeeded                                        (L2583)
  ├── [11] 分析分数: setAnalysisFraction()                             (L2585-2588)
  ├── [12] 时间点 / Schedule Script                                    (L2590-2603)
  ├── [13] Feature Symbology Overrides                                 (L2605-2609)
  ├── [14] 场景创建: createScene() → changeScene()                     (L2611-2628) ← 最重
  ├── [15] 渲染计划验证: validateRenderPlan()                          (L2630-2635)
  ├── [16] 装饰收集: addDecorations() → changeDecorations()            (L2637-2645)
  ├── [17] Flash 处理: processFlash() → setFlashed()                   (L2647-2652)
  ├── [18] Pre-render hook: target.onBeforeRender()                    (L2654-2658)
  ├── [19] 结束计时器                                                   (L2660-2661)
  ├── [20] 实际绘制: target.drawFrame()                                (L2662-2665) ← GPU 提交
  ├── [21] 帧统计结束: _frameStatsCollector.endFrame()                 (L2666)
  ├── [22] 尺寸变化事件: onResized                                     (L2669-2670)
  ├── [23] 变更事件分发: onViewportChanged 等                           (L2672-2698)
  └── [24] 持续渲染请求: requestNextAnimation()                        (L2701-2702)
```

### 3.2 详细流程

#### Step 1: ChangeFlags 快照与重置

```typescript
const changeFlags = this._changeFlags;
if (changeFlags.hasChanges)
  this._changeFlags = new MutableChangeFlags(ChangeFlag.None);
```

将累积的变更标志取出并重置。后续本帧新产生的变更会累积到新对象上，留给下一帧处理。

#### Step 2: 动画执行

```typescript
this._frameStatsCollector.beginTime("animationTime");
this.animate();  // 调用 this._animator?.animate()
this._frameStatsCollector.endTime("animationTime");
```

Animator 返回 `true` 表示动画完成，清除 animator。

#### Step 3: 尺寸变化检测

```typescript
const resized = target.updateViewRect();
if (resized) {
  target.onResized();
  this.invalidateController();  // 级联失效
}
```

#### Step 4: 控制器同步

```typescript
if (!this._controllerValid)
  this.setupFromView();
```

`setupFromView()` → `doSetupFromView(view)` → `ViewingSpace.createFromViewport()` + `invalidateRenderPlan()`。

#### Step 5: 选择集更新

```typescript
if (this._selectionSetDirty) {
  target.setHiliteSet(view.iModel.hilited);
  this._selectionSetDirty = false;
  isRedrawNeeded = true;
}
```

#### Step 6-7: 分析分数 & 时间点

```typescript
if (!this._analysisFractionValid) {
  target.setAnalysisFraction(this.analysisFraction);
  isRedrawNeeded = true;
}

if (!this._timePointValid) {
  // 解析 schedule script，设置 target.animationBranches
  isRedrawNeeded = true;
}
```

#### Step 8: Feature Symbology Overrides

```typescript
if (overridesNeeded) {
  const ovr = new FeatureSymbology.Overrides(this);
  target.overrideFeatureSymbology(ovr);
  isRedrawNeeded = true;
}
```

`overridesNeeded` 由 `changeFlags.areFeatureOverridesDirty` 驱动，当 AlwaysDrawn/NeverDrawn/ViewedCategories/DisplayStyle/FeatureOverrideProvider/ViewedCategoriesPerModel 变化时触发。

#### Step 9: 场景创建（最重阶段）

```typescript
if (!this._sceneValid) {
  if (!this._freezeScene) {
    IModelApp.tileAdmin.clearTilesForUser(this);
    IModelApp.tileAdmin.clearUsageForUser(this);

    const context = this.createSceneContext();  // SceneContext(this)
    this.createScene(context);                  // 填充场景图
    context.requestMissingTiles();              // 请求缺失瓦片

    this._hasMissingTiles = context.hasMissingTiles;
    target.changeScene(context.scene);          // 推送到渲染目标
    isRedrawNeeded = true;
  }
  this._sceneValid = true;
}
```

#### Step 10: 渲染计划验证

```typescript
if (!this._renderPlanValid) {
  this.validateRenderPlan();  // -> target.changeRenderPlan(createRenderPlanFromViewport(this))
  isRedrawNeeded = true;
}
```

#### Step 11: 装饰收集

```typescript
if (!this._decorationsValid) {
  const decorations = new Decorations();
  this.addDecorations(decorations);
  target.changeDecorations(decorations);
  this._decorationsValid = true;
  isRedrawNeeded = true;
}
```

#### Step 12-13: Flash & Pre-render

```typescript
if (this.processFlash()) {
  target.setFlashed(this.flashedId, this._flashIntensity);
  isRedrawNeeded = true;
}

target.onBeforeRender(this, (redraw) => {
  isRedrawNeeded = isRedrawNeeded || redraw;
});
```

#### Step 14: 实际绘制

```typescript
if (isRedrawNeeded) {
  target.drawFrame(timer.elapsed.milliseconds);
  this.onRender.raiseEvent(this);
}
```

**关键优化：仅在 `isRedrawNeeded = true` 时才调用 GPU 绘制。**

#### Step 20-21: GPU 绘制和帧统计

```typescript
if (isRedrawNeeded) {
  target.drawFrame(timer.elapsed.milliseconds);  // L2664
  this.onRender.raiseEvent(this);                // L2664
}
_frameStatsCollector.endFrame(isRedrawNeeded);   // L2666
```

#### Step 22-23: 变更事件分发 (L2669-2698)

```typescript
if (resized) this.onResized.raiseEvent(this);                          // L2670

if (changeFlags.hasChanges) {
  this.onViewportChanged.raiseEvent(this, changeFlags);                // L2673
  if (changeFlags.displayStyle) this.onDisplayStyleChanged.raiseEvent(this);  // L2676
  if (changeFlags.viewedModels) this.onViewedModelsChanged.raiseEvent(this);  // L2679
  if (changeFlags.areFeatureOverridesDirty) {
    this.onFeatureOverridesChanged.raiseEvent(this);                   // L2682
    if (changeFlags.alwaysDrawn) this.onAlwaysDrawnChanged.raiseEvent(this);  // L2685
    if (changeFlags.neverDrawn) this.onNeverDrawnChanged.raiseEvent(this);    // L2688
    if (changeFlags.viewedCategories) this.onViewedCategoriesChanged.raiseEvent(this);  // L2691
    if (changeFlags.viewedCategoriesPerModel) this.onViewedCategoriesPerModelChanged.raiseEvent(this);  // L2694
    if (changeFlags.featureOverrideProvider) this.onFeatureOverrideProviderChanged.raiseEvent(this);  // L2697
  }
}
```

#### Step 16: 持续渲染请求

```typescript
if (requestNextAnimation || undefined !== this._animator || this.continuousRendering)
  IModelApp.requestNextAnimation();
```

---

## 四、ChangeFlags 变更标志系统

### 4.1 标志位定义

| 标志 | 位值 | 含义 | 触发方法 |
|------|------|------|---------|
| `AlwaysDrawn` | `1 << 0` | 始终绘制元素集变化 | `setAlwaysDrawn()`, `clearAlwaysDrawn()` |
| `NeverDrawn` | `1 << 1` | 从不绘制元素集变化 | `setNeverDrawn()`, `clearNeverDrawn()` |
| `ViewedCategories` | `1 << 2` | 可见类别变化 | CategorySelector 事件 |
| `ViewedModels` | `1 << 3` | 显示模型集变化 | ModelSelector 事件 |
| `DisplayStyle` | `1 << 4` | 显示样式/ViewFlags 变化 | DisplayStyle 事件族 |
| `FeatureOverrideProvider` | `1 << 5` | 覆盖提供者变化 | `setFeatureOverrideProviderChanged()` |
| `ViewedCategoriesPerModel` | `1 << 6` | 按模型类别可见性变化 | `setViewedCategoriesPerModelChanged()` |
| `ViewState` | `1 << 7` | ViewState 被替换 | `changeView()` |

复合标志：
- `Overrides = All & ~(ViewedModels | ViewState)` — 影响 FeatureSymbology 的所有方面
- `Initial = ViewedCategories | ViewedModels | DisplayStyle` — 新建 Viewport 初始状态

### 4.2 级联失效链

```
invalidateDecorations()   → 仅重建装饰（最轻量）
requestRedraw()           → 仅重绘现有内容（轻量）
invalidateRenderPlan()    → 重建渲染计划 → invalidateScene()
invalidateController()    → 重建控制器 → invalidateRenderPlan()
invalidateScene()         → 重建场景 + 装饰（中等）
changeView()              → 替换 ViewState + 全部重建（最重）
```

### 4.3 DisplayStyle 事件族

DisplayStyle 标志由大量子事件触发：

```
DisplayStyle 标志触发源:
  ├── onSubCategoryOverridesChanged
  ├── onModelAppearanceOverrideChanged
  ├── onBackgroundColorChanged
  ├── onMonochromeColorChanged / onMonochromeModeChanged
  ├── onClipStyleChanged
  ├── onViewFlagsChanged
  ├── onBackgroundMapChanged / onMapImageryChanged
  ├── onLightsChanged / onSolarShadowsChanged
  ├── onThematicChanged
  ├── onAnalysisFractionChanged / onAnalysisStyleChanged
  ├── onTimePointChanged / onScheduleScriptChanged
  └── ... 更多
```

---

## 五、RenderSystem — 渲染系统

### 5.1 类层次

```
RenderSystem (abstract)                    [render/RenderSystem.ts]
  ├── webgl.System                         [internal/render/webgl/System.ts]
  └── MockRender.System                    [internal/render/MockRender.ts]
```

### 5.2 工厂方法分类

| 类别 | 方法 | 说明 |
|------|------|------|
| **高级图形** | `createGraphic()` | 创建 GraphicBuilder (abstract, L235) |
| | `createGraphicBuilder()` | 便捷包装器 (L226) |
| | `createBranch()` | 委托给 createGraphicBranch (L454) |
| | `createGraphicBranch()` | 包装 GraphicBranch + Transform (abstract, L459) |
| | `createBatch()` | 包装 Graphic + FeatureTable (abstract, L477) |
| | `createGraphicOwner()` | 防自动释放的包装 (L490) |
| | `createGraphicList()` | 图形列表（单元素优化）(abstract, L451) |
| | `createGraphicFromTemplate()` | 从模板创建 (abstract, L332) |
| | `createRenderGraphic()` | 从 RenderGeometry 创建 (abstract, L337) |
| **低级几何** | `createTriMesh()`, `createIndexedPolylines()` | 低级网格 (L249) |
| | `createMesh()`, `createPolyline()`, `createPointString()` | 几何图形 (L354-364) |
| **RenderGeometry** | `createMeshGeometry()` 等 | 不可渲染的几何表示 |
| **纹理** | `createTexture()`, `createTextureFromSource()` | 纹理创建 |
| | `getGradientTexture()` | 渐变纹理 |
| **材质** | `createRenderMaterial()` | 渲染材质 |
| **目标** | `createTarget()` | 屏幕渲染目标 (abstract, L193) |
| | `createOffscreenTarget()` | 离屏渲染目标 (abstract, L195) |

### 5.3 图形创建路径

```
高级 API:
  createGraphic(options) → PrimitiveBuilder → finish() → toTemplate() → createGraphicFromTemplate()

低级 API:
  createTriMesh(MeshArgs)
    → createMeshParams(MeshArgs)
    → createMeshGeometry(MeshParams) → MeshRenderGeometry
    → createRenderGraphic(geometry) → MeshGraphic.create()
```

### 5.4 WebGL Graphic 层次

```
RenderGraphic (abstract)
  └── Graphic (abstract, WebGL)
        ├── GraphicOwner          // dispose 不释放内部
        ├── Branch                // GraphicBranch + Transform + Clips
        ├── Batch                 // Graphic + FeatureTable + Range
        ├── GraphicsArray         // RenderGraphic[] 包装
        ├── AnimationTransformBranch
        ├── WorldDecorations
        ├── Primitive             // 单一 CachedGeometry
        └── MeshGraphic           // 网格，含多个 Primitive
```

### 5.5 资源缓存（IdMap）

```typescript
class IdMap {
  materials: Map<string, RenderMaterial>;
  textures: Map<string, RenderTexture>;
  gradients: Dictionary<Gradient.Symb, RenderTexture>;
  texturesFromImageSources: Map<string, Promise<RenderTexture>>;
}
```

按 IModelConnection 分区，iModel 关闭时自动释放。

---

## 六、RenderTarget — 渲染目标

### 6.1 类层次

```
RenderTarget (abstract)               [render/RenderTarget.ts]
  └── Target (abstract, WebGL)        [internal/render/webgl/Target.ts]
        ├── OnScreenTarget            // 屏幕渲染
        └── OffScreenTarget           // 离屏渲染
```

### 6.2 RenderTarget 接口核心方法

| 方法 | 说明 |
|------|------|
| `changeScene(scene)` | 接收新场景 (L109) |
| `changeDynamics(fg, overlay)` | 接收动态图形 (L111) |
| `changeDecorations(decorations)` | 接收装饰 (L113) |
| `changeRenderPlan(plan)` | 接收渲染计划 (L115) |
| `drawFrame(elapsed?)` | 执行 GPU 绘制 (L117) |
| `setViewRect(rect, temporary)` | 设置视口尺寸 (L127) |
| `updateViewRect()` | 尺寸更新，返回是否变化 (L131) |
| `readPixels(rect, selector, receiver)` | 像素拾取 (L134) |
| `screenSpaceEffects` | 屏幕后处理效果列表 (L158) |
| `overrideFeatureSymbology(ovr)` | Feature 覆盖 |

### 6.3 drawFrame() 实现

```typescript
// Target.ts:546-556
drawFrame(sceneMilSecElapsed?) {
  assert(this.renderSystem.frameBufferStack.isEmpty);
  if (!this.assignDC()) return;
  this.paintScene(sceneMilSecElapsed);      // WebGL 绘制
  this.drawOverlayDecorations();            // Canvas 2D 装饰
  assert(this.renderSystem.frameBufferStack.isEmpty);
}
```

### 6.4 paintScene() — WebGL 核心流程 (L657-738)

```
paintScene()
  │
  ├── _beginPaint(fbo)                    // push FBO 到栈 (L665)
  ├── gl.viewport(0, 0, width, height)    // 设置 GL 视口 (L669-670)
  │
  ├── [正常渲染路径]
  │     ├── compositor.preDraw()          // 更新 FBO 纹理
  │     ├── drawPlanarClassifiers()       // 平面分类器
  │     ├── drawSolarShadowMap()          // 太阳阴影贴图
  │     ├── drawTextureDrapes()           // 纹理覆盖
  │     ├── renderCommands.initForRender() // 构建渲染命令
  │     ├── compositor.draw(commands)     // 主场景合成
  │     ├── drawPass(WorldOverlay)        // 世界叠加层
  │     └── drawPass(ViewOverlay)         // 视图叠加层
  │
  ├── [拾取路径] (drawForReadPixels)
  │     ├── beginReadPixels()
  │     ├── compositor.drawForReadPixels()
  │     └── compositor.readPixels()
  │
  ├── screenSpaceEffects.apply()          // 屏幕后处理 (L728)
  ├── uniforms.batch.resetBatchState()    // 重置批次状态 (L730)
  └── _endPaint()                         // blit FBO → canvas (L733)
```

### 6.5 OnScreenTarget vs OffScreenTarget

| 特性 | OnScreenTarget | OffScreenTarget |
|------|---------------|-----------------|
| FBO → Canvas blit | ✅ `_endPaint()` 执行 blit | ❌ 仅 push/pop FBO |
| Canvas 2D 装饰 | ✅ `drawOverlayDecorations()` | ❌ 不支持 |
| 设备像素比 | ✅ `devicePixelRatio` | 固定 1 |
| 拾取装饰 | ✅ `pickOverlayDecoration()` | ❌ |

---

## 七、Scene — 场景系统

### 7.1 Scene 数据结构

```typescript
class Scene {
  foreground: RenderGraphic[];   // 正常场景几何体（有深度测试）
  background: RenderGraphic[];   // 背景（天空盒、背景地图）
  overlay: RenderGraphic[];      // 叠加层（无深度测试）

  planarClassifiers: Map<RenderGraphic, RenderPlanarClassifier>;
  textureDrapes: Map<RenderGraphic, RenderTextureDrape>;
  volumeClassifier?: RenderVolumeClassifier;
}
```

### 7.2 场景创建流程

```
Viewport.createScene(context)
  │
  ├── view.createScene(context)           // ViewState 创建场景
  │     └── for ref in getTileTreeRefs():
  │           ref.addToScene(context)
  │             ├── createDrawArgs(context)  // TileDrawArgs
  │             └── tree.draw(args)
  │                   ├── selectTiles(args)   // LOD 选择
  │                   ├── getTileGraphics(tile)
  │                   └── drawGraphics()      // 生成 RenderGraphic
  │
  ├── mapTiledGraphicsProvider.addToScene()  // 背景地图
  │
  └── for provider in tiledGraphicsProviders:
        provider.addToScene(context)          // 自定义瓦片提供者
```

### 7.3 SceneContext 输出路由

```typescript
// ViewContext.ts:406
outputGraphic(graphic) {
  switch (this._graphicType):
    BackgroundMap → scene.background.push(graphic)
    Overlay       → scene.overlay.push(graphic)
    default       → scene.foreground.push(graphic)
}
```

---

## 八、Decorations — 装饰系统

### 8.1 装饰层分类

| 字段 | GraphicType | 坐标系 | 深度测试 | 说明 |
|------|------------|--------|---------|------|
| `skyBox` | 特殊 | 世界 | -- | 天空盒 |
| `viewBackground` | ViewBackground | 视图 | 禁用 | 视图背景 |
| `normal` | Scene | 世界 | 启用 | 场景装饰 |
| `world` | WorldDecoration | 世界 | 启用 | 世界装饰（忽略 ViewFlags） |
| `worldOverlay` | WorldOverlay | 世界 | 禁用 | 世界叠加 |
| `viewOverlay` | ViewOverlay | 视图 | 禁用 | 视图叠加 |
| `canvasDecorations` | 2D Canvas | 屏幕 | -- | Canvas 2D 绘制 |

### 8.2 装饰收集流程

```
ScreenViewport.addDecorations(decorations)
  │
  ├── context = new DecorateContext(this, decorations, cache)
  │
  ├── context.addFromDecorator(view)           // ViewState 装饰（网格等）
  ├── for ref in getTileTreeRefs():
  │     context.addFromDecorator(ref)          // 瓦片树装饰
  └── for decorator in viewManager.decorators:
        context.addFromDecorator(decorator)    // 用户注册的装饰器

DecorateContext.addFromDecorator(decorator)
  │
  ├── if useCachedDecorations && cached:
  │     restoreCache(cached); return           // 快速路径：复用缓存
  │
  └── decorator.decorate(this)                 // 调用装饰器
        └── addDecoration(type, graphic)       // 路由到 Decorations 对应字段
```

### 8.3 DecorationsCache

- 键：`ViewportDecorator` 对象
- 值：`CachedDecoration[]`（标记类型：graphic/canvas/html）
- 场景变化时清除
- 装饰器可显式失效

### 8.4 Canvas 装饰绘制

```typescript
// OnScreenTarget.drawOverlayDecorations()
drawOverlayDecorations() {
  const ctx = this._2dCanvas.getContext("2d", { alpha: true });
  for (const overlay of canvasDecorations) {
    ctx.save();
    if (overlay.position) ctx.translate(overlay.position.x, overlay.position.y);
    overlay.drawDecoration(ctx);
    ctx.restore();
  }
}
```

### 8.5 Canvas 装饰拾取

```typescript
// OnScreenTarget.pickOverlayDecoration()
pickOverlayDecoration(pt) {
  for (let i = overlays.length - 1; i >= 0; --i) {
    if (overlay.pick?.(pt)) return overlay;
  }
}
```

---

## 九、GraphicBranch — 场景图节点

### 9.1 核心属性

```typescript
class GraphicBranch {
  entries: RenderGraphic[];                     // 子图形列表 (L35)
  ownsEntries: boolean;                         // dispose 时是否释放子项 (L37)
  viewFlagOverrides: ViewFlagOverrides;         // ViewFlags 覆盖 (L41)
  realityModelDisplaySettings?: RealityModelDisplaySettings;  // Reality model 显示设置 (L45)
  realityModelRange?: Range3d;                  // Reality model 范围 (L47)
  symbologyOverrides?: FeatureSymbology.Overrides;  // 符号覆盖 (L49)
  animationId?: string;                         // 动画 ID (L53)
  animationNodeId?: AnimationNodeId | number;   // 动画节点 ID (L57)
  groupNodeId?: number;                         // 跨切面子集选择 (L68)
}
```

### 9.2 GraphicBranchOptions

```typescript
interface GraphicBranchOptions {
  clipVolume?: RenderClipVolume;
  classifierOrDrape?: RenderPlanarClassifier | RenderTextureDrape;
  hline?: HiddenLine.Settings;
  iModel?: IModelConnection;
  transformFromIModel?: Transform;
  frustum?: Frustum;
  appearanceProvider?: FeatureAppearanceProvider;
  secondaryClassifiers?: Map<RenderGraphic, RenderPlanarClassifier>;
  viewAttachmentId?: Id64String;
  contours?: RenderContours;
}
```

### 9.3 变换矩阵传递链

```
Geometry (tile-local)
  │
  ├── TileDrawArgs.location           // tile-tree → iModel 坐标
  │     └── createGraphicBranch(branch, location, opts)
  │
  ├── GraphicBranchOptions.transformFromIModel  // 跨 iModel 变换
  │
  ├── Animation transform node        // schedule script 动画变换
  │
  └── Group node                      // groupNodeId 选择性渲染
```

GPU 端：遇到每个 GraphicBranch 节点时，其变换乘入当前 model-to-view 矩阵，ViewFlagOverrides 和 symbologyOverrides 压入覆盖栈。

---

## 十、拾取（Picking）流程

### 10.1 Canvas 装饰拾取

```
ScreenViewport.pickCanvasDecoration(pt)
  └── target.pickOverlayDecoration(pt)
        └── 反向遍历 canvasDecorations，调用 overlay.pick(pt)
```

### 10.2 GPU 像素拾取

```
Viewport.pickAtPoint(x, y)
  └── target.readPixels(rect, selector, receiver)
        │
        ├── createOrReuseReadPixelResources()  // 离屏 FBO
        ├── readPixelsFromFbo()
        │     ├── 构建裁剪视锥体
        │     ├── beginReadPixels()            // 简化 ViewFlags
        │     ├── compositor.drawForReadPixels() // 使用 feature ID 着色
        │     └── compositor.readPixels()      // glReadPixels
        └── 返回 ElementId / SubCategoryId 等
```

---

## 十一、完整帧循环时序图

```
┌─────────────────────────────────────────────────────────────────────┐
│ requestNextAnimation()                                              │
│   └─> requestAnimationFrame(() => eventLoop())                      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│ IModelApp.eventLoop()                                               │
│   ├─> ToolAdmin.processEvent()                                      │
│   │     ├─> 分发鼠标/键盘事件到当前工具                                │
│   │     ├─> updateDynamics() → tool.onDynamicFrame()                 │
│   │     └─> invalidateDecorations() (locate circle 等)               │
│   │                                                                  │
│   ├─> ViewManager.renderLoop()                                      │
│   │     ├─> onBeginRender 事件                                       │
│   │     ├─> for each vp: vp.renderFrame()                           │
│   │     │     ├─> [1] 快照 ChangeFlags, 重置为 None                  │
│   │     │     ├─> [2] animate() — 执行 Animator                      │
│   │     │     ├─> [3] updateViewRect() — 尺寸检测                     │
│   │     │     ├─> [4] setupFromView() — 视图同步                      │
│   │     │     ├─> [5] setHiliteSet() — 选择集                        │
│   │     │     ├─> [6] setAnalysisFraction()                          │
│   │     │     ├─> [7] animationBranches — schedule script            │
│   │     │     ├─> [8] overrideFeatureSymbology()                     │
│   │     │     ├─> [9] createScene() → changeScene()                  │
│   │     │     ├─> [10] validateRenderPlan() → changeRenderPlan()     │
│   │     │     ├─> [11] addDecorations() → changeDecorations()        │
│   │     │     ├─> [12] processFlash() → setFlashed()                 │
│   │     │     ├─> [13] onBeforeRender()                              │
│   │     │     ├─> [14] IF isRedrawNeeded:                            │
│   │     │     │       drawFrame()                                    │
│   │     │     │         └── paintScene()                             │
│   │     │     │               ├── compositor.preDraw()               │
│   │     │     │               ├── drawPlanarClassifiers()            │
│   │     │     │               ├── drawSolarShadowMap()               │
│   │     │     │               ├── drawTextureDrapes()                │
│   │     │     │               ├── renderCommands.initForRender()     │
│   │     │     │               ├── compositor.draw()                  │
│   │     │     │               ├── drawPass(WorldOverlay)             │
│   │     │     │               ├── drawPass(ViewOverlay)              │
│   │     │     │               ├── screenSpaceEffects.apply()         │
│   │     │     │               └── _endPaint() (blit → canvas)        │
│   │     │     │                                                      │
│   │     │     ├─> [15] 分发 onViewportChanged 等事件                   │
│   │     │     └─> [16] requestNextAnimation() 如需要                   │
│   │     │                                                            │
│   │     └─> onFinishRender 事件                                      │
│   │                                                                  │
│   └─> TileAdmin.process()                                           │
│         └─> 加载/处理瓦片请求                                          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 十二、渲染系统初始化流程

### 12.1 IModelApp.startup() 完整初始化序列

`IModelApp.startup()` 是整个前端系统的入口，按以下严格顺序执行：

```
IModelApp.startup()
  │
  ├── [A] 守卫检查 + 基础配置
  │     ├── _initialized 幂等性保护
  │     ├── sessionId (GUID)
  │     ├── applicationId / applicationVersion
  │     ├── authorizationClient
  │     └── _noRender 标志
  │
  ├── [B] RPC 请求上下文配置
  │     ├── RpcConfiguration.requestContext.getId
  │     ├── RpcConfiguration.requestContext.serialize
  │     └── CSRF token 处理 (XSRF-TOKEN cookie → X-XSRF-TOKEN header)
  │
  ├── [C] 本地化初始化
  │     └── localization.initialize(["iModelJs", "CoreTools"])
  │
  ├── [D] 内置工具注册
  │     └── SelectTool, IdleTool, ViewTool, ClipViewTool, MeasureTool, AccuDrawTool
  │
  ├── [E] EntityState 注册
  │     └── EntityState, ElementState, ModelState, ViewState, DisplayStyleState 等
  │
  ├── [F] 子系统实例化（按顺序，源码行 423-440）
  │     ├── 1. RenderSystem          ← System.create() 或自定义实例 (L423)
  │     ├── 2. UserPreferences       (可选) (L424)
  │     ├── 3. ViewManager           (L426)
  │     ├── 4. TileAdmin             ← async, 查询 CPU 并发数 (L427)
  │     ├── 5. NotificationManager   (L428)
  │     ├── 6. ToolAdmin             (L429)
  │     ├── 7. AccuDraw
  │     ├── 8. AccuSnap
  │     ├── 9. ElementLocateManager
  │     ├── 10. TentativePoint
  │     ├── 11. QuantityFormatter
  │     ├── 12. UiAdmin
  │     ├── 13. MapLayerFormatRegistry
  │     ├── 14. TerrainProviderRegistry
  │     └── 15. RealityDataSourceProviderRegistry
  │
  ├── [G] onInitialized() 批量回调
  │     ├── RenderSystem.onInitialized()    ← Techniques 编译、纹理创建
  │     ├── ViewManager.onInitialized()     ← 注册装饰器、启动 idle work
  │     ├── ToolAdmin.onInitialized()       ← 创建 IdleTool、注册键盘事件
  │     ├── AccuDraw/AccuSnap/LocateManager/TentativePoint/UiAdmin
  │     └── QuantityFormatter.onInitialized()  ← async
  │
  └── [H] onAfterStartup.raiseEvent()
        └── ExtensionAdmin.onStartup() → 激活扩展
```

**关键点：事件循环不在 startup() 中启动。** 它是惰性触发的 — 当第一个 Viewport 被添加到 ViewManager 时才启动。

### 12.2 WebGL System 初始化

```
System.create(options)
  │
  ├── document.createElement("canvas")           // 创建隐藏的离屏 canvas
  ├── canvas.getContext("webgl2", {               // WebGL2 强制要求
  │     powerPreference: "high-performance"       // 请求独立 GPU
  │   })
  │
  ├── Capabilities.create(gl, disabledExtensions)
  │     ├── 查询 GL 参数 (maxTextureSize, maxFragTextureUnits 等)
  │     ├── 检测驱动 Bug
  │     │     ├── Intel UHD/HD 620/630: fragDepthDoesNotDisableEarlyZ
  │     │     └── Mali G71/G72/G76: msaaWillHang → maxAntialiasSamples=1
  │     ├── 枚举并启用 18 个已知扩展
  │     ├── 测试颜色渲染类型 (Float > HalfFloat > UnsignedByte)
  │     ├── 测试深度缓冲类型 (TextureUnsignedInt24Stencil8)
  │     └── 收集 feature 列表，验证必需特征
  │
  ├── context.depthFunc(GL.DepthFunc.Default)     // LessOrEqual
  │
  └── new System(canvas, context, capabilities, options)
        ├── resourceCache = new Map<IModelConnection, IdMap>()
        ├── glTimer = GLTimer.create(this)
        ├── frameBufferStack = new FrameBufferStack()
        ├── currentRenderState = new RenderState()
        ├── 监听 IModelConnection.onClose → 清理缓存
        └── 监听 canvas.webglcontextlost
```

**onInitialized() 回调（L451-467）：**

```
System.onInitialized()
  ├── LineCode.initializeCapacity(maxTextureSize)      (L452)
  ├── Techniques.create(gl)                             (L453)
  │     └── initializeBuiltIns(gl)
  │           ├── ~30+ SingularTechnique (ClearPickAndColor, CopyColor, SkyBox, Blur 等)
  │           ├── SurfaceTechnique (最复杂，数百个变体)
  │           ├── EdgeTechnique, SilhouetteEdge, IndexedEdge, PolylineTechnique
  │           ├── PointStringTechnique, PointCloudTechnique, RealityMeshTechnique (194 变体)
  │           ├── SkySphereTechnique (gradient + texture 两个变体)
  │           └── 7 个 Composite 程序 (compositeFlags 1-7)
  │           (所有着色器仅创建 ShaderProgram 对象，不编译)
  ├── 创建 noiseTexture (4x4 Luminance)                (L456)
  ├── 创建 lineCodeTexture (线型虚线)                   (L459)
  ├── LineCode.onTextureUpdated → reloadLineCodeTexture (L464) ← 遗漏补充
  └── new ScreenSpaceEffects()                          (L467)
```

### 12.3 Techniques 着色器初始化

#### 编译策略：Lazy Compilation

```
构造阶段:
  ProgramBuilder.buildProgram(gl)
    → gl.createProgram()              // 创建空 GL 程序对象
    → new ShaderProgram(UNCOMPILED)   // 仅保存源码，不编译

按需编译（首次使用）:
  ShaderProgram.use()
    → if Uncompiled: compile()
        → gl.createShader(VERTEX) + gl.shaderSource + gl.compileShader
        → gl.createShader(FRAGMENT) + gl.shaderSource + gl.compileShader
        → gl.attachShader × 2 + gl.bindAttribLocation × N
        → gl.linkProgram + gl.validateProgram
        → compileUniforms() → gl.getUniformLocation × N

空闲预编译:
  ViewManager idle work → setTimeout(1ms) 循环
    → techniques.idleCompileNextShader()
        → 按优先级编译 1 个着色器 → 返回
        → 重复直到全部完成或有 Viewport 打开
```

#### 着色器变体选择 (TechniqueFlags)

| 标志 | 类型 | 影响 |
|------|------|------|
| `featureMode` | None/Pick/Overrides | Feature table 查找 |
| `isInstanced` | No/Yes | 实例化渲染 |
| `isAnimated` | No/Yes | 动画 |
| `isClassified` | No/Yes | 分类器 |
| `isEdgeTestNeeded` | No/Yes | 边/面剔除 |
| `isShadowable` | No/Yes | 太阳阴影 |
| `isThematic` | No/Yes | 主题色显示 |
| `isWiremesh` | No/Yes | 网格线 |
| `isTranslucent` | boolean | 半透明 |
| `positionType` | quantized/unquantized | 位置编码 |
| `numClipPlanes` | number | 裁剪平面 |
| `enableAtmosphere` | No/Yes | 大气散射 |

SurfaceTechnique 变体索引计算（位偏移累加）：
```
index = isTranslucent ? 1 : 0
      + 2 * isInstanced
      + 4 * isAnimated
      + 8 * isWiremesh
      + 16 * isShadowable
      + 32 * isThematic
      + 48 * featureMode
      + idxOffset (unquantized 时翻倍)
```

#### 优先编译顺序

```
阶段 1（最优先）— 10 个高优先级变体:
  Surface 不透明/半透明 × Pick/Overrides/None × 非实例化

阶段 2 — 按 Technique 逐个编译:
  Surface (全部) → Edge → SilhouetteEdge → Polyline
  → PointString → PointCloud → RealityMesh
  → SingularTechnique (OITClear, CopyPick, Composite 等)
```

### 12.4 Viewport 创建和首帧

```
1. view = await ViewState.create(iModel)     // 从数据库创建
2. await view.load()                         // 并行加载状态+样式+子分类

3. ScreenViewport.create(parentDiv, view)
   ├── 校验 parentDiv 非零尺寸
   ├── 创建 HTMLCanvasElement
   ├── RenderSystem.createTarget(canvas)     // 创建 GPU 渲染目标
   ├── new ScreenViewport(canvas, parentDiv, target)
   │     ├── Viewport 构造: registerTileUser, PerModelCategoryVisibility
   │     ├── _changeFlags = MutableChangeFlags(Initial)  // ViewedCategories|ViewedModels|DisplayStyle
   │     ├── 所有 valid 标志 = false (_sceneValid, _decorationsValid, _renderPlanValid, _controllerValid)
   │     └── ScreenViewport 构造: DOM 层级 (vpDiv > canvas + decorationDiv + toolTipDiv)
   ├── initialize()                          // 空钩子
   └── changeView(view)
         ├── updateChangeFlags()             // 首次无旧视图，跳过
         ├── doSetupFromView(view)
         │     ├── setView(view) → attachToView()
         │     │     ├── registerViewListeners()       // onViewedCategoriesChanged 等
         │     │     ├── registerDisplayStyleListeners() // ViewFlags/SubCategory/Map 等
         │     │     └── MapTiledGraphicsProvider 创建
         │     ├── ViewingSpace.createFromViewport()   // 坐标转换空间
         │     └── invalidateRenderPlan()
         ├── invalidateController()          // _controllerValid = false
         ├── target.reset()
         └── _changeFlags.setViewState()

4. ViewManager.addViewport(vp)
   ├── vp.onViewManagerAdd()                // EventController + ResizeObserver
   ├── _viewports.push(vp)
   ├── setSelectedView(vp)
   └── if (1 === _viewports.length)
         IModelApp.startEventLoop()          // 启动渲染循环
           ├── _wantEventLoop = true
           ├── window.addEventListener("resize", requestNextAnimation)
           ├── requestIntervalAnimation()    // 周期性定时器
           └── requestNextAnimation()        // ← 第一帧触发点

5. 第一帧 (requestAnimationFrame 回调):
   IModelApp.eventLoop()
     → ToolAdmin.processEvent()
     → ViewManager.renderLoop()
         → vp.renderFrame()
             → 捕获 ChangeFlags (Initial = 0x1c)
             → setupFromView()              // 同步 ViewState
             → createScene()                // 场景创建 + 瓦片请求
             → validateRenderPlan()         // 渲染计划
             → addDecorations()             // 装饰收集
             → target.drawFrame()           // GPU 绘制
             → 分发 onViewportChanged + onDisplayStyleChanged + onViewedModelsChanged
     → TileAdmin.process()
```

### 12.5 TileAdmin 瓦片系统初始化

```
TileAdmin.create(props)
  │
  ├── TileRequestChannels(rpcConcurrency, cacheMetadata)
  │     ├── ElementGraphicsChannel ("itwinjs-elem-rpc")
  │     └── IModelTileRequestChannels
  │           ├── IModelTileChannel ("itwinjs-tile-rpc")
  │           ├── CloudStorageCacheChannel ("itwinjs-cloud-cache")
  │           └── (可选) IModelTileMetadataCacheChannel
  │
  ├── LRUTileList 初始化 (哨兵节点分区)
  │     └── Not Selected | sentinel | Selected
  │
  ├── GPU 内存限制
  │     ├── 非移动: 1024 MB (default) / 500 MB (aggressive) / 2560 MB (relaxed)
  │     └── 移动: 200 MB (default) / 75 MB (aggressive) / 500 MB (relaxed)
  │
  ├── 过期时间
  │     ├── tileExpirationTime = 20s
  │     └── tileTreeExpirationTime = 300s
  │
  └── 注册事件监听器
        ├── IModelConnection.onClose → onIModelClosed
        ├── onTileLoad → invalidateAllScenes()
        └── onTileChildrenLoad → invalidateAllScenes()
```

#### 瓦片请求和加载流程

```
场景创建时:
  TileTree.draw(args)
    → selectTiles(args)                    // LOD 选择
    → insertMissingTile(tile)              // 未加载的瓦片

  SceneContext.requestMissingTiles()
    → TileAdmin.requestTiles(viewport, missingTiles)

每帧 TileAdmin.process():
  processQueue()
    → channels.swapPending()
    → 为每个用户创建 TileRequest 并加入通道队列
    → channels.process()
        → 重新计算优先级 (tile.tree.loadPriority + tile.depth)
        → 排序 (PriorityQueue 堆排序)
        → 分发请求直到达到并发限制
            → TileRequest.dispatch()
                → channel.requestContent()  // RPC/HTTP 请求
                → tile.readContent()        // 解码 (可能在 Worker)
                → tile.setContent()
                    → LRUTileList.add(tile)
                    → onTileLoad.raiseEvent()
                    → viewport.invalidateScene()

  pruneAndPurge()
    → 过期瓦片清理 (tileExpirationTime=20s)
    → 过期 TileTree 清理 (treeExpirationTime=300s)

  freeMemory()
    → LRUTileList.freeMemory(maxBytes)
        → 从 least-recently-used 开始释放
```

#### 瓦片优先级

```
TileLoadPriority 枚举:
  Dynamic   = 5    // 动态编辑几何 (最高)
  Terrain   = 10   // 3D 地形
  Map       = 15   // 背景地图
  Primary   = 20   // 几何模型
  Context   = 40   // Reality 模型
  Classifier = 50  // 分类器 (最低)

优先级 = TileTree.loadPriority (主) + Tile.depth (次)
```

---

## 十三、对 DanQing 的映射建议

### 13.1 核心对齐点

| itwinjs-core 概念 | DanQing 实现要点 |
|---|---|
| `requestAnimationFrame` 循环 | Qt `QTimer` 或 `requestAnimationFrame`（Web 端） |
| `ChangeFlags` 位掩码 | 直接对齐，枚举值和复合标志一致 |
| `renderFrame()` 17 步管线 | 严格对齐每一步，不遗漏 |
| `Scene` 三层容器 | foreground/background/overlay 分层 |
| `Decorations` 6 层 | 按 GraphicType 分层 |
| `GraphicBranch` 场景图 | 变换矩阵 + ViewFlagOverrides + symbologyOverrides |
| `RenderTarget` 抽象 | 分离接口和实现，支持 OnScreen/OffScreen |
| `paintScene()` 多 Pass | SceneCompositor 管理 Opaque/Translucent/Hilite Pass |

### 13.2 关键差异处理

1. **浏览器 vs Qt 事件循环**：`requestAnimationFrame` 替换为 Qt 的渲染定时器
2. **WebGL vs OpenGL**：`WebGLRenderingContext` 替换为原生 OpenGL 函数调用
3. **Canvas 2D 装饰**：可选用 QPainter 实现，或标记为 TODO
4. **Feature Symbology**：需要完整移植 Overrides 算法

### 13.3 实现优先级

1. **P0**：`renderFrame()` 管线 + `ChangeFlags` + `RenderTarget` 接口
2. **P1**：`Scene` + `Decorations` + `GraphicBranch`
3. **P2**：`paintScene()` 多 Pass + SceneCompositor
4. **P3**：拾取系统 + Canvas 装饰
