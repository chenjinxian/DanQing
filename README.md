# 丹青（DanQing）

A **pure client-side graphics engine** — itwinjs-core's large-scale digital-twin
rendering capability combined with Filament's high-quality real-time rendering
and cross-platform reach. Target users: graphics/CAD application developers.

DanQing is a **reference implementation**: all code, naming, and tests are
strictly aligned with the reference projects — no self-invented algorithms,
interfaces, data structures, or constants.

[English](#english) | [中文](#中文)

---

## English

### Modules

| Module | Namespace | Role |
|--------|-----------|------|
| **dqBase** | `dqBase` | Shared foundation (smart pointers, Result, containers, events). Zero domain knowledge, zero Qt |
| **dqGeom** | `dqGeom` | Pure math geometry (curves/surfaces/meshes/topology/clipping/BSpline). Zero solid-kernel dependency |
| **dqCommon** | `dqCommon` | Shared types (ColorDef, ViewFlags, Frustum, FeatureTable, GeometryStream), mirroring @itwin/core-common |
| **dqRender** | `dqRender` | Rendering engine (RHI / Tile system / multi-pass / shaders), mirroring itwinjs core/frontend + filament |
| **dqApp** | `dqApp` | Render host layer (Application / ViewManager / Viewport 24-step pipeline / ToolAdmin; Qt only as host-window bridge) |

`samples/DisplayTestApp` is a pixel-level regression test host (not part of the SDK).

### Reference projects (read-only, checked out outside this repo)

| Project | Role |
|---------|------|
| **itwinjs-core** | Primary rendering-stack reference: render pipeline, frontend API, Tile system, glTF |
| **filament** | RHI driver, OpenGL backend, authoritative reference for the Windows WGL platform layer |
| **imodel-native** | dqGeom geometry reference |
| **FreeCAD** | Test-host (DisplayTestApp) Gui command framework only |

### Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- Development is always **Debug**
- Qt path: `third_party/qt/windows/6.11.1/msvc2022_64` (used only by dqApp and
  the test host; the engine SDK modules' public API is Qt-free).
  Bootstrap script: `scripts/bootstrap_third_party.*`
- GoogleTest: `third_party/googletest`

### Documentation

- **CLAUDE.md** — the single authority for development rules (reference fidelity,
  naming alignment, test provenance, architecture constraints)
- `docs/DanQing-图形引擎架构设计.md` — current architecture description
- `docs/DanQing-C++代码规范.md` — C++ format rules (subordinate to CLAUDE.md)
- `docs/itwinjs-core-渲染系统执行流程分析.md` — full render-pipeline analysis
- Other docs in `docs/` — alignment analyses and gap matrices for
  DisplayTestApp startup / BlankConnection / glTF / Tile loading

### License

Apache License 2.0 — see [LICENSE](LICENSE). Portions derived from upstream
projects retain their original licenses — see [NOTICE](NOTICE).

---

## 中文

丹青（DanQing）是**纯客户端图形引擎**：itwinjs-core 数字孪生大体量渲染能力 × Filament 高质量实时渲染与全平台能力的共同体。目标用户是图形/CAD 应用开发者。在天工开物平台家族中的分工：渲染层——几何内核归真形（私有仓，自主研发），CAD 应用归鲁班CAD，数据/事务归 imodel-native。

丹青是**参考实现**（reference implementation）：所有代码、命名、测试严格对齐参考项目，不自创算法/接口/数据结构/常量。

### 模块

| 模块 | 命名空间 | 职责 |
|------|---------|------|
| **dqBase** | `dqBase` | 共享基础（智能指针、Result、容器、事件），零领域知识，零 Qt |
| **dqGeom** | `dqGeom` | 纯数学几何（曲线/曲面/网格/拓扑/裁剪/BSpline），零固体内核依赖 |
| **dqCommon** | `dqCommon` | 共享类型（ColorDef、ViewFlags、Frustum、FeatureTable、GeometryStream），对应 @itwin/core-common |
| **dqRender** | `dqRender` | 渲染引擎（RHI/Tile/多 Pass/Shader），对应 itwinjs core/frontend + filament |
| **dqApp** | `dqApp` | 渲染宿主层（Application/ViewManager/Viewport 24 步管线/ToolAdmin；Qt 仅作宿主窗口桥接） |

`samples/DisplayTestApp` 是像素级回归测试宿主（非 SDK）。

### 参考项目（仓外只读检出）

| 参考项目 | 角色 |
|---------|------|
| **itwinjs-core** | 渲染栈主参考：渲染管线、前端 API、Tile 系统、glTF |
| **filament** | RHI 驱动、OpenGL 后端、Windows WGL 平台层权威参考 |
| **imodel-native** | dqGeom 几何参考 |
| **FreeCAD** | 仅测试宿主（DisplayTestApp）Gui 命令框架参考 |

### 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- 开发一律 **Debug**
- Qt 路径：`third_party/qt/windows/6.11.1/msvc2022_64`（仅 dqApp 与测试宿主使用；引擎 SDK 模块公开 API 零 Qt）。初始化脚本：`scripts/bootstrap_third_party.*`
- GoogleTest：`third_party/googletest`

### 文档

- **CLAUDE.md** — 开发规则的唯一权威（参考保真、命名对齐、测试溯源、架构约束）
- `docs/DanQing-图形引擎架构设计.md` — 现行架构描述
- `docs/DanQing-C++代码规范.md` — C++ 格式规范（CLAUDE.md 的从属参考）
- `docs/itwinjs-core-渲染系统执行流程分析.md` — 渲染管线全流程分析
- `docs/` 其余各篇 — DisplayTestApp 启动/BlankConnection/glTF/Tile 加载的对齐分析与差距矩阵

### 许可证

Apache License 2.0 —— 见 [LICENSE](LICENSE)。衍生自上游项目的部分保留其原始许可证 —— 见 [NOTICE](NOTICE)。
