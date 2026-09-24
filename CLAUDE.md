# CLAUDE.md

本文件是 DanQing 项目的开发准则，每次会话自动加载。执行任何代码或文档变更前，先读完本文件。
**规则优先于默认行为。违反规则是错误，不是风格偏好。**

> **规则严重度标签**（每条规则标注）：
> - `[P0 硬约束]` — 不可妥协的架构/契约红线，违反即错误，不得合入主干
> - `[P1 强规则]` — 必须遵守，例外需显式论证，否则评审打回
> - `[P2 指南]` — 推荐做法，非阻塞
>
> （未标 `[P?]` 的章节为描述性/过程性内容，无规则可评级；§14 技术债务表按行标注严重度）
>
> **文档层级**：本文件（CLAUDE.md）是开发规则的**唯一权威**。`docs/DanQing-C++代码规范.md` 为从属格式参考（include 顺序、clang-format、Doxygen 等），不得与本文件矛盾；冲突时以 CLAUDE.md 为准。

---

## 0. 首要指令 (The Prime Directive) `[P0 硬约束]`

DanQing 是参考实现（reference implementation），不是原创设计。所有代码、命名、测试必须严格对齐参考项目（itwinjs-core / imodel-native / filament / FreeCAD）。

**三条铁律：**

1. **来源铁律**：所有【代码与测试】= 参考项目源码的 C++ 改写——**含修 bug：行为修复前先读参考的对应实现，照机制修，不做症状推理补丁**（§11 第 8 条，案例 §12.8、§12.9）。
   - 代码：参考未覆盖的功能标 TODO + 参考来源，不得自创算法/接口/数据结构/常量。
   - 测试：测试场景、边界条件、断言值全部来自参考项目，不得自行构造。优先级 imodel-native C++ 测试 > itwinjs-core TS 测试。仅当两处参考均无对应测试时才可自写，且必须标注 `// Authored: no reference test exists in <project> for <feature>`。
   - `// Ported from:` 行号必须对应**真实读过**的参考实现——用行号装点自创代码是双重违规（§12.8 教训 2）。
   - 参考项目的代码与测试是"规范"，不是"可借鉴的资源"。

2. **命名铁律**：所有标识符（文件/类/枚举/变量/方法/函数）1:1 对齐参考项目（详见 §3）；方法/函数按参考语言原样（TS→camelCase，C++ 参考→PascalCase）。可追溯性由每符号 `// Ported from:` 注释保证。

3. **规范铁律**：严格遵守 C++ 编码规范（§9）。CLAUDE.md 是规则唯一权威，`docs/DanQing-C++代码规范.md` 为从属格式参考，冲突时以 CLAUDE.md 为准。

违反 `[P0]` 是错误，不是风格偏好，不得合入主干。

---

## 1. 项目定义与总目标

丹青（DanQing）是**纯客户端图形引擎**（不是 CAD 应用、不是几何内核）：itwinjs-core 数字孪生大体量渲染能力 × Filament 高质量实时渲染与全平台能力的共同体。目标用户是图形/CAD 应用开发者。在天工开物平台家族中的分工：渲染层——几何内核归真形（私有仓，自主研发），CAD 应用归鲁班CAD，数据/事务归 imodel-native。

5 个已实现模块：

| 模块 | 命名空间 | 状态 | 职责 |
|------|---------|------|------|
| **dqBase** | `dqBase` | ✅ 已实现 | 共享基础（智能指针、Result、容器、事件），零领域知识 |
| **dqGeom** | `dqGeom` | ✅ 已实现 | 纯数学几何（曲线/曲面/网格/拓扑/裁剪/BSpline），零固体内核依赖 |
| **dqCommon** | `dqCommon` | ✅ 已实现 | 共享类型（ColorDef、ViewFlags、Frustum、FeatureTable、GeometryStream、Cartographic），对应 itwinjs-core @itwin/core-common |
| **dqRender** | `dqRender` | ✅ 已实现 | 渲染引擎（RHI/Tile/多Pass/Shader），对应 itwinjs core/frontend + filament |
| **dqApp** | `dqApp` | ✅ 已实现 | 渲染宿主层（Application/ViewManager/Viewport 24 步管线/ToolAdmin；Qt 仅作宿主窗口桥接） |

> **改名完成说明（2026-09-24）**：项目级与代码标识符改名均已落地——`dq*` 命名空间、`DQ_*` 宏、`DANQING_*` 环境变量、`danqing_*` CMake 目标、`Dq*` 基础设施类型前缀。
> 旧定位遗留（CNC 底座 / 文生 3D 平台 / zoBRep·zoData·zoPlatform 规划）已于 2026-09-24 随定位重写废弃，对应文档已删除。

**当前进度基准（2026-09-23）**：Windows 呈现链路已端到端打通并经真实 app 验证——WGL 平台层对齐 filament（swapchain 自持 DC/统一像素格式/表面自愈），resize=表面过期重建交换链，OIT 合成程序生命周期修复；真实 app 配方（空白连接→Grid+ACS→最大化→深度缩放）网格与三轴全程稳定。像素级回归 harness 在树（View3DResizeTest/ZoomBlackBoxSeqTest/ResizePixelConsistencyTest 等）。测试全绿：dqAppTest 353/353，DtaTest 73 通过 + 2 跳过 + 0 失败（TD-11~TD-19 于 2026-09-23 清偿，见 §14）。

架构详情：`docs/DanQing-图形引擎架构设计.md`
C++ 规范：`docs/DanQing-C++代码规范.md`（从属格式参考）

---

## 2. 参考实现定位

DanQing 是参考实现（reference implementation），不是原创设计。所有代码、测试、接口必须严格对齐参考项目，确保行为兼容性。

| 参考项目 | 语言 | 角色与覆盖范围 |
|---------|------|---------|
| **itwinjs-core** | TypeScript | **渲染栈主参考**：渲染管线、前端 API、Tile 系统、glTF |
| **filament** | C++ | RHI 驱动、OpenGL 后端、**平台层（Windows WGL 平台层的权威参考——PlatformWGL.cpp 结构：swapchain 自持 DC、dummy 承载窗口、像素格式匹配）**、高质量实时渲染与全平台后端抽象 |
| **imodel-native** | C++ | dqGeom 几何参考 |
| **FreeCAD** | C++ | 仅测试宿主（samples/DisplayTestApp）Gui 命令框架参考 |

参考项目为仓外检出，只读参考，不是本仓库的一部分。
权重变化只影响未来开发优先级与模块规划；§0 来源铁律、§5 测试优先级（imodel-native > itwinjs-core）等规则原文不变。

---

## 3. 命名对齐规则 `[P0 硬约束]`

### 3.1 总原则

> **所有标识符（文件名、类名、枚举名、枚举值、变量名、方法名、函数名）1:1 对齐参考项目。方法/函数名按参考语言原样保留（TS→camelCase；C++ 参考→PascalCase），不归一。**

可追溯性由每符号 `// Ported from:` 注释保证，不依赖方法名拼写一致。

### 3.2 命名主决策表

| 标识符 | 规则 | TS 参考（itwinjs） | C++ 结果 |
|---|---|---|---|
| 文件名 | 1:1 对齐参考类/文件 | `FrustumUniforms.ts` | `FrustumUniforms.h/.cpp` |
| 类/struct 名 | 1:1（PascalCase） | `BranchUniforms` | `BranchUniforms` |
| 枚举类型名 | 1:1 | `FrustumUniformType` | `FrustumUniformType` |
| 枚举值 | 1:1（PascalCase） | `TwoDee`/`Orthographic` | `TwoDee`/`Orthographic` |
| **方法名** | **1:1（TS→camelCase）** | `changeFrustum` | `changeFrustum` |
| **自由函数名** | **1:1（TS→camelCase）** | `normalizedDifference` | `normalizedDifference` |
| 局部变量 | 1:1（camelCase） | `viewX`/`cameraPosition` | `viewX`/`cameraPosition` |
| 参数 | 1:1（camelCase） | `isViewCoords` | `isViewCoords` |
| 成员变量 | `m_` + 参考基名（camelCase） | `_viewClipEnabled` | `m_viewClipEnabled` |
| 静态成员 | `s_` + camelCase | — | `s_defaultTimeout` |
| 常量 | `k` + PascalCase | — | `kMaxTileDepth` |
| 命名空间 | 模块名 | — | `dqRender` |
| 宏 | UPPER_SNAKE + 模块前缀 | — | `DQ_RENDER_EXPORT` |

> 方法/函数名按参考语言原样：TS 移植→camelCase（`changeFrustum`、`bindProjectionMatrix`），C++ 参考（imodel-native/filament）→PascalCase。两种参考都 1:1，无归一、无碰撞负担。

**如何验证：** clang-tidy 命名检查；code review 比对参考标识符；`// Ported from:` 注释覆盖率。

### 3.3 方法/函数名对齐规则 `[P1 强规则]`

- 方法名/函数名 1:1 按参考语言原样保留，**不归一**：TS 移植→camelCase（`changeFrustum`、`bindProjectionMatrix`、`normalizedDifference`）；C++ 参考（imodel-native/filament）→PascalCase。
- 这与结构性标识符（类/枚举/变量）的 1:1 原则一致；不因 C++ 规范对 TS 方法做 PascalCase 归一（实测会产生大量类型名碰撞 + 跨模块 virtual 不一致，见 §12.6）。
- TS `get xxx` 属性 → C++ 访问器保留参考命名（camelCase）；查找型访问器的动词约定见 `docs/DanQing-C++代码规范.md` §3.4。

### 3.4 C++ 强制偏差表（允许的适配） `[P1 强规则]`

移植时这些偏差是 C++ 必要的，必须按下表映射，不得自创：

| 参考构造 | C++ 适配 |
|---|---|
| TS `_field` 私有 | `m_field`（前缀 `_`→`m_`，基名 1:1） |
| TS GC 引用 / `extends RefCounted` | `RefCounted<T>` CRTP + `RefPtr<T>`（禁 `shared_ptr`） |
| TS `const enum`/union/string-literal | `enum class Name : <type>`（显式底层类型） |
| TS 模块路径 | `dqXxx` 命名空间（单数 PascalCase） |
| TS `T \| undefined` | `std::optional<T>` 或 `RefPtr<T>`(null) |
| TS `throw E` | `Result<T,E>` 返回（核心引擎禁异常） |
| TS `interface`/abstract class | 纯虚 `IXxx`（I 前缀，规范 §3.2） |
| TS `as X` / 类型断言 | `static_cast<X>(...)`（禁 C 风格） |
| TS `class Foo<T>` | `template<typename T> class Foo` |
| TS `get width()` 只读属性 | `Width() const noexcept`（无 Get 前缀） |
| 参考原名与同作用域已有类型/符号冲突 | 保留最小区分后缀（如 `Projection`）+ `// Ported from:` 记录参考原名 | 碰撞例外，如 TS `frustum()`→`frustumProjection()` 避让 `Frustum` 类型 |

### 3.5 禁止 `[P0 硬约束]`

- 自创参考中不存在的名字（重命名 `FrustumUniforms`、加模块前缀 `RenderFrustumUniforms`）。
- 缩写/简写参考标识符（`BindProjMat`）。
- 翻译成中文命名；必须保留参考英文标识符。
- 方法/函数名脱离参考语言自创大小写（TS 移植改成 PascalCase，或 C++ 参考改成 camelCase）。
- 成员变量用 `m`+PascalCase（`mProjection`）或无前缀；必须 `m_`+camelCase。

---

## 4. 代码溯源（Code Provenance） `[P0 硬约束]`

每个源文件必须有明确出处，可追溯到参考项目之一。

- 每个文件顶部标注：
  ```cpp
  // Ported from: <project> <relative-path-to-source-file>
  ```
- 出处必须指向具体源文件，不得使用模糊描述。
- 无出处的代码视为未验证实现，不得合入主干。

**如何验证：** CI 检查每个 `.h`/`.cpp` 顶部含 `// Ported from:` 或 `// Authored:` 行；缺失则构建失败。

---

## 5. 测试保真（Test Fidelity） `[P0 硬约束]`

测试用例是行为兼容性的契约。所有 `TEST()` 必须从参考项目移植，不得臆造。

**规则：**

(a) **来源优先级**：
1. `imodel-native/iModelCore/<模块>/Tests/NonPublished/*.cpp`（C++，直接移植）
2. `itwinjs-core/core/<包>/src/test/*.test.ts`（TS，翻译为 C++/GoogleTest）

(b) **可追溯**：每个 `TEST()` 顶部必须注释出处：
```cpp
// Ported from: <project> <test-file-path>
//              TEST(SuiteName, CaseName)
TEST(MySuite, MyCase) { ... }
```

(c) **不发明场景**：测试场景、边界条件、断言值全部来自参考实现。DanQing 类型与参考不同时，仅调整断言适配类型，场景不变。

(d) **RED-GREEN**：参考测试揭示缺失 API 时，先移植测试（RED），再补实现（GREEN）。

(e) **TS→C++ 映射**：`describe/it` → `TEST(Suite,Case)`；`expect().to.throw(E)` → `EXPECT_THROW(fn,E)`；`@ts-expect-error` 块忽略。

(f) **例外**：仅当 imodel-native 与 itwinjs-core 均无对应测试时方可自写，必须标注 `// Authored: no reference test exists in <project> for <X>`。

(g) **渲染/窗口行为的回归测试授权**：窗口系统/呈现层行为（resize、最大化、swapchain 生命周期）在参考项目（浏览器 WebGL）中不存在对应测试——此类场景允许按 §12.9 的取证结论自写**像素级**回归（readPixels 断言内容存活，不是只断言"不崩"），标注 `// Authored:` 并在注释中记录复现配方与证据链。此类回归还须满足 §11.11 的判据有效性要求：至少一个**位置断言**（内容在哪个象限/哪侧/朝向），需要不对称标记时**新建专用测试资产**、禁止原地突变既有资产。

**如何验证：** CI grep 每个 `TEST(` 上方 5 行内含 `Ported from` 或 `Authored` 注释；CI 统计 `// Authored:` 占比并报警（异常增长→复查是否绕过移植）。

---

## 6. 一致性验证（Conformance Verification） `[P1 强规则]`

每完成一个开发任务，必须对照参考项目进行一致性验证。

| 维度 | 验证内容 |
|------|---------|
| 接口签名 | 方法名、参数类型、返回类型与参考一致 |
| 类型命名 | 遵循 §3 命名对齐规则 |
| 数值精度 | 容差常量、默认值与参考精确值一致 |
| 行为语义 | 算法流程、分支逻辑、错误处理与参考一致 |
| 调用链接线 | 参考侧调用者/数据来源 → DanQing 侧调用者逐条对照（**ported-but-uncalled = 移植未完成**，见 §11.10） |
| 测试覆盖 | 移植的测试全部通过 |

**如何验证：** 每个任务 PR 附五维度一致性对照表（checklist 勾选）；执行时机为每个任务完成后、提交前。

---

## 7. 禁止事项（Prohibited Practices） `[P0 硬约束]`

| 类别 | 禁止内容 |
|------|---------|
| 算法 | 自行设计排序、查找、几何运算等算法 |
| 接口 | 自行定义方法名、参数、返回值 |
| 数据结构 | 自行设计成员变量、内存布局 |
| 常量 | 自行设定容差、默认值、阈值 |
| 测试 | 自行构造测试场景、边界条件、断言值 |
| 架构 | 自行引入设计模式、抽象层次、扩展点 |

若参考项目未覆盖某个功能，标记为待实现（TODO + 参考来源说明），而非自行补全。

**如何验证：** code review 强制核对；clang-tidy 规则覆盖可机械检查项。

---

## 8. 架构约束（硬约束） `[P0 硬约束]`

### 8.1 依赖方向

```
dqApp → { dqRender, dqCommon, dqGeom }
dqRender → { dqGeom, dqCommon } → dqBase
dqCommon → { dqBase, dqGeom }
dqGeom → dqBase
```

- 依赖只许向下，永不反向
- **dqCommon ⊥ dqRender/dqApp**：共享类型不知道上层
- **dqRender → dqGeom** 是唯一允许的引擎间边
- 被禁止的横向依赖或反向依赖是 P0 错误

### 8.2 SDK 独立性

引擎模块（dqBase、dqGeom、dqCommon、dqRender）必须作为独立 SDK 提供给 CAD 应用开发者，**不得依赖 Qt**。

| 模块 | Qt 依赖 | 说明 |
|------|---------|------|
| **dqBase** | **零** | 所有 Qt 类型由 thin wrapper 替代（DqVector/DqString/DqMutex 等） |
| **dqGeom** | **零** | 容器用 std::vector，字符串用 std::string |
| **dqCommon** | **零** | Export.h 用平台原生宏 |
| **dqRender** | 公开 API **零**；src/ 内部允许 | TileRequester 用依赖注入接口，GL context 用原生 API；**窗口系统交互只经原生句柄（void* nativeWindow），窗口生命周期归应用层——WGL/平台层不得链接/调用 Qt（2026-09-14 用户指令：渲染与相机视口操作全走图形 API，不绑死 Qt，未来可换其他图形 API）** |
| **dqApp** | **允许** | 应用层，不是 SDK，可以使用 Qt（Qt ↔ GL 的唯一桥接点是 winId()/WinIdChange → `Swapchain::rebind`） |

dqBase 的 thin wrapper 类型对齐 imodel-native 的 bvector/Utf8String 模式：
- `DqVector<T>` = `std::vector<T>`（DqTypes.h）
- `DqString` = `std::string`（DqTypes.h）
- `DqMutex` = `std::mutex`（DqSync.h）
- `DqGuid` = 自定义 128-bit GUID（DqGuid.h）

### 8.3 第三方库集成

参考项目的第三方库直接集成到 dqBase 源码中（不作为外部依赖）：

| 库 | 来源 | 许可证 | 位置 |
|---|------|-------|------|
| **btree** | Google cpp-btree | Apache 2.0 | `dqBase/PublicAPI/dqBase/btree/` |
| **bpool** | Boost.Pool | BSL 1.0 | `dqBase/PublicAPI/dqBase/bpool.h` |
| **stb_image** | nothings/stb | Public Domain / MIT | `third_party/stb_image/` |
| **cgltf** | jkuhlmann/cgltf | MIT | `third_party/cgltf/` |

不得用 std::map/std::set 替代 btree，不得用 std::malloc 替代 bpool——这些是性能关键路径，不是风格偏好。

> **`std::unordered_*` 的处理（§0 逐文件参考保真）**：本条只禁 `std::map`/`std::set`（有序）。`std::unordered_map`/`std::unordered_set` 的去留由**被移植文件的参考源**决定，而非 imodel-native 的容器风格：移植自 itwinjs-core 且参考用 TS `Map<K,V>`/`Set<V>`（哈希 + 插入序）的文件，`std::unordered_*` 是**忠实**实现，保留；移植自 imodel-native C++（`bmap`/`bset`）的文件用 `bmap`/`bset`。即"imodel-native 无哈希容器"不等于"TS-ref 文件里的 unordered 是偏差"。例外（技术必要，登记 TD-9/TD-10）：imodel-native `bpool.h` 本身用 `std::set<void*>`（1:1 保留）；`LRUTileList` 用 `std::map<Tile*,LRUNode>`（mapped-value 地址稳定性，`bmap` 不可替代）。

> **cgltf（GltfReader 批准偏差，架构师签字 2026-07-10，audit F2b）**：`GltfReader.h` 用 cgltf（jkuhlmann/cgltf，C99 glTF 2.0 spec 解析器，MIT，`third_party/cgltf/`）替代逐行移植 itwinjs `GltfReader.ts`（2975 行）的 Bentley glTF 栈。理由：glTF 2.0 是 Khronos 开放标准（即真正"参考"），`GltfReader.ts` 仅是其一种实现；cgltf 是成熟 spec-conformant 解析器，行为等价、无 Bentley 专有算法/数据结构。登记为**批准的第三方解析器替换**（非 §0 违规）；`GltfReader.h` 仍引 §0 参考 + 标注 APPROVED DEVIATION，mesh 几何提取（→ `IndexedPolyface`）行为对齐参考。

> **stb_image（GltfReader baseColor 纹理解码，批准偏差，架构师签字 2026-09-15）**：dqCommon/dqRender 公开 API 零 Qt（§8.2），glTF `baseColorTexture` 的 PNG/JPEG 解码用 stb_image（nothings/stb，单头，PD/MIT）替代自写解码器（§7 禁自创算法）。图像解码是成熟开放标准，stb_image 是广泛使用的 spec-conformant 解码器，无 Bentley 专有算法。登记为批准的第三方解码器替换（非 §0 违规）。

### 8.4 SDK 边界

- 公开 API 在 `PublicAPI/<模块名>/`，唯一可被跨模块 `#include` 的目录
- 实现细节在 `src/`，绝不暴露到 `PublicAPI/`
- 跨模块通信只通过 SDK 接口（抽象基类、回调、句柄）

**如何验证（§8 全节）：** (a) include 依赖图工具检查无反向/横向依赖；(b) 引擎模块公开头 `grep -rE 'QString|QVector|QHash|QMap|<Q' PublicAPI/` 不得命中 Qt 符号。

---

## 9. C++ 编码规范纲要 `[P0/P1 混合]`

完整格式细节见 `docs/DanQing-C++代码规范.md`（从属格式参考，不得与本节矛盾）。

- **C++20**（对齐 imodel-native），编译标志 `-std=c++20 -Wall -Wextra -Werror -fno-exceptions -fno-rtti`
- **命名**：严格遵循 §3（类型 PascalCase；方法/函数 1:1 对齐参考；前缀 `m_` 成员 / `s_` 静态 / `k` 常量 / `I` 纯虚接口）
- **禁止（硬约束，DanQing 架构选择，与参考一致）**：异常、RTTI、裸指针、`shared_ptr`、`NULL`
- **格式偏好（从属于 §0/§3 1:1 参考对齐）**：`typedef`→`using`、C 风格转换→`static_cast`、运算符重载——**仅当参考项目（imodel-native/filament C++）未使用时对新代码强制**；参考已用则 1:1 保留（证据见 §12.7、§14 TD-7）
- **允许**：STL 容器头（公开头）—— 对齐 imodel-native Bentley/PublicAPI 和 filament include/ 的实际做法
- **必须**：`enum class` + 显式底层类型、`override`/`final`、`noexcept`、`explicit`、`#pragma once`、`using` 别名
- **所有权**：`RefCounted<T>` CRTP + `RefPtr<T>`（禁 `shared_ptr`）
- **错误**：`Result<T,E>`（核心引擎禁 `throw`；仅模块边界捕获第三方异常转 `Result`）
- **公开头文件**：Qt-free（见 §8.2）；字符串/集合用 `std::string`/`std::vector` 或 dqBase thin wrapper（`DqString`/`DqVector`）

---

## 10. 渲染架构

DanQing 的渲染管线严格对齐 itwinjs-core 的 `core/frontend/src/render/` 架构；Windows 平台层（WGL）严格对齐 filament 的 `PlatformWGL.cpp`。

### 10.1 核心组件

| 组件 | itwinjs-core | DanQing | 说明 |
|------|-------------|-------|------|
| RenderSystem | `RenderSystem.ts` | `dqRender/RenderSystem.h` | 全局单例，工厂模式 |
| RenderTarget | `RenderTarget.ts` | `dqRender/RenderTarget.h` | 声明式接口 |
| Scene | `Scene.ts` | `dqRender/Scene.h` | foreground/background/overlay |
| Decorations | `Decorations.ts` | `dqRender/Decorations.h` | 装饰器图形容器 |
| DecorateContext | `ViewContext.ts` | `dqApp/DecorateContext.h` | 装饰器收集上下文 |
| ChangeFlags | `ChangeFlags.ts` | `dqApp/ChangeFlags.h` | 变更标志位掩码 |
| GraphicBranch | `GraphicBranch.ts` | `dqRender/GraphicBranch.h` | 场景图节点 |
| RenderGraphicOwner | `RenderGraphic.ts` | `dqRender/RenderGraphic.h` | 防止自动释放 |

### 10.2 渲染管线

`Viewport::renderFrame()` 对齐 itwinjs-core 的 **24 步**管线（`Viewport.ts:2546-2703`，源码逐行核对）：

```
1.  帧统计开始 (beginFrame)           13. Feature Symbology Overrides
2.  ChangeFlags 快照与重置            14. 场景创建 (createScene→changeScene) ← 最重
3.  缓存 view/target                 15. 渲染计划验证 (validateRenderPlan)
4.  StopWatch 计时                    16. 装饰收集 (addDecorations→changeDecorations)
5.  动画执行 (animate)                17. Flash 处理 (processFlash→setFlashed)
6.  isRedrawNeeded 初始化             18. Pre-render hook (onBeforeRender)
7.  尺寸变化检测 (updateViewRect)     19. 计时结束
8.  控制器同步 (setupFromView,条件)   20. 实际绘制 (drawFrame, 仅 isRedrawNeeded) ← GPU 提交
9.  选择集更新 (setHiliteSet)         21. 帧统计结束 (endFrame)
10. overridesNeeded 计算              22. 尺寸事件 (onResized)
11. 分析分数 (setAnalysisFraction)    23. 变更事件分发 (onViewportChanged 族)
12. 时间点 / ScheduleScript           24. 持续渲染请求 (requestNextAnimation)
```

关键约束（详见分析文档 §3）：
- **isRedrawNeeded=false 时跳过 GPU 提交**（置位来源共 10 处）
- **步骤 24 条件不含 missing tiles**——瓦片重绘走 `invalidateScene()` 级联
- **四级失效级联**：invalidateController → invalidateRenderPlan → invalidateScene → invalidateDecorations（每级主动 requestNextAnimation）
- 步骤 12 的 `containsTransform` 会**本帧内** invalidateScene（变换动画每帧重建场景）

### 10.3 SceneCompositor 多 Pass 渲染

```
SceneCompositor::Draw(commands)                    // SceneCompositor.ts:1410-1519
  ├─ ClearOpaque()                                 // 3-MRT 清屏（color/featureId/depthAndOrder）
  ├─ RenderBackground() / RenderSkyBox() / RenderBackgroundMap()
  ├─ pushViewClip()
  ├─ RenderVolumeClassification()
  ├─ RenderLayers(OpaqueLayers)
  ├─ onRenderOpaque 事件                            // GPU 厂商扩展点
  ├─ RenderPointClouds()                            // 可选 EDL
  ├─ RenderOpaque()    → DrawPass(OpaqueLinear/Planar 写 pick; OpaqueGeneral 不写)
  ├─ RenderLayers(TranslucentLayers)
  ├─ IF needComposite:                             // CompositeFlags=None 时整段跳过
  │    ClearTranslucent() → RenderTranslucent()    // WBOIT 加权混合 OIT（无需排序）
  │    → RenderHilite() → Composite()              // 7 变体全屏合成
  └─ RenderLayers(OverlayLayers) + popViewClip()
```

> 渲染管线全流程、WebGL 后端内部实现（DrawCommand 构建/着色器变体/Uniform 与 GL 状态/几何与 Imdl 解码/Tile 系统/拾取路径）与性能设计原理详见 `docs/itwinjs-core-渲染系统执行流程分析.md`。

### 10.4 Windows 呈现层（WGL，filament 对齐） `[P1 强规则]`

Windows 呈现链路的结构规则（每条都有真实事故背书，案例 §12.9）：

- **结构对齐 filament PlatformWGL**：swapchain（WglSwapChain）自持 DC（create 时 GetDC 一次/destroy 时 ReleaseDC）；dummy 承载窗口创建 GL 上下文；所有窗口共用同一 `m_pfd`（HDC 与 HGLRC 像素格式必须匹配）；`destroySwapChain` 只销毁 headless 自建窗口（**永不 DestroyWindow 应用窗口**）。
- **resize = 表面过期**：GL 子窗口表面 extent 变化（最大化/还原/拖拽）必须原地重建平台交换链（Vulkan `VK_ERROR_OUT_OF_DATE_KHR` 的 WGL 对应物）。"OpenGL 无需重建交换链"是错误假设——SwapBuffers 可逐帧成功而屏幕冻结旧帧。
- **窗口句柄生命周期归应用层**：Qt 在状态跃迁时可销毁重建原生子窗口，**Windows 会回收复用同一 HWND 地址**——`rebind` 不得做句柄相等短路；WinIdChange 即"旧表面已死"语义，无条件重建。
- **句柄失效必须显式传播**：销毁 GL 对象时必须同步失效所有缓存它的层（编译状态/句柄成员/状态跟踪器）；销毁后继续使用 = use-after-destroy，且下游静默跳过（如 `useProgram` 的 `if (prog && prog->isValid())`）会把故障放大成错误输出而非报错。
- **可见性跃迁（最小化/恢复）分两层分锅：表面层 ≠ 触发层**（2026-09-14 最小化黑屏 saga）：屏幕黑 ≠ 表面坏——先用 present 计数器 + "窗口完全可见后显式帧能否上屏"区分。两个陷阱：①恢复窗口期内的**第一帧会 present 进虚空**（原生窗口尚未完成映射，Win11 恢复动画期内），`m_redrawPending` 已消费则再无第二帧；②`QEvent::Paint` 是**单次触发语义**（Qt validate 后不再补发 WM_PAINT），唯一一次 Paint 也可能落在映射前。修复范式 = Show→rebind（表面层）+ Paint→RequestRedraw（触发层）+ Show 时延迟补帧（150ms/400ms，有界事件驱动，保证至少一帧落在完全可见之后）。

---

## 11. 工作流程

1. **先完整阅读参考项目源码**，建立完整的功能清单（每个文件、每个类、每个函数）
2. 对照清单逐项移植，不跳过任何一项
3. 确认模块归属，是否违反 §8 依赖方向
4. 看现有代码，匹配周围风格（命名遵循 §3）
5. 先写测试（来自参考，§5），再写实现
6. 完成后对照 §6 一致性验证
7. 文档反映实际状态，不是理想状态
8. **修 bug 同样适用第 1 条** `[P1 强规则]`：任何行为修复前，先定位并**完整阅读**参考项目的对应实现（不是只查行号），确认参考在该场景下的真实机制后再改。参考没有该机制时（如某 GL 适配细节），先验证差异是否源于移植缺口（补齐移植），而不是发明替代方案。症状推理（"大概是 GPU 同步"→延迟销毁/"重建太频繁"→防抖）是自创实现的入口，禁止（案例见 §12.8）。
9. **调试取证纪律** `[P1 强规则]`（案例 §12.9）：
   - **先要精确复现配方**：用户的操作序列（从哪个初始状态、哪些开关、什么动作顺序）是唯一 ground truth；拿到配方前不做复现尝试——初始状态差异（如 Grid 默认关闭）会让数小时的自主探索全部无效。
   - **证据来自插桩轨迹，不来自假设**：在层间边界（窗口事件/swapchain 生命周期/GL 状态/FBO 回读）放 env 门控探针，让轨迹说话；轨迹与假设矛盾时信轨迹。
   - **症状重叠 = 可能多根因**：修好一条根因后症状仍在，不是"修复无效"，是还有一条——每次修复后必须复核全部证据是否闭环。
   - **渲染/视觉行为改动，像素级回归先行**：先建 readPixels 断言（内容存活/基准帧对比），再动代码；"不崩"不是"画对了"。
   - **真实窗口取证三件套**：①每击前台守卫（`GetForegroundWindow` == 目标）+ 按钮 `WM_NCHITTEST` 实时探测，不满足即中止，绝不盲点；②坐标不推算——裁剪目标区域截图亲眼量，点击后再截图确认；③取证工具先自测（stderr 重定向是全缓冲、.cmd 必须纯 ASCII、env 门控要验证真的生效）。

10. **移植完整性以调用链为单位** `[P1 强规则]`（案例 §12.10）：
    - "完成"的度量单位是**调用链**，不是类/函数：移植任何组件时，先在参考侧检索它的全部**调用者与数据喂入者**，DanQing 侧必须有等价接线。**组件忠实但无人调用（ported-but-uncalled）= 移植未完成**——忠实的组件放在断裂的链上平时不可见，直到角案（符号/纵横比/时序/坐标系）引爆。
    - 上游缺数据时**禁止从相邻数据自创旁路合成**（症状：两实现"看起来等价"却在某维度反号）——必须补齐参考的上游链（调用者/喂入者），这与第 8 条同源：旁路是自创实现的入口。
    - **等价替换登记制**（唯一例外通道）：确需用相邻数据合成参考从别处取得的值时，必须 (a) 注释 `EQUIVALENCE: 参考源=<project path:line>；发散=<清单 | 未发现，验证法=...>`；(b) 每条已知/潜在发散配一个能抓住它的回归测试。
    - **等价性是全定义域命题**：宣称等价前枚举输出的全部维度（数值、符号、边界、时序、坐标系），逐维度验证；抽查常见路径不算验证。

11. **判据与仪器的有效性纪律** `[P1 强规则]`（案例 §12.9/§12.10）：
    - **位置断言制度**：视觉特性的回归至少一个 WHERE 断言（内容在哪个象限/哪侧/朝向），断言信息量 ≥ 失败模式自由度——面交换类缺陷有一个自由度，断言就必须钉住一个朝向标记（几何缺口/纹理色点）。需要不对称标记时新建专用测试资产（BoxTexturedDots 模式），**禁止原地突变既有资产**（突变令历史测量不可复现）。
    - **仪器自检**：新取证工具首次使用前用已知答案校准（合成图过同一写入器 / 已知状态的程序过同一探针）；输出一律**绝对路径**，读回前验证文件时间戳；env 门控验证真的生效；机器视觉/模型的方位结论（左右/上下）必须**字节级复核**后才可采信。
    - **测前重 dump**：每次测量前重新确认实验对象状态——资产内容（哈希）、exe 构建时间（库改动后 app 必须重建）、双实现加载的是同一文件；上一轮的 dump 不代表这一轮。
    - **A/B 对比协议**：双实现对比必须同输入（打印哈希）、同视图/状态、干净初始态，缺一即结论无效；合成驱动（CDP/模拟点击）建立的状态与用户真实流程不等价时，其观测只作线索不作结论；破坏用户会话状态的动作（reload/重启用户的应用）先征得同意。

12. **多根因排除与交付前全扫** `[P1 强规则]`（案例 §12.8/§12.9/§12.10）：
    - 每次修复后执行**全量复扫**：全部标准视图 × 双实现矩阵 + 复核全部既有证据闭环；症状消失 ≠ 根因清零。
    - "修复无效"的第一解释是**还有一条根因**，不是"修复错了"——除非复扫证据直接推翻修复本身；禁止在未复核证据前回滚或叠加新补丁（§12.8 三层补丁全回滚的原案）。
    - 交付用户验证前：全量重建所有受影响目标并核对 exe 时间戳——"库改了没重建 app"会制造假"修复无效"。

---

## 12. 经验教训（反模式）

### 12.1 不要先设计再对照，要先读再写
**错误**：先设计 API，再对照参考项目检查遗漏。
**正确**：先完整阅读参考项目全部源码，建立功能清单，再逐项移植。
**教训**：dqBase 第一版只有 22 个头文件（覆盖率 27%），因为没有先读参考项目就动手写代码。

### 12.2 不要用"优先级"代替"完整性"
**错误**：把功能分为高/中/低优先级，低优先级的推迟或跳过。
**正确**：参考项目做了什么，就做什么。没有"可以不实现"的选项。
**教训**：btree/bpool 被标记为"不实现"，实际上是性能关键路径和公开 API 的一部分。

### 12.3 不要自己判断"够不够用"
**错误**：认为 std::map 可以替代 btree，std::malloc 可以替代 bpool。
**正确**：参考项目选择 btree/bpool 有其原因，不要用自己的判断替代参考项目的判断。
**教训**：btree 比 std::map 快 3-10x，bpool 是固定大小块分配器——这些不是"风格偏好"，是架构决策。

### 12.4 不要假设，要验证
**错误**：假设 Qt 是稳定的跨模块 ABI 就直接植入。
**正确**：从 §8 架构约束推导——引擎模块必须独立，因此不能依赖 Qt。
**教训**：Qt-first 设计导致整个 dqBase 需要重写。

### 12.5 参考项目的代码是"规范"，不是"参考"
**错误**：把参考项目当作"可以借鉴的资源"。
**正确**：DanQing 是参考实现（reference implementation），参考项目的代码就是规范。
**教训**：§0 已经明确写了这个原则，但执行时没有严格遵守。

### 12.6 方法名归一不可行——让规则让步于参考一致性
**错误**：要求 TS 移植的方法名统一 PascalCase（"统一 C++ 风格"），用 clang-tidy 批量改写。
**正确**：方法名 1:1 按参考语言（TS→camelCase），与结构性标识符一致；规则反转，TD-1 消解。
**教训**：PascalCase 方法归一在本代码库不可行——众多 getter 以类型命名（`shaderLanguage()`→`ShaderLanguage()` 与枚举同名，132 处碰撞）；且 dqRender 重写 dqCommon 虚函数，跨模块归一会破坏多态（17 处 virtual 隐藏）。命名规则应让步于参考一致性，而非强求语言统一。

### 12.7 格式规则让步于参考一致性——typedef / C 风格转换 / 运算符重载
**错误**：把 C++ 规范 §2.2（禁 `typedef`）、§2.3（禁 C 风格转换）、§9（禁运算符重载）当作机械红线，用 clang-tidy 批量改写存量代码。
**正确**：这三项是**格式偏好**，从属于 §0（参考是规范）与 §3（1:1 对齐）。当参考项目（imodel-native/filament C++）本身使用 `typedef`/C 风格转换/比较·哈希运算符时，DanQing 移植代码 1:1 保留；仅参考未涉及的新代码才强制 `using`/`static_cast`/避免运算符重载。
**教训**：审计确认存量"违规"全部是逐字移植：`DqTime.h` 的 `(double)`/`(uint32_t)`/`(double)(int64_t)` 转换 = imodel-native `BeTimeUtilities.h:58/69/111/179`；`BeThreadLocalStorage.h:32`/`LocalState.h:68`/`PTypesU.h`/`Version.h` 的 `typedef` = imodel-native 同名行；`DqGuid`/`DqId`/`Version`/`DqTime`/`ScopedArray` 的比较/哈希/下标运算符是 `std::map`/`set`/`unordered_map` 键所需且对齐参考。强行归一会引入 P0 违规（破坏 1:1）。与 §12.6 同构：格式规则让步于参考一致性。登记见 §14 TD-7。

### 12.8 修 bug 不是自创许可——症状推理补丁 vs 参考机制移植（2026-09-13 resize 事故）
**错误**：用户报告"缩放后 resize 卡顿"，在**从未打开参考项目对应实现**的情况下，连续三层症状推理补丁：①"glDelete 驱动同步等待 80-110ms"→ 自创延迟销毁（retireAll/flushRetired）；②"Target 整只销毁太重"→ 自创 `setViewRect`/`resetForResize`（还在注释里写 `Ported from: Viewport.ts:2604-2608` 装门面，但从未读 Target.ts 的 updateViewRect 实现）；③"连续 resize 每帧重建"→ 自创 150ms 防抖 timer。三次全部引入视觉回归（Grid/ACS 位置错乱 → Fit 视口下 Grid 不可见），靠用户肉眼发现，最终整段回滚。
**正确**：修复前先完整阅读参考的 resize 链路——`Viewport.ts:2604` `resized → target.updateViewRect()` 之后 Target.ts/OnScreenTarget 里 updateViewRect/onResized 到底做什么（它如何重分配 GL 资源、哪些状态失效），照机制移植；若 DanQing 当初的 `delete+createRenderTarget` 本身就是移植缺口（updateViewRect 空壳），修复方向是**补齐参考的 updateViewRect 实现**，而不是在自创路径外面再包三层自创补丁。
**教训**：
1. **修 bug 是 §0 暴露面最大的场景**——移植新功能时"先读参考"是自然动作，调试时的紧迫感让人退回"合理推理+快速补丁"习惯，而渲染管线的症状（卡顿/闪烁）推理出的"病因"十有八九不是参考机制的真实结构。
2. **`// Ported from:` 行号引用必须对应真实读过的实现**。引用行号装点自创代码是双重违规：既自创了实现，又污染了溯源体系（§0 的可追溯性机制被当作遮羞布）。写下行号前自问：这个文件的这个函数，我打开过吗？
3. **渲染/视觉行为的改动必须有"画对了"的验证，不能只验证"不崩"**。View3DResizeTest 只断言进程存活，三次视觉回归全部漏网。改动渲染管线资源生命周期前，先建像素级回归（readPixels 对比基准帧/断言装饰图元屏幕位置），再动代码。
4. **用户等待压力不是降低流程标准的理由**——"猜测→让用户验证→又错"的循环比"先读参考再修"浪费的时间多得多（本案例三次返工 + 用户三次无效验证）。

### 12.9 黑方块 saga——多根因、句柄生命周期与真实窗口取证（2026-09-14）
**事故**：用户报告"最大化后 Grid 消失、滚轮放大后出现黑色大方块"。历经：看门狗两版（症状补丁，皆弃）→ 桌面注入复现（被终端遮挡吃掉全部点击）→ 自主探索数小时无效（Grid 默认关闭，复现配方错误）→ 用户给出精确配方后立刻复现。
**最终双根因**：
1. **呈现层**：`OpenGLSwapchain::resize` 假设"OpenGL 无需重建交换链"——GL 子窗口表面 extent 变化（最大化）后 WGL 呈现关联失效，SwapBuffers 逐帧成功但屏幕停在按新尺寸缩放的旧帧（旧帧网格平面区域=用户看到的"黑色大方块"）。修复=resize 即表面过期、原地重建平台交换链（Vulkan OUT_OF_DATE 的 WGL 对应物）。
2. **内容层**：resize 重建 OIT 资源时**销毁了合成 shader 的 GL 程序**（参考 dispose(_fbos) 只毁 FBO/纹理、程序随上下文存活），但 `ShaderProgram` 编译缓存仍 Success、句柄悬空 → `use()` 把死句柄交给 `driver.useProgram` → `if (prog && prog->isValid())` **静默跳过 glUseProgram** → 合成 quad 被残留 program 画黑。
**教训**：
1. **症状重叠 = 可能多根因**。修好呈现层后网格仍消失——不是修复无效，是还有第二条。每次修复后必须复核全部证据是否闭环（本次判据：OIT 三纹理回读全对而 composite 输出中心纯黑 → 损失在合成层 → [COMPDIAG] 探针抓 curProg 165→3 实锤）。
2. **销毁 GL 对象必须同步失效所有缓存层**（编译状态/句柄成员/状态跟踪器）。下游对失效句柄的"防御性静默跳过"会把 use-after-destroy 放大成错误输出——失效传播是设计义务，不是可选项。
3. **HWND 会被回收复用同地址**。"句柄相等"不等于"同一对象还活着"；表面过期语义无条件重建，不看句柄值。
4. **先要精确复现配方再动手**。用户的操作序列（初始状态+开关+动作序）是唯一 ground truth；我自主探索数小时（各种最大化循环/缩放风暴）全部无效，因为 Grid 默认关闭、配方从根上就错了。
5. **真实窗口取证**：桌面注入点击必须前台守卫 + hit-test 实时探测（无守卫的 12 轮循环全打在被最大化的终端上，还误关了它）；坐标不推算（analyze_image 两次给错、窗口每次记住不同几何）——裁剪截图亲眼量、点击后再截图确认；取证工具先自测（.cmd 中文注释编码让 set 静默失效、stderr 文件重定向全缓冲 4KB 不落盘、env 门控要验证真生效）。
6. **像素级回归先行**：`MaximizeKeepsGridVisible`（readPixels 断言）在修复前就是红色锁定、修复后转绿——比真实 app 反复点鼠标可靠一个数量级。


### 12.10 深度反转 saga——旁路参考投影源、隐性面切换与“层层忠实却结果矛盾”（2026-09-16）
**事故**：用户报告 glTF 立方体贴图与 DTA 左右相反（F 朝向不同）。取证中每一层单独验证都“忠实”（文件 UV→polyface→CPU 顶点→GPU VBO 字节→attrib 指针→着色器源码→纹理对象回读），渲染结果却呈 u 镜像——逻辑死局。
**根因**：`Viewport::renderFrame` 把 `worldToNdc`（视域盒→NDC 线性直通，m22 恒正）经 `setViewportTransform→changeProjectionMatrix` 当投影，**旁路了参考唯一的投影来源**（`changeRenderPlan(plan.frustum) → FrustumUniforms.changeFrustum → lookIn+ortho(0,depth)`，m22=−2/depth）。正值 m22 + LEQUAL = 最远面获胜 → 正交 Top 视图渲染的是立方体**底面**——底面 UV 与顶面左右相反 → 假性“贴图 u 镜像”（v 不受影响，故只有左右反）。
**修复**：RenderPlan 补 `frustum/fraction/is3d`（RenderPlan.ts:48/66/67）→ `changeRenderPlan` 调 `changeFrustum`（Target.ts:534 顺序：在 updateRenderPlan 前）→ `ValidateRenderPlan` 从 ViewingSpace 填充 → `setViewportTransform` 推送 lookIn/ortho 一致对。DanQing 的 `FrustumUniforms::changeFrustum` 本就是忠实移植——只是无人调用。
**教训**：
1. **逐层验证全部通过却结果矛盾时，怀疑隐性面切换**。深度反转不改变 x/y 投影：几何轮廓、贴图存在性、甚至像素内容都“看起来对”，只有 UV 场的梯度方向与数据对不上。解法：着色器输出 `v_texCoord` 为颜色（DANQING_UV_DEBUG）+ 面内大样本回归梯度，一次定位是哪个面的场。
2. **`[MVP]` m22 符号是深度反转的一击必杀探针**：正 = 反转（LEQUAL 下最远获胜），负 = 参考惯例（近平面 depth 0）。
3. **不对称标记物**：几何用缺口角（bbox 四角内容存在性），纹理用色点；字形质心有歧义（臂/stem 分布不均）勿用。
4. **测试 exe 的文件输出路径相对 CWD**——必须从仓库根跑，否则写失败读旧帧（本 saga 两次假观测的直接来源；探针文件路径建议绝对化或先自测）。
5. CDP 驱动参考 app（DTA）的坑：`Page.reload` 丢 blank connection 视口；合成点击开的视口 `renderTarget:none`（画布零内容，采集不可信）；rAF 被遮挡挂死（用 setTimeout 泵）；大数组 returnByValue 极慢（页内分析/`Page.captureScreenshot`+canvas rect 裁剪替代）。
6. **像素级回归锁**：`GltfStandardView.TopViewRendersTopFaceNotBottom`（BoxTexturedDots 色点资产，断言 Top 视图蓝点在面右/红点在左 = +Z 面忠实场）——修复前 RED、修复后 GREEN，已双向验证。

---

## 13. 构建与测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

- **开发一律 Debug**（用户指令）。
- Qt 路径：`third_party/qt/windows/6.11.1/msvc2022_64`（Windows，当前开发平台）；`third_party/qt/mac/6.11.1/macos`（仅 dqApp 使用，引擎 SDK 不依赖）。GoogleTest：`third_party/googletest`。
- 常用测试目标：`DisplayTestAppDtaTest`（真窗口 harness，View3DResize/ZoomBlackBox 等像素级回归）、`DisplayTestAppTest`、`dqAppTest`、`dqGeomTest` 等；ctest 选择器如 `-R "View3DResize|ZoomBlackBox"`。
- DtaTest 全量为回归门（TD-16 于 2026-09-23 清偿：73 通过 + 2 跳过 + 0 失败；此前「不跑全量」的临时指令 2026-09-21 随修复失效）。

### 13.1 渲染/窗口诊断开关（env 门控，默认零开销）

| 环境变量 | 作用 |
|---|---|
| `DANQING_GL_TRACE=1` | 窗口事件轨迹（[VPEVT] WinIdChange/Show/Resize）、swapchain 生命周期（[WGL] create/destroy/makeCurrent 失败+自愈/SwapBuffers 失败、[SC] rebind/resize 重建） |
| `DANQING_NM_TRACE=1` | 法线贴图链取证（[NMDIAG]）：GltfDecoration 的 normalMapTexture 解析结果（尺寸）+ 绘制点门控状态（surfTex/normalTex 句柄、HasNormalMap 位、displayNormalMaps、renderMode、textures、applyLighting）——2026-09-17 法线贴图接线 saga 所加 |
| `DANQING_OIT_DUMP=1` | 合成器帧尾 dump（accum/revealage/opaqueSnapshot 回读、mainRT pre/postComposite/endDraw、translucent 命令清单）+ [PRES] present 计数与尺寸 |
| `DANQING_SV_TRACE=1` | ViewingSpace.adjustZPlanes 分支轨迹（[SVADJ]：进入的 org/delta/grid/bgMap/extents、StronglyOutside 门的平面数与分类结果、depthRange 与 delta.z 决策）——2026-09-15 Front/Back 甩位 saga 取证所加 |
| `DANQING_UV_DEBUG=1` | Surface 片元着色器 sampleSurfaceTexture 改为输出 `v_texCoord`（R=u,G=v）——贴图镜像/UV 链取证的活体场可视化（2026-09-16 深度反转 saga） |
| `DANQING_MVP_TRACE=1` | 每图元 u_mvp 16 元素 + 行列式（**m22 符号 = 深度反转探针**：正=最远面获胜） |
| `DANQING_UV_TRACE=1` / `DANQING_VAO_TRACE=1` / `DANQING_DRAW_TRACE=1` | PolyfaceGraphic CPU 顶点 dump（[PGV]）/ draw 时 VBO 字节+attrib 绑定回读（[VAO]/[DRAW3]，含 GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING） |
| `DANQING_TEX_DUMP=1` / `DANQING_SHADER_DUMP=1` / `DANQING_ATTR_TRACE=1` / `DANQING_WRAP_TRACE=1` | 256×256 纹理上传后 glGetTexImage 回读（[TEXDUMP]）/ 着色器源码落盘 build/v(f)shader-*.glsl / activateProgram 时 a_texCoord location+attrib3 状态（[ATTR]）/ 绘制时 wrap 状态（[WRAP]） |
| `DANQING_CURSOR_TRACE=1` | 光标/光圈链取证：ToolAdmin::Decorate 光圈守卫（[CURSOR]）+ Viewport::leaveEvent（[LEAVE]，fflush 落盘）+ drawCanvasDecorations 的 flush/输出目标块回读（写 C:/Windows/Temp/danqing_cursor_flush.log——GUI 进程 stderr 不落盘的旁路）——2026-09-19 leave 取证所加 |
| `DANQING_DP_TRACE=1` | 深度预览链轨迹（[DP]）：ViewManip::previewDepthPoint 门控状态（preview/inDyn/nPts）+ CollectDecorations 运行 + drawFrame/drawOverlays 的 WorldOverlay 命令数——2026-09-19 Rotate 锚定态取证所加 |
| `DANQING_DUMP_OUT=1` | canvas 帧的输出目标全帧 PPM 落盘 build/outtarget-%03d.ppm（有界 40 帧，绝对路径）——亲眼定位 canvas 装饰在输出目标中的实际落点（2026-09-19） |
| `DANQING_AUTO_OPEN_DECO=1` | DisplayTestApp 启动 3.5s 后自动打开 Decoration Geometry Example（合成点击在 Start 页卡片上不生效的绕路——真实 app 桌面注入取证用） |

真实 app 取证启动器（stderr 落盘）：`build/run-traced.cmd`（纯 ASCII；调试会话可按需重建）。桌面注入取证脚本模板：`build/leave-forensics.ps1`（前台守卫 + 物理光标 SetCursorPos + 截图对拍 + 优雅退出保 stderr 落盘；**先杀僵尸 DisplayTestApp.exe 再跑，否则前台/测试全污染**）。

---

## 14. 现有命名技术债务

以下存量违规是历史代码积累，**不阻塞新规则落地**，但必须显式登记、按严重度排期清理。新代码必须零违规。

| 编号 | 存量违规 | 范围 | 修正 | 严重度 |
|---|---|---|---|---|
| ~~TD-1~~ ✅ 已解决（规则反转） | dqRender 方法名 camelCase | dqRender 全模块移植代码 | §3 规则反转：方法/函数名 1:1 按参考语言（TS→camelCase），不再 PascalCase 归一。现有 camelCase 代码已合规，无需重命名。证据见 §12.6 | — |
| ~~TD-2~~ ✅ 已解决 | 成员变量 `m`+PascalCase | 全模块 | 全模块清理完成：dqBase/dqGeom/dqCommon（15）+ dqRender（~557）+ dqApp；word-boundary perl | — |
| ~~TD-3~~ ✅ 已解决 | 自由函数 `frustumProjection`/`orthoProjection` 归一为 `Frustum`/`Ortho` 会与 `dqCommon::Frustum` 类型同名冲突 | `FrustumUniforms.{h,cpp}` | 按 §3.4 碰撞例外保留后缀（非违规） | — |
| ~~TD-4~~ ✅ 已解决 | dqRender 链接 Qt6（`TileRequestFetcher` + 死代码 `QtContextBridge`）违反 §8.2 | `dqRender/src/tile/`、`dqRender/src/platform/` | Qt 实现移至 dqApp（`QtTileRequestFetcher`）经 DI 注入；`TileAdmin` 默认改为 Qt-free `NullTileFetcher`；删除死代码 `QtContextBridge.mm`；dqRender 零 Qt 链接 | — |
| ~~TD-5~~ ✅ 已解决 | 公开头 Qt 类型审计 | 引擎模块 PublicAPI | 已审计：公开头代码零 Qt（命中均为"// 替代 Qt 容器"说明性注释，保留）；`ITileFetcher.h` 已 Qt-free | — |
| ~~TD-6~~ ✅ 已解决 | 现有 1783 个测试中未标注 `// Ported from:`/`// Authored:` 的 `TEST()` | 全模块 tests/ | 全部补齐：文件级出处传播到每个 `TEST()`（脚本 `scripts/annotate_test_provenance.pl`）；0 个未标注 | — |
| ~~TD-7~~ ✅ 已解决（规则澄清） | `typedef` / C 风格转换 / 运算符重载 被误判为 §2.2/§2.3/§9 违规 | dqBase 移植代码（DqTime、BeThreadLocalStorage、LocalState、PTypesU、Version、DqGuid、DqId、ScopedArray） | §9 澄清：这三项是**格式偏好**，从属于 §0/§3 1:1 参考对齐；存量均为 imodel-native 逐字移植，1:1 保留合归。新代码仍优先 `using`/`static_cast`/避免运算符重载。证据见 §12.7 | — |
| ~~TD-8~~ ✅ 已解决 | `GradientSymb`/`GradientSymbProps` 用 `std::shared_ptr<ThematicGradientSettings(+Props)>` 违反 §9（指针仅用于打断 `Gradient.h ↔ ThematicDisplay.h` 循环包含） | `dqCommon/Gradient.h`、`ThematicDisplay.h`、`Gradient.cpp`、`GradientThematicTest.cpp`、新增 `GradientKeyColor.h` | 解决方案 B1：**保持 `ThematicGradientSettings` 为值类型**（忠实 itwinjs 值对象语义，避免 `RefCounted` 删拷贝经 `optional<GradientSymb>`→`GraphicParams::Clone()` 的级联）。将 `GradientKeyColor`/`GradientKeyColorProps` 抽到独立 `GradientKeyColor.h`，使 `ThematicDisplay.h` 仅依赖它（不再 include `Gradient.h`），从而打断循环；`Gradient.h` 反向 include `ThematicDisplay.h`，`thematicSettings` 字段由 `shared_ptr` 改为 `std::optional`（可空 + 可拷贝）。引擎公开头 `shared_ptr`/`weak_ptr` 归零；1783 测试全绿 | — |
| TD-9（保留，非违规） | `bpool.h` 用 `std::set<void*>`（free_list/used_list）看似违反 §8.3 | `dqBase/PublicAPI/dqBase/bpool.h`（L873/883/888/971） | **1:1 移植 imodel-native `bpool.h`**（参考源码 L867/877/882/971 同样用 `std::set<void*>`）。§0/§12.7 要求 C++ 参考 1:1 保真；参考本身选 `std::set<void*>`，DanQing 逐字保留合归，**不得**改 `bset`（否则偏离 imodel-native）。同 TD-7/§12.7 性质：格式/容器规则让步于参考一致性 | — |
| TD-10（保留，非违规） | `LRUTileList` 用 `std::map<Tile*, LRUNode>` 而非 `bmap` 看似违反 §8.3 | `dqRender/src/tile/LRUTileList.{h,cpp}` | **技术必要性**：侵入式双向链表持有指向 mapped value 的裸 `LRUNode*`（previous/next）。`std::map` 保证 mapped-value 地址稳定（node-based 堆分配）；`bmap`（B-tree）插入时节点分裂会重定位 value，导致这些指针悬空（UB，实测 `FreeMemoryEvictsAfterClearUsed` 失败）。TS 参考 `LRUTileList.ts` 把节点嵌进 Tile；本 C++ port 用 map 避免 Tile 循环头依赖。如要消除需重构为 `bmap<Tile*, unique_ptr<LRUNode>>`（独立 pass）。已就地注释说明 | — |
| ~~TD-11~~ ✅ 已解决（2026-09-23） | dqApp ToolAdmin ViewTool 所有权 | dqApp ToolAdmin.{h,cpp} + ViewTool.cpp + DtaToolBars.cpp + Viewport.cpp | **修复（adopt/run 分离，架构师选型）**：①`adoptViewTool`/`disownViewTool` 接管点——生产 `runViewTool(new ...)` 先 adopt（登记 m_ownedViewTools）再 run，失败 disown+caller 删；②`startViewTool` **不登记**（栈对象/裸 run() 测试路径零删除——参考 GC 语义的 C++ 表达）；③自退出工具（ViewUndoTool::onPostInstall→exitTool 在 run() 调用链内）同步 delete this 是 UB——exitViewTool 改 **deferred 队列**，`flushDeferredViewToolDeletes` 在 RenderFrameGuard 析构（帧边界，参考 async run() 微任务恢复语义 ViewTool.ts:100-112）执行；④installViewTool 统一 dispose 旧 owned（含防悬空 scrub）。**教训**：startViewTool 清槽后重查槽值会误建 SuspendedToolState 快照（参考 if/else 在置空前判断 ToolAdmin.ts:1820-1827；快照到已变更 toolState → exit 恢复错态） | — |
| ~~TD-12~~ ✅ 已解决（2026-09-23） | dqAppTest 7 项跨测试失败 | — | 根因：AcsTriadDecorator 缓存 graphic 属于创建时的 RenderSystem——跨视口（前测 driver 已死）时 disposeGraphic 触死 driver（MeshGraphic::~MeshGraphic → SEH 0xc0000005 符号化栈实锤）。修复：缓存记录 m_creatingSystem，Decorate 时 system 变更则指针置空不 dispose（死 driver GL 资源随上下文消亡）。连带修 TileTreeRegistry.DropSupplier 指针同一性断言（堆回收复用地址→改状态判定）。结果：dqAppTest 353/353 | — |
| ~~TD-13~~ ✅ 已解决（2026-09-23） | IndexedPolyface::IsAlmostEqual 通道缺口 | dqGeom/src/polyface/IndexedPolyface.cpp | **修复**：对齐参考 PolyfaceData.ts:184-214 全通道面——points/normals/params 按 tol、point/normal/color/paramIndex 精确、colors 精确、edgeVisible、twoSided、expectedClosure。**锁**：`IsAlmostEqualCoversAllDataChannels`（Authored：参考无 GeometryQuery 级 IsAlmostEqual 对应物——10 通道差异检出 RED→GREEN） | — |
| ~~TD-14~~ ✅ 已解决（2026-09-23） | GltfDecoration 纹理不去重 | dqApp/src/GltfDecoration.cpp + GltfDecoration.h + PolyfaceGraphic + RenderPipeline/Viewport(external 透传) | **修复**：①resolvedTextures 成员级缓存（1:1 GltfReader._resolvedTextures GltfReader.ts:533——共享图一次解码+上传；**生命周期=decoration 成员**，~GltfDecoration 在 disposeGraphic **之后**销毁——局部缓存在 BuildGraphic 尾销毁会让纹理死于渲染前，glTF 像素回归抓过）；②PolyfaceGraphic `setTextureExternal/setNormalMapTextureExternal`（CreateTextureArgs.ownership="external" 语义 CreateTextureArgs.ts:50-52——graphic 不删共享句柄）；③createGraphicFromPolyface 三层透传 external 默认 false 零行为变化 | — |
| ~~TD-15~~ ✅ 已解决（2026-09-23） | 绑定派发缺口 | dqRender/src/render/UniformHandle.h + ShaderProgramImpl.{h,cpp} + SurfaceNormal.h + SceneCompositorImpl.cpp + GlLoader.{h,cpp} | **根因**：UniformHandle 是纯缓存（set* 只写 m_data 不触 GL），use()/draw() 的绑定派发在跑但值从未上屏——参考 UniformHandle.ts:90-138 每个 dirty set* 直接 context.uniform*，DanQing 移植丢了 GL 派发半截。**修复**（1:1 参考机制）：①`UniformHandle::create(glProgram, name)`（UniformHandle.ts:41-55）解析 location 存 handle，-1 = 缺失静默跳过（参考 null-location no-op）；②每个 set* dirty 后直接 glUniform*；③`Uniform::compile` 经 `ShaderProgram::getGlProgram()`（link 时缓存的裸 GL 句柄，参考 prog.glProgram）解析；④SurfaceNormal 的 u_normalMatrix 从 nullptr 注册补为 wireNormalMatrix 绑定（surface 侧漏注册——参考算 g_nmx 于 shader 内，DanQing §3.4 偏差走 CPU 上传）；⑤拆除 SceneCompositorImpl 4 处 legacy 名值上传（u_sunDir/u_lightSettings/u_surfaceFlags/u_normalMatrix）。**锁**：`UniformBindingDispatch.ProgramUniform/GraphicUniformBindingDispatchesToGl`（真 GL 探针：绑定→改值→重绑→readPixels 断言红通道 64→255；RED→GREEN 双向验证）。**注意**：glUniform4fv 写 mat4 location 是 INVALID_OPERATION no-op——wireModelViewMatrix 的 u_mv 走 setUniform4fv(mv,4) 与名值路径同值双写，无害但应改 setMatrix4（后续清理） | — |
| ~~TD-16~~ ✅ 已解决（2026-09-23） | DtaTest 全量 37 项失败 | — | ①DtaToolBars 两断言测试漂移修正（工具栏 5→6 加 Deco Example；Debug 点亮）②跨测试污染同 TD-12 根因修复。结果：DtaTest 73 PASSED + 2 SKIPPED + 0 FAILED，全量恢复为回归门 | — |
| ~~TD-17~~ ✅ 已解决（2026-09-23） | 无纹理纯色路径缺陷 | dqRender/src/render/PolyfaceGraphic | **根因**：`buildFromPolyface` 的 defaultColor 解包布局与调用方打包错位——打包 `(a<<24)|(b<<16)|(g<<8)|r`（GL RGBA 字节序），解包却按 `(r<<24)|(g<<16)|(b<<8)|a` → 通道错位（绿→品红/蓝→黄/alpha 落低位→透明→黑），纹理路径（纹理供色）掩盖。**修复**：解包对齐打包布局（defaultColor 与 per-vertex color 两处）。**锁**：`TileTreeRender.SolidBaseColorFactorRendersMaterialColors`（minimal-solid 资产，三色像素断言） | — |

| ~~TD-18~~ ✅ 已解决（2026-09-23 两轮收尾） | imdl 消费栈 | dqRender/src/tile | **已完成**：格式层（ImdlHeader/GltfHeader 变体/ImdlDocument）+ 量化顶点 CPU 解码（`decodeImdlGraphics`，TD-19 修复 1-based 索引后窗口端到端绿）+ **material fillColor 接线**（0x00BBGGRR→DanQing RGBA 打包，去硬编码绿）+ **FeatureTable 解析**（12B 头+3×u32/feature packed words→`dqCommon::FeatureTable`，PackedFeatureTable.ts:139-141 布局）+ `computeImdlChildTileProps` + `ImdlTile/ImdlTileTree` + `PrimaryTileTreeSupplier` + SpatialRefs 默认工厂。**测试**：ImdlHeader 5 + ImdlDocument 5 + ImdlGraphics 3 + ImdlTileTree 8 + 窗口端到端 1（全绿）。**后续增强**（独立登记，非通路缺口）：per-vertex 色/法线（oct-encoded）/纹理 UV、meshopt 压缩（v37 夹具在 ImdlParser.test.ts:284-443）、多材质/gradient、requestTileTreeProps/generateTileContent RPC 化 | — |

| ~~TD-19~~ ✅ 已解决（2026-09-23） | imdl 渲染端未上屏 | dqRender/src/tile/ImdlGraphics.cpp | **根因**：24-bit surface 索引 0-based 直传 `AddPointIndex`——IndexedPolyface 是 **1-based**（对照组 GltfReader.cpp:343 "cgltf indices are 0-based" 明确 +1；索引 0 是 buildFromPolyface 的跳过哨兵 PolyfaceGraphic.cpp:83）→ 每 facet 全部 corner 被跳过 → 0 顶点 → draw 空提交（graphics=1 但空、[MVP] 无 tile 矩阵、三视图不上屏——全部症状吻合）。**修复**：+1 转换（与 glTF reader 同惯例）。**锁**：`TileTreeRender.ImdlTilesetRendersRecordedFixture`（录制夹具端到端——绿色矩形 212 万像素上屏） | — |
> 清理属独立后续 pass，不在本规则重写范围内。
