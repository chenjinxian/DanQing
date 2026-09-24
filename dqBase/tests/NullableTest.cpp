// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in imodel-native for Nullable<T>
// dqBase tests — Nullable<T> 行为验证
//
// imodel-native 中 Nullable<T> 是一个公开头（Bentley/Nullable.h）但无对应的单元测试文件
// （Bentley/Tests/ 下没有 Nullable_test.cpp）。按 §4 标注为 Authored，断言与语义对照
// 参考实现的公开 API（get() nullptr-when-null、BeAssert(IsValid()) on operator->/Value）。
#include <gtest/gtest.h>

#include <dqBase/Nullable.h>
#include <dqBase/BeAssert.h>

#include <string>
#include <utility>

using namespace dqBase;

namespace {
// 捕获 DqAssert 失败的 RAII guard —— BeAssertFunctions::SetBeTestAssertHandler 可注入。
// (renamed SetTestAssertHandler → SetBeTestAssertHandler per ref BeAssert.h:24)
class AssertCapture {
public:
    AssertCapture() {
        lastExpr_ = nullptr;
        lastFile_ = nullptr;
        lastLine_ = 0;
        fired_ = false;
        BeAssertFunctions::SetBeTestAssertHandler(
            [](const char* expr, const char* file, unsigned line, BeAssertFunctions::AssertType) {
                lastExpr_ = expr;
                lastFile_ = file;
                lastLine_ = line;
                ++fired_;
            });
    }
    ~AssertCapture() {
        BeAssertFunctions::SetBeTestAssertHandler(nullptr);
    }
    static int firedCount() { return fired_; }
    static const char* lastExpr() { return lastExpr_; }

private:
    static int fired_;
    static const char* lastExpr_;
    static const char* lastFile_;
    static unsigned lastLine_;
};
int AssertCapture::fired_ = 0;
const char* AssertCapture::lastExpr_ = nullptr;
const char* AssertCapture::lastFile_ = nullptr;
unsigned AssertCapture::lastLine_ = 0;
} // namespace

// --- 默认构造为 null ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, DefaultConstructedIsNull) {
    Nullable<int> n;
    EXPECT_TRUE(n.IsNull());
    EXPECT_FALSE(n.IsValid());
    EXPECT_FALSE(static_cast<bool>(n));
    EXPECT_TRUE(n == nullptr);
}

// --- nullptr 构造为 null ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, NullptrConstructedIsNull) {
    Nullable<int> n(nullptr);
    EXPECT_TRUE(n.IsNull());
    EXPECT_FALSE(n.IsValid());
}

// --- 值构造与 Value() ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueConstructionHoldsValue) {
    Nullable<int> n(42);
    EXPECT_FALSE(n.IsNull());
    EXPECT_TRUE(n.IsValid());
    EXPECT_EQ(n.Value(), 42);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueROnValidNullable) {
    Nullable<int> n(7);
    n.ValueR() = 99;
    EXPECT_EQ(n.Value(), 99);
}

// --- operator*() / operator->() ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, DereferenceValidNullable) {
    Nullable<int> n(42);
    EXPECT_EQ(*n, 42);
    *n = 17;
    EXPECT_EQ(n.Value(), 17);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ArrowOnValidNullable) {
    Nullable<std::string> s(std::string("hello"));
    EXPECT_EQ(s->size(), 5u);
    s->append(" world");
    EXPECT_EQ(s->size(), 11u);
}

// --- 赋值 ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, AssignValueMakesValid) {
    Nullable<int> n;
    n = 5;
    EXPECT_TRUE(n.IsValid());
    EXPECT_EQ(n.Value(), 5);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, AssignNullptrMakesNull) {
    Nullable<int> n(42);
    n = nullptr;
    EXPECT_TRUE(n.IsNull());
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, AssignMoveValue) {
    Nullable<std::string> n;
    std::string src("payload");
    n = std::move(src);
    EXPECT_EQ(n.Value(), "payload");
}

// --- 比较 ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, EqualityBothNull) {
    Nullable<int> a, b;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, EqualityBothValid) {
    Nullable<int> a(1), b(1), c(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, EqualityMixedNullness) {
    Nullable<int> a, b(1);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, CompareWithNullptr) {
    Nullable<int> a, b(1);
    EXPECT_TRUE(a == nullptr);
    EXPECT_FALSE(b == nullptr);
    EXPECT_TRUE(b != nullptr);
}

// --- ValueOr()（DanQing 扩展，参考实现无；audit tolerates per §6） ---

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueOrReturnsValueWhenValid) {
    Nullable<int> n(7);
    EXPECT_EQ(n.ValueOr(0), 7);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueOrReturnsFallbackWhenNull) {
    Nullable<int> n;
    EXPECT_EQ(n.ValueOr(99), 99);
}

// =================================================================
// Reference-alignment surfaces (P2 #41) — get() + IsValid() assert.
// These cover the drifts called out by the audit.
// =================================================================

// --- get() overload: returns nullptr when null, valid pointer otherwise ---

// Authored: aligns Nullable::get() to imodel-native Bentley/Nullable.h.
// Reference: T const* get() const { return IsValid() ? &m_value : nullptr; }
TEST(NullableTest, GetOnNullReturnsNullptr) {
    Nullable<int> n;
    EXPECT_EQ(n.get(), nullptr);
    const Nullable<int> cn;
    EXPECT_EQ(cn.get(), nullptr);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, GetOnValidReturnsUsablePointer) {
    Nullable<int> n(42);
    int* p = n.get();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
    *p = 100;
    EXPECT_EQ(n.Value(), 100);

    const Nullable<int> cn(7);
    const int* cp = cn.get();
    ASSERT_NE(cp, nullptr);
    EXPECT_EQ(*cp, 7);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, GetReturnsNullptrAfterNullptrAssignment) {
    Nullable<int> n(5);
    EXPECT_NE(n.get(), nullptr);
    n = nullptr;
    EXPECT_EQ(n.get(), nullptr);
}

// --- DqAssert(IsValid()) on operator->/Value/ValueR ---
//
// Authored: aligns Nullable<T> operator-> / Value() / ValueR() to imodel-native which
// calls BeAssert(IsValid()) before returning m_value. DanQing maps BeAssert -> DqAssert,
// which is a no-op under NDEBUG (Release). We therefore:
//   (a) assert the API shape (Value/ValueR/operator-> compile and round-trip) — always
//       runnable;
//   (b) only assert the assert-fires behaviour when NDEBUG is NOT defined, matching how
//       imodel-native's BeAssert behaves under release builds.
#ifdef NDEBUG
// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, AssertPathDisabledInRelease) {
    // 在 Release 构建下 DqAssert 编译为空；此测试仅占位，确认链接 OK。
    SUCCEED();
}
#else
// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueOnNullFiresAssert) {
    AssertCapture cap;
    Nullable<int> n;
    (void)n.Value();
    EXPECT_GE(AssertCapture::firedCount(), 1);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValueROnNullFiresAssert) {
    AssertCapture cap;
    Nullable<int> n;
    (void)n.ValueR();
    EXPECT_GE(AssertCapture::firedCount(), 1);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ArrowOnNullFiresAssert) {
    AssertCapture cap;
    Nullable<std::string> n;
    (void)n->size(); // operator-> on null should fire DqAssert(IsValid())
    EXPECT_GE(AssertCapture::firedCount(), 1);
}

// Authored: no reference test exists in imodel-native for Nullable<T>
TEST(NullableTest, ValidNullableDoesNotFireAssert) {
    AssertCapture cap;
    Nullable<int> n(42);
    (void)n.Value();
    (void)n.ValueR();
    (void)n.operator->(); // operator-> on valid Nullable returns a usable ptr
    EXPECT_EQ(AssertCapture::firedCount(), 0);
}
#endif
