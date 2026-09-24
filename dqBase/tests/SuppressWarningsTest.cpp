// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/suppress_warnings.h
//              AND iModelCore/Bentley/PublicAPI/Bentley/Bentley.h:342-401
//                  PUSH_MSVC_IGNORE / POP_MSVC_IGNORE
//                  PUSH_CLANG_IGNORE / POP_CLANG_IGNORE
//                  UNREACHABLE_CODE(stmt)
//
// imodel-native has no unit test for suppress_warnings.h (pure macro header).
// Per §4, the behaviorally testable assertion is the UNREACHABLE_CODE pass-through:
// ref suppress_warnings.h:34-36 comment — "Use the UNREACHABLE_CODE macro to mark code
// that you know is unreachable. This may be necessary when adding a return statement
// to avoid a compiler warning." Under default (no BENTLEY_WARNINGS_HIGHEST_LEVEL),
// UNREACHABLE_CODE(stmt) expands to `stmt` (the statement is EMITTED).
#include <gtest/gtest.h>

#include <dqBase/suppress_warnings.h>

// Reference: suppress_warnings.h:35-36
//   #if !defined (BENTLEY_WARNINGS_HIGHEST_LEVEL)
//   #define UNREACHABLE_CODE(stmt)  stmt
// We assert pass-through: the inner statement executes and is observable.
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/suppress_warnings.h
//              TEST(SuppressWarningsTest, UnreachableCodeEmitsStatementByDefault)
TEST(SuppressWarningsTest, UnreachableCodeEmitsStatementByDefault) {
    int x = 0;
    UNREACHABLE_CODE(x = 1);
    EXPECT_EQ(x, 1) << "UNREACHABLE_CODE(stmt) must expand to stmt (ref suppress_warnings.h:36)";
}

// A return-statement inside UNREACHABLE_CODE must take effect (this is the ref's
// documented use case at suppress_warnings.h:34: "when adding a return statement").
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/suppress_warnings.h
//              TEST(SuppressWarningsTest, UnreachableCodeAllowsReturnStatement)
TEST(SuppressWarningsTest, UnreachableCodeAllowsReturnStatement) {
    struct Func {
        static int compute(int v) {
            if (v != 0) return v;
            // "unreachable" path emits a return to satisfy the compiler:
            UNREACHABLE_CODE(return -1;);
        }
    };
    EXPECT_EQ(Func::compute(7), 7);
    EXPECT_EQ(Func::compute(0), -1); // pass-through makes this reachable at runtime
}
