// SPDX-License-Identifier: Apache-2.0
// dqBase tests — ClassUtils (IsProperSubclassOf / IsSubclassOf)
// Ported from: itwinjs-core core/bentley/src/test/ClassUtils.test.ts
//              describe("ClassUtils") isProperSubclassOf / isSubclassOf
//
// Note: the reference `ClassUtils.ts` exports ONLY isProperSubclassOf/isSubclassOf.
// The DanQing self-additions IsInstanceOf/AsInstanceOf (which used banned dynamic_cast,
// see CLAUDE.md §9 -fno-rtti) were dropped as §6 self-additions (P1 #20). This test
// covers the retained, reference-aligned constexpr surface.
#include <gtest/gtest.h>

#include <dqBase/ClassUtils.h>

using ::dqBase::IsProperSubclassOf;
using ::dqBase::IsSubclassOf;

namespace {

class A {};
class B : public A {};
class C : public B {};
class M {};

} // namespace

// ---------------------------------------------------------------------------
// Compile-time proof that the retained helpers work in a constant expression
// context (no dynamic_cast, no RTTI required). Mirrors the spirit of the
// TypeScript test's "won't compile if our type assumptions aren't met" guards.
// ---------------------------------------------------------------------------
static_assert(IsSubclassOf<A, A>(), "A is a subclass of A");
static_assert(!IsProperSubclassOf<A, A>(), "A is NOT a proper subclass of A");
static_assert(IsSubclassOf<C, A>() && IsProperSubclassOf<C, A>(), "C derives from A");
static_assert(IsSubclassOf<C, B>() && IsProperSubclassOf<C, B>(), "C derives from B");
static_assert(!IsSubclassOf<A, M>(), "A and M are unrelated");

// NOTE: each templated call is wrapped in parens so the preprocessor treats the
// (comma-bearing) expression as a single macro argument.
// Ported from: itwinjs-core core/bentley/src/test/ClassUtils.test.ts
//              it("isProperSubclassOf")
TEST(ClassUtils, IsProperSubclassOf) {
    // A is not a proper subclass of itself, B, C, or M
    EXPECT_FALSE((IsProperSubclassOf<A, A>()));
    EXPECT_FALSE((IsProperSubclassOf<A, B>()));
    EXPECT_FALSE((IsProperSubclassOf<A, C>()));
    EXPECT_FALSE((IsProperSubclassOf<A, M>()));

    // B is a proper subclass of A only
    EXPECT_TRUE((IsProperSubclassOf<B, A>()));
    EXPECT_FALSE((IsProperSubclassOf<B, B>()));
    EXPECT_FALSE((IsProperSubclassOf<B, C>()));
    EXPECT_FALSE((IsProperSubclassOf<B, M>()));

    // C is a proper subclass of A and B
    EXPECT_TRUE((IsProperSubclassOf<C, A>()));
    EXPECT_TRUE((IsProperSubclassOf<C, B>()));
    EXPECT_FALSE((IsProperSubclassOf<C, C>()));
    EXPECT_FALSE((IsProperSubclassOf<C, M>()));
}

// Ported from: itwinjs-core core/bentley/src/test/ClassUtils.test.ts
//              it("isSubclassOf")
TEST(ClassUtils, IsSubclassOf) {
    // A is a subclass of A only
    EXPECT_TRUE((IsSubclassOf<A, A>()));
    EXPECT_FALSE((IsSubclassOf<A, B>()));
    EXPECT_FALSE((IsSubclassOf<A, C>()));

    // B is a subclass of A and B
    EXPECT_TRUE((IsSubclassOf<B, A>()));
    EXPECT_TRUE((IsSubclassOf<B, B>()));
    EXPECT_FALSE((IsSubclassOf<B, C>()));

    // C is a subclass of A, B, and C
    EXPECT_TRUE((IsSubclassOf<C, A>()));
    EXPECT_TRUE((IsSubclassOf<C, B>()));
    EXPECT_TRUE((IsSubclassOf<C, C>()));
}
