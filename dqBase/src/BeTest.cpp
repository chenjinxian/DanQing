// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTest.h
//              + imodel-native BeTest.cpp（Host::InvokeP / GetDocumentsRoot /
//              GetOutputRoot / GetTempDir 转发至 _InvokeP / _GetXxxRoot 实现）
// DanQing dqBase — 测试工具实现
//
// 实现策略：
//   - BeTest 持有一个 Host 引用（Initialize 替换，Uninitialize 还原默认）。
//   - 默认 Host 是内置单例 DefaultHost，提供 dqBase 已有的目录默认值
//     （/tmp/dqBIM_test_output 等），使 GetHost() 永不返回 null。
//   - 与 ref 行为一致：Host 生命周期由调用方管理，BeTest 仅持裸指针。
#include "dqBase/BeTest.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <csignal>
    #include <unistd.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

namespace {
// ---------------------------------------------------------------------------
// DefaultHost —— 内置默认测试宿主（DanQing 自有，无对应 ref 单例）
// 提供 GetHost() 在 Initialize 之前的非 null 兜底，并复用原有目录默认值。
// 与 ref 行为一致：测试代码可在任意时刻调用 BeTest::GetHost() 而不崩溃。
// ---------------------------------------------------------------------------
class DefaultHost : public BeTest::Host {
public:
    // 静态单例由静态存储期管理（非堆分配），故禁用引用计数驱动的析构：
    // AddRef/Release 改为 no-op。这避免了 Release 归零后 `delete this`
    // 试图释放非堆对象（UB）。RefCount() 仍继承自 RefCounted<Host>（非虚，
    // 读原子计数），保持基类接口不变。
    void AddRef() const noexcept override {}
    void Release() const noexcept override {}

protected:
    void* _InvokeP(char const* /*function*/, void* /*args*/) override {
        // 默认宿主不提供任何平台 up-call；返回 nullptr 表示未实现。
        return nullptr;
    }
    void _GetDocumentsRoot(DqString& path) override { path = "/tmp/dqBIM_test_documents"; }
    void _GetOutputRoot(DqString& path) override { path = "/tmp/dqBIM_test_output"; }
    void _GetTempDir(DqString& path) override {
#if defined(_WIN32)
        char buf[MAX_PATH];
        GetTempPathA(MAX_PATH, buf);
        path = DqString(buf);
#else
        path = "/tmp";
#endif
    }
};

DefaultHost& DefaultHostInstance() {
    static DefaultHost s_instance;
    return s_instance;
}

// 全局状态（与 ref BeTest.cpp 中的 s_host / s_isInitialized 等静态字段对齐）
BeTest::Host* s_host = nullptr;
bool s_isInitialized = false;
bool s_runningUnderGtest = false;
}  // namespace

// ---------------------------------------------------------------------------
// Host 公共方法（转发至受保护的纯虚 _InvokeP / _GetXxx，ref:143-154）
// ---------------------------------------------------------------------------
void* BeTest::Host::InvokeP(char const* function, void* args) {
    return _InvokeP(function, args);
}

void BeTest::Host::GetDocumentsRoot(DqString& path) {
    _GetDocumentsRoot(path);
}

void BeTest::Host::GetOutputRoot(DqString& path) {
    _GetOutputRoot(path);
}

void BeTest::Host::GetTempDir(DqString& path) {
    _GetTempDir(path);
}

// ---------------------------------------------------------------------------
// 生命周期（ref:158-198）
// ---------------------------------------------------------------------------
bool BeTest::IsInitialized() {
    return s_isInitialized;
}

BeTest::Host& BeTest::GetHost() {
    // 永不返回 null：未 Initialize 时返回内置默认宿主（对齐 ref 行为）。
    return s_host ? *s_host : DefaultHostInstance();
}

void BeTest::Initialize(Host& host) {
    s_host = &host;
    s_isInitialized = true;
}

void BeTest::SetRunningUnderGtest() {
    s_runningUnderGtest = true;
}

void BeTest::Uninitialize() {
    s_isInitialized = false;
    s_host = nullptr;
    // 注意：不还原 s_runningUnderGtest——gtest 不会"退出"。
}

// ---------------------------------------------------------------------------
// 目录助手静态便捷层（早期 DanQing 已提供，从默认宿主派生以保持单一来源）
// ---------------------------------------------------------------------------
DqString BeTest::GetOutputRoot() {
    DqString path;
    GetHost().GetOutputRoot(path);
    return path;
}

DqString BeTest::GetTempDir() {
    DqString path;
    GetHost().GetTempDir(path);
    return path;
}

DqString BeTest::GetDocumentsRoot() {
    DqString path;
    GetHost().GetDocumentsRoot(path);
    return path;
}

// ---------------------------------------------------------------------------
// 调试中断（ref:284）
// ---------------------------------------------------------------------------
void BeTest::BreakInDebugger(char const* /*msg1*/, char const* /*msg2*/) {
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(_WIN32)
    DebugBreak();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    __builtin_trap();
#else
    std::raise(SIGTRAP);
#endif
}

// ---------------------------------------------------------------------------
// 日志（ref:299）
// DanQing 的日志基础设施是 Logger（见 dqBase/Logger.h）。BeTest::Log 在此做
// 最小落地：写入 stderr，并带上 category/priority 前缀。
// 不直接依赖 Logger，以避免循环（Logger 可能反向使用 BeTest 测试工具）。
// ---------------------------------------------------------------------------
void BeTest::Log(char const* category, LogPriority priority, char const* message) {
    char const* prioStr = "?";
    switch (priority) {
        case PRIORITY_FATAL:   prioStr = "FATAL";   break;
        case PRIORITY_ERROR:   prioStr = "ERROR";   break;
        case PRIORITY_WARNING: prioStr = "WARNING"; break;
        case PRIORITY_INFO:    prioStr = "INFO";    break;
        case PRIORITY_TRACE:   prioStr = "TRACE";   break;
    }
    if (category == nullptr) category = "";
    if (message == nullptr) message = "";
    std::fprintf(stderr, "[BeTest][%s][%s] %s\n", prioStr, category, message);
}

// ---------------------------------------------------------------------------
// 库代码失败上报（ref:481）
// 无 gtest 链接的库代码也可调用：落地为 stderr 写入。
// ---------------------------------------------------------------------------
void BeTest::Fail(char const* msg) {
    if (msg == nullptr) msg = "";
    std::fprintf(stderr, "[BeTest][FAIL] %s\n", msg);
}

END_DQ_BASE_NAMESPACE
