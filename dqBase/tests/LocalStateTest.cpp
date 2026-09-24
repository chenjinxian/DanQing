// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, *)
//
// Covers the (namespace,key) tuple keying mandated by the reference
// ILocalState/RuntimeLocalState vtable shape + bmap<bpair> storage (CLAUDE.md §6, §7.3).
#include <gtest/gtest.h>

#include <dqBase/LocalState.h>

using ::dqBase::ILocalState;
using ::dqBase::RuntimeLocalState;
using ::dqBase::bpair;
using ::dqBase::DqString;

class RuntimeLocalStateTests : public ::testing::Test {};

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, Ctor_Default_NoValues)
TEST_F(RuntimeLocalStateTests, Ctor_Default_NoValues) {
    EXPECT_EQ(0u, RuntimeLocalState().GetValues().size());
}

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, GetValue_NoValue_Empty)
TEST_F(RuntimeLocalStateTests, GetValue_NoValue_Empty) {
    RuntimeLocalState localState;
    EXPECT_STREQ("", localState.GetValue("Foo", "Boo").c_str());
}

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, GetValue_ExistingValue_ReturnsValue)
TEST_F(RuntimeLocalStateTests, GetValue_ExistingValue_ReturnsValue) {
    RuntimeLocalState localState;
    localState.SaveValue("Foo", "Boo", "42");
    EXPECT_STREQ("42", localState.GetValue("Foo", "Boo").c_str());
}

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, SaveValue_EmptyStr_RemovesValue)
TEST_F(RuntimeLocalStateTests, SaveValue_EmptyStr_RemovesValue) {
    RuntimeLocalState localState;
    localState.SaveValue("Foo", "Boo", "42");
    EXPECT_EQ(1u, localState.GetValues().size());
    localState.SaveValue("Foo", "Boo", "");
    EXPECT_EQ(0u, localState.GetValues().size());
}

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, SaveValue_DifferentKeys_StoresDifferentValues)
TEST_F(RuntimeLocalStateTests, SaveValue_DifferentKeys_StoresDifferentValues) {
    RuntimeLocalState localState;
    localState.SaveValue("Foo", "A", "ValueA");
    localState.SaveValue("Foo", "B", "ValueB");
    EXPECT_STREQ("ValueA", localState.GetValue("Foo", "A").c_str());
    EXPECT_STREQ("ValueB", localState.GetValue("Foo", "B").c_str());
    bpair<DqString, DqString> pairA {"Foo", "A"};
    bpair<DqString, DqString> pairB {"Foo", "B"};
    EXPECT_STREQ("ValueA", localState.GetValues()[pairA].c_str());
    EXPECT_STREQ("ValueB", localState.GetValues()[pairB].c_str());
    EXPECT_EQ(2u, localState.GetValues().size());
}

// Ported from: imodel-native LocalStateTests.cpp
//              TEST_F(RuntimeLocalStateTests, SaveValue_DifferentNamespaces_StoresDifferentValues)
TEST_F(RuntimeLocalStateTests, SaveValue_DifferentNamespaces_StoresDifferentValues) {
    RuntimeLocalState localState;
    localState.SaveValue("A", "Foo", "ValueA");
    localState.SaveValue("B", "Foo", "ValueB");
    EXPECT_STREQ("ValueA", localState.GetValue("A", "Foo").c_str());
    EXPECT_STREQ("ValueB", localState.GetValue("B", "Foo").c_str());
    bpair<DqString, DqString> pairA {"A", "Foo"};
    bpair<DqString, DqString> pairB {"B", "Foo"};
    EXPECT_STREQ("ValueA", localState.GetValues()[pairA].c_str());
    EXPECT_STREQ("ValueB", localState.GetValues()[pairB].c_str());
    EXPECT_EQ(2u, localState.GetValues().size());
}

// Authored: no reference test exists in imodel-native for the tuple-vs-flattened-key
// collision guarantee. The reference's bmap<bpair<Utf8String,Utf8String>,Utf8String>
// storage treats (namespace,key) as an atomic tuple; a naive flattened
// "namespace.key" serialization would collide for ("a.b","c") vs ("a","b.c").
// This test pins the ref's tuple-keyed contract.
TEST_F(RuntimeLocalStateTests, SaveValue_TupleKeying_NamespaceKeyCollisionSafe) {
    RuntimeLocalState localState;
    localState.SaveValue("a.b", "c", "First");
    localState.SaveValue("a", "b.c", "Second");

    EXPECT_STREQ("First", localState.GetValue("a.b", "c").c_str());
    EXPECT_STREQ("Second", localState.GetValue("a", "b.c").c_str());
    EXPECT_EQ(2u, localState.GetValues().size());

    // The two distinct tuple keys must coexist as independent bmap entries.
    bpair<DqString, DqString> tupleFirst {"a.b", "c"};
    bpair<DqString, DqString> tupleSecond {"a", "b.c"};
    EXPECT_STREQ("First", localState.GetValues()[tupleFirst].c_str());
    EXPECT_STREQ("Second", localState.GetValues()[tupleSecond].c_str());
}
