# DisplayTestApp open blank connection 权威执行路径分析（运行验证 + 代码级精读）

> 2026-09-09。方法：**实际运行 itwinjs-core 的 display-test-app**（Electron + Vite，localhost:3000）确认行为基线（blank connection = 天空背景），再逐函数精读源码得到权威执行路径。用途：DanQing dqApp 桌面端 BlankConnection 端到端接线的 100% 对齐依据。
> 参考：`D:\Github\itwinjs-core\test-apps\display-test-app`（display-test-app）+ `core/frontend`（core-frontend）。

---

## 1. 行为基线（运行验证）

display-test-app 启动后点 "Open Blank Connection"：**天空背景**铺满视口，无几何、无网格、无 ACS（grid/ACS 默认 OFF）。与 DanQing DisplayTestApp 的 blank connection 画面一致。

CDP 连通性已验证（electron --remote-debugging-port=9223，Runtime.evaluate 工作）；PreciseCoverage 在该 electron 渲染进程返回空，执行路径以**代码级精读**为准（含参数/分支，比采样 trace 更精确）。

## 2. 权威执行路径（逐函数 + 参数）

```
[入口] Surface.ts:138 工具栏 "Open Blank Connection"
  → Surface.ts:183 openBlankConnection(props?)
      BlankConnection.create({                      // IModelConnection.ts:786
        location: Cartographic.fromDegrees(-75.686694, 40.065757, 0),  // Exton, PA（背景地图定位）
        extents:  Range3d(-1000,-1000,-100, 1000,1000,100),
        name:     "blank connection test" })
        → new BlankConnection({name, rootSubject:{name}, projectExtents:extents,
                               globalOrigin, ecefLocation:EcefLocation.createFromCartographicOrigin(location),
                               key:"", iTwinId})
        → IModelConnection.onOpen.raiseEvent(connection)
        → 基类构造创建全部前端对象：Models/Elements/Views/Categories/SelectionSet/
          HiliteSet/Tiles/GeoServices/transientIds（isClosed 恒 true：一切 RPC/ECSQL 短路）
  → Surface.ts:190 createViewer({iModel, configuration})   // Surface.ts:300
  → Viewer.ts:183 Viewer.create(surface, props)
  → ViewPicker.ts:57 ViewList.create(iModel, defaultViewName)
      → populate(iModel)                                  // ViewPicker.ts:69
          iModel.views.getViewList({wantPrivate:false}) → [](isClosed 短路，不失败不发 RPC)
          getViewList({wantPrivate:true}) → []
          _defaultViewId 仍 invalid → line:140 insert({id:Id64.invalid, name:"Spatial View",
                                                     class:SpatialViewState.classFullName})
          line:143 getView(_defaultViewId=invalid, iModel)
              → iModel.views.load(invalid) 抛错 → catch(ViewPicker.ts:39-44)
              → manufactureSpatialView(iModel)            // ViewPicker.ts:149 ★ 核心
                  ext = iModel.projectExtents
                  SpatialViewState.createBlank(iModel, ext.low, ext.high.minus(ext.low), undefined)
                    // 空 CategorySelector + 空 ModelSelector + 默认 DisplayStyle3d + top view
                  style.viewFlags.copy({backgroundMap:true, lighting:true, renderMode:SmoothShade})
                  style.backgroundColor = ColorDef.white
                  style.environment = environment.withDisplay({sky:true})   // 天空开
              → view.clone()                              // 返回 clone（初始持久态）
  → Viewer.ts:185 views.getDefaultView(iModel)  → 上面的 manufacture + clone
  → Viewer.ts:219 new Viewer(surface, view, views, props)
      → Viewer.ts:231 ScreenViewport.create(this.contentDiv, view)   // Viewport.ts:3196 ★
          检查 div 尺寸非 0
          new canvas + new ScreenViewport(canvas, parentDiv,
                                          IModelApp.renderSystem.createTarget(canvas))
          vp.initialize()            // EventController 输入绑定
          vp.changeView(view)        // 装配 view → 24 步 renderFrame 管线
      → toolbar / ViewPicker / ModelPicker / CategoryPicker …
  → Surface.ts:306 addViewer(viewer)
      → addWindow + IModelApp.viewManager.addViewport(viewer.viewport)  // 入渲染循环
  → viewer.dock(Dock.Full)           // Surface.ts:191
```

### 关键参数不变量（对齐断言用）

| 项 | 值 | 来源 |
|---|---|---|
| extents | `(-1000,-1000,-100, 1000,1000,100)` | Surface.ts:186 |
| location | Cartographic(-75.686694°, 40.065757°, 0) | Surface.ts:185 |
| 取景 | origin=ext.low，extents=diagonal，Top view | ViewPicker.ts:153 |
| viewFlags | backgroundMap=ON, lighting=ON, SmoothShade, grid=OFF, acsTriad=OFF | ViewPicker.ts:157-161 |
| 背景 | 白（ColorDef.white）| ViewPicker.ts:163 |
| sky | ON（environment.withDisplay sky:true）| ViewPicker.ts:166 |

## 3. DanQing 对照（dqApp 链路现状）

| 参考步骤 | DanQing 对应 | 状态 |
|---|---|---|
| BlankConnection.create(props) | `dqApp::BlankConnection::create`（View3DInventor.cpp:79，props.name/extents/locationIsCartographic/cartographicLocation）| ✅ |
| ViewList.create→populate→getView(invalid)→回退 | `dqApp::ViewList::create` + `getDefaultView`（ViewPicker.h/.cpp，View3DInventor.cpp:92-104 接线）——含 isClosed 短路（`IModelConnection::Views::getViewList`）、合成 "Spatial View" 条目、load 失败回退 manufactureSpatialView、clone 出缓存 | ✅ 已补层 |
| manufactureSpatialView | `dqApp::manufactureSpatialView`（ViewPicker.cpp:13）——viewFlags/白背景/sky 1:1 | ✅ |
| createBlank(iModel, ext.low, diagonal) | `SpatialViewState::CreateBlank`（ViewState.cpp:804）| ✅ |
| ScreenViewport.create+initialize+changeView | `dqApp::Viewport::Create`（View3DInventor.cpp:96）| ✅ |
| addViewport → renderLoop | `ViewManager::AddViewport`（View3DInventor.cpp:107）| ✅ |

**已修复的对齐缺口**（2026-09-09 前轮）：ViewingSpace::buildAffineViewMatrix 的 u_mv 平移参考点从取景 origin（far 面角）改为 near 面中心（参考 lookIn 语义）——修复 ACS triad 在桌面端初始不可见（commit `20be531`）。

## 4. 第二轮全链审计（2026-09-10）——发现与处置

方法：对照 §2 权威路径 + IModelApp.startup（IModelApp.ts:413-509）+ DisplayTestApp.startup（display-test-app App.ts:270-480）+ dtaFrontendMain（DisplayTestApp.ts:186-311）逐函数核对 DanQing main.cpp / Application::Startup / BlankConnection / ViewPicker / Viewport / ViewManager。

### 4.1 已修复的行为/链差异（全部 TDD：先改测试见 RED，再改实现见 GREEN）

| # | 差异 | 参考出处 | 修复 |
|---|---|---|---|
| F1 | `BlankConnection::IsClosed()` 之前返回 `m_closed`（false 直到 Close()） | IModelConnection.ts:777-781（`isClosed` 恒 true——一切 RPC/ECSQL 短路的语义根基） | 基类 `IsClosed()` 改 virtual；BlankConnection 覆盖恒 true；`IsOpen()` 走虚派发。测试断言反转（BlankConnectionTest） |
| F2 | `BlankConnection::Close()` 有 `m_closed` 守卫（二次 close 不 raise） | IModelConnection.ts:804-806（`close() → beforeClose()` 无守卫；守卫是 SnapshotConnection 形态 :872-884） | 基类 `Close()` 保留守卫（留给未来文件连接）；BlankConnection 覆盖为无守卫 + 设 `m_closed` 仅供 RAII 析构去重（C++ 适配，参考是 GC 无析构）。测试 `CloseIsNotGuardedForBlankConnection` |
| F3 | manufactureSpatialView 背景色 `0xFFFFFFFF`（alpha 错） | ViewPicker.ts:163 `ColorDef.white` = tbgr `0x00FFFFFF`（ColorByName.white=0xFFFFFF） | 改 `dqCommon::ColorDef::white.getTbgr()`；测试断言同步 |
| F4 | `Application::EventLoop` 顺序 processEvent→tileAdmin→renderLoop | IModelApp.ts:620-622：processEvent→**renderLoop→tileAdmin.process** | 交换为参考顺序（blank 下两侧均无观测效应；保链一致） |
| F5 | 默认工具在 Startup 启动；AddViewport 仅首个视口设为选中；SetSelectedViewport 不通知 ToolAdmin | ToolAdmin.ts:510-525（onInitialized **不**启动默认工具）；ViewManager.ts:294（setSelectedView 无条件选新）、:246-247（首次选择启动默认工具）、:253-259（先通知 toolAdmin 再发事件） | `ToolAdmin::OnInitialized` 移除 StartDefaultTool；`AddViewport` 总是 `SetSelectedViewport(vp)`；`SetSelectedViewport` 增加 nullptr→首视口回退、toolAdmin.onSelectedViewportChanged 通知、首次选择启动默认工具。StartupTest/stub 同步修正 |
| F6 | DanQing 跳过 ViewList 层直接 manufactureSpatialView | ViewPicker.ts:14-170（ViewList：SortedArray+缓存+clone）+ IModelConnection.ts:1461-1566（Views） | 移植 `IModelConnection::Views`（getViewList/queryDefaultViewId 的 isClosed/isOpen 短路 + load 失败返回空——§3.4 throw→错误返回适配）与 `ViewList`（populate 全链：双 getViewList、viewName 匹配、未命名回退、默认选择、合成条目、预载缓存）；View3DInventor 改走 ViewList::create→getDefaultView。新测试 ViewListTest（3 项） |
| F7 | BlankConnectionTest/ViewManagerTest 多个测试的 `Ported from: BlankConnection.test.ts/ViewManager.test.ts` 标注为假（参考无这些测试），且断言与参考实现相反（isClosed=false、"first added stays selected"） | §5 测试保真 | 逐条修正出处（2 条真实存在的参考测试保留 Ported 并精确到行；其余改 Authored + 参考实现行号），断言全部对齐参考语义 |

### 4.2 记录的等价/平台差异（不修复，附理由）

| 项 | 参考 | DanQing | 理由 |
|---|---|---|---|
| 基类前端对象群 | IModelConnection ctor 建 Models/Elements/CodeSpecs/Views/Categories/SelectionSet/HiliteSet/Tiles/GeoServices/transientIds（IModelConnection.ts:218-236） | SelectionSet/HiliteSet/**Views**（本轮补）有；其余无 | blank 路径除 Views 外均不可达；已就地在 IModelConnection.h 标 TODO（随 SnapshotConnection 补） |
| hilited.onModelSubCategoryModeChanged→viewManager.onSelectionSetChanged 接线 | IModelConnection.ts:233-235 | 无（HiliteSet 无此事件） | blank 不触发；TODO 随 HiliteSet 完整移植 |
| TileAdmin props | DTA setConfigurationResults：retryInterval:50、enableInstancing:true、enableIndexedEdges:true、enableExternalTextures:true、enableFrontendScheduleScripts:true（DisplayTestApp.ts:112-155） | TileAdmin::instance() 无 props | blank 无瓦片树，全部不可达 |
| RenderSystem.Options | DTA：logarithmicDepthBuffer 默认 **true**、dpiAwareViewports true、useWebGL2 true、planProjections true、antialiasSamples 默认 1（Target.ts:185） | DanQing RenderSystemOptions（dpiAwareViewports/antialiasSamples=4/debugShaders/logarithmicDepthBuffer=false）；GL 初始化在 per-viewport RenderPipeline（WGL context per-window 架构必需） | log-z 未实现（无大场景深度精度需求时无观测差异）；MSAA：DTA 默认 1（无 MSAA），DanQing QSurfaceFormat samples=4 但离屏 FBO 管线不消费它——渲染输出等价 |
| IModelApp.startup 杂项 | sessionId=Guid、applicationId 默认 "2686"、Logger、RPC request context、二次 startup（RPC env 重取）、localization | 无 | RPC/遥测/本地化设施，桌面 blank 无观测效应 |
| DTA SVT 工具注册 | ~60 个工具 + defaultToolId="SVTSelect"（App.ts:391-460） | 核心 Select/Idle/View.* 注册；defaultToolId="Select"（SVTSelect 仅多设 locateManager.options.allowExternalIModels，blank 无 locate） | UI 拓扑差异（预期）；blank 流程只经默认工具安装 |
| ViewManager.onInitialized 4 装饰器 | accuSnap/tentativePoint/accuDraw/toolAdmin 注册为 always-on 装饰器（ViewManager.ts:126-137） | ViewManager 构造注册 AcsTriadDecorator（5.x 的 ACS 在 AccuDraw.decorate 内，DanQing 独立装饰器为上游旧形态） | blank 下四个参考装饰器均不产出图形（无 snap/tentative/accudraw 活动） |
| ScreenViewport.create 零尺寸 throw + 同步 createTarget + initialize(EventController) | Viewport.ts:3196-3206 | DanQing 构造不设 GL（Qt 原生窗口未就绪），showEvent 延迟 Init；输入经 QWidget 事件覆盖（EventController 的平台等价物） | 平台适配（Qt 原生窗口生命周期）；首帧前必初始化，结果等价 |
| IModelConnection.onOpen 的 DTA 监听器 | App.ts:377-389（非 blank 才设 formatsProvider） | 无监听 | blank 下参考实现立即 return——等价 |
| view.clone() 的 ViewPicker 消费 | Viewer 持有 ViewList 供视图切换下拉框 | View3DInventor 局部 ViewList（无视图切换 UI） | UI 拓扑差异；clone 语义已在 ViewList 内保持 |

### 4.3 本轮新发现的非代码问题（已修）

- **DisplayTestApp 是 console 子系统**（`add_executable` 未标 WIN32）：从终端启动时附着宿主控制台/管道，GUI 存活期间持有宿主终端句柄，异常退出/管道悬挂可拖垮宿主终端（用户报告"启动应用后终端退出"）。修复：改 WIN32 GUI 子系统 + `Qt6::EntryPointPrivate` WinMain 转发（FreeCAD 的 GUI 应用形态；macOS 上 WIN32 关键字被忽略，跨平台安全）。验证：PE 子系统字节=2（GUI）；启动存活 8s、干净退出、事件日志零崩溃。

### 4.4 遗留（已知，不阻塞）

- 缩放时 triad 双影/跳跃（origin 每帧两态交替，根因未定，独立排查）。
- GUI 自动化限制：blank 打开后视口的原生 WGL 窗口使主窗口 Qt UIA 树塌缩，工具栏 Grid/Sky/ACS 无法经 UIA 寻址；物理坐标点击不安全（前台窗口不确定）。grid/ACS/sky 的像素验证由 dqRender RenderSmokeTest 无头覆盖；GUI 侧验证天空背景（`scripts/gui_check.ps1`）。

## 5. 第三轮审计：初始天空渲染链（2026-09-10）

目标：open blank connection 的初始天空渲染，从 `environment.withDisplay({sky:true})` 到像素的每环节与 display-test-app 一致。

### 5.1 参考天空执行路径（运行验证 + 精读）

```
ViewState3d.attachToViewport (ViewState.ts:2355-2363)
  → new EnvironmentDecorations(view, onLoaded=invalidateDecorations, onDispose)
      ctor: loadSkyBox() — environment.sky 非 SkyCube/SkySphere →
      setSky(createSkyGradientParams()) = { type:"gradient", gradient: env.sky.gradient,
                                            zOffset: iModel.globalOrigin.z }   // EnvironmentDecorations.ts:274-280
Viewport.renderFrame → addDecorations（装饰失效时）
  → view.decorate(context) (ViewState.ts:2211-2215)
      → EnvironmentDecorations.decorate (EnvironmentDecorations.ts:87-97):
          displayAtmosphere? gradient : (displaySky && params) →
          sky = IModelApp.renderSystem.createSkyBox(_sky.params)   // 每次 decorate 重建
          context.setSkyBox(sky) → Decorations.skyBox (ViewContext.ts:354-358)
  → target.changeDecorations → RenderCommands.init → addSkyBox (RenderCommands.ts:556→:192-201,
      forcedRenderPass=SkyBox + decorationsState push/pop)
System.createSkyBox (System.ts:632-637)
  → gradient: SkySpherePrimitive.create(SkySphereViewportQuadGeometry.createGeometry(params))
      geometry ctor (CachedGeometry.ts:656-718): zenith/nadir 常入；twoColor→{-1,4,4}+sky/ground 置 0，
      否则 {1,skyExp,groundExp}+sky/ground 入（默认 SkyGradient：sky=(142,205,255) ground=(143,205,125)
      zenith=(54,117,255) nadir=(40,125,0) exp=4.0 twoColor=false — core-common SkyBox.ts:139-176）
SkySpherePrimitive.draw (Primitive.ts:161-164) → initWorldPos（每 geometry 一次）：
      非 globe：worldPos=视锥 Rear↔Front 0.5 中深四角 (lb,rb,rt,lt)（CachedGeometry.ts:583-596）
SceneCompositor.draw：clearOpaque（背景色清屏）→ renderBackground → renderSkyBox (:1744-1756,
      FBO=backgroundFbo，renderState=_noDepthMaskRenderState{cull/depthTest/blend=false,depthMask=false}，
      default 分支 :2332) → renderBackgroundMap → renderOpaque …
SkySphereGradient shader（glsl/SkySphere.ts:24-48 + ViewportQuad.ts:14）：
      顶点 gl_Position=rawPos（NDC z=0；depth test 关 → 等效任意 z）；computeGradientValue 逐顶点：
      eyeToVert=a_worldPos-u_worldEye；radius=√(x²+y²)；zValue=z-radius·u_zOffset；d=atan(zValue,radius)；
      2 色 d=0.5-d/π；4 色 d/1.5708 → vec4(d, 1-(d-h)/(1-h), 1-(-d-h)/(1-h), (d+h)/(2h))，h=horizonSize=0.0015
      片段：2 色 mix(zenith,nadir,d)；4 色：上半球 mix(zenith,sky,pow(y,skyExp)) /
      下半球 mix(nadir,ground,pow(z,groundExp)) / 地平线带 mix(ground,sky,w)
      u_worldEye（SkySphere.ts:237-266）：透视=planFrustum 角点+1/(1-planFraction)；
      正交=伪相机：focalLength=|LBR→RTR|/(2·**Math.atan**(22.5°))（注意 atan 非 tan——参考怪癖，逐字保留）、
      zScale=max(focalLength/|LBF-LBR|, 1.000001)、eye=rearCenter+delta·zScale
      uniform 绑定按 plan.backgroundMapOn 每帧替换 ground/nadir←skyColor（非 globe）——SkySphere.ts:145-180
```

### 5.2 DanQing 对照结论（初始天空 = blank 正交顶视 + 4 色渐变 + backgroundMapOn）

| # | 环节 | 状态 |
|---|---|---|
| 触发链 | 参考：attachToViewport→EnvironmentDecorations→decorate→createSkyBox（装饰重建即重建 graphic） | DanQing：Viewport::CollectDecorations 内联（create-once + updateSkySphere 每次重建 + SetSkyBox）——blank 下等价；EnvironmentDecorations 类/onEnvironmentChanged 重载链未移植（TODO） |
| params | gradient ✅；zOffset 原硬编码 0 → 本轮改读 `iModel->GetGlobalOrigin().z`（EnvironmentDecorations.ts:278；blank 等值 0） | ✅ 已修 |
| createSkyBox | gradient → SkySphereViewportQuadGeometry::createGeometry + Primitive | ✅ 1:1（cube/纹理路径 TODO 已标） |
| geometry 渐变解包 | 2/4 色分支、指数、颜色 /255 | ✅ 1:1（SkySphereMathTest 锁） |
| backgroundMapOn | 参考：uniform 绑定时按 plan 每帧替换 ground/nadir←skyColor | DanQing：烘焙在 geometry ctor（创建时 viewFlags）——初始等价；运行时 toggle backgroundMap 后 DanQing 不刷新（缓存 graphic）——记录差距 |
| worldPos | 中深四角、顺序 lb/rb/rt/lt | ✅ 1:1（globe 分支未移植——blank 非 globe，TODO） |
| **u_worldEye** | **focalLength = diagonal/(2·Math.atan(22.5°))** | 原用 `tan` → **本轮修复为 atan**（SkySphereMathTest 期望同步：eye.z=-8.8967）；透视分支（planFraction）未移植——blank 初始正交不经过，TODO |
| 顶点/片段 shader | computeGradientValue/computeSkySphereColorGradient/horizonSize=0.0015 | ✅ 逐字一致（kSkySphereGradientVert/Frag）；gl_Position z 值不同（z=0 vs .xyww z=1）但 sky pass depthTest/depthMask 双关 → 像素等价 |
| sky pass 状态 | _noDepthMaskRenderState（cull/depthTest/blend=false, depthMask=false） | ✅ m_noDepthMaskRenderState 同值（RenderState 默认值 1:1） |
| pass 顺序 | clearOpaque(bg)→background→skyBox→backgroundMap→opaque（LOAD 色/清深度保天空） | ✅ SceneCompositorImpl::draw 同序 |
| 命令路由 | decorations.skyBox→addSkyBox（forcedRenderPass + decorationsState push/pop） | ✅ forcedRenderPass；无 decorationsState push/pop（天空不消费 branch symbology，等价；记录） |
| SkyGradient 默认值 | sky=(142,205,255) ground=(143,205,125) zenith=(54,117,255) nadir=(40,125,0) exp=4.0 twoColor=false | ✅ dqCommon/SkyBox.h 1:1 |
| atmosphere/ground plane | displayAtmosphere/displayGround 分支（EnvironmentDecorations.ts:91-124） | 未移植（blank 默认双关）——TODO |
| cube/纹理天空 shader | glsl/SkyBox.ts（u_rot swizzle）、SkySphere.ts 纹理分支 | DanQing kSkyBox*/kSkySphereTexture* 为占位实现（非 1:1）——blank 不用，TODO 标记 |

**GUI 复验**（atan 修复后）：blank connection 天空渐变正常（顶部深→下方浅蓝），应用稳定。

### 5.3 修复清单（本轮）

- F-sky-1：`ComputeSkySphereWorldPosAndEye` focalLength `tan`→`atan`（SkySphere.ts:255 参考怪癖逐字保留）。TDD：SkySphereMathTest.WorldPosMidDepthAndOrthoEye 期望值 -7.0711→-8.8967（先 RED 后 GREEN）。
- F-sky-2：`Viewport::CollectDecorations` 天空 zOffset 硬编码 0 → `iModel->GetGlobalOrigin().z`（EnvironmentDecorations.ts:278）。

## 6. 附：display-test-app 运行方式（备查）

```bash
cd D:\Github\itwinjs-core\test-apps\display-test-app
npm start   # vite(localhost:3000) + electron(DtaElectronMain.js, --remote-debugging-port=9223)
```
- 前端 vite dev server（首次 build 较慢）；Electron 主进程带 remote-debugging。
- DevTools：Ctrl+Shift+I（DtaElectronMain.js 注册了快捷键）。
- 已知坑：vite 文件监视偶发 EBUSY（cesium worker 被杀毒/索引占用，重试即可）；3000 端口残留需清理。
