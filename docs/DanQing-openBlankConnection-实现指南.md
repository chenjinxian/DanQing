# DanQing openBlankConnection 实现指南

> **用途**: 基于 itwinjs-core 的 openBlankConnection 完整执行流程，逐项指导 DanQing 的实现工作。
>
> **核心原则**: itwinjs-core 是 RPC 前后端分离架构；DanQing 是单进程桌面程序。所有 RPC 相关步骤不需要实现，所有纯前端逻辑必须实现。

---

## 目录

1. [实现全景](#1-实现全景)
2. [已实现项验证清单](#2-已实现项验证清单)
3. [必须实现项详细规格](#3-必须实现项详细规格)
4. [后期实现项](#4-后期实现项)
5. [不需要实现项](#5-不需要实现项)
6. [实现顺序建议](#6-实现顺序建议)

---

## 1. 实现全景

```
openBlankConnection 执行流程 (共 ~80 步)
  │
  ├── Phase 1: BlankConnection 创建        ✅ 已完成 (5 步实现, 7 步跳过)
  ├── Phase 2: ViewState 加载              ✅ 已完成 (简化路径, 跳过 RPC)
  ├── Phase 3: Viewport 创建               ⚠️ 部分完成 (5 步已验证, 缺 5 步)
  ├── Phase 4: changeView 变换链           ⚠️ 部分完成 (4 步已验证, 缺 ~8 步)
  ├── Phase 5: addViewport 渲染循环        ✅ 已完成 (3 步已验证)
  └── Phase 6: 首帧渲染                    ⚠️ 部分完成 (10 步已验证, 缺 4 步)
```

---

## 2. 已实现项验证清单

### Phase 1: BlankConnection 创建

#### 已实现步骤

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 1.1 | `Cartographic → EcefLocation` 转换 | `BlankConnection.cpp:20-26` | ✅ |
| 1.2 | `IModelConnection` 属性赋值 | `BlankConnection.cpp:29-35` | ✅ |
| 1.3 | `OnOpen.Raise()` 事件 | `BlankConnection.cpp:38` | ✅ |
| 1.4 | `SelectionSet` 创建 | `IModelConnection.h:94` | ✅ |
| 1.5 | `HiliteSet` 创建 + 同步 | `IModelConnection.h:95` | ✅ |

#### 跳过的步骤（BlankConnection 不需要）

| # | itwinjs-core 步骤 | 跳过原因 |
|---|---|---|
| 1.6 | `new Models(this)` + onOpen/onClose 监听 | BlankConnection 无模型数据 |
| 1.7 | `new Elements(this)` | BlankConnection 无元素数据 |
| 1.8 | `new CodeSpecs(this)` | BlankConnection 无代码规格 |
| 1.9 | `new Views(this)` | BlankConnection 无视图定义 |
| 1.10 | `new Categories(this)` + SubCategoriesCache | BlankConnection 无类别数据 |
| 1.11 | `new Tiles(this)` + ecefLocation/projectExtents 监听 | BlankConnection 无瓦片 |
| 1.12 | `new GeoServices(this)` | BlankConnection 无地理服务 |

**无需修改**。BlankConnection 创建流程完整。跳过的步骤只在 SnapshotConnection 中有意义。

### Phase 2: ViewState 加载

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 2.1 | `SpatialViewState::CreateBlank()` | `DisplayTestApp.cpp:78` | ✅ 等价于 `manufactureSpatialView()` |
| 2.2 | origin/extents 设置 | `DisplayTestApp.cpp:76-77` | ✅ |

**无需修改**。BlankConnection 无需 Phase 2 的 RPC 路径。

### Phase 3: Viewport 创建（基础部分）

#### 已实现步骤

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 3.1 | `Viewport::Create()` 工厂方法 | `Viewport.cpp:80-88` | ✅ |
| 3.2 | QOpenGLWidget 构造 | `Viewport.cpp:42-66` | ✅ |
| 3.3 | QSurfaceFormat 设置 | `Viewport.cpp:49-56` | ✅ |
| 3.4 | CameraController 创建 | `Viewport.cpp:45` | ✅ |
| 3.5 | viewportId 分配 | `Viewport.cpp:47` | ✅ |

#### 缺失步骤

| # | itwinjs-core 步骤 | DanQing 状态 | 说明 |
|---|---|---|---|
| 3.6 | `PerModelCategoryVisibility.createOverrides(this)` | ❌ 缺失 | M3 项 |
| 3.7 | `IModelApp.tileAdmin.registerUser(this)` | ❌ 缺失 | M4 项 |
| 3.8 | `RenderSystem.createTarget(canvas)` → RenderTarget | ⚠️ 延迟 | Phase 2 依赖 |
| 3.9 | `EventController` 创建 (鼠标/键盘/触摸) | ⚠️ 差异 | DanQing 用 Qt 事件直接处理 |
| 3.10 | `ResizeObserver` 创建 | ✅ Qt 等价 | `resizeGL()` |

### Phase 4: changeView（基础部分）

#### 已实现步骤

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 4.1 | `SetupFromView()` | `Viewport.cpp:517-535` | ✅ |
| 4.2 | `ViewingSpace::Update()` | `Viewport.cpp:533` | ✅ |
| 4.3 | 无效化级联 (Controller→RenderPlan→Scene→Decorations) | `Viewport.cpp:255-278` | ✅ |
| 4.4 | `RequestRedraw()` | `Viewport.cpp:280-284` | ✅ |

#### 当前 ChangeView() 实现（简化版）

```cpp
// Viewport.cpp:96-105 — 当前实现
void Viewport::ChangeView(dqBase::RefPtr<ViewState> view)
{
    if (!view) return;
    mView = std::move(view);
    mCameraController->SetView(mView.Get());
    emit ViewChanged();
    update();  // trigger repaint
}
```

#### 缺失步骤（与 itwinjs-core 对比）

| # | itwinjs-core 步骤 | DanQing 状态 | 说明 |
|---|---|---|---|
| 4.5 | `detachFromView()` — 注销旧事件监听 | ❌ 缺失 | M1 项 |
| 4.6 | `attachToView()` — 注册新事件监听 | ❌ 缺失 | M1 项 |
| 4.7 | `view.fixAspectRatio(viewRect.aspect)` | ❌ 缺失 | M5 项 |
| 4.8 | `ViewingSpace.createFromViewport()` | ✅ 已实现 | `SetupFromView()` 内部 |
| 4.9 | `invalidateRenderPlan()` | ✅ 已实现 | 无效化级联 |
| 4.10 | `onViewChanged.raiseEvent()` | ❌ 缺失 | 无事件系统 |
| 4.11 | `target.reset()` | ⚠️ 部分 | 无 RenderTarget |
| 4.12 | `onChangeView.raiseEvent()` | ❌ 缺失 | 无事件系统 |
| 4.13 | `view.attachToViewport(this)` — 双向绑定 | ❌ 缺失 | M9 项 |

### Phase 5: addViewport

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 5.1 | `ViewManager::AddViewport()` | `ViewManager.cpp:16-38` | ✅ |
| 5.2 | 自动启动事件循环 | `ViewManager.cpp:33-35` | ✅ |
| 5.3 | `OnViewOpen.Raise()` | `ViewManager.cpp:37` | ✅ |

### Phase 6: 首帧渲染（基础部分）

#### 已实现步骤

| # | 步骤 | DanQing 文件:行 | 验证 |
|---|---|---|---|
| 6.1 | `Application::EventLoop()` | `Application.cpp:125-140` | ✅ |
| 6.2 | `TileAdmin::process()` | `Application.cpp:136` | ✅ |
| 6.3 | `ViewManager::RenderLoop()` | `ViewManager.cpp:56-69` | ✅ |
| 6.4 | `Viewport::RenderFrame()` 17 步框架 | `Viewport.cpp:385-511` | ✅ |
| 6.5 | Step 1: changeFlags 捕获 | `Viewport.cpp:390-392` | ✅ |
| 6.6 | Step 5: 控制器同步 | `Viewport.cpp:405-408` | ✅ |
| 6.7 | Step 6: 选择集更新 | `Viewport.cpp:411-423` | ✅ |
| 6.8 | Step 10: 场景创建 | `Viewport.cpp:430-437` | ✅ |
| 6.9 | Step 12: 装饰收集 | `Viewport.cpp:445-452` | ✅ |
| 6.10 | Step 15: 绘制 (双路径) | `Viewport.cpp:458-487` | ✅ |
| 6.11 | Step 16: 事件派发 | `Viewport.cpp:490-505` | ✅ |
| 6.12 | Step 17: 请求下一帧 | `Viewport.cpp:508-510` | ✅ |

#### 缺失步骤

| # | 步骤 | DanQing 状态 | 说明 |
|---|---|---|---|
| 6.13 | Step 2-3: Animation (`animate()`) | ⚠️ Phase 2 | 注释掉 `// animate();` |
| 6.14 | Step 7-9: Analysis/TimePoint/Overrides | ⚠️ Phase 2 | 注释掉 |
| 6.15 | Step 11: RenderPlan 验证 | ⚠️ 存根 | M7 项 |
| 6.16 | Step 13-14: Flash/PreRender | ⚠️ Phase 2 | 注释掉 |

---

## 3. 必须实现项详细规格

### M1: 事件监听系统 — `attachToView()` / `detachFromView()`

**itwinjs-core 参考**: `Viewport.ts:1198+` 的 `attachToView()` 和 `detachFromView()`

**当前状态**: DanQing 的 `Viewport` 没有事件监听注册机制。视图/样式变更不会触发重绘。

**需要实现**:

```cpp
// dqApp/src/Viewport.cpp

void Viewport::AttachToView()
{
    if (!mView) return;

    // DisplayStyle 事件监听 (约 20 个)
    // ← itwinjs-core: registerDisplayStyleListeners()
    auto& style = mView->GetDisplayStyle();

    style.OnViewFlagsChanged.AddListener([this]() {
        InvalidateRenderPlan();
        RequestRedraw();
    });

    style.OnBackgroundColorChanged.AddListener([this]() {
        RequestRedraw();
    });

    style.OnSubCategoryOverridesChanged.AddListener([this]() {
        InvalidateScene();
    });

    style.OnLightSettingsChanged.AddListener([this]() {
        RequestRedraw();
    });

    // ... 更多 DisplayStyle 事件

    // View 事件监听 (约 10 个)
    // ← itwinjs-core: registerViewListeners()
    mView->OnViewedCategoriesChanged.AddListener([this]() {
        InvalidateScene();
    });

    mView->OnViewedModelsChanged.AddListener([this]() {
        InvalidateScene();
    });

    mView->OnDisplayStyleChanged.AddListener([this]() {
        InvalidateRenderPlan();
    });

    mView->OnClipVectorChanged.AddListener([this]() {
        InvalidateScene();
    });

    // ... 更多 View 事件

    // 双向绑定
    // ← itwinjs-core: view.attachToViewport(this)
    mView->AttachToViewport(this);

    // 子类别缓存更新
    // ← itwinjs-core: updateSubCategories()
    // UpdateSubCategories();  // 后期实现
}

void Viewport::DetachFromView()
{
    if (!mView) return;

    // 注销所有事件监听
    // ← itwinjs-core: detachFromView() 中的 removeAllListeners()
    mView->GetDisplayStyle().RemoveAllListeners(this);
    mView->RemoveAllListeners(this);
    mView->DetachFromViewport(this);
}
```

**依赖项**:
- `DisplayStyle` 需要实现事件系统 (`OnViewFlagsChanged`, `OnBackgroundColorChanged`, 等)
- `ViewState` 需要实现事件系统 (`OnViewedCategoriesChanged`, `OnViewedModelsChanged`, 等)
- `ViewState` 需要实现 `AttachToViewport()` / `DetachFromViewport()`

**优先级**: P0 — 这是最大的缺口，影响所有后续功能

---

### M2: `ChangeView()` 完整实现

**itwinjs-core 参考**: `Viewport.ts:1834` + `Viewport.ts:3564`

**当前状态**: DanQing 有 `ChangeView()` 方法但严重简化，缺少事件注销/注册、宽高比修正、变换链重建。

**当前实现** (`Viewport.cpp:96-105`):
```cpp
void Viewport::ChangeView(dqBase::RefPtr<ViewState> view)
{
    if (!view) return;
    mView = std::move(view);
    mCameraController->SetView(mView.Get());
    emit ViewChanged();
    update();  // trigger repaint
}
```

**当前实现缺失的步骤**:
- ❌ `DetachFromView()` — 注销旧视图事件监听
- ❌ `AttachToView()` — 注册新视图事件监听
- ❌ `view.fixAspectRatio()` — 宽高比修正
- ❌ `SetupFromView()` — 变换链重建
- ❌ `InvalidateController()` — 渲染状态无效化
- ❌ `target.reset()` — 渲染目标重置
- ❌ `onChangeView.raiseEvent()` — 视图变更事件

**需要实现**:

```cpp
// dqApp/src/Viewport.cpp

void Viewport::ChangeView(dqBase::RefPtr<ViewState> view)
{
    if (!view) return;

    // 1. 保存旧视图引用
    auto prevView = std::move(mView);

    // 2. 注销旧视图事件监听
    // ← itwinjs-core: detachFromView()
    DetachFromView();

    // 3. 分配新视图
    mView = std::move(view);

    // 4. 注册新视图事件监听
    // ← itwinjs-core: attachToView()
    AttachToView();

    // 5. 更新相机控制器
    mCameraController->SetView(mView.Get());

    // 6. 修正宽高比
    // ← itwinjs-core: view.fixAspectRatio(viewRect.aspect)
    if (width() > 0 && height() > 0) {
        float aspect = static_cast<float>(width()) / static_cast<float>(height());
        mView->FixAspectRatio(aspect);
    }

    // 7. 重建变换链
    // ← itwinjs-core: doSetupFromView()
    SetupFromView();

    // 8. 使所有渲染状态无效
    // ← itwinjs-core: invalidateController()
    InvalidateController();

    // 9. 重置渲染目标
    if (mRenderTarget) {
        mRenderTarget->Reset();
    }

    // 10. 触发事件
    // ← itwinjs-core: onChangeView.raiseEvent(this, prevView)
    emit ViewChanged();

    // 11. 请求重绘
    RequestRedraw();
}
```

**依赖项**:
- M1 (事件监听系统)
- `ViewState::FixAspectRatio()` 方法
- `RenderTarget::Reset()` 方法

**优先级**: P0

---

### M3: `PerModelCategoryVisibility`

**itwinjs-core 参考**: `PerModelCategoryVisibility.ts`

**当前状态**: 完全缺失。

**需要实现**:

```cpp
// dqCommon/PublicAPI/dqCommon/PerModelCategoryVisibility.h

namespace dqCommon {

enum class PerModelCategoryOverride : uint8_t {
    None = 0,
    Show = 1,
    Hide = 2,
};

struct PerModelCategoryOverrideEntry {
    std::string modelId;
    std::string categoryId;
    bool visible;
};

class PerModelCategoryVisibilityOverrides {
public:
    PerModelCategoryOverride GetOverride(std::string const& modelId,
                                         std::string const& categoryId) const;
    void SetOverride(std::string const& modelId,
                     std::string const& categoryId,
                     PerModelCategoryOverride override);
    void ClearOverrides();
    void AddOverrides(/* FeatureSymbology.Overrides& overrides */);

private:
    std::vector<PerModelCategoryOverrideEntry> mOverrides;
};

} // namespace dqCommon
```

**Viewport 集成**:

```cpp
// dqApp/src/Viewport.cpp — 在 RenderFrame() Step 9 中使用
// ← itwinjs-core: PerModelCategoryVisibility.addOverrides()
if (mPerModelCategoryVisibility) {
    mPerModelCategoryVisibility->AddOverrides(featureOverrides);
}
```

**优先级**: P1

---

### M4: `TileAdmin::registerUser()` / `forgetUser()`

**itwinjs-core 参考**: `TileAdmin.ts` 的 `registerUser()`

**当前状态**: `TileAdmin` 存在但无用户注册机制。

**需要实现**:

```cpp
// dqRender/PublicAPI/dqRender/tile/TileAdmin.h — 补充方法
void registerUser(dqApp::Viewport* user);
void forgetUser(dqApp::Viewport* user);
```

```cpp
// dqApp/src/Viewport.cpp — 在构造函数和析构函数中
Viewport::Viewport(QWidget* parent, dqBase::RefPtr<ViewState> view)
    : QOpenGLWidget(parent), mView(std::move(view)), ...
{
    // ← itwinjs-core: IModelApp.tileAdmin.registerUser(this)
    dqRender::TileAdmin::instance().registerUser(this);
}

Viewport::~Viewport()
{
    // ← itwinjs-core: IModelApp.tileAdmin.forgetUser(this)
    dqRender::TileAdmin::instance().forgetUser(this);
    // ...
}
```

**优先级**: P1

---

### M5: `ViewState::FixAspectRatio()`

**itwinjs-core 参考**: `ViewState.fixAspectRatio()` (ViewState.ts)

**当前状态**: 缺失。

**需要实现**:

```cpp
// dqApp/PublicAPI/dqApp/ViewState.h
void FixAspectRatio(float targetAspect);

// dqApp/src/ViewState.cpp
void ViewState::FixAspectRatio(float targetAspect)
{
    // ← itwinjs-core: ViewState.fixAspectRatio()
    // 调整 extents 使宽高比匹配目标
    auto extents = GetExtents();
    float currentAspect = static_cast<float>(extents.x) / static_cast<float>(extents.y);
    if (std::abs(currentAspect - targetAspect) > 0.001f) {
        // 调整 extents.x 以匹配宽高比
        extents.x = extents.y * targetAspect;
        SetExtents(extents);
    }
}
```

**优先级**: P1

---

### M6: `FeatureSymbology.Overrides` 计算

**itwinjs-core 参考**: `FeatureSymbology.ts` + `Viewport.ts` Step 9

**当前状态**: RenderFrame Step 9 是空操作。

**需要实现**:

```cpp
// dqRender/PublicAPI/dqRender/FeatureSymbology.h
namespace dqRender {

class FeatureSymbologyOverrides {
public:
    struct Override {
        bool visible = true;
        uint32_t color = 0;  // 0 = no override
        uint32_t weight = 0;
        // ...
    };

    void SetOverrides(uint32_t featureId, Override const& override);
    Override const* GetOverride(uint32_t featureId) const;
    void Clear();

private:
    std::unordered_map<uint32_t, Override> mOverrides;
};

} // namespace dqRender
```

```cpp
// dqApp/src/Viewport.cpp — RenderFrame() Step 9
// ← itwinjs-core: recomputeFeatureSymbologyOverrides()
if (mFeatureOverridesDirty) {
    mFeatureOverridesDirty = false;
    mFeatureOverrides = std::make_unique<dqRender::FeatureSymbologyOverrides>();

    // 遍历所有 FeatureOverrideProvider
    // ← itwinjs-core: addFeatureOverrides()
    for (auto* provider : mViewManager.GetFeatureOverrideProviders()) {
        provider->AddFeatureOverrides(*mFeatureOverrides, *this);
    }

    // PerModelCategoryVisibility 覆盖
    // ← itwinjs-core: PerModelCategoryVisibility.addOverrides()
    if (mPerModelCategoryVisibility) {
        mPerModelCategoryVisibility->AddOverrides(*mFeatureOverrides);
    }

    isRedrawNeeded = true;
}
```

**依赖项**: M3 (PerModelCategoryVisibility)

**优先级**: P1

---

### M7: `RenderPlan` 创建与验证

**itwinjs-core 参考**: `createRenderPlanFromViewport()` (Viewport.ts)

**当前状态**: `ValidateRenderPlan()` 是存根。

**需要实现**:

```cpp
// dqApp/src/Viewport.cpp
void Viewport::ValidateRenderPlan()
{
    // ← itwinjs-core: createRenderPlanFromViewport(this)
    if (!mRenderTarget) {
        mRenderPlanValid = true;
        return;
    }

    // 构建渲染计划
    RenderPlan plan;
    plan.viewFlags = mView->GetDisplayStyle().GetViewFlags();
    plan.backgroundColor = mView->GetDisplayStyle().GetBackgroundColor();
    plan.lights = mView->GetDisplayStyle().GetLightSettings();
    // ... 更多属性

    // 传递给渲染目标
    // ← itwinjs-core: target.changeRenderPlan(plan)
    mRenderTarget->ChangeRenderPlan(plan);

    mRenderPlanValid = true;
}
```

**依赖项**: `RenderPlan` 结构体定义，`RenderTarget::ChangeRenderPlan()` 方法

**优先级**: P1

---

### M8: `DisplayStyle` 事件系统

**itwinjs-core 参考**: `DisplayStyleSettings.ts` 的事件

**当前状态**: `DisplayStyle` 无事件触发机制。

**需要实现**:

```cpp
// dqApp/PublicAPI/dqApp/DisplayStyle.h — 补充事件
class DisplayStyle {
public:
    // 现有属性...

    // 事件 (← itwinjs-core DisplayStyleSettings events)
    dqBase::DqEvent<> OnViewFlagsChanged;
    dqBase::DqEvent<> OnBackgroundColorChanged;
    dqBase::DqEvent<> OnSubCategoryOverridesChanged;
    dqBase::DqEvent<> OnLightSettingsChanged;
    dqBase::DqEvent<> OnModelAppearanceOverrideChanged;
    dqBase::DqEvent<> OnBackgroundMapChanged;

    // 属性 setter 中触发事件
    void SetViewFlags(ViewFlags const& flags) {
        mViewFlags = flags;
        OnViewFlagsChanged.Raise();  // ← 触发事件
    }

    void SetBackgroundColor(uint32_t color) {
        mBackgroundColor = color;
        OnBackgroundColorChanged.Raise();  // ← 触发事件
    }

    // ... 更多属性 setter
};
```

**优先级**: P0 — M1 的前置依赖

---

### M9: `ViewState` 事件系统

**itwinjs-core 参考**: `ViewState.ts` 的事件

**当前状态**: `ViewState` 无事件触发机制，无 `AttachToViewport()` 双向绑定。

**`AttachToViewport()` 的作用**: 在 itwinjs-core 中，ViewState 维护一个关联的 Viewport 列表。当 ViewState 属性变更时（如 SetDisplayStyle、SetExtents），它会通知所有关联的 Viewport 触发重绘。这是双向绑定的关键——Viewport 通过 `attachToView()` 注册监听，ViewState 通过 `attachToViewport()` 记录关联。

**需要实现**:

```cpp
// dqApp/PublicAPI/dqApp/ViewState.h — 补充事件
class ViewState {
public:
    // 现有属性...

    // 事件 (← itwinjs-core ViewState events)
    dqBase::DqEvent<> OnViewedCategoriesChanged;
    dqBase::DqEvent<> OnViewedModelsChanged;
    dqBase::DqEvent<> OnDisplayStyleChanged;
    dqBase::DqEvent<> OnClipVectorChanged;

    // Viewport 绑定
    void AttachToViewport(Viewport* vp);
    void DetachFromViewport(Viewport* vp);

    // 属性 setter 中触发事件
    void SetDisplayStyle(dqBase::RefPtr<DisplayStyle> style) {
        mDisplayStyle = std::move(style);
        OnDisplayStyleChanged.Raise();  // ← 触发事件
    }

    // ... 更多属性 setter

private:
    std::vector<Viewport*> mAttachedViewports;
};
```

**优先级**: P0 — M1 的前置依赖

---

## 4. 后期实现项

### L1: `MapTiledGraphicsProvider`

**itwinjs-core 参考**: `MapTiledGraphicsProvider.ts`

**说明**: 背景地图瓦片（Bing/MapBox/Google）。BlankConnection 默认启用背景地图。

**优先级**: P2

---

### L2: `updateSubCategories()`

**itwinjs-core 参考**: `Viewport.ts` 的 `updateSubCategories()`

**说明**: 更新子类别缓存。BlankConnection 无类别数据，暂时不需要。

**优先级**: P2

---

### L3: RenderFrame Step 7-8 (Analysis/TimePoint)

**itwinjs-core 参考**: `Viewport.ts` Step 7-8

**说明**: 分析分数和时间点/调度脚本。Phase 2 功能。

**优先级**: P3

---

### L4: RenderFrame Step 13-14 (Flash/PreRender)

**itwinjs-core 参考**: `Viewport.ts` Step 13-14

**说明**: Flash 效果和预渲染钩子。Phase 2 功能。

**优先级**: P3

---

### L5: RenderFrame Step 2-3 (Animation)

**itwinjs-core 参考**: `Viewport.ts` Step 2-3 (`animate()`)

**说明**: 动画处理。Phase 2 功能。当前代码中有 `// animate();` 注释。

**优先级**: P3

---

### L6: `ScreenSpaceEffects`

**itwinjs-core 参考**: `System.ts` `onInitialized()` 中的 `ScreenSpaceEffects`

**说明**: 屏幕后处理效果（SSAO, EDL, Blur）。在 itwinjs-core 的 `System.onInitialized()` 中创建。

**优先级**: P3

---

### L7: `GeoServices`

**itwinjs-core 参考**: `GeoServices.ts`

**说明**: 地理坐标服务。BlankConnection 有 EcefLocation 但无地理查询需求。

**优先级**: P3

---

## 5. 不需要实现项

| # | 步骤 | 原因 |
|---|---|---|
| N1 | `ViewList.create()` + `getViewList()` | BlankConnection 会失败并回退到 `manufactureSpatialView()` |
| N2 | `iModel.views.load()` | BlankConnection 无后端，会失败 |
| N3 | `iModel.views.queryDefaultViewId()` | BlankConnection 无后端 |
| N4 | ECSQL 视图查询 | BlankConnection 无数据库 |
| N5 | `Models` / `Elements` / `CodeSpecs` / `Views` / `Categories` 子对象 | BlankConnection 无数据，这些只在 SnapshotConnection 中有意义 |
| N6 | `GeoServices` (RPC 回调) | BlankConnection 无地理查询 | L7 项 |
| N7 | `addLogo()` | 桌面程序不需要 iTwin.js logo |

---

## 6. 实现顺序建议

### 阶段 1: 事件系统基础 (M8, M9)

```
M8: DisplayStyle 事件系统
  ├── OnViewFlagsChanged
  ├── OnBackgroundColorChanged
  ├── OnSubCategoryOverridesChanged
  └── OnLightSettingsChanged

M9: ViewState 事件系统
  ├── OnViewedCategoriesChanged
  ├── OnViewedModelsChanged
  ├── OnDisplayStyleChanged
  ├── OnClipVectorChanged
  ├── AttachToViewport()
  └── DetachFromViewport()
```

### 阶段 2: Viewport 事件绑定 (M1, M2, M5)

```
M1: attachToView() / detachFromView()
  ├── 注册 ~20 个 DisplayStyle 事件监听
  ├── 注册 ~10 个 View 事件监听
  └── 双向绑定 (AttachToViewport)

M2: ChangeView() 完整实现
  ├── DetachFromView()
  ├── AttachToView()
  ├── FixAspectRatio()
  └── InvalidateController()

M5: ViewState::FixAspectRatio()
```

### 阶段 3: 渲染状态 (M3, M4, M6, M7)

```
M3: PerModelCategoryVisibility
M4: TileAdmin::registerUser() / forgetUser()
M6: FeatureSymbology.Overrides 计算
M7: RenderPlan 创建与验证
```

### 阶段 4: 渲染路径稳定化

```
SceneCompositor 激活
  ├── UBO 依赖解决
  ├── DescriptorSet 依赖解决
  └── RenderTarget::DrawFrame() 路径启用
```

---

## 附录: 完整事件监听清单

### DisplayStyle 事件 (约 20 个)

| # | 事件 | 触发的无效化 | itwinjs-core 参考 |
|---|---|---|---|
| E1 | `OnViewFlagsChanged` | InvalidateRenderPlan | `DisplayStyleSettings.ts` |
| E2 | `OnBackgroundColorChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E3 | `OnSubCategoryOverridesChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E4 | `OnLightSettingsChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E5 | `OnModelAppearanceOverrideChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E6 | `OnBackgroundMapChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E7 | `OnMonochromeColorChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E8 | `OnClipStyleChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E9 | `OnPlanProjectionChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E10 | `OnThematicChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E11 | `OnHiddenLineChanged` | RequestRedraw | `DisplayStyle3dSettings.ts` |
| E12 | `OnAmbientOcclusionChanged` | RequestRedraw | `DisplayStyle3dSettings.ts` |
| E13 | `OnEnvironmentChanged` | RequestRedraw | `DisplayStyle3dSettings.ts` |
| E14 | `OnSolarShadowsChanged` | RequestRedraw | `DisplayStyle3dSettings.ts` |
| E15 | `OnContourChanged` | RequestRedraw | `DisplayStyle3dSettings.ts` |
| E16 | `OnWhiteOnWhiteReversalChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E17 | `OnRenderTimelineChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E18 | `OnScheduleScriptChanged` | InvalidateScene | `DisplayStyleSettings.ts` |
| E19 | `OnAnalysisStyleChanged` | RequestRedraw | `DisplayStyleSettings.ts` |
| E20 | `OnExcludedElementsChanged` | InvalidateScene | `DisplayStyleSettings.ts` |

### View 事件 (约 10 个)

| # | 事件 | 触发的无效化 | itwinjs-core 参考 |
|---|---|---|---|
| E21 | `OnViewedCategoriesChanged` | InvalidateScene | `ViewState.ts` |
| E22 | `OnViewedModelsChanged` | InvalidateScene | `ViewState.ts` |
| E23 | `OnDisplayStyleChanged` | InvalidateRenderPlan | `ViewState.ts` |
| E24 | `OnClipVectorChanged` | InvalidateScene | `ViewState.ts` |
| E25 | `OnModelSelectorChanged` | InvalidateScene | `ViewState.ts` |
| E26 | `OnCategorySelectorChanged` | InvalidateScene | `ViewState.ts` |
| E27 | `OnRenderModeChanged` | RequestRedraw | `ViewState.ts` |
| E28 | `OnDetailsChanged` | RequestRedraw | `ViewState.ts` |
| E29 | `OnSectionDrawingInfoChanged` | InvalidateScene | `ViewState.ts` |
| E30 | `OnGridOrientationChanged` | RequestRedraw | `ViewState3d.ts` |
