# 渲染独立验证：BlankConnection 深入分析（display-test-app 对照）

> 2026-09-09。目标：不依赖仓外数据层（SQLite 数据栈不在本引擎），用 BlankConnection（无 SQLite 的空 iModel）驱动 dqRender/dqApp 的端到端渲染验证。
> 参考：`D:\Github\itwinjs-core\test-apps\display-test-app`（DTA，core/frontend 的开发/回归测试床，非示例模板）+ `core/frontend/src/test/openBlankViewport.ts`（**官方渲染独立验证范式**）。

---

## 1. open blank connection 完整链路（itwinjs 代码级）

```
Surface.ts:134 工具栏按钮 "Open Blank Connection"
  → Surface.ts:183 openBlankConnection()
      props: location=Cartographic(-75.686694, 40.065757)（Exton，为背景地图定位）
             extents=Range3d(-1000,-1000,-100, 1000,1000,100)（默认）
  → IModelConnection.ts:786 BlankConnection 构造
      （BlankConnectionProps 只有 name/location/extents/globalOrigin/iTwinId，无 drawingToModelTransform）
  → Surface.ts:300 createViewer → Viewer.create → ViewPicker ViewList.create
      → iModel.views.getViewList() —— isClosed 恒 true → 返回 []（不失败、不发 RPC）
      → 插入合成条目 {id: Id64.invalid, name:"Spatial View"}
      → views.load(invalid) 抛错 → catch（ViewPicker.ts:38，注释明示 intentional）
      → 【关键】manufactureSpatialView(iModel)（ViewPicker.ts:149-169）
          = SpatialViewState.createBlank(iModel, ext.low, size, undefined)
            （空 CategorySelector + 空 ModelSelector + 默认 DisplayStyle3d，默认 top view）
          + viewFlags{backgroundMap:true, lighting:true, SmoothShade} + 白背景 + sky
          （grid 默认 OFF）
  → Viewer.ts:219 ScreenViewport.create(canvas, renderSystem.createTarget(canvas))
      → vp.initialize()（EventController 输入绑定）→ vp.changeView(view)
  → IModelApp.viewManager.addViewport(viewport) → rAF renderLoop
```

## 2. BlankConnection 本体（最小数据面）

`IModelConnection.ts:762-812`：`isClosed` 恒 true（一切 RPC/ECSQL 短路）；但基类构造器照常创建**全部前端侧对象**（Models/Elements/Views/Categories + SelectionSet + HiliteSet + Tiles + GeoServices + transientIds）。

- **可用**：hilite/选择（纯前端状态机）、装饰系统（ViewManager decorators）、transientIds（临时图形）、ecefLocation/projectExtents/rootSubject/globalOrigin。
- **不可用**：持久化查询、views.load、真实 tile 内容、写操作。
- 类注释点明设计意图："our display system **requires** an IModelConnection type even if only reality data is drawn"——给渲染提供坐标参考 + 感兴趣体积 + 空选择器。

## 3. 空连接下的渲染验证能力

- **默认画面**：白背景 + sky + backgroundMap（无凭据即纯背景）+ 无几何 + grid OFF。
- **塞测试几何的三条机制**（全走 GraphicBuilder 直画，不碰 DB）：
  1. `EmptyExample.ts:50-496 CesiumDecorator`——最全图元清单：WorldDecoration/WorldOverlay/Scene 三类 + addPointString/addLineString/addShape/addArc/addPath/addLoop/addPolyface/addSolidPrimitive(Box/Sphere/Cone)；另示范 setStandardRotation(Iso)/turnCameraOn/zoomToVolume/viewFlags 覆写。
  2. `AnalysisStyleExample.ts:187`——createGraphicBuilder + RenderGraphicOwner 跨帧缓存。
  3. `DecorationGeometryExample.ts:70`——GraphicBranch + pickable id + 材质/纹理。
- **内建渲染内容**：renderFrame 的 decorations 阶段 = ViewState.decorate（drawGrid/GridDecorator）+ viewManager.decorators（accuSnap/tentativePoint/accuDraw/toolAdmin）；**ACS triad 在 5.x 由 AccuDraw.decorate 画**（AccuDraw.ts:2290，viewFlags.acsTriad 控制；独立 AcsTriadDecorator.ts 已不存在）。
- **官方渲染独立验证范式（DanQing 两份旧文档都未发现）**：
  `core/frontend/src/test/createBlankConnection.ts` + `openBlankViewport.ts`——`testBlankViewport()` = 空连接 + 100x100 div + `SpatialViewState.createBlank({0,0,0},{1,1,1})` + BlankViewport.create，然后**像素回读断言**：`readUniqueColors/expectUniqueColors/readPixel/readUniquePixelData`（vp.readPixels + vp.readImageBuffer）。itwinjs 全部 WebGL 回归测试（core/frontend/src/test/render/**）建立于此。
  另有 `ViewCreator3d.ts:65 createDefaultView` 为官方通用「无视图时造 ViewState」路径。

## 4. 最小渲染闭环组件清单与 DanQing 移植状态

| # | 组件 | DanQing 状态 |
|---|---|---|
| 1 | IModelApp.startup | ✅ dqApp/Application.h |
| 2 | RenderSystem + onInitialized | ✅ dqRender/RenderSystem.h、RenderPipeline.h |
| 3 | TileAdmin | ✅ dqRender/tile（空连接下跑空 process） |
| 4 | ViewManager | ✅ dqApp/ViewManager.h |
| 5 | ToolAdmin + EventController | ✅ dqApp/ToolAdmin.h（Qt 事件等价） |
| 6 | BlankConnection | ✅ dqApp/BlankConnection.h（create 与上游 1:1） |
| 7 | SpatialViewState.createBlank + 空 Selector + DisplayStyle3d | ✅ dqApp/ViewState.h（CreateBlank @ ViewState.cpp:804） |
| 8 | manufactureSpatialView | ✅ dqApp/src/ViewPicker.cpp:13（含 viewFlags/白背景，grid OFF，DTA-faithful） |
| 9 | RenderTarget（createTarget） | ✅ dqRender/RenderTarget.h |
| 10 | ScreenViewport.create/initialize/changeView | ✅ dqApp/Viewport.h（QOpenGLWidget 等价） |
| 11 | addViewport → renderLoop → renderFrame | ✅ Application 事件循环 + ViewManager |
| 12 | ViewState.decorate → GridDecorator | ✅ PlanarGrid（Viewport.cpp:1141 drawStandardGrid） |
| 13 | ACS triad | ✅ dqApp/AcsTriadDecorator（注意 5.x 上游已并入 AccuDraw.decorate） |
| 14 | Decorator + GraphicBuilder 测试几何 | ✅ dqApp/Decorator.h、DecorateContext.h；dqRender/GraphicBuilder.h |
| **15** | **像素回读断言（readImageBuffer/expectUniqueColors）** | **⚠️ 唯一缺口**——Pixel.h 在，离屏断言工具需移植/自建 |
| 16 | ViewingSpace/ViewPose/StandardView 变换链 | ✅ 均在 |

**结论：15/16 组件已就位，缺的是验证工具本身。**

## 5. DanQing 旧文档核对

- `DanQing-DisplayTestApp-启动流程实现对照.md`：拓扑正确；但未揭示 blank 下 `isClosed→[]` 短路 + invalid id + catch 回退机制；「当前实现状态」自带 2026-07-17 过时声明（现状已远超：ViewPicker/ViewTool/AccuDraw/AccuSnap/AcsTriad/PlanarGrid 等已落地，行号漂移）。
- `DanQing-openBlankConnection-实现指南.md`：六阶段正确；N1 描述偏差（getViewList 不失败而是返回空数组，真正失败点在 views.load(invalid)）；M1/M8/M9 事件系统当时缺失现已补。
- **两份共同遗漏**：官方 `openBlankViewport.ts` 像素回读范式（本分析的核心增量）、ACS triad 归属变化（5.x 并入 AccuDraw）、ViewCreator3d。

## 6. 实施路径（渲染独立验证）

**需移植（小件，照 openBlankViewport.ts 抄）**：
1. `testBlankViewport` 等价物：固定 100x100 **离屏** Viewport + `SpatialViewState::CreateBlank({0,0,0},{1,1,1})` 确定性最小视口（不走 DTA 白背景/backgroundMap 分支，排除地图依赖）
2. `readImageBuffer` + `expectUniqueColors`：渲染一帧 → 回读 framebuffer → 颜色集合断言（背景色/grid 线色/装饰色）
3. （二阶段）`readPixels` 深度/Feature 断言

**需自建**：
4. `RenderSmokeDecorator`：对标 CesiumDecorator 的最小子集（addLineString + addShape + addSolidPrimitive(Box) 三图元）
5. 标准方向用例：SetStandardRotation(Iso) + 相机 + ZoomToVolume 的像素断言
6. 失败调试钩子：dump PNG（glReadPixels→QtImage）

**验收顺序**：背景+网格（grid=ON 确定性视图）→ 装饰图元 → 标准方向/相机 → 选择/hilite（SelectionSet 已有）→ 动画/RenderPlan（后续）。
