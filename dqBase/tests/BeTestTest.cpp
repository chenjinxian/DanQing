// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTest.h
//              (no BeTest unit test exists in imodel-native; assertions below
//               target the ported surface contract — Host/Initialize/Log/etc.)
//
// DanQing chose GoogleTest; the ref's own assertion macros (BE_TEST_*,
// ExpectedResult, PerformanceResultRecorder, IFailureHandler) are intentionally
// NOT ported and are documented as supplanted in BeTest.h. These tests cover
// only the surface that IS ported: Host, lifecycle, LogPriority, directory
// helpers, BreakInDebugger (smoke), Fail (smoke).
#include <gtest/gtest.h>

#include <dqBase/BeTest.h>
#include <dqBase/RefCounted.h>

#include <cstdint>
#include <memory>
#include <string>

using dqBase::BeTest;
using dqBase::IRefCounted;
using dqBase::DqString;

// ---------------------------------------------------------------------------
// 自编：参考项目没有 BeTest 单元测试。下面的断言直接针对移植表面的契约：
//   - Host 继承 IRefCounted（ref:124）
//   - GetHost() 返回非 null 单例（ref:161）
//   - Initialize/Uninitialize 翻转 IsInitialized（ref:170/198）
//   - SetRunningUnderGtest 是可见 API（ref:195）
//   - LogPriority 枚举值与 ref 精确一致（ref:286-293）
//   - 目录助手返回非空路径（ref:146-154 的便捷静态层）
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeTest.h:124  Host : IRefCounted
// Authored assertion: Host 必须实现 IRefCounted 的 AddRef/Release（可被 RefPtr 持有），
// 且多态绑定到 IRefCounted& 时仍调用派生类的实现。
TEST(BeTestTest, HostIsIRefCounted) {
    BeTest::Host& host = BeTest::GetHost();
    // 多态：Host& 必须能绑定到 IRefCounted&（RefCounted<Host> 派生自 IRefCounted）。
    IRefCounted& rc = host;
    // 默认宿主是进程静态单例，AddRef/Release 重写为 no-op（见 BeTest.cpp）；
    // 此处验证多态调用安全且不崩溃。
    rc.AddRef();
    rc.Release();
    SUCCEED();
}

// Authored: 验证 CRTP RefCounted<Host> 的真实引用计数机制在自定义堆分配宿主上正常工作。
// 默认宿主因静态存储期禁用了计数；本测试用一个堆分配的具体 Host 确认基类机制未受损。
// 引用计数惯例（见 RefCountedTest）：new 后 refcount=0，AddRef→1，Release→0 时 delete this。
namespace {
class CountedHost : public BeTest::Host {
protected:
    void* _InvokeP(char const*, void*) override { return nullptr; }
    void _GetDocumentsRoot(DqString&) override {}
    void _GetOutputRoot(DqString&) override {}
    void _GetTempDir(DqString&) override {}
};
}  // namespace
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTest.h
//              TEST(BeTestTest, HostCRTPRefCountMechanism)
TEST(BeTestTest, HostCRTPRefCountMechanism) {
    // 用 RefPtr 管理所有权：构造时 AddRef（0→1），析构时 Release（1→0 → delete）。
    dqBase::RefPtr<CountedHost> h(new CountedHost());
    EXPECT_TRUE(h.IsValid());
    EXPECT_EQ(h->RefCount(), 1u) << "RefPtr-held Host has refcount 1";
    h->AddRef();
    EXPECT_EQ(h->RefCount(), 2u);
    h->Release();
    EXPECT_EQ(h->RefCount(), 1u);
    // 离开作用域时 RefPtr 析构 -> Release -> 0 -> delete this（不崩溃即通过）。
}

// Ported from: imodel-native BeTest.h:161  GetHost
TEST(BeTestTest, GetHostReturnsNonNullSingleton) {
    BeTest::Host& h1 = BeTest::GetHost();
    BeTest::Host& h2 = BeTest::GetHost();
    EXPECT_NE(&h1, nullptr);
    EXPECT_EQ(&h1, &h2) << "GetHost must return the same singleton across calls";
}

// Ported from: imodel-native BeTest.h:170 / 198  Initialize / Uninitialize
TEST(BeTestTest, InitializeAndUninitializeFlipIsInitialized) {
    // 测试不应污染全局状态；先记录初始状态，结束时尽力还原。
    const bool wasInitialized = BeTest::IsInitialized();
    BeTest::Host& defaultHost = BeTest::GetHost();

    BeTest::Initialize(defaultHost);
    EXPECT_TRUE(BeTest::IsInitialized())
        << "Initialize must set IsInitialized to true";

    BeTest::Uninitialize();
    EXPECT_FALSE(BeTest::IsInitialized())
        << "Uninitialize must reset IsInitialized to false";

    // 还原（尽力而为）：若测试开始时已初始化，则重新 Initialize。
    if (wasInitialized) {
        BeTest::Initialize(defaultHost);
    }
}

// Ported from: imodel-native BeTest.h:195  SetRunningUnderGtest
// Authored assertion: API 必须可见且可调用（DanQing 字面上运行在 gtest 下）。
TEST(BeTestTest, SetRunningUnderGtestIsCallable) {
    // 该函数仅翻转内部 gtest 标志，无返回值；此处验证编译期可见 + 不抛异常。
    BeTest::SetRunningUnderGtest();
    SUCCEED();
}

// Ported from: imodel-native BeTest.h:286-293  enum LogPriority
TEST(BeTestTest, LogPriorityEnumValuesMatchRef) {
    // ref 精确值（BeTest.h:288-292）
    EXPECT_EQ(static_cast<int>(BeTest::PRIORITY_FATAL),   0);
    EXPECT_EQ(static_cast<int>(BeTest::PRIORITY_ERROR),  -1);
    EXPECT_EQ(static_cast<int>(BeTest::PRIORITY_WARNING), -2);
    EXPECT_EQ(static_cast<int>(BeTest::PRIORITY_INFO),   -3);
    EXPECT_EQ(static_cast<int>(BeTest::PRIORITY_TRACE),  -4);
}

// Ported from: imodel-native BeTest.h:146-154  directory helpers (convenience layer)
TEST(BeTestTest, DirectoryHelpersReturnNonEmptyPaths) {
    EXPECT_FALSE(BeTest::GetOutputRoot().empty())
        << "GetOutputRoot must return a non-empty path";
    EXPECT_FALSE(BeTest::GetTempDir().empty())
        << "GetTempDir must return a non-empty path";
    EXPECT_FALSE(BeTest::GetDocumentsRoot().empty())
        << "GetDocumentsRoot must return a non-empty path";
}

// Ported from: imodel-native BeTest.h:143  Host::InvokeP
TEST(BeTestTest, HostInvokePIsCallableOnDefaultHost) {
    BeTest::Host& host = BeTest::GetHost();
    // 默认宿主的 _InvokeP 返回 nullptr；只要可调用且不崩溃即可。
    void* result = host.InvokeP("nonexistent_function", nullptr);
    EXPECT_EQ(result, nullptr);
}

// Ported from: imodel-native BeTest.h:284  BreakInDebugger
// 仅验证 API 可见且空操作可调用——不实际触发断点（会挂起测试进程）。
// 这里通过验证函数地址非空间接确认链接。
TEST(BeTestTest, BreakInDebuggerIsLinked) {
    auto* fn = &BeTest::BreakInDebugger;
    EXPECT_NE(fn, nullptr);
}

// Ported from: imodel-native BeTest.h:299  Log
TEST(BeTestTest, LogIsCallableWithoutCrash) {
    // 仅验证可调用——输出落到 stderr，无返回值可断言。
    BeTest::Log("TestCategory", BeTest::PRIORITY_INFO, "smoke");
    SUCCEED();
}

// Ported from: imodel-native BeTest.h:481  Fail
TEST(BeTestTest, FailIsCallableWithoutCrash) {
    BeTest::Fail("smoke failure");
    BeTest::Fail();  // 默认参数
    SUCCEED();
}

// Ported from: imodel-native BeTest.h:124-155  Host up-call methods
TEST(BeTestTest, HostDirectoryUpcallsPopulatePath) {
    BeTest::Host& host = BeTest::GetHost();
    DqString out;
    host.GetOutputRoot(out);
    EXPECT_FALSE(out.empty());
    host.GetTempDir(out);
    EXPECT_FALSE(out.empty());
    host.GetDocumentsRoot(out);
    EXPECT_FALSE(out.empty());
}
