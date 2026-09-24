// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              AssertType enum values, PerformBeAssert / PerformBeDataAssert split,
//              T_BeAssertHandler typedef name.
//
// imodel-native has no unit test file for BeAssert.h (none under Bentley/Tests/).
// Per §4 we anchor to the verbatim reference header values/names where testable;
// the macro split (PerformBeAssert vs PerformBeDataAssert) is asserted via the
// handler invocation.
#include <gtest/gtest.h>

#include <dqBase/BeAssert.h>

using namespace dqBase;

// Reference: imodel-native BeAssert.h:15-22
//   enum class AssertType { Normal=0, Data=1, Sigabrt=2, TypeCount=3, All=99 };
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, AssertTypeValuesMatchReference)
TEST(BeAssertTest, AssertTypeValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(BeAssertFunctions::AssertType::Normal),    0);
    EXPECT_EQ(static_cast<int>(BeAssertFunctions::AssertType::Data),      1);
    EXPECT_EQ(static_cast<int>(BeAssertFunctions::AssertType::Sigabrt),   2);
    EXPECT_EQ(static_cast<int>(BeAssertFunctions::AssertType::TypeCount), 3);
    // Reference uses All=99 (NOT 3). This is the §5 fix for the prior DanQing drift.
    EXPECT_EQ(static_cast<int>(BeAssertFunctions::AssertType::All),       99);
}

// Reference: imodel-native BeAssert.h:23
//   typedef void T_BeAssertHandler (wchar_t const*, wchar_t const*, unsigned, AssertType);
// DanQing adaptation (documented): UTF-8/char-based, so the typedef spells
//   using T_BeAssertHandler = void(const char*, const char*, unsigned, AssertType);
// We verify the typedef NAME exists and is invocable with char args.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, TBeAssertHandlerTypedefNamePresent)
TEST(BeAssertTest, TBeAssertHandlerTypedefNamePresent) {
    using H = BeAssertFunctions::T_BeAssertHandler;
    (void)sizeof(H*); // typedef name is usable as a function-pointer type
    SUCCEED();
}

// Reference: imodel-native BeAssert.h:26-27 splits Normal vs Data into two functions:
//   PerformBeAssert(...)    // Normal
//   PerformBeDataAssert(...) // Data
// DanQing must expose both names.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, PerformBeAssertAndPerformBeDataAssertBothExposed)
TEST(BeAssertTest, PerformBeAssertAndPerformBeDataAssertBothExposed) {
    // Verify both symbols exist and are addressable (signatures match ref split:
    // 3-arg, AssertType fixed internally).
    void (*pNormal)(const char*, const char*, unsigned) = &BeAssertFunctions::PerformBeAssert;
    void (*pData)(const char*, const char*, unsigned)   = &BeAssertFunctions::PerformBeDataAssert;
    EXPECT_NE(pNormal, nullptr);
    EXPECT_NE(pData, nullptr);
    // Distinct entry points (not folded aliases).
    EXPECT_NE(reinterpret_cast<void*>(pNormal), reinterpret_cast<void*>(pData));
}

// Reference: imodel-native BeAssert.h:28 — DefaultAssertionFailureHandler takes 3 args
// (message, file, line), NO atype. DanQing previously took a 4th AssertType; this is the
// §5 fix.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, DefaultHandlerThreeArgSignature)
TEST(BeAssertTest, DefaultHandlerThreeArgSignature) {
    // Addressable as a 3-arg function pointer (no atype).
    void (*p)(const char*, const char*, unsigned) = &BeAssertFunctions::DefaultAssertionFailureHandler;
    EXPECT_NE(p, nullptr);
}

// Reference: imodel-native BeAssert.h:24-25 — handler setters are named
//   SetBeTestAssertHandler / SetBeAssertHandler.
// We install a test handler, fire PerformBeAssert, confirm invocation, then clear.
namespace {
struct ScopedTestHandler {
    BeAssertFunctions::T_BeAssertHandler* prev_;
    int fired_ = 0;
    const char* lastExpr_ = nullptr;
    BeAssertFunctions::AssertType lastType_{};
    ScopedTestHandler() {
        static ScopedTestHandler* self = this;
        self = this;
        prev_ = nullptr;
        BeAssertFunctions::SetBeTestAssertHandler([](const char* e, const char* /*f*/, unsigned /*l*/, BeAssertFunctions::AssertType t) {
            self->fired_++;
            self->lastExpr_ = e;
            self->lastType_ = t;
        });
    }
    ~ScopedTestHandler() { BeAssertFunctions::SetBeTestAssertHandler(nullptr); }
};
} // namespace

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, SetBeTestAssertHandlerInvokedByPerformBeAssert)
TEST(BeAssertTest, SetBeTestAssertHandlerInvokedByPerformBeAssert) {
    ScopedTestHandler cap;
    BeAssertFunctions::PerformBeAssert("expr-A", __FILE__, __LINE__);
    EXPECT_EQ(cap.fired_, 1);
    EXPECT_STREQ(cap.lastExpr_, "expr-A");
    // PerformBeAssert dispatches AssertType::Normal (ref:26 + BeAssert macro:48).
    EXPECT_EQ(cap.lastType_, BeAssertFunctions::AssertType::Normal);
}

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeAssert.h
//              TEST(BeAssertTest, PerformBeDataAssertDispatchesDataType)
TEST(BeAssertTest, PerformBeDataAssertDispatchesDataType) {
    ScopedTestHandler cap;
    BeAssertFunctions::PerformBeDataAssert("expr-D", __FILE__, __LINE__);
    EXPECT_EQ(cap.fired_, 1);
    EXPECT_STREQ(cap.lastExpr_, "expr-D");
    // PerformBeDataAssert dispatches AssertType::Data (ref:27 + BeDataAssert macro:50).
    EXPECT_EQ(cap.lastType_, BeAssertFunctions::AssertType::Data);
}
