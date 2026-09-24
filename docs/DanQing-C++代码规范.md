# DanQing C++ 代码规范

> 本文件是 DanQing 5 模块（dqBase / dqGeom / dqCommon / dqRender / dqApp）统一的 C++ 代码规范。本文件是 CLAUDE.md 的从属格式参考；冲突时以 CLAUDE.md 为准。
> 配套 `.clang-format`（格式）与 `.clang-tidy`（静态分析）位于仓库根目录，机器强制执行。

---

## 0. 一句话总则

> **现代 C++20，K&R 大括号，4 空格缩进，100 列限宽；类型用 PascalCase，方法/函数名 1:1 对齐参考语言（TS 移植→camelCase，C++ 参考→PascalCase，不归一），成员用 `m_` 前缀，常量用 `k` 前缀；用 `Result<T,E>` 而非异常；`RefCounted<T>` + `RefPtr<T>` 管共享所有权；每个模块一个 `PublicAPI/` 目录与一个 `DQ_模块_EXPORT` 宏；公开头文件用 `#pragma once` 且禁止暴露 Qt 类型（STL 容器允许）。**

---

## 1. 设计原则（优先级从高到低）

| # | 原则 | 含义 |
|---|------|------|
| P1 | **正确性优先** | 代码必须可证明地正确，宁可慢也要对 |
| P2 | **可读性 > 巧妙** | 给 6 个月后维护它的人写，不是给自己 |
| P3 | **零开销抽象** | 抽象不引入运行时成本 |
| P4 | **显式 > 隐式** | 所有权、线程安全、生命周期必须显式声明 |
| P5 | **编译期 > 运行期** | 能 `constexpr`/模板/`static_assert` 解决的不留到运行期 |
| P6 | **稳定性可演进** | 公开 API 必须有稳定性标签，破坏性变更走弃用流程 |
| P7 | **单一事实来源** | 一个概念只在一处定义，其余位置引用它 |

违反 P1/P2 需在代码评审中显式论证。

---

## 2. C++ 标准与语言特性

### 2.1 标准：**C++20**

对齐 imodel-native。可用 `constinit`/`consteval`/`concepts`/`ranges`/`<=>`。编译标志 `-std=c++20 -Wall -Wextra -Werror -fno-exceptions -fno-rtti`。

### 2.2 必须使用的特性

| 特性 | 规则 | 示例 |
|------|------|------|
| `enum class` | **所有新枚举必须用 `enum class`**，带显式底层类型 | `enum class DbResult : int { Success = 0, ... };` |
| `override` / `final` | 重写虚函数必须 `override`；叶子类标 `final` | `bool IsValid() const override;` |
| `noexcept` | 不抛异常的访问器必须标 `noexcept` | `size_t Size() const noexcept;` |
| `= default` / `= delete` | 显式表达特殊成员意图 | `Foo(const Foo&) = delete;` |
| `explicit` | 单参构造函数必须 `explicit` | `explicit Point3d(double v);` |
| `constexpr` | 编译期常量与简单函数尽量 `constexpr` | `static constexpr size_t kMaxTileDepth = 32;` |
| `nullptr` | 禁止 `NULL` 或 `0` 作空指针 | `T* p = nullptr;` |
| `using` | 禁止 `typedef`，统一用 `using` 别名 | `using TileId = uint64_t;` |
| 结构化绑定 / `if-init` / `std::optional` / `std::string_view` / `std::variant` | 鼓励使用 | — |

### 2.3 禁止使用的特性

| 特性 | 原因 |
|------|------|
| 异常（`throw`/`try`/`catch`） | 核心引擎禁用，见 §6；仅允许在模块边界与第三方库桥接处使用 |
| RTTI（`dynamic_cast`） | 性能与二进制体积；用 `RefPtr`/虚函数/passkey 替代 |
| 裸 `new` / `delete` | 用 `RefCounted`/`make` 工厂/`std::make_unique` |
| `goto` | — |
| C 风格转换 `(int)x` | 用 `static_cast<int>(x)` |
| 全局可变状态 | 用单例类 + 显式 `Initialize()` |
| 变长数组（VLA） | 非标准 |

### 2.4 谨慎使用的特性

| 特性 | 何时可用 |
|------|---------|
| 多继承 | 仅"一个实现 + 多个纯接口" |
| 模板元编程 | 简单 trait/CRTP 可用；禁止 Boost.MPL 级复杂度 |
| 宏 | 仅用于导出宏、日志、断言；禁止用宏定义常量（用 `constexpr`） |
| `friend` | 谨慎，优先用 passkey 惯用法（§7.4） |

---

## 3. 命名规范

### 3.1 总表

| 类别 | 规则 | 示例 |
|------|------|------|
| **类型**（class/struct/enum/typedef） | PascalCase | `RenderSystem`、`SchemaDb`、`DbResult` |
| **方法/函数** | 1:1 对齐参考语言（TS→camelCase；C++ 参考→PascalCase） | TS: `changeFrustum()`；C++: `CreateBox()` |
| **自由函数/静态方法** | 1:1 对齐参考语言（同上） | TS: `normalizedDifference()`；C++: `Create()` |
| **成员变量** | `m_` + camelCase | `m_refCount`、`m_filePath` |
| **静态成员变量** | `s_` + camelCase | `s_defaultTimeout` |
| **全局变量** | `g_` + camelCase（尽量不用） | `g_instance` |
| **局部变量** | camelCase | `result`、`tileCount` |
| **参数** | camelCase | `entityId`、`outStatus` |
| **常量**（`constexpr`/`static const`） | `k` + PascalCase | `kMaxTileDepth`、`kNullId` |
| **宏** | UPPER_SNAKE + 模块前缀 | `DQ_RENDER_EXPORT`、`DQ_ASSERT` |
| **命名空间** | PascalCase，单数 | `dqRender`、`dqBase` |
| **文件名** | PascalCase，`.h`/`.cpp` | `RenderSystem.h`、`RenderSystem.cpp` |

> **方法/函数名以 CLAUDE.md §3 为准**：移植代码方法/函数名 1:1 按参考语言，**不归一**（TS 移植→camelCase；C++ 参考→PascalCase）。上表 camelCase/PascalCase 仅描述参考语言风格；强制 PascalCase 归一在本代码库不可行（类型名碰撞 + 跨模块 virtual），见 CLAUDE.md §3.3 与 §12.6。

### 3.2 前缀约定

| 前缀 | 含义 | 示例 |
|------|------|------|
| `I` | 纯虚接口（所有方法都是 `= 0`） | `ITileDataProvider`、`IFeatureOverrideProvider` |
| `Dq` | dqBase 基础设施类型前缀 | `DqId`、`DqString`（dqBase 内部类型可省略） |
| 无前缀 | 具体类/struct | `RenderSystem`、`TileAdmin` |

### 3.3 枚举值命名

```cpp
// 普通 enum class：PascalCase
enum class DbResult : int {
    Success = 0,        // 成功值固定为 0
    NotFound,
    InvalidArgument,
    IoError,
};

// 标志位 enum class：PascalCase + 位运算
enum class OpenMode : uint8_t {
    None      = 0,
    ReadWrite = 1 << 0,
    ReadOnly  = 1 << 1,
    Create    = 1 << 2,
};
// 标志位用 DQ_ENABLE_BITMASK_OPERATORS(OpenMode) 启用 | & 运算符
```

### 3.4 方法命名动词约定

| 动词 | 语义 | 失败行为 |
|------|------|---------|
| `Get*()` | 取值，**找不到时抛出/返回错误** | 见 §6 |
| `TryGet*()` | 取值，**找不到返回 `std::optional`/空** | 不抛 |
| `Find*()` | 同 `TryGet`，返回指针/optional | 不抛 |
| `Is*()` / `Has*()` / `Can*()` | 布尔查询，`noexcept` | 不抛 |
| `Create*()` | 工厂方法，返回 `RefPtr<T>` 或 `Result` | 见 §6 |
| `Add*()` / `Remove*()` | 容器/集合修改 | 返回 `Result` |
| `Insert*()` / `Delete*()` | 持久化 CRUD | 返回 `Result` |

---

## 4. 头文件与模块组织

### 4.1 目录结构（每个模块）

```
dqXxx/                          # 模块根（dqBase/dqRender/...）
├── PublicAPI/dqXxx/            # 对外公开头文件（唯一被其他模块 #include 的目录）
│   ├── DqXxx.h                 # 模块总入口（include 本模块所有公开头）
│   ├── Export.h                # DQ_XXX_EXPORT 宏定义
│   ├── RenderSystem.h          # 一类一文件
│   └── ...
├── src/                        # 实现文件（不对外）
│   ├── RenderSystem.cpp
│   ├── detail/                 # 内部实现细节
│   │   ├── RenderSystemImpl.h
│   │   └── ...
│   └── DqXxxPch.h              # 预编译头（可选）
├── tests/                      # 单元测试
└── CMakeLists.txt
```

**规则：**
- **只有 `PublicAPI/` 下的头文件可被其他模块 `#include`**。
- 实现细节放 `src/detail/`，绝不暴露到公开目录。
- 一类一文件。紧密相关的小类型可合并（如 `Types.h` 汇总基础 typedef）。

### 4.2 include guard

**统一用 `#pragma once`**。

### 4.3 include 顺序

按 6 层分组，组间一个空行：

```cpp
// 1. 对应的头文件（仅 .cpp 中，放第一）
#include "RenderSystem.h"

// 2. 本模块内部头（用 ""）
#include "detail/RenderSystemImpl.h"

// 3. DanQing 其他模块公开头（用 <>，按依赖顺序）
#include <dqBase/RefCounted.h>
#include <dqBase/Result.h>

// 4. 第三方库（用 <>）
#include <sqlite3.h>

// 5. C++ 标准库（用 <>）
#include <vector>
#include <memory>

// 6. C 系统头（用 <>）
#include <cstdint>
```

### 4.4 公开头文件的类型限制（Qt-free）

引擎 SDK（dqBase/dqGeom/dqCommon/dqRender）必须 Qt-free（CLAUDE.md §8.2）。公开头文件（`PublicAPI/`）**直接使用 C++ 标准库类型**：

| 需求 | 用 STL 类型 | 不用 Qt |
|------|-----------|---------|
| 字符串 | `std::string` / `std::string_view`（或 dqBase `DqString`） | `QString` |
| 集合 | `std::vector` / `std::unordered_map` / `std::map` / `std::set` | `QVector`/`QHash`/`QMap`/`QSet` |
| 函数对象 | `std::function`（仅回调签名） | — |
| Optional | `std::optional<T>` | — |
| 变体 | `std::variant<T,E>` | — |

**允许的 STL 头**（公开头）：`<string>`、`<vector>`、`<array>`、`<map>`、`<unordered_map>`、`<set>`、`<cstdint>`、`<optional>`、`<variant>`、`<memory>`、`<functional>`、`<type_traits>`、`<utility>`、`<string_view>`、`<initializer_list>`、`<iterator>`、`<limits>`。

**禁止**：公开头出现任何 Qt 类型（`QString`/`QVector`/`QHash` 等）或 `<Q...>` 头。

**规则：**
- 返回集合 → 用 `std::vector<T>`。
- 传/返字符串 → 用 `std::string`。
- 回调参数 → 用 `std::function<...>`（类型签名）或纯虚接口。
- dqBase 提供 thin wrapper（`DqString`=`std::string`、`DqVector<T>`=`std::vector<T>`），对齐 imodel-native bvector/Utf8String 模式。

> **依据**：CLAUDE.md §8.2（SDK 独立性硬约束）。Qt 仅允许在 dqApp（应用层）。

### 4.5 前向声明

公开头文件**优先前向声明**，避免不必要的 `#include`：

```cpp
// RenderSystem.h
namespace dqRender {
    class RenderTarget;          // 前向声明
    class TileAdmin;
    struct RenderSystemProps;
}
```

### 4.6 热循环类型限制（性能关键路径）

dqApp 应用层可用 Qt；**引擎 SDK 性能热循环用 std/原始缓冲**。详见 `docs/性能分析-Qt与imodel基础层.md` §5。

在性能敏感的热循环（细分、布尔、BVH 遍历、大文件解析）中：

| 场景 | 用 | 禁用 |
|------|-----|------|
| 顶点/法线大数组 | `std::vector<T>` / `std::array` | — |
| 字节流解析（SAT/STEP/OBJ/JSON 大文件） | `std::string` / `std::string_view` | — |
| 需迭代器稳定性的哈希 | `std::unordered_map` | — |
| 小定长数组 | `std::array`（或栈预留小定长缓冲） | — |
| 值类型对象 | 普通 struct | — |

额外规则：
- `std::vector` 无 CoW，正常 range-for / `cbegin()/cend()`。
- 全局替换 malloc 为 jemalloc 或 mimalloc（CMake 链接）。

> dqApp 边界与编排层（属性系统、文档模型、UI）不受此限，正常用 Qt；引擎 SDK 公开头仍须 Qt-free（§4.4）。

---

## 5. 内存管理

### 5.1 三种所有权语义

| 语义 | 表达 | 用途 |
|------|------|------|
| **共享所有权** | `RefPtr<T>`（侵入式引用计数） | 多处持有、生命周期不明的对象 |
| **独占所有权** | `std::unique_ptr<T>` | 单一所有者，可转移不可共享 |
| **非拥有观察** | 裸 `T*` 或 `T&` | 借用，不参与生命周期管理 |

### 5.2 `RefCounted<T>` 基类

```cpp
// dqBase/PublicAPI/dqBase/RefCounted.h
namespace dqBase {

struct IRefCounted {
    virtual ~IRefCounted() = default;
    virtual uint32_t AddRef() const noexcept = 0;
    virtual uint32_t Release() const noexcept = 0;
};

// CRTP 基类：注入原子引用计数
template <typename Derived>
class RefCounted : public IRefCounted {
public:
    RefCounted() noexcept = default;
    uint32_t AddRef() const noexcept override;
    uint32_t Release() const noexcept override;
private:
    mutable std::atomic<uint32_t> m_refCount{0};
};

// 非侵入式智能指针
template <typename T>
class RefPtr {
public:
    RefPtr() noexcept = default;
    explicit RefPtr(T* p) noexcept;
    // ... 拷贝/移动/解引用
private:
    T* m_ptr = nullptr;
};

} // namespace dqBase
```

**规则：**
- 共享对象继承 `RefCounted<T>`（CRTP）或 `RefCountedBase`。
- 用 `RefPtr<T>` 持有，**禁止 `shared_ptr`**（侵入式计数跨 DLL 更安全，无控制块开销）。
- 引用计数用 `std::atomic`，`AddRef` 用 `memory_order_relaxed`，`Release` 用 `memory_order_acq_rel`。

### 5.3 智能指针写法

统一直接写 `RefPtr<T>` 和 `RefPtr<const Foo>`，**不生成 `FooPtr`/`FooCPtr` 这类 typedef**。

```cpp
// 好
RefPtr<RenderSystem> CreateRenderSystem(const RenderSystemProps&);

// 坏
// RenderSystemPtr CreateRenderSystem(...);
```

### 5.4 工厂与创建

- **禁止裸 `new`**：用静态工厂 `Create()` 返回 `RefPtr<T>`，或 `std::make_unique`。
- **禁止裸 `delete`**：`RefCounted` 对象由 `Release()` 自动释放；`unique_ptr` 自动析构。

```cpp
class RenderSystem : public RefCounted<RenderSystem> {
public:
    static RefPtr<RenderSystem> Create(const RenderSystemProps& props);
};
```

### 5.5 所有权与 ABI

跨 DLL 边界传递对象时：
- `RefPtr<T>` 安全（侵入式计数在对象内部）。
- `std::unique_ptr<T>` **不安全**（删除器类型跨 ABI 不兼容）——跨边界只用 `RefPtr<T>` 或值类型。
- 裸指针作为"观察"安全，但调用方必须保证对象存活。

---

## 6. 错误处理

### 6.1 核心机制：`Result<T,E>`

比状态码更类型安全（强制处理错误），比异常更可预测（无隐藏控制流）。

```cpp
// dqBase/PublicAPI/dqBase/Result.h
namespace dqBase {

template <typename T, typename E>
class Result {
public:
    Result(T value) noexcept;        // 成功构造
    Result(E error) noexcept;        // 失败构造
    bool IsOk() const noexcept;
    bool IsErr() const noexcept;
    const T& Value() const;          // IsErr 时 DQ_ASSERT 失败
    const E& Error() const;
    T ValueOr(T fallback) const noexcept;
    // ... Map/MapErr/AndThen 组合子
};

} // namespace dqBase
```

**使用规则：**

```cpp
// 返回 Result，强制调用方处理
Result<RefPtr<Db>, DbError> OpenDb(const DqFileName& path);

// 调用方
auto result = OpenDb(path);
if (result.IsErr()) {
    return result.Error();  // 错误传播
}
RefPtr<Db> db = result.Value();
```

### 6.2 错误类型设计

- 每个模块定义自己的错误枚举：`DbError`、`GeomError`、`RenderError`。
- 错误枚举值用 PascalCase，`Success`/`None` 固定为 `0`。
- 需要附带上下文时，错误类型用 struct 而非 enum：

```cpp
struct GeomError {
    GeomErrorCode code;       // 枚举码
    DqString message;         // 人类可读描述
    DqString detail;          // 可选诊断信息
};
```

### 6.3 何时可用 `std::optional` / `nullptr`

- `TryGet*()` 模式返回 `std::optional<T>` 或 `RefPtr<T>`（null 即失败，无需错误信息）。
- 仅当"不存在"是正常情况、且无需诊断信息时使用。

### 6.4 断言：`DQ_ASSERT`

```cpp
// Debug 编译期检查，Release 编译为空
DQ_ASSERT(index < m_size);
DQ_ASSERT(ptr != nullptr && "Document must be open");

// 不可恢复的致命错误（始终生效）
DQ_PANIC("Unreachable: unexpected enum value");
```

- 断言检查**不变量**（程序员的错误），不检查**预期错误**（用户输入、IO 失败）。
- 预期错误必须用 `Result` 返回，绝不用断言。

### 6.5 异常禁用

- 核心引擎代码（`dqRender`/`dqGeom`/`dqCommon`/`dqApp` 内部）**禁止 `throw`**。
- 仅在**模块边界**捕获第三方库异常（若未来引入会抛异常的第三方库），转换为 `Result`。
- 编译选项 `-fno-exceptions`（GCC/Clang）或 `/EHsc` + 限制（MSVC）。

---

## 7. API 设计模式

### 7.1 抽象基类 + 内部实现

公开头文件声明抽象类，具体实现放 `src/detail/`，工厂方法创建实例。

```cpp
// PublicAPI/dqRender/RenderSystem.h  —— 公开抽象接口
namespace dqRender {
class DQ_RENDER_EXPORT RenderSystem : public dqBase::RefCounted<RenderSystem> {
public:
    static RefPtr<RenderSystem> Create(const RenderSystemProps& props);
    virtual RefPtr<RenderTarget> CreateTarget(const TargetProps&) = 0;
    virtual void DoIdleWork() = 0;
    virtual ~RenderSystem() = default;
};
}

// src/detail/RenderSystemImpl.h  —— 内部具体实现
namespace dqRender::detail {
class OpenGLRenderSystem final : public RenderSystem {
public:
    RefPtr<RenderTarget> CreateTarget(const TargetProps&) override;
    void DoIdleWork() override;
private:
    OpenGLContext m_context;
};
}
```

**规则：**
- 公开类用抽象方法（`= 0`）或受保护构造函数（防止外部 `new`）。
- 具体实现类放 `detail::` 命名空间，标 `final`。
- 工厂方法返回抽象基类的 `RefPtr`。

### 7.2 Builder 模式

复杂对象构造用 Builder，fluent 链式调用：

```cpp
class RenderableManager {
public:
    class Builder {
    public:
        Builder& Geometry(size_t index, const GeometryPtr& geom) noexcept;
        Builder& Material(size_t index, const MaterialPtr& mat) noexcept;
        Builder& BoundingBox(const Range3d& bounds) noexcept;
        Builder& Culling(bool enabled) noexcept;
        Result<RenderablePtr, RenderError> Build(RenderSystem& engine) const;
    private:
        struct BuilderDetails* m_impl;   // pImpl 隐藏细节
    };
};

// 使用
auto result = RenderableManager::Builder()
    .Geometry(0, geom)
    .Material(0, mat)
    .Build(engine);
```

**何时用 Builder：** 构造参数 ≥4 个，或参数有可选/默认值，或需要验证步骤。

### 7.3 配置对象模式

简单工厂用 `Props`/`Options` struct 传参：

```cpp
struct RenderSystemProps {
    Backend backend = Backend::OpenGL;       // 默认值
    bool enableDebugLayer = false;
    uint32_t gpuMemoryLimitMB = 1024;
};

RefPtr<RenderSystem> RenderSystem::Create(const RenderSystemProps& props);
```

### 7.4 passkey 惯用法

防止外部代码实现内部接口或调用受限方法：

```cpp
class TileAdmin;  // 仅 TileAdmin 可构造 ImplementationKey

class ImplementationKey {
private:
    friend class TileAdmin;
    ImplementationKey() = default;
};

class ITileDataProvider {
public:
    virtual ~ITileDataProvider() = default;
    // 仅 TileAdmin 能调用此方法（必须传入它无法构造的 key）
    virtual void OnTileEvicted(ImplementationKey, TileId) = 0;
};
```

外部代码无法构造 `ImplementationKey`，因此无法调用 `OnTileEvicted`。

### 7.5 嵌套命名空间类

大型 API 表面用嵌套类分组：

```cpp
class SchemaDb : public Db {
public:
    class Models;      // 前向声明
    class Elements;
    Models& Models() { return m_models; }
private:
    SchemaDb::Models m_models{*this};
};

class SchemaDb::Models {
public:
    Result<ModelId, DbError> Insert(const ModelProps&);
    Result<Model, DbError> Get(ModelId);
private:
    friend class SchemaDb;
    explicit Models(SchemaDb& owner) : m_owner{owner} {}
    SchemaDb& m_owner;
};
```

### 7.6 类型化句柄

GPU 资源等轻量引用用类型化句柄而非指针：

```cpp
namespace dqRender {
struct HwTexture;   // 仅作类型标签，不实例化
struct HwProgram;
using TextureHandle = Handle<HwTexture>;
using ProgramHandle = Handle<HwProgram>;

struct HandleBase {
    using Id = uint32_t;
    static constexpr Id kNull = ~0u;
    constexpr HandleBase() noexcept = default;
    constexpr explicit operator bool() const noexcept { return m_id != kNull; }
    Id m_id = kNull;
};
template <typename T> struct Handle : HandleBase {};
}
```

句柄是 32 位平凡类型，零开销、类型安全（`TextureHandle` 与 `ProgramHandle` 编译期不兼容）。

---

## 8. 导出宏与 ABI

### 8.1 每模块一个导出宏

```cpp
// dqRender/PublicAPI/dqRender/Export.h
#if defined(DQ_RENDER_BUILDING)
    #define DQ_RENDER_EXPORT [[gnu::visibility("default")]]  // GCC/Clang
    // MSVC: __declspec(dllexport)
#else
    #define DQ_RENDER_EXPORT [[gnu::visibility("default")]]
    // MSVC: __declspec(dllimport)
#endif
```

**规则：**
- 构建该模块 DLL 时定义 `DQ_模块_BUILDING`。
- 每个模块的导出宏统一命名：`DQ_BASE_EXPORT`、`DQ_RENDER_EXPORT`、`DQ_DATA_EXPORT`、`DQ_GEOM_EXPORT`、`DQ_APP_EXPORT`。
- 导出宏标注在**类**或**方法声明**上；**优先标类**，整类导出更简洁。

### 8.2 ABI 版本

- 共享库 soname 含主版本：`libuerender.so.1`。
- Profile 版本字段（数据引擎必需）：`static constexpr ProfileVersion kCurrentProfile{1,0,0,0};`，支持就地升级。

### 8.3 五级稳定性标签

```
\public        稳定，破坏性变更走弃用流程
\beta          可用，但随时变更
\alpha         实验性，可能随时移除
\internal      仅模块内部使用，不进入安装头文件集
\deprecated    已弃用，注明版本、替代方案、移除日期
```

- CI 检查：未标注稳定性的公开符号构建失败。
- 仅 `\public`/`\beta` 进入发布 SDK 的 `PublicAPI/` 目录；`\internal` 留在 `src/`。
- 弃用示例：`[[deprecated("in 1.2, removed after 2027-06-01, use NewMethod()")]]`。

---

## 9. 代码格式

完整规则见仓库根 `.clang-format`。关键设置：

| 设置 | 值 | 说明 |
|------|-----|------|
| `IndentWidth` / `TabWidth` / `UseTab` | `4` / `4` / `Never` | 4 空格，禁用 Tab |
| `ColumnLimit` | `100` | 100 列限宽 |
| `BreakBeforeBraces` | `Custom`（全 `false`） | K&R 大括号，`{` 不换行 |
| `AccessModifierOffset` | `-4` | `public:` 顶格 |
| `PointerAlignment` | `Left` | `Type* name` |
| `NamespaceIndentation` | `None` | 命名空间内容不缩进 |
| `AllowShortFunctionsOnASingleLine` | `Empty` | 仅空函数体可单行 |
| `AlwaysBreakTemplateDeclarations` | `Yes` | `template<...>` 独占一行 |

**文件末尾必须有一个空行；禁止行尾空格。**

---

## 10. 注释与文档

### 10.1 文件头（强制）

每个 `.h`/`.cpp` 顶部：

```cpp
// SPDX-License-Identifier: <license>
// DanQing <模块名> — <一句话模块职责>
// Copyright (c) 2026 DanQing Contributors
```

### 10.2 Doxygen 文档注释

公开 API（`PublicAPI/`）**必须**有 Doxygen 注释；内部代码鼓励。

```cpp
/**
 * 创建渲染系统实例。
 *
 * \param props 创建参数，见 RenderSystemProps。
 * \return 成功返回 RefPtr<RenderSystem>；失败返回 RenderError。
 * \public
 *
 * \code
 * auto result = RenderSystem::Create({.backend = Backend::OpenGL});
 * \endcode
 */
Result<RefPtr<RenderSystem>, RenderError> Create(const RenderSystemProps& props);
```

- 类/方法用 `/** */` 块注释。
- 成员变量/枚举值用 `//!` 行注释。
- 参数标注方向：`\param[in]`、`\param[out]`、`\param[in,out]`。

### 10.3 实现注释

非显而易见的实现选择用 `//` 行注释，解释**为什么**（why），而非**是什么**（what）：

```cpp
// Release 用 acq_rel 而非 release：确保析构前所有先前写操作对其他线程可见
uint32_t AddRef() const noexcept override { ... }
```

---

## 11. 并发

### 11.1 线程安全标注

公开 API 必须用注释声明线程安全级别：

```cpp
/**
 * 线程安全：可从任意线程调用。
 * \public
 */
void DoIdleWork();

/**
 * 线程安全：仅限渲染线程。调用方必须保证单线程访问。
 * \public
 */
void DrawScene();
```

### 11.2 同步原语

- 用 dqBase 提供的原语：`Mutex`、`Atomic<T>`、`Event`，不直接用 `std::mutex`（跨 DLL ABI）。
- 标注 `[[ue::guarded_by(m_mutex)]]`，静态分析检查。

### 11.3 数据竞争

- 共享可变状态必须用 `Atomic` 或 `Mutex` 保护。
- 无锁数据结构仅限性能关键路径，且必须附正确性论证。

---

## 12. 测试

### 12.1 框架

- 单元测试用 **GoogleTest**。
- 测试文件放模块的 `tests/` 目录，命名 `XxxTest.cpp`。

### 12.2 测试移植规则（强制）

> **所有单元测试用例必须从 `itwinjs-core` 或 `imodel-native` 直接移植，不得自行编写。**

- **来源优先级**：
  1. `imodel-native/iModelCore/<模块>/Tests/NonPublished/*.cpp`（C++，直接移植）
  2. `itwinjs-core/core/<包>/src/test/*.test.ts`（TypeScript，翻译为 C++/GoogleTest）
- **可追溯**：每个 `TEST()` 顶部必须注释出处：
  ```cpp
  // Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
  //              TEST(RefCountedTests, Test2)
  TEST(RefCountedTest, SharedRefCountAcrossInterfaces) { ... }
  ```
- **不发明场景**：测试场景、边界条件、断言数据来自参考实现。dqBase API 与参考不同时，调整断言适配 dqBase 类型，但场景不变。
- **参考揭示缺失 API**：先移植测试（RED），再补 dqBase API（GREEN）。
- **例外**：仅当 itwinjs/imodel 都无对应测试（如 `Nullable`）才可自写，并注释 `// Authored: no reference test exists for <X>`。
- **TS→C++ 映射**：`describe/it` → `TEST(Suite, Case)`；`expect().to.throw(E)` → `EXPECT_THROW(fn, E)`；`@ts-expect-error` 块忽略。

### 12.3 测试要求

| 类型 | 要求 |
|------|------|
| 公开 API | 每个公开方法至少有正常路径 + 错误路径测试 |
| 几何引擎 | 关键算法输出逐位验证（SAT 比对） |
| 数据引擎 | 事务回滚、崩溃恢复、并发查询一致性测试 |
| 渲染引擎 | 帧率基准（60fps @ 100 万三角面）、GPU 内存上限测试 |
| 覆盖率 | 核心路径 100%，整体 ≥80% |

### 12.4 断言风格

```cpp
EXPECT_EQ(result.IsOk(), true);
EXPECT_EQ(db->GetElementCount(), 100u);
EXPECT_NEAR(point.x, 1.0, 1e-9);   // 浮点用 EXPECT_NEAR
```

---

## 13. 模块依赖与禁令

### 13.1 依赖方向（硬约束）

```
dqApp → { dqRender, dqCommon, dqGeom }
dqRender → { dqGeom, dqCommon } → dqBase
dqCommon → { dqBase, dqGeom }
dqGeom → dqBase
```

- 依赖只许向下，永不反向。
- 引擎间仅允许 `dqRender → dqGeom`（渲染经 `PolyfaceQuery` 只读接口消费几何；对应 imodel `DgnCore`→`GeomLibs`、itwinjs `core-frontend`→`core-geometry`）。
- 所有引擎依赖 `dqBase`。

> dqCommon ⊥ dqRender/dqApp；dqRender → dqGeom 是唯一允许的引擎间边。权威表述以 CLAUDE.md §8.1 为准。

### 13.2 跨模块禁止

- 禁止任何模块依赖 `dqApp`（宿主层在最上层）。
- **`dqRender → dqGeom` 是唯一允许的引擎间依赖**（向下；经 `PolyfaceQuery` 只读接口）；其余引擎组合禁止互相 `#include`。

### 13.3 第三方依赖

每个第三方依赖必须有：
1. `FetchContent` 配置（CMake）。
2. 许可证记录（与商用兼容）。
3. 封装层（不直接在业务代码暴露第三方类型）。

---

## 14. 命名空间

```cpp
namespace dqBase { ... }      // 共享基础
namespace dqRender { ... }    // 渲染引擎

namespace dqGeom { ... }      // 几何引擎
namespace dqApp { ... }       // 应用层
```

- 每个模块一个顶级命名空间，与模块名一致。
- 子命名空间用 PascalCase：`dqBase::detail`、`dqRender::webgl`（实现细节，不公开）。
- **禁止** `using namespace` 在头文件中（污染）。
- `.cpp` 文件顶部可 `using namespace dqBase;`（仅限实现文件）。

---

## 15. 检查清单（提交前自检）

- [ ] 编译无警告（`-Wall -Wextra -Werror`）
- [ ] `clang-format` 已应用（`git diff` 无格式变更）
- [ ] `clang-tidy` 无新告警
- [ ] 公开 API 有 Doxygen 注释 + 稳定性标签
- [ ] 新错误路径有测试覆盖
- [ ] 无裸 `new`/`delete`，所有权用 `RefPtr`/`unique_ptr`
- [ ] 无异常（`throw`）在核心引擎代码
- [ ] 无 `dynamic_cast`、无 C 风格转换
- [ ] 公开头文件不得出现 Qt 类型（`QString`/`QVector` 等）或 `<Q...>` 头
- [ ] 依赖方向正确（不引入横向或反向依赖）

---

## 附录：完整示例文件

```cpp
// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 渲染系统抽象
// Copyright (c) 2026 DanQing Contributors
#pragma once

#include <dqBase/RefCounted.h>
#include <dqBase/Result.h>
#include <cstdint>

#include "Export.h"

namespace dqRender {

class RenderTarget;
struct RenderSystemProps;

enum class Backend : uint8_t {
    OpenGL = 0,
    Vulkan = 1,
    Metal = 2,
};

enum class RenderError : int {
    Success = 0,
    InvalidBackend,
    GpuNotSupported,
    OutOfMemory,
};

class DQ_RENDER_EXPORT RenderSystem : public dqBase::RefCounted<RenderSystem> {
public:
    static dqBase::Result<dqBase::RefPtr<RenderSystem>, RenderError>
    Create(const RenderSystemProps& props);

    virtual dqBase::RefPtr<RenderTarget> CreateTarget() = 0;
    virtual bool DoIdleWork() noexcept = 0;
    virtual ~RenderSystem() = default;

protected:
    RenderSystem() = default;
};

} // namespace dqRender
```
