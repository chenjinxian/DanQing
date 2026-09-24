# DanQing DisplayTestApp 启动流程实现对照

> **用途**: 对照 itwinjs-core display-test-app 的完整启动流程，逐项检查 DanQing 的实现状态，指导后续开发。
>
> **范围**: 从进程启动到主页面就绪（用户可以点击"Open Blank Connection"之前）的全部步骤。
>
> **架构差异**: itwinjs-core 是 RPC 前后端分离架构；DanQing 是单进程桌面程序，所有层直接函数调用。

---

## 目录

1. [架构差异总览](#1-架构差异总览)
2. [itwinjs-core 启动流程全景](#2-itwinjs-core-启动流程全景)
3. [逐阶段对照分析](#3-逐阶段对照分析)
4. [DanQing 当前实现状态](#4-danqing-当前实现状态)
5. [缺失项详细分析](#5-缺失项详细分析)
6. [实现优先级矩阵](#6-实现优先级矩阵)

---

## 1. 架构差异总览

### itwinjs-core: RPC 前后端分离

```
┌─────────────────────┐     RPC/IPC      ┌─────────────────────┐
│  Frontend           │ ◄──────────────► │  Backend            │
│  (Browser/Electron) │  DtaRpcInterface │  (Node.js)          │
│                     │  IModelReadRpc   │                     │
│  IModelApp          │  IModelTileRpc   │  IModelHost         │
│  RenderSystem       │  ECSchemaRpc     │  IModelDb           │
│  ViewManager        │                  │  TileStorage        │
└─────────────────────┘                  └─────────────────────┘
```

### DanQing: 单进程直接调用

```
┌─────────────────────────────────────────────────────────┐
│  单一桌面进程 (C++ / Qt)                                 │
│                                                         │
│  samples/DisplayTestApp                                 │
│       ↓                                                 │
│  dqApp (Application + ViewManager + Viewport + Tools)   │
│       ↓                                                 │
│  dqRender (RenderSystem + TileAdmin + RHI Driver)       │
│       ↓                                                 │
│  dqCommon (ViewFlags + Frustum + Camera + ColorDef)     │
│       ↓                                                 │
│  dqBase (RefCounted + DqEvent + DqId)                   │
└─────────────────────────────────────────────────────────┘
```

**关键影响**:
- 所有 RPC/IPC 代码 → **删除**
- 所有认证代码 → **删除**
- 所有 Hub 访问代码 → **删除**
- 双重启动机制 → **删除**
- 后端配置获取 → **删除**（直接读取配置）

---

## 2. itwinjs-core 启动流程全景

```
index.html 加载
  │
  ▼
src/index.ts → import "./frontend/DisplayTestApp"
  │
  ▼
dtaFrontendMain() 执行
  │
  ├── [Phase A] 配置加载
  │   ├── RpcConfiguration.developmentMode = true
  │   ├── RpcConfiguration.disableRoutingValidation = true
  │   ├── getFrontendConfig()  // 读取 IMJS_* 环境变量
  │   └── setConfigurationResults()  // 构建 RenderSystem.Options + TileAdmin.Props
  │
  ├── [Phase B] 框架启动（第一次）
  │   └── DisplayTestApp.startup()
  │       ├── 构建 IModelAppOptions
  │       ├── 平台启动 (Electron/Mobile/Browser)
  │       └── IModelApp.startup()
  │           ├── _setupRpcRequestContext()
  │           ├── Localization.initialize()
  │           ├── Register built-in tools (Select, Idle, View, ClipView, Measure, AccuDraw)
  │           ├── Register entity states (EntityState, ElementState, ModelState, etc.)
  │           ├── RenderSystem.create()  ← WebGL 上下文
  │           ├── TileAdmin.create()     ← 瓦片管理器
  │           ├── 创建子系统 (ViewManager, ToolAdmin, AccuDraw, AccuSnap,
  │           │               ElementLocateManager, TentativePoint, QuantityFormatter,
  │           │               UiAdmin, MapLayerFormatRegistry, TerrainProviderRegistry,
  │           │               RealityDataSourceProviderRegistry, FormatsProviderManager)
  │           ├── onInitialized() 各子系统
  │           │   ├── System.onInitialized()
  │           │   │   ├── LineCode.initializeCapacity(maxTextureSize)
  │           │   │   ├── Techniques.create()  ← 编译所有着色器
  │           │   │   ├── 创建 noise texture (4x4)
  │           │   │   ├── 创建 line code texture
  │           │   │   └── ScreenSpaceEffects 初始化
  │           │   ├── ViewManager.onInitialized()
  │           │   │   ├── 添加 AccuSnap 装饰器
  │           │   │   ├── 添加 TentativePoint 装饰器
  │           │   │   ├── 添加 AccuDraw 装饰器
  │           │   │   ├── 添加 ToolAdmin 装饰器
  │           │   │   ├── 设置光标为 "default"
  │           │   │   └── [可选] 启动 idle shader 预编译定时器
  │           │   ├── ToolAdmin.onInitialized()
  │           │   ├── AccuDraw.onInitialized()
  │           │   ├── AccuSnap.onInitialized()
  │           │   ├── ElementLocateManager.onInitialized()
  │           │   ├── TentativePoint.onInitialized()
  │           │   └── UiAdmin.onInitialized()
  │           ├── QuantityFormatter.onInitialized()
  │           └── onAfterStartup.raiseEvent()  ← 触发 ExtensionAdmin.onStartup
  │
  ├── [Phase B'] DisplayTestApp.startup() 后续
  │   ├── IModelApp.applicationLogoCard 设置
  │   ├── IModelConnection.onOpen 监听器 (SchemaFormatsProvider)
  │   ├── 注册 ~60+ 工具到 "SVTTools" 命名空间
  │   ├── defaultToolId = SVTSelectionTool
  │   ├── BingTerrainMeshProvider.register()
  │   ├── FrontendDevTools.initialize()
  │   ├── HyperModeling.initialize()
  │   ├── EditTools.initialize()
  │   ├── MapLayersFormats.initialize()
  │   └── EditTools.registerProjectLocationTools()
  │
  ├── [Phase C] 框架启动（第二次，非移动端）
  │   ├── getFrontendConfig(true)  ← RPC 从后端获取真实配置
  │   ├── IModelApp.shutdown()
  │   └── DisplayTestApp.startup()  ← 重复上述全部流程
  │
  ├── [Phase C'] 前端瓦片初始化（可选）
  │   └── initializeFrontendTiles({enableEdges, computeSpatialTilesetBaseUrl})
  │
  ├── [Phase D] UI 准备与认证
  │   ├── enableDiagnostics?  // IModelApp.renderSystem.debugControl?.enableDiagnostics()
  │   ├── Logger.initializeToConsole()
  │   ├── Logger.setLevelDefault(LogLevel.Warning)
  │   ├── Logger.setLevel("core-frontend.Render", LogLevel.Error)
  │   ├── displayUi() → showSpinner()
  │   └── signIn()  ← 认证循环 (BrowserAuthorizationClient / ElectronRendererAuthorization)
  │
  ├── [Phase E] iModel 打开
  │   ├── [有 iModelName] openFile({fileName, writable})
  │   │   └── openIModel()
  │   │       ├── SnapshotConnection.openFile() 或
  │   │       └── BriefcaseConnection.openFile()
  │   │       └── setTitle(iModel)
  │   ├── [有 iModelId+iTwinId] openFile({iModelId, iTwinId, writable})
  │   │   └── openHubIModel()
  │   │       ├── NativeApp.requestDownloadBriefcase()
  │   │       └── openIModelFile()
  │   │       └── setTitle(iModel)
  │   └── [移动端] MobileMessenger.postMessage("modelOpened", iModelName)
  │
  └── [Phase F] 视图初始化
      └── initView(iModel)
          ├── showStatus("opening View", configuration.viewName)
          ├── new Surface(configuration, appSurfaceDiv, toolBarDiv, fileSelector, openReadWrite)
          │   ├── window.onbeforeunload = closeAllViewers
          │   ├── 创建主工具栏 (5 个按钮)
          │   │   ├── "Open iModel from disk" (icon: )
          │   │   ├── "Open Blank Connection" (icon: )
          │   │   ├── "Analysis Style Example" (icon: )
          │   │   ├── "Decoration Geometry Example"
          │   │   └── "Cesium Renderer Example"
          │   ├── addSnapModes()  ← 捕捉模式组合框
          │   ├── new TileLoadIndicator()  ← 瓦片加载进度条
          │   ├── new FpsMonitor()  ← FPS 监控
          │   ├── new KeyinField()  ← 命令输入框
          │   ├── new NotificationsWindow()  ← 通知窗口
          │   ├── 全局快捷键监听 (` → keyin, Ctrl+[/] → 窗口切换, etc.)
          │   ├── window.onresize → 重新停靠窗口
          │   └── viewManager.onSelectedViewportChanged → 切换工具栏
          ├── await documentLoaded()
          ├── surface.createViewer({iModel, defaultViewName, disableEdges, configuration})
          │   └── Viewer.create(surface, props)
          │       ├── ViewList.create(iModel, viewName)  ← 查询 iModel 所有视图定义
          │       ├── views.getDefaultView(iModel)  ← 加载默认视图状态
          │       └── new Viewer(surface, view, views, props)
          │           ├── Window constructor (浮动窗口容器, 可拖拽/调整大小)
          │           ├── ScreenViewport.create(contentDiv, view)  ← 第一次渲染！
          │           ├── ToolBar 创建 (20+ 按钮/下拉菜单)
          │           │   ├── Debug info, Open from disk/hub
          │           │   ├── View picker, Models, Categories
          │           │   ├── Saved views, Camera paths
          │           │   ├── Element selection, Measure distance
          │           │   ├── View settings, Fit view, Rotate, Walk
          │           │   ├── Undo/Redo, Animation/Timeline
          │           │   ├── Sectioning, Spatial Classification
          │           │   ├── Feature overrides, Point cloud, Contours
          │           │   └── Google Maps (if configured)
          │           └── viewer.dock(Dock.Full)  ← 全屏最大化
          ├── showStatus("View Ready")
          ├── hideSpinner()
          └── [可选] startupMacro → IModelApp.tools.parseAndRun("dta macro ...")
```

---

## 3. 逐阶段对照分析

### Phase A: 配置加载

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 说明 |
|---|---|---|---|---|
| A1 | `RpcConfiguration.developmentMode = true` | ❌ 删除 | 无 | 无 RPC |
| A2 | `RpcConfiguration.disableRoutingValidation = true` | ❌ 删除 | 无 | 无 RPC |
| A3 | `getFrontendConfig()` 读取 `IMJS_*` 环境变量 | ✅ 需要 | ⚠️ 简化版 | `Application::Options` 手动构建 |
| A4 | `setConfigurationResults()` 构建渲染配置 | ✅ 需要 | ⚠️ 简化版 | `RenderSystemOptions` 只有 4 个字段 |
| A5 | `DtaConfiguration` 类型定义 | ✅ 需要 | ⚠️ 不完整 | 已有 `Application::Options`，但字段不全 |

**DanQing 当前实现** (`samples/DisplayTestApp/main.cpp:32-36`):
```cpp
dqApp::Application::Options opts;
opts.applicationId = "DisplayTestApp";
opts.applicationVersion = "1.0.0";
opts.renderSystemOptions.antialiasSamples = 4;
opts.renderSystemOptions.dpiAwareViewports = true;
```

**缺失**:
- [ ] 无统一配置文件/环境变量读取机制
- [ ] `RenderSystemOptions` 字段不全（缺少 `logarithmicDepthBuffer`, `useWebGL2`, `debugShaders` 等）
- [ ] 无 `TileAdmin.Props` 配置

---

### Phase B: 框架启动 — `IModelApp.startup()`

#### B1: 基础初始化

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| B1.1 | Guard 防重复初始化 | ✅ 必须 | ✅ 已实现 | `Application.cpp:21` |
| B1.2 | `_initialized = true` | ✅ 必须 | ✅ 已实现 | `Application.cpp:45` |
| B1.3 | Security options | ❌ 删除 | 无 | — |
| B1.4 | Dev mode (`window.IModelApp`) | ❌ 删除 | 无 | — |
| B1.5 | Session/Application identity | ⚠️ 简化 | ✅ 已实现 | `Application.cpp:24-25` |
| B1.6 | `_setupRpcRequestContext()` | ❌ 删除 | 无 | — |

#### B2: 国际化

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 说明 |
|---|---|---|---|---|
| B2.1 | `Localization.initialize(["iModelJs", "CoreTools"])` | ⚠️ 后期 | ❌ 缺失 | 后期实现 |

#### B3: 内置工具注册

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| B3.1 | `SelectTool` 注册 | ✅ 必须 | ✅ 已实现 | `ToolAdmin.cpp` |
| B3.2 | `IdleTool` 注册 | ✅ 必须 | ✅ 已实现 | `ToolAdmin.cpp` |
| B3.3 | `ViewTool` 注册 | ⚠️ 后期 | ⚠️ 抽象基类 | — | ViewTool 是抽象类，不直接注册；itwinjs-core 也不直接注册 |
| B3.4 | `ClipViewTool` 注册 | ⚠️ 后期 | ❌ 缺失 | — |
| B3.5 | `MeasureTool` 注册 | ⚠️ 后期 | ❌ 缺失 | — |
| B3.6 | `AccuDrawTool` 注册 | ⚠️ 后期 | ❌ 缺失 | — |

#### B4: Entity State 注册

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| B4.1 | `EntityState` 注册 | ✅ 必须 | ⚠️ 间接 | `Application.cpp:85-108` | EntityState 有 protected 构造，通过具体子类（Element, Model 等）注册 |
| B4.2 | `ElementState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:85-87` |
| B4.3 | `ModelState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:88-90` |
| B4.4 | `GeometricModelState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:91-93` |
| B4.5 | `GeometricModel3dState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:94-96` |
| B4.6 | `GeometricModel2dState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:97-99` |
| B4.7 | `SpatialModelState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:100-102` |
| B4.8 | `CategorySelectorState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:103-105` |
| B4.9 | `ModelSelectorState` 注册 | ✅ 必须 | ✅ 已实现 | `Application.cpp:106-108` |

#### B5: RenderSystem 创建 — **核心**

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| B5.1 | `document.createElement("canvas")` | ✅ 必须 | ✅ 已实现 | `Viewport.cpp:43` | QOpenGLWidget 提供 |
| B5.2 | `canvas.getContext("webgl2")` | ✅ 必须 | ✅ 已实现 | `Viewport.cpp:107-113` | Qt 创建 GL 上下文 |
| B5.3 | `Capabilities.create(context)` | ✅ 必须 | ✅ 已实现 | `RenderPipeline.cpp` | Driver 内部查询 |
| B5.4 | `context.depthFunc(GL.DepthFunc.Default)` | ✅ 必须 | ✅ 已实现 | Driver 初始化 | — |
| B5.5 | `new System(canvas, context, capabilities)` | ✅ 必须 | ✅ 已实现 | `RenderPipeline.cpp` | `RenderSystemImpl` |
| B5.6 | Resource cache 创建 | ✅ 必须 | ⚠️ 部分 | `RenderSystemImpl` | `IdMap` 缓存 |
| B5.7 | `GLTimer` 创建 | ⚠️ 可选 | ❌ 缺失 | — | 性能监控 |
| B5.8 | `webglcontextlost` 事件 | ❌ 删除 | 无 | — | 桌面程序无此问题 |

**DanQing 当前实现** (`Viewport.cpp:137-152`):
```cpp
void Viewport::SetupDriver()
{
    mPipeline = std::make_unique<dqRender::RenderPipeline>();
    auto* qtContext = QOpenGLContext::currentContext();
    if (!mPipeline->Initialize(qtContext)) {
        mPipeline.reset();
        return;
    }
    // RenderTarget creation deferred — SceneCompositor path has Phase 2/3 dependencies
}
```

**关键差异**: itwinjs-core 在 `IModelApp.startup()` 中创建 RenderSystem（全局单例）；DanQing 在每个 Viewport 的 `initializeGL()` 中创建 RenderPipeline（非全局单例）。这是合理的设计差异——Qt 的 QOpenGLWidget 要求在 `initializeGL()` 中初始化 GL 资源。

#### B6: TileAdmin 创建

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| B6.1 | `queryConcurrency("cpu")` | ✅ 必须 | ❌ 缺失 | — | `std::thread::hardware_concurrency()` |
| B6.2 | `ProcessDetector.isMobileBrowser` | ❌ 删除 | 无 | — | 桌面程序 |
| B6.3 | `new TileAdmin(isMobile, rpcConcurrency, props)` | ✅ 必须 | ✅ 已实现 | `TileAdmin.h` | 单例模式 |
| B6.4 | `TileRequestChannels` 配置 | ✅ 必须 | ⚠️ 部分 | `TileRequestChannel.h` | 基础通道 |
| B6.5 | GPU 内存限制 | ✅ 必须 | ❌ 缺失 | — | 内存管理 |
| B6.6 | Tile 过期/重试 | ✅ 必须 | ⚠️ 部分 | `TileAdmin.h` | 基础生命周期 |
| B6.7 | `IModelConnection.onClose` 监听 | ✅ 必须 | ❌ 缺失 | — | 清理瓦片缓存 |

**DanQing 当前实现**: `TileAdmin` 作为单例存在，有基础的 `process()` 循环，但缺少完整的配置和内存管理。

#### B7: 子系统创建

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| B7.1 | `ViewManager` 创建 | ✅ 必须 | ✅ 已实现 | `Application.h:90` (成员变量) |
| B7.2 | `NotificationManager` 创建 | ✅ 必须 | ✅ 已实现 | `Application.h:92` (成员变量) |
| B7.3 | `ToolAdmin` 创建 | ✅ 必须 | ✅ 已实现 | `Application.h:91` (成员变量) |
| B7.4 | `AccuDraw` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.5 | `AccuSnap` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.6 | `ElementLocateManager` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.7 | `TentativePoint` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.8 | `QuantityFormatter` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.9 | `UiAdmin` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.10 | `MapLayerFormatRegistry` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.11 | `TerrainProviderRegistry` 创建 | ⚠️ 后期 | ❌ 缺失 | — |
| B7.12 | `FormatsProviderManager` 创建 | ⚠️ 后期 | ❌ 缺失 | — |

#### B8: `onInitialized()` 调用

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| B8.1 | `System.onInitialized()` | ✅ 必须 | ⚠️ 延迟 | `RenderPipeline::Initialize()` | 在 Viewport 中完成 |
| B8.1.1 | — `LineCode.initializeCapacity(maxTextureSize)` | ✅ 必须 | ❌ 缺失 | — | 线型纹理，依赖 RenderSystem.maxTextureSize |
| B8.1.2 | — `Techniques.create()` | ✅ 必须 | ✅ 已实现 | `RenderPipeline.cpp` | 16+ 着色器 |
| B8.1.3 | — 创建 noise texture | ✅ 必须 | ❌ 缺失 | — | 4x4 噪声纹理 |
| B8.1.4 | — 创建 line code texture | ✅ 必须 | ❌ 缺失 | — | 线型纹理 |
| B8.1.5 | — `ScreenSpaceEffects` 初始化 | ⚠️ 后期 | ❌ 缺失 | — | 后处理 |
| B8.2 | `ViewManager.onInitialized()` | ✅ 必须 | ❌ 缺失 | — | DanQing 无此方法；itwinjs-core 在此添加 AccuSnap/TentativePoint/AccuDraw/ToolAdmin 装饰器并设置光标 |
| B8.3 | `ToolAdmin.onInitialized()` | ✅ 必须 | ✅ 已实现 | `Application.cpp:43` | 注册核心工具 |
| B8.4 | `AccuDraw.onInitialized()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B8.5 | `AccuSnap.onInitialized()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B8.6 | `QuantityFormatter.onInitialized()` | ⚠️ 后期 | ❌ 缺失 | — | — |

#### B9: 后续初始化

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| B9.1 | `onAfterStartup.raiseEvent()` | ✅ 必须 | ✅ 已实现 | `Application.cpp:46` |
| B9.2 | `ExtensionAdmin.onStartup` | ⚠️ 后期 | ❌ 缺失 | — |

#### B10: DisplayTestApp.startup() 后续步骤

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| B10.1 | `IModelApp.applicationLogoCard` | ❌ 不需要 | 无 | — | 桌面程序 |
| B10.2 | `IModelConnection.onOpen` 监听器 | ⚠️ 后期 | ❌ 缺失 | — | SchemaFormatsProvider |
| B10.3 | 注册 ~60+ 工具到 "SVTTools" | ⚠️ 部分 | ⚠️ 仅 2 个 | `ToolAdmin.cpp:34-35` | 只注册了 Select 和 Idle，itwinjs-core 有 60+ 工具 |
| B10.4 | `defaultToolId = SVTSelectionTool` | ✅ 必须 | ✅ 已实现 | `ToolAdmin.cpp` | — |
| B10.5 | `BingTerrainMeshProvider.register()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B10.6 | `FrontendDevTools.initialize()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B10.7 | `HyperModeling.initialize()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B10.8 | `EditTools.initialize()` | ⚠️ 后期 | ❌ 缺失 | — | — |
| B10.9 | `MapLayersFormats.initialize()` | ⚠️ 后期 | ❌ 缺失 | — | — |

---

### Phase C: 双重启动（非移动端）

| # | itwinjs-core 步骤 | DanQing 需要? | 说明 |
|---|---|---|---|
| C1 | `getFrontendConfig(true)` via RPC | ❌ 删除 | 无 RPC |
| C2 | `IModelApp.shutdown()` | ❌ 删除 | 不需要重启 |
| C3 | 重新 `startup()` | ❌ 删除 | 一次性启动 |

**这是 RPC 架构的特有 workaround**，DanQing 完全不需要。

---

### Phase C': 前端瓦片初始化（可选）

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 说明 |
|---|---|---|---|---|
| C'.1 | `initializeFrontendTiles()` | ❌ 不需要 | 无 | DanQing 从本地文件读取瓦片，无 HTTP 瓦片 |

---

### Phase D: UI 准备与认证

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 |
|---|---|---|---|---|
| D1 | `enableDiagnostics()` | ⚠️ 可选 | ❌ 缺失 | — |
| D2 | `Logger.initializeToConsole()` | ✅ 需要 | ⚠️ 部分 | dqBase 有日志系统 |
| D3 | `Logger.setLevelDefault(LogLevel.Warning)` | ✅ 需要 | ⚠️ 部分 | — |
| D4 | `Logger.setLevel("core-frontend.Render", LogLevel.Error)` | ✅ 需要 | ❌ 缺失 | — |
| D5 | `displayUi()` → `showSpinner()` | ✅ 需要等价 | ✅ 已实现 | Qt 等待光标 |
| D6 | `signIn()` 认证循环 | ❌ 删除 | 无 | — |

---

### Phase E: iModel 打开

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| E1 | `SnapshotConnection.openFile()` | ✅ 必须 | ❌ 缺失 | — | 打开本地 .bim 文件 |
| E2 | `BriefcaseConnection.openFile()` | ⚠️ 后期 | ❌ 缺失 | — | Hub briefcase |
| E3 | `NativeApp.requestDownloadBriefcase()` | ❌ 删除 | 无 | — | 无 Hub |
| E4 | `openIModelFile()` | ✅ 必须 | ❌ 缺失 | — | 文件打开核心 |
| E5 | `setTitle(iModel)` | ✅ 需要 | ✅ 已实现 | `DisplayTestApp.cpp:33` | 窗口标题 |
| E6 | `BlankConnection.create()` | ✅ 必须 | ✅ 已实现 | `DisplayTestApp.cpp:71` | 空白连接 |
| E7 | `MobileMessenger.postMessage("modelOpened")` | ❌ 删除 | 无 | — | 移动端消息 |

**DanQing 当前实现**: 只有 `BlankConnection`，无真实 iModel 文件打开能力。

---

### Phase F: 视图初始化 — `initView()`

#### F1: Surface 构造

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| F1.1 | `window.onbeforeunload` | ✅ 需要 | ✅ 已实现 | `MainWindow` | Qt `closeEvent` |
| F1.2 | 主工具栏创建 (5 按钮) | ✅ 需要 | ⚠️ 部分 | `MainWindow.cpp:79-91` | 工具栏存在 (File/Edit/View)，但只有 New/Close 按钮，缺少 itwinjs-core 的 5 个特定按钮 (Open File, Open Blank, Analysis, Decoration, Cesium) |
| F1.3 | `addSnapModes()` | ✅ 需要 | ❌ 缺失 | — | 捕捉模式 UI |
| F1.4 | `TileLoadIndicator` | ✅ 需要 | ❌ 缺失 | — | 瓦片加载进度 |
| F1.5 | `FpsMonitor` | ⚠️ 可选 | ❌ 缺失 | — | FPS 显示 |
| F1.6 | `KeyinField` | ✅ 需要 | ❌ 缺失 | — | 命令输入框 |
| F1.7 | `NotificationsWindow` | ✅ 需要 | ✅ 已实现 | `ReportView` | 报告视图 |
| F1.8 | 全局快捷键 | ✅ 需要 | ⚠️ 部分 | `DisplayTestApp.cpp:48` | 只有 Ctrl+I，缺少 `, Ctrl+[/], Ctrl+\, Ctrl+\|, Ctrl+n, Ctrl+p, Ctrl+i, Ctrl+m, Ctrl+h/l/k/j |
| F1.9 | `window.onresize` | ✅ 需要 | ✅ 已实现 | Qt 自动处理 | — |
| F1.10 | viewport 切换监听 | ✅ 需要 | ❌ 缺失 | — | 视口焦点管理，切换工具栏 |

**DanQing 当前实现**: 使用 FreeCAD 风格的 `MainWindow` + `QMdiArea`，有菜单栏但缺少工具栏。

#### F2: Viewer 构造

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| F2.1 | Window 浮动窗口容器 | ✅ 需要 | ✅ 已实现 | `MDIView` | QMdiSubWindow |
| F2.2 | `ScreenViewport.create()` | ✅ 必须 | ✅ 已实现 | `Viewport.cpp:80-88` | QOpenGLWidget |
| F2.3 | `ViewList.create()` | ✅ 必须 | ❌ 缺失 | — | 查询视图定义 |
| F2.4 | `views.getDefaultView()` | ✅ 必须 | ❌ 缺失 | — | 加载默认视图 |
| F2.5 | ToolBar 创建 (20+ 按钮) | ✅ 需要 | ❌ 缺失 | — | 视口专属工具栏 |
| F2.6 | `viewer.dock(Dock.Full)` | ✅ 需要 | ✅ 已实现 | `addWindow()` | QMdiArea 最大化 |
| F2.7 | `viewManager.addViewport()` | ✅ 必须 | ✅ 已实现 | `DisplayTestApp.cpp:100` | 注册视口 |

**DanQing 当前实现**: 视口创建流程完整，但缺少视图查询和视口专属工具栏。

#### F3: 首次渲染

| # | itwinjs-core 步骤 | DanQing 需要? | DanQing 当前状态 | 文件:行 | 说明 |
|---|---|---|---|---|---|
| F3.1 | WebGL canvas 创建 | ✅ 必须 | ✅ 已实现 | `Viewport.cpp:43` | QOpenGLWidget |
| F3.2 | GL 上下文获取 | ✅ 必须 | ✅ 已实现 | `Viewport.cpp:107-113` | Qt 自动 |
| F3.3 | RenderTarget 创建 | ✅ 必须 | ⚠️ 延迟 | `Viewport.cpp:148-151` | Phase 2 依赖 |
| F3.4 | ViewState 加载 | ✅ 必须 | ✅ 已实现 | `SpatialViewState::CreateBlank` | — |
| F3.5 | 首帧渲染 | ✅ 必须 | ✅ 已实现 | `Viewport::RenderFrame()` | 17 步管线 |

---

## 4. DanQing 当前实现状态

> **⚠️ 快照时效性说明（2026-07-17）：** 本节"当前实现状态"及下方核心路径快照编写于 **FreeCAD UI 外壳移植（Step 1）之前**，描述的是更早的 `DisplayTestApp` MainWindow 直调 `Application::Get().Startup()` 架构。Step 1（commit `b740124`）将其替换为 FreeCAD 外壳 + `stubs/`；**Step 2（2026-07-17）** 进一步新增 `Gui::Application` 门面（`sendMsgToActiveView`/`sendHasMsgToActiveView`/`newDocument`/`getGuiApplication`，1:1 移植 FreeCAD `src/Gui/Application`）+ `MainWindow::activeWindow()`，并将命令层（View 域 + `StdCmdNew` + Window 域）分发到 dqApp。本节的 itwinjs-core 对照分析（§2/§3/§5/§6）仍有效；但"当前实现状态"快照已过时，请以代码为准。

### 已完成的核心路径

```
main()
  ├── QSurfaceFormat::setDefaultFormat()     ✅ OpenGL 4.1 Core, 24-bit depth, 8-bit stencil, 4x MSAA
  ├── QApplication(argc, argv)               ✅
  ├── Application::Get().Startup(opts)       ✅
  │   ├── RegisterCoreEntityStates()         ✅ 8 BisCore 类型 (Element, Model, GeometricModel, etc.)
  │   ├── mViewManager.OnSelectionSetChanged() ✅ (no-op, 建立模式)
  │   └── ToolAdmin::OnInitialized()         ✅ 注册 2 个核心工具 (Select, Idle)，启动默认工具
  ├── DisplayTestApp()                       ✅ MainWindow 子类
  │   ├── setupMenuBar()                     ✅ File/Edit/View/Tools/Window/Help
  │   ├── setupToolBars()                    ✅ File(New,Close)/Edit/View
  │   ├── setupDockWindows()                 ✅ ComboView(左) + ReportView(下)
  │   └── setupStatusBar()                   ✅ 状态标签
  ├── OpenBlankConnection()                  ✅
  │   ├── BlankConnection::Create(props)     ✅
  │   ├── SpatialViewState::CreateBlank()    ✅
  │   ├── Viewport::Create(parent, view)     ✅ QOpenGLWidget
  │   ├── MDIView 包装 Viewport              ✅ QMdiSubWindow
  │   └── ViewManager::AddViewport(vp)       ✅ 自动启动事件循环
  ├── StartRenderLoop()                      ✅ QTimer 16ms (~60fps)
  │   └── Application::EventLoop()           ✅
  │       ├── TileAdmin::instance().process() ✅
  │       └── ViewManager::RenderLoop()      ✅
  │           └── Viewport::RenderFrame()    ✅ 17 步管线
  └── app.exec()                             ✅ Qt 事件循环
```

### 渲染管线状态

| 步骤 | 描述 | 状态 | 说明 |
|---|---|---|---|
| 1 | Capture changeFlags | ✅ | — |
| 2 | Animation | ⚠️ | Phase 2，注释掉 |
| 3 | Redraw check | ✅ | — |
| 4 | Resize detection | ✅ | Qt 处理 |
| 5 | Controller sync | ✅ | `SetupFromView()` |
| 6 | Selection set | ✅ | Hilite LUT 上传 |
| 7 | Analysis fraction | ⚠️ | Phase 2 |
| 8 | Time point | ⚠️ | Phase 2 |
| 9 | Feature overrides | ⚠️ | Phase 2 |
| 10 | Scene creation | ✅ | 地面网格 + 瓦片树 |
| 11 | Render plan | ⚠️ | 存根 |
| 12 | Decorations | ✅ | 装饰器系统完整 |
| 13 | Flash processing | ⚠️ | Phase 2 |
| 14 | Pre-render hook | ⚠️ | Phase 2 |
| 15 | Draw | ✅ | 双路径（新/旧） |
| 16 | Post-frame events | ✅ | Qt 信号 |
| 17 | Continuous rendering | ✅ | `RequestNextAnimation()` |

---

## 5. 缺失项详细分析

### 🔴 一级缺失（必须实现）

#### M1: 统一配置系统

**itwinjs-core 参考**: `DtaConfiguration.ts` + `getConfig()`

**当前状态**: `Application::Options` 只有 4 个渲染选项，无瓦片配置。

**需要实现**:
```cpp
// dqApp/PublicAPI/dqApp/AppConfig.h
struct AppConfig {
    // 应用标识
    std::string applicationId;
    std::string applicationVersion;

    // 渲染配置
    struct RenderConfig {
        bool dpiAwareViewports = true;
        int antialiasSamples = 4;
        bool debugShaders = false;
        bool logarithmicDepthBuffer = false;
        bool useWebGL2 = true;  // DanQing: useOpenGL33
    } render;

    // 瓦片配置
    struct TileConfig {
        int retryInterval = 50;
        bool enableInstancing = true;
        bool enableIndexedEdges = true;
        bool enableExternalTextures = true;
        int gpuMemoryLimitMB = 512;
    } tile;

    // iModel 路径
    std::string iModelPath;
};
```

**优先级**: P1（影响配置灵活性）

---

#### M2: SnapshotConnection — 真实 iModel 文件打开

**itwinjs-core 参考**: `BriefcaseConnection.openFile()` / `SnapshotConnection.openFile()`

**当前状态**: 只有 `BlankConnection`，无法打开 `.bim` 文件。

**需要实现**:
```cpp
// dqApp/PublicAPI/dqApp/SnapshotConnection.h
class SnapshotConnection : public IModelConnection {
public:
    static dqBase::RefPtr<SnapshotConnection> OpenFile(const std::string& path);

    // 数据访问
    DgnDb& GetDgnDb();
    ElementStatePtr GetElement(dqBase::DqId const& id);
    std::vector<ModelStatePtr> GetModels();
    std::vector<ViewStatePtr> GetViews();
};
```

**优先级**: P0（无法打开真实模型）

---

#### M3: ViewDefinition 查询

**itwinjs-core 参考**: `ViewList.create()` + `views.getDefaultView()`

**当前状态**: 视图通过 `SpatialViewState::CreateBlank()` 硬编码创建，无法从 iModel 查询。

**需要实现**:
```cpp
// dqApp/PublicAPI/dqApp/ViewDefinition.h
class ViewDefinition {
public:
    struct ViewInfo {
        dqBase::DqId id;
        std::string name;
        std::string className;  // "BisCore:SpatialViewDefinition"
    };

    static std::vector<ViewInfo> QueryAll(DgnDb& db);
    static dqBase::RefPtr<ViewState> LoadDefault(DgnDb& db);
    static dqBase::RefPtr<ViewState> Load(DgnDb& db, dqBase::DqId const& viewId);
};
```

**优先级**: P0（无法加载真实视图）

---

#### M4: RenderTarget 激活

**itwinjs-core 参考**: `TargetImpl` + `SceneCompositor`

**当前状态**: `Viewport.cpp:148-151` 明确注释 RenderTarget 延迟创建，使用旧路径。

**需要实现**:
1. 稳定 SceneCompositor 依赖（UBO, DescriptorSet）
2. 在 `SetupDriver()` 中创建 RenderTarget
3. 切换到 `RenderTarget::DrawFrame()` 路径

**优先级**: P1（影响渲染质量）

---

#### M5: 瓦片系统集成

**itwinjs-core 参考**: `TileAdmin` + `TileTree` + `TileRequest`

**当前状态**: `TileAdmin` 单例存在，`TileTree` 抽象存在，但无实际瓦片加载。

**需要实现**:
1. iModel 瓦片树创建（从 DgnDb 读取）
2. 瓦片请求通道（本地文件读取）
3. 瓦片 LOD 选择
4. 瓦片 GPU 上传

**优先级**: P1（影响大模型渲染）

---

### 🟡 二级缺失（功能不完整但可启动）

#### M6: 工具栏系统

**itwinjs-core 参考**: `Surface.createToolBar()` + `Viewer` ToolBar

**当前状态**: 只有菜单栏，无工具栏。

**需要实现**:
1. 主工具栏（Open File, Open Blank Connection, etc.）
2. 视口专属工具栏（View Picker, Models, Categories, etc.）
3. 工具栏切换（视口焦点变化时）

**优先级**: P2

---

#### M7: 捕捉系统

**itwinjs-core 参考**: `AccuSnap` + `SnapMode`

**当前状态**: 无捕捉系统。

**需要实现**:
1. `AccuSnap` 类
2. `SnapMode` 枚举（NearestKeypoint, Center, MidPoint, etc.）
3. 捕捉 UI（组合框）

**优先级**: P2

---

#### M8: 命令输入系统

**itwinjs-core 参考**: `KeyinField` + `IModelApp.tools.parseAndRun()`

**当前状态**: 无命令输入。

**需要实现**:
1. `KeyinField` widget
2. 命令解析器
3. 命令历史

**优先级**: P2

---

#### M9: 瓦片加载指示器

**itwinjs-core 参考**: `TileLoadIndicator`

**当前状态**: 无瓦片加载进度显示。

**需要实现**:
1. `TileLoadIndicator` widget
2. 连接 `ViewManager::OnFinishRender` 事件

**优先级**: P2

---

### 🟢 三级缺失（可后期实现）

| # | 缺失项 | 优先级 | 说明 |
|---|---|---|---|
| M10 | AccuDraw | P3 | BIM 核心交互，后期实现 |
| M11 | 国际化 | P3 | 多语言支持 |
| M12 | 地图图层 | P3 | MapBox/Bing/Google |
| M13 | 现实数据 | P3 | 点云、实景模型 |
| M14 | 扩展系统 | P3 | 插件架构 |
| M15 | 命令行宏 | P3 | 自动化脚本 |
| M16 | Animation | P3 | 时间线动画 |
| M17 | Flash | P3 | 选中闪烁 |
| M18 | ScreenSpaceEffects | P3 | SSAO, EDL, Blur |

---

## 6. 实现优先级矩阵

### P0: 必须立即实现（阻塞基本功能）

```
M1  统一配置系统          → AppConfig 结构 + 环境变量/文件读取
M2  SnapshotConnection    → DgnDb 打开 + IModelConnection 创建
M3  ViewDefinition 查询   → 视图列表 + 默认视图加载
```

### P1: 尽快实现（影响渲染质量）

```
M4  RenderTarget 激活     → SceneCompositor 稳定化
M5  瓦片系统集成          → TileTree + TileRequest + LOD
```

### P2: 近期实现（改善用户体验）

```
M6  工具栏系统            → 主工具栏 + 视口工具栏
M7  捕捉系统              → AccuSnap + SnapMode
M8  命令输入系统          → KeyinField
M9  瓦片加载指示器        → TileLoadIndicator
```

### P3: 后期实现（高级功能）

```
M10 AccuDraw
M11 国际化
M12 地图图层
M13 现实数据
M14 扩展系统
M15 命令行宏
M16 Animation
M17 Flash
M18 ScreenSpaceEffects
```

---

## 附录: 文件对照表

| itwinjs-core 文件 | DanQing 对应文件 | 状态 | 说明 |
|---|---|---|---|
| `DisplayTestApp.ts` (入口) | `samples/DisplayTestApp/main.cpp` | ✅ | 启动链完整 |
| `App.ts` (DisplayTestApp.startup) | `dqApp/src/Application.cpp` | ✅ | singleton + Startup/Shutdown |
| `Surface.ts` | `samples/DisplayTestApp/DisplayTestApp.cpp` | ⚠️ 部分 | 缺少工具栏按钮、快捷键 |
| `Viewer.ts` | `dqApp/src/Viewport.cpp` | ✅ | 17 步管线完整 |
| `Window.ts` | `dqApp/src/MDIView.cpp` | ✅ | QMdiSubWindow |
| `IModelApp.ts` | `dqApp/src/Application.cpp` | ✅ | 核心初始化 |
| `System.ts` (WebGL) | `dqRender/src/render/RenderPipeline.cpp` | ✅ | RHI Driver + Techniques |
| `TileAdmin.ts` | `dqRender/src/tile/TileAdmin.cpp` | ⚠️ 部分 | 基础 process() 循环 |
| `ViewManager.ts` | `dqApp/src/ViewManager.cpp` | ✅ | 视口管理 + 装饰器 |
| `Viewport.ts` (ScreenViewport) | `dqApp/src/Viewport.cpp` | ✅ | QOpenGLWidget + 坐标变换 |
| `DtaConfiguration.ts` | `dqApp/PublicAPI/dqApp/Application.h` (Options) | ⚠️ 不完整 | 缺少瓦片配置字段 |
| `openIModel.ts` | ❌ 缺失 | 需要实现 | SnapshotConnection |
| `ViewPicker.ts` | ❌ 缺失 | 需要实现 | 视图选择下拉框 |
| `ToolBar.ts` | ❌ 缺失 | 需要实现 | 工具栏系统 |
| `SnapModes.ts` | ❌ 缺失 | 需要实现 | 捕捉模式 UI |
| `TileLoadIndicator.ts` | ❌ 缺失 | 需要实现 | 瓦片加载进度 |
| `KeyinField` (frontend-devtools) | ❌ 缺失 | 需要实现 | 命令输入框 |
| `signIn.ts` | ❌ 不需要 | 无认证 | 桌面程序 |
| `Notifications.ts` | `dqApp/src/ReportView.cpp` | ✅ 等价 | Qt 报告视图 |
| `FpsMonitor.ts` | ❌ 缺失 | 可选 | FPS 监控 |
| `DtaElectronMain.ts` | ❌ 不需要 | 无 Electron | 后端入口 |
| `WebMain.ts` | ❌ 不需要 | 无 Web 服务器 | 后端入口 |
| `Backend.ts` | ❌ 不需要 | 无后端 | IModelHost 初始化 |
