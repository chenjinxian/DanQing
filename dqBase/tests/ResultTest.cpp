// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
// dqBase tests — Result<T,E> 行为验证（TDD：先写测试）
#include <gtest/gtest.h>

#include <dqBase/Result.h>
#include <dqBase/DqStatus.h>

#include <string>

namespace {
struct TestError {
    int code;
    std::string message;
};

template<typename T>
dqBase::Result<T, TestError> Ok(T v) {
    return dqBase::Result<T, TestError>::Ok(std::move(v));
}
dqBase::Result<int, TestError> Err(TestError e) {
    return dqBase::Result<int, TestError>::Err(std::move(e));
}
} // namespace

// --- 基础状态 ---

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultTest, OkHoldsValue) {
    auto r = Ok(42);
    EXPECT_TRUE(r.IsOk());
    EXPECT_FALSE(r.IsErr());
    EXPECT_EQ(r.Value(), 42);
}

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultTest, ErrHoldsError) {
    auto r = Err({ 7, "boom" });
    EXPECT_TRUE(r.IsErr());
    EXPECT_FALSE(r.IsOk());
    EXPECT_EQ(r.Error().code, 7);
    EXPECT_EQ(r.Error().message, "boom");
}

// --- ValueOr ---

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultTest, ValueOrReturnsValueWhenOk) {
    auto r = Ok(42);
    EXPECT_EQ(r.ValueOr(99), 42);
}

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultTest, ValueOrReturnsFallbackWhenErr) {
    auto r = Err({ 1, "x" });
    EXPECT_EQ(r.ValueOr(99), 99);
}

// --- void 特化 ---

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultVoidTest, DefaultIsOk) {
    dqBase::Result<void, TestError> r;
    EXPECT_TRUE(r.IsOk());
    EXPECT_FALSE(r.IsErr());
}

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(ResultVoidTest, HoldsError) {
    dqBase::Result<void, TestError> r = dqBase::Result<void, TestError>::Err({ 3, "fail" });
    EXPECT_TRUE(r.IsErr());
    EXPECT_EQ(r.Error().code, 3);
}

// --- DqError 通用结构 ---

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(DqErrorTest, DefaultIsZeroCode) {
    dqBase::DqError e;
    EXPECT_EQ(e.code, 0);
    EXPECT_TRUE(e.message.empty());
}

// Authored: no reference test exists in itwinjs-core for Result monad (itwinjs uses exceptions)
TEST(DqErrorTest, ConstructsFromMembers) {
    dqBase::DqError e{ 42, "bad", "detail" };
    EXPECT_EQ(e.code, 42);
    EXPECT_EQ(e.message, "bad");
    EXPECT_EQ(e.detail, "detail");
}
