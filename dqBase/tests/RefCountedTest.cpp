// SPDX-License-Identifier: Apache-2.0
// dqBase tests — RefCounted / RefPtr 行为验证
// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
//              TEST(RefCountedTests, Test1)
//           (remaining RefPtr cases: Authored — no reference test exists; see per-case notes)
//
// 注：imodel-native 仅在 RefCounted_test.cpp TEST(RefCountedTests, Test1) 中覆盖了
// 裸 AddRef/Release、refcount 从 0 起、RefPtr(new T) 持有与 nullptr 释放、bvector
// push_back/pop_back 四类场景。RefPtr 的拷贝/移动/默认/Reset/StaticCast 在参考项目
// 中无对应测试，按 §4 标注为 Authored。
#include <gtest/gtest.h>

#include <dqBase/RefCounted.h>

#include <utility>
#include <vector>

namespace {
// 测试用 RefCounted 类型，带实例计数器验证生命周期
class TestObject : public dqBase::RefCounted<TestObject> {
public:
    TestObject() {
        ++s_instanceCount;
    }
    ~TestObject() override {
        --s_instanceCount;
    }
    static int s_instanceCount;
};
int TestObject::s_instanceCount = 0;
} // namespace

// --- 基础生命周期 ---

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
//              TEST(RefCountedTests, Test1) — basic addref/release, refcount starts at 0
TEST(RefCountedTest, FreshBareObjectHasRefCountZero) {
    auto* raw = new TestObject();
    EXPECT_EQ(raw->RefCount(), 0u);
    raw->AddRef();
    EXPECT_EQ(raw->RefCount(), 1u);
    raw->Release();
    EXPECT_EQ(TestObject::s_instanceCount, 0);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
//              TEST(RefCountedTests, Test1) — RefCountedPtr(new Derived) holds, releases on destruct
TEST(RefCountedTest, RefPtrOwnsObjectAndReleasesOnDestruct) {
    {
        dqBase::RefPtr<TestObject> p(new TestObject());
        EXPECT_EQ(TestObject::s_instanceCount, 1);
        EXPECT_EQ(p->RefCount(), 1u);
    }
    EXPECT_EQ(TestObject::s_instanceCount, 0);
}

// --- 拷贝 / 移动语义 ---

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for RefPtr copy-ctor refcount bump
TEST(RefPtrTest, CopyIncrementsRefCount) {
    dqBase::RefPtr<TestObject> a(new TestObject());
    EXPECT_EQ(a->RefCount(), 1u);
    {
        dqBase::RefPtr<TestObject> b = a;
        EXPECT_EQ(a->RefCount(), 2u);
        EXPECT_EQ(b->RefCount(), 2u);
        EXPECT_EQ(a.Get(), b.Get());
    }
    EXPECT_EQ(a->RefCount(), 1u);
}

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for RefPtr move-ctor (no refcount change)
TEST(RefPtrTest, MoveDoesNotChangeRefCount) {
    dqBase::RefPtr<TestObject> a(new TestObject());
    EXPECT_EQ(a->RefCount(), 1u);
    dqBase::RefPtr<TestObject> b = std::move(a);
    EXPECT_EQ(b->RefCount(), 1u);
    EXPECT_EQ(a.Get(), nullptr);
    EXPECT_TRUE(a.IsNull());
}

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for RefPtr self-assignment safety
TEST(RefPtrTest, SelfCopyAssignIsSafe) {
    dqBase::RefPtr<TestObject> a(new TestObject());
    auto* raw = a.Get();
    a = dqBase::RefPtr<TestObject>(a);
    EXPECT_EQ(a.Get(), raw);
    EXPECT_EQ(a->RefCount(), 1u);
}

// --- Reset / 空状态 ---

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for default-constructed RefPtr nullness
TEST(RefPtrTest, DefaultConstructedIsNull) {
    dqBase::RefPtr<TestObject> p;
    EXPECT_TRUE(p.IsNull());
    EXPECT_FALSE(p.IsValid());
    EXPECT_FALSE(static_cast<bool>(p));
    EXPECT_EQ(p.Get(), nullptr);
}

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for RefPtr::Reset swap-and-release
TEST(RefPtrTest, ResetReleasesPreviousAndTakesNew) {
    dqBase::RefPtr<TestObject> a(new TestObject());
    auto* first = a.Get();
    a.Reset(new TestObject());
    EXPECT_NE(a.Get(), first);
    EXPECT_EQ(TestObject::s_instanceCount, 1);
    a.Reset();
    EXPECT_TRUE(a.IsNull());
    EXPECT_EQ(TestObject::s_instanceCount, 0);
}

// --- 静态转换（继承层次）---

// Authored: no reference test exists in imodel-native RefCounted_test.cpp for static_pointer_cast across hierarchy
TEST(RefPtrTest, StaticCastAcrossHierarchy) {
    class Base : public dqBase::RefCounted<Base> {
    public:
        virtual ~Base() = default;
    };
    class Derived : public Base {
    public:
        int value = 42;
    };

    dqBase::RefPtr<Base> b(new Derived());
    auto d = dqBase::StaticCast<Derived>(b);
    EXPECT_EQ(d->value, 42);
    EXPECT_EQ(b->RefCount(), 2u);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
//              TEST(RefCountedTests, Test1) part 4
TEST(RefPtrTest, ContainerPushBackAddRefsAndPopBackReleases) {
    dqBase::RefPtr<TestObject> a(new TestObject());
    EXPECT_EQ(a->RefCount(), 1u);
    {
        std::vector<dqBase::RefPtr<TestObject>> vec;
        vec.push_back(a);
        vec.push_back(a);
        EXPECT_EQ(a->RefCount(), 3u);
        vec.pop_back();
        EXPECT_EQ(a->RefCount(), 2u);
    }
    EXPECT_EQ(a->RefCount(), 1u);
    EXPECT_EQ(TestObject::s_instanceCount, 1);
}

// Ported from: imodel-native iModelCore/Bentley/Tests/NonPublished/RefCounted_test.cpp
//              TEST(RefCountedTests, Test1) part 2
TEST(RefPtrTest, AssignNullptrReleasesAndDestructs) {
    dqBase::RefPtr<TestObject> p(new TestObject());
    EXPECT_EQ(TestObject::s_instanceCount, 1);
    EXPECT_EQ(p->RefCount(), 1u);
    p = nullptr;
    EXPECT_TRUE(p.IsNull());
    EXPECT_EQ(TestObject::s_instanceCount, 0);
}
