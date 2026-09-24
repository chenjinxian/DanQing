// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeTest.h
// DanQing dqBase — 测试框架基础设施
//
// 1:1 bounded port of imodel-native BeTest.h. DanQing 选用 GoogleTest 作为
// 测试框架（见 dqBase/tests/*Test.cpp 均使用 gtest 的 TEST()/EXPECT_*），
// 因此参考中与断言宏 / 失败处理 / 性能记录器相关的表面被 GoogleTest 原生
// 替代——这些符号以 `// Supplanted by GoogleTest:` 注释块显式标注（不静默）。
// 本头文件只移植未被替代、且具有独立工具价值的表面：
//   - Host（IRefCounted 测试环境宿主，ref:124-155）
//   - Initialize / Uninitialize / GetHost / IsInitialized（ref:158-198）
//   - SetRunningUnderGtest（ref:195，DanQing 字面意义上运行在 gtest 下）
//   - LogPriority 枚举 + Log（ref:286-299）
//   - BreakInDebugger（ref:284）
//   - Fail（ref:481，库代码无 gtest 链接时也可见的失败上报）
//   - 目录助手 GetOutputRoot/GetTempDir/GetDocumentsRoot（ref:146-154，
//     DanQing 适配为返回 DqString 而非 BeFileName& out-param，因 dqBase 无 Qt）
#pragma once

#include "Export.h"
#include "RefCounted.h"
#include "DqTypes.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeTest — 与测试宿主解耦的可移植单元测试工具集
// Ported from: imodel-native BeTest.h (struct BeTest, ref:108-483)
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeTest {
    // -----------------------------------------------------------------------
    // Host — 单元测试运行宿主接口（ref:124-155）
    // 对齐 ref 的 `struct Host : IRefCounted`。dqBase 的 IRefCounted 将
    // AddRef/Release 声明为纯虚，故 Host 改以 CRTP `RefCounted<Host>` 取得
    // 具体原子引用计数实现（见 dqBase/RefCounted.h:30-57），语义等价：
    // 仍可在继承层次中作为 IRefCounted 使用（RefCounted<Host> 派生自 IRefCounted）。
    // 宿主提供平台 up-call：函数调用代理 + 数据目录来源。
    // -----------------------------------------------------------------------
    struct DQ_BASE_EXPORT Host : RefCounted<Host> {
    protected:
        Host() = default;  // 隐藏引用计数类的构造（ref:127）

        // 平台 up-calls（ref:130-136）—— DanQing 保持纯虚，由具体宿主实现。
        virtual void* _InvokeP(char const* function, void* args) = 0;
        virtual void _GetDocumentsRoot(DqString& path) = 0;
        virtual void _GetOutputRoot(DqString& path) = 0;
        virtual void _GetTempDir(DqString& path) = 0;

    public:
        virtual ~Host() = default;

        //! 调用原生测试宿主中的函数（ref:143）
        //! @param function 函数标识
        //! @param args      函数参数
        //! @return 平台相关返回指针
        void* InvokeP(char const* function, void* args = nullptr);

        //! 单元测试可找到 BIM 及其它数据文件的目录（ref:146）
        void GetDocumentsRoot(DqString& path);
        //! 单元测试写入临时数据文件的目录（ref:150）
        void GetOutputRoot(DqString& path);
        //! 单元测试可存放纯临时文件的目录（ref:154）
        //! @note 不要在此目录存放大量文件；测试结束时会清理。
        void GetTempDir(DqString& path);
    };

    // -----------------------------------------------------------------------
    // 生命周期（ref:158-198）
    // -----------------------------------------------------------------------

    //! 查询 BeTest 是否处于已初始化（运行中）状态（ref:158）
    static bool IsInitialized();

    //! 获取测试宿主（ref:161）。未 Initialize 时返回内置默认单例宿主，
    //! 保证 GetHost() 永不返回 null——与 ref 行为一致（Initialize 仅替换宿主）。
    static Host& GetHost();

    //! 测试主程序入口调用一次（ref:170）。
    //! @param host 测试运行宿主；生命周期由调用方管理（BeTest 不持有所有权）。
    static void Initialize(Host& host);

    //! 标记 BeTest 已切换至 gtest 之下运行（ref:195）。
    //! DanQing 字面意义上运行在 GoogleTest 中，该函数保留并翻转内部 gtest 标志。
    static void SetRunningUnderGtest();

    //! 所有测试结束后调用（ref:198）。
    static void Uninitialize();

    // -----------------------------------------------------------------------
    // 目录助手（DanQing 直接返回 DqString 的便捷静态版本，
    // 对齐 ref 中通过 Host up-call 获取的同一组目录：ref:146-154）
    // 早期 DanQing 已提供此便捷层，并被 B1p1RemainingTest 等用例使用，故保留。
    // -----------------------------------------------------------------------
    static DqString GetOutputRoot();
    static DqString GetTempDir();
    static DqString GetDocumentsRoot();

    // -----------------------------------------------------------------------
    // 调试中断（ref:284）
    // -----------------------------------------------------------------------
    //! 触发调试器断点。ref 说明：Android 上仅打印日志并自旋等待 ndk-gdb 附加。
    static void BreakInDebugger(char const* msg1 = "BeTest", char const* msg2 = "Break!");

    // -----------------------------------------------------------------------
    // 日志优先级 + Log（ref:286-299）
    // -----------------------------------------------------------------------
    enum LogPriority {
        PRIORITY_FATAL   = 0,   //!< 致命错误，将终止应用（ref:288）
        PRIORITY_ERROR   = -1,  //!< 一般错误（ref:289）
        PRIORITY_WARNING = -2,  //!< 一般警告（ref:290）
        PRIORITY_INFO    = -3,  //!< 一般信息（ref:291）
        PRIORITY_TRACE   = -4,  //!< 函数调用级跟踪（ref:292）
    };

    //! 显示一条日志消息（ref:299）
    //! @param category 消息类别 / 命名空间
    //! @param message  消息正文
    //! @param priority 优先级 / 严重性
    static void Log(char const* category, LogPriority priority, char const* message);

    // -----------------------------------------------------------------------
    // 库代码失败上报（ref:481）
    // -----------------------------------------------------------------------
    //! 标记当前测试失败。未链接 gtest 的库代码也可调用（ref:481）。
    static void Fail(char const* msg = "");

    // =======================================================================
    // 以下参考表面被 GoogleTest 原生替代，不予移植——明确标注，非静默省略。
    // （DanQing 全量使用 gtest 的 TEST()/EXPECT_*/ASSERT_*；引入第二套断言/
    //  失败处理/性能记录框架会形成竞争性测试基础设施，违反单一框架原则。）
    // =======================================================================

    // Supplanted by GoogleTest: imodel-native BeTest.h:187-192 IFailureHandler
    //   —— gtest 的事件监听器（::testing::EventListener / 失败处理）替代。

    // Supplanted by GoogleTest: imodel-native BeTest.h:178-183 TestCaseInfo +
    //   T_SetUpFunc/T_TearDownFunc (ref:175-176) + RegisterTestCase/SetUpTestCase/
    //   TearDownTestCase (ref:309-311) —— gtest 的 TEST()/TEST_F() 与 SetUpTestSuite/
    //   TearDownTestSuite 替代测试用例注册与 setup/teardown。

    // Supplanted by GoogleTest: imodel-native BeTest.h:200-231 assertion-failure
    //   控制面（SetFailOnAssert / GetFailOnAssert / SetFailOnInvalidParameterAssert /
    //   GetAssertionFailed / T_BeAssertListener / SetBeAssertListener / setS_mainThreadId）
    //   —— gtest 的断言失败语义与 ASSERT_DEATH*/EXPECT_DEATH*、
    //   ::testing::Test::HasFailure() 替代。

    // Supplanted by GoogleTest: imodel-native BeTest.h:233-281 当前测试名查询
    //   (GetNameOfCurrentTest / GetNameOfCurrentTestCase + Internal 变体 +
    //   SetNameOfCurrentTestInternal) —— gtest 的
    //   ::testing::UnitTest::GetInstance()->current_test_info() 替代
    //   （参考本身在 USE_GTEST 分支也调用该 gtest API，ref:242/261）。

    // Supplanted by GoogleTest: imodel-native BeTest.h:333-378 BE_TEST_* 与
    //   EXPECT_*/ASSERT_* 宏族 —— gtest 同名宏原生提供。

    // Supplanted by GoogleTest: imodel-native BeTest.h:385-405 ExpectedResult ——
    //   gtest 的 ::testing::AssertionResult 替代。

    // Supplanted by GoogleTest: imodel-native BeTest.h:408-430 EqNear/EqTol/EqStr/
    //   ClearErrorCount/IncrementErrorCount/GetErrorCount/RecordFailedTest/
    //   GetFailedTests/SetBreakOnFailure/GetBreakOnFailure —— gtest 的
    //   EXPECT_NEAR/ASSERT_NEAR、::testing::Test::HasFailure()、
    //   ::testing::UnitTest::GetInstance()->FailedTestCount() 等替代。

    // Supplanted by GoogleTest: imodel-native BeTest.h:441-455
    //   IncrementErrorCountAndEnableThrows / RethrowAssertFromOtherTreads /
    //   SetFailureJmpbuf —— 非 gtest 自定义运行器专用，DanQing 不使用。

    // Supplanted by GoogleTest: imodel-native BeTest.h:459-478 非 gtest 分支的
    //   测试列表解析（ReadTestList/LoadTestList/LoadFilters）+ 断言失败处理
    //   （T_AssertionFailureHandler/SetAssertionFailureHandler）—— gtest 的
    //   --gtest_filter= 与事件监听器替代。

    // Supplanted by GoogleTest: imodel-native BeTest.h:26 BE_TEST_BREAK_IN_DEBUGGER
    //   宏 —— 直接使用 BeTest::BreakInDebugger()。

    // Supplanted by GoogleTest: imodel-native BeTest.h:491-496 LOGTODB/LOGPERFDB/
    //   PERFORMANCELOG/TEST_FIXTURE_NAME/TEST_NAME/TEST_DETAILS 性能记录宏 ——
    //   DanQing 无 PerformanceResultRecorder 基础设施（见下），且 gtest 的
    //   ::testing::Test::GetCurrentTestTypeInfo() 提供测试名上下文。

    // Supplanted by GoogleTest: imodel-native BeTest.h:502-512
    //   PerformanceResultRecorder（WriteResults/WriteResultsPerf）——
    //   DanQing 暂无性能 CSV 记录需求；如未来需要，应作为独立模块单独移植，
    //   不与断言/失败处理测试框架耦合。

    // Supplanted by GoogleTest: imodel-native BeTest.h:514-522 EXPECT_CONTAINS/
    //   EXPECT_NCONTAIN/EXPECT_BETWEEN 便利宏 —— DanQing 直接使用 gtest 原生
    //   EXPECT_THAT(container, ::testing::Contains(val)) 与组合断言替代。
};

END_DQ_BASE_NAMESPACE
