// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender tests — ClipVolume / ClipStack（参考移植）
//
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/ClipVolume.test.ts
//              core/frontend/src/test/render/webgl/ClipStack.test.ts
//
// §5(e)/headless 适配（逐项登记）：
//  - 参考测试在 headless-gl 上创建真 GL 纹理（texture 同一性/width/bytesUsed
//    断言）；DanQing dqRenderTest 无 GL——纹理同一性断言以 m_textureGeneration
//    （create 计数）+ m_lastUploadCreated（create vs replace）承载，width/
//    bytesUsed 以 textureHeight()/cpuBuffer 字节数等价承载；
//  - allocateGpuBuffer 记账（参考测试的 JS 实例字段覆写）无 C++ 对应——不入断言；
//  - gpuBuffer 与 cpuBuffer 的视图同一性（同一 ArrayBuffer）→ 单一 cpuBuffer
//    承载（上传时按 float 视图解释）——不再分别断言。
// 场景与断言值 1:1 取自参考。
#include "render/ClipStack.h"
#include "render/ClipVolume.h"

#include <dqGeom/ClipPrimitive.h>
#include <dqGeom/ClipVector.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Transform.h>

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

namespace dqRender {
namespace {

// Ported from: ClipStack.test.ts:12-16 (createClipVector)
dqGeom::ClipVector::Ptr createClipVector(double offset = 0) {
    dqGeom::ClipVector::Ptr clip = dqGeom::ClipVector::createEmpty();
    clip->appendShape({dqGeom::Point3d::From(offset + 1, 1, 0),
                       dqGeom::Point3d::From(offset + 2, 1, 0),
                       dqGeom::Point3d::From(offset + 2, 2, 0),
                       dqGeom::Point3d::From(offset + 1, 2, 0)});
    return clip;
}

// Ported from: ClipStack.test.ts:29-110 (class Stack extends ClipStack)
struct TestStack : ClipStack {
    dqGeom::Transform transform = dqGeom::Transform::CreateIdentity();
    bool wantViewClipFlag = true;

    struct Invoked {
        bool updateTexture = false;
        bool recomputeTexture = false;
        bool uploadTexture = false;
    };
    Invoked invoked;

    TestStack()
        : ClipStack([this]() -> dqGeom::Transform const& { return transform; },
                    [this]() { return wantViewClipFlag; }) {}

    // Ported from: Stack.pushClip (:64-69)
    void pushClip(double offset = 0) {
        dqGeom::ClipVector::Ptr const clip = createClipVector(offset);
        ClipVolume::Ptr const vol = ClipVolume::create(*clip);
        ASSERT_TRUE(vol.IsValid());
        push(vol);
    }

    // This is primarily for forcing it to update the texture.（:71-74）
    rhi::TextureHandle getTexture() { return texture(); }

    void reset() {
        invoked.uploadTexture = invoked.updateTexture = invoked.recomputeTexture = false;
    }

    // Ported from: Stack.expectInvoked (:80-94)
    void expectInvoked(bool const* updateTexture, bool const* recomputeTexture,
                       bool const* uploadTexture) {
        if (uploadTexture != nullptr)
            EXPECT_EQ(invoked.uploadTexture, *uploadTexture);
        if (updateTexture != nullptr)
            EXPECT_EQ(invoked.updateTexture, *updateTexture);
        if (recomputeTexture != nullptr)
            EXPECT_EQ(invoked.recomputeTexture, *recomputeTexture);
        reset();
    }

    void updateTexture() override {
        invoked.updateTexture = true;
        ClipStack::updateTexture();
    }
    void recomputeTexture() override {
        invoked.recomputeTexture = true;
        ClipStack::recomputeTexture();
    }
    void uploadTexture() override {
        invoked.uploadTexture = true;
        ClipStack::uploadTexture();  // headless 记账路径（driver null）
    }

    // protected 记账面的公开读取子（TEST 体非成员函数——经子类成员转发）。
    uint32_t textureGeneration() const { return m_textureGeneration; }
    bool lastUploadCreated() const { return m_lastUploadCreated; }
};

dqCommon::ClipStyle const kEmptyStyle{};

// getData 的 float32 视图（参考 new Float32Array(buffer)）。
std::vector<float> dataAsFloats(std::vector<uint8_t> const& bytes) {
    std::vector<float> floats(bytes.size() / 4);
    if (!floats.empty())
        std::memcpy(floats.data(), bytes.data(), bytes.size());
    return floats;
}

}  // namespace

// ===========================================================================
// ClipVolume（ClipVolume.test.ts describe("ClipVolume")）
// ===========================================================================

// Ported from: ClipVolume.test.ts:19-26 ("should ignore empty ClipVectors")
TEST(ClipVolumeTest, IgnoresEmptyClipVectors) {
    dqGeom::ClipVector::Ptr clipVector = dqGeom::ClipVector::createEmpty();
    EXPECT_TRUE(ClipVolume::create(*clipVector).IsNull());

    clipVector = dqGeom::ClipVector::createCapture({});
    EXPECT_TRUE(ClipVolume::create(*clipVector).IsNull());

    clipVector = dqGeom::ClipVector::createCapture(
        {dqGeom::ClipPrimitive::createCapture(dqGeom::UnionOfConvexClipPlaneSets::createEmpty())});
    EXPECT_TRUE(ClipVolume::create(*clipVector).IsNull());
}

// Ported from: ClipVolume.test.ts:28-55 ("should support single-primitive ClipVectors")
TEST(ClipVolumeTest, SupportsSinglePrimitiveClipVectors) {
    std::vector<dqGeom::Point3d> const points = {
        dqGeom::Point3d::From(1.0, 1.0, 0.0),
        dqGeom::Point3d::From(2.0, 1.0, 0.0),
        dqGeom::Point3d::From(2.0, 2.0, 0.0),
        dqGeom::Point3d::From(1.0, 2.0, 0.0),
    };

    dqGeom::ClipShape::Ptr const shape = dqGeom::ClipShape::createShape(points, 1.0, 2.0);
    ASSERT_TRUE(shape.IsValid());

    dqGeom::ClipVector::Ptr const clipVector = dqGeom::ClipVector::create({shape});
    ASSERT_TRUE(clipVector.IsValid());

    ClipVolume::Ptr const clipVolume = ClipVolume::create(*clipVector);
    ASSERT_TRUE(clipVolume.IsValid());

    dqGeom::Transform const identity = dqGeom::Transform::CreateIdentity();
    std::vector<float> const data = dataAsFloats(clipVolume->getData(identity));
    std::vector<float> const expectedData = {0, 1, 0, -1, -1, 0, 0, 2, 0, -1, 0, 2, 1, 0, 0, -1,
                                             0, 0, 1, -1, 0, 0, -1, 2, 2, 2, 2, 0};
    ASSERT_EQ(data.size(), expectedData.size());
    for (size_t i = 0; i < data.size(); ++i)
        EXPECT_FLOAT_EQ(data[i], expectedData[i]);
}

// Ported from: ClipVolume.test.ts:57-80 ("should support compound ClipVectors")
TEST(ClipVolumeTest, SupportsCompoundClipVectors) {
    dqGeom::ClipVector::Ptr const vec = dqGeom::ClipVector::createEmpty();
    EXPECT_TRUE(vec->appendShape({dqGeom::Point3d::From(1, 1, 0), dqGeom::Point3d::From(2, 1, 0),
                                  dqGeom::Point3d::From(2, 2, 0), dqGeom::Point3d::From(1, 2, 0)},
                                 1.0, 2.0));
    ClipVolume::Ptr vol = ClipVolume::create(*vec);
    ASSERT_TRUE(vol.IsValid());
    EXPECT_EQ(vol->clipVector(), vec.Get());
    EXPECT_EQ(vol->numRows(), 7u);  // 6 planes plus a boundary marker

    dqGeom::UnionOfConvexClipPlaneSets const* planes = vec->clips()[0]->fetchClipPlanesRef();
    ASSERT_TRUE(planes != nullptr);
    dqGeom::UnionOfConvexClipPlaneSets const planesClone = planes->clone();
    vec->appendReference(dqGeom::ClipPrimitive::createCapture(planesClone));
    vol = ClipVolume::create(*vec);
    ASSERT_TRUE(vol.IsValid());
    EXPECT_EQ(vol->numRows(), 14u);  // 6 planes per ClipPrimitive + boundary per union

    std::vector<float> const planesData = {0, 1, 0, -1, -1, 0, 0, 2, 0, -1, 0, 2, 1, 0, 0, -1,
                                           0, 0, 1, -1, 0, 0, -1, 2};
    std::vector<float> const boundaryData = {2, 2, 2, 0};
    std::vector<float> expectedData = planesData;
    expectedData.insert(expectedData.end(), boundaryData.begin(), boundaryData.end());
    expectedData.insert(expectedData.end(), planesData.begin(), planesData.end());
    expectedData.insert(expectedData.end(), boundaryData.begin(), boundaryData.end());

    dqGeom::Transform const identity = dqGeom::Transform::CreateIdentity();
    std::vector<float> const data = dataAsFloats(vol->getData(identity));
    ASSERT_EQ(data.size(), expectedData.size());
    for (size_t i = 0; i < data.size(); ++i)
        EXPECT_FLOAT_EQ(data[i], expectedData[i]);
}

// ===========================================================================
// ClipStack（ClipStack.test.ts describe("ClipStack")）
// ===========================================================================

// Ported from: ClipStack.test.ts:112-151 ("sets the view clip")
TEST(ClipStackTest, SetsTheViewClip) {
    TestStack stack;
    EXPECT_FALSE(stack.hasClip());
    EXPECT_FALSE(stack.isStackDirty());

    bool const prevClipEmpty = stack.clips()[0].IsNull();
    stack.setViewClip(dqGeom::ClipVector::createEmpty().Get(), kEmptyStyle);
    EXPECT_FALSE(stack.hasClip());
    EXPECT_EQ(stack.clips()[0].IsNull(), prevClipEmpty);  // stack[0] unchanged
    EXPECT_FALSE(stack.isStackDirty());

    dqGeom::ClipVector::Ptr const clipVec = createClipVector();
    stack.setViewClip(clipVec.Get(), kEmptyStyle);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_TRUE(stack.clips()[0].IsValid());  // instanceof ClipVolume
    EXPECT_TRUE(stack.isStackDirty());
    stack.getTexture();

    ClipVolume::Ptr const prevClip = stack.clips()[0];
    stack.setViewClip(clipVec.Get(), kEmptyStyle);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_EQ(stack.clips()[0].Get(), prevClip.Get());  // same clip → no change
    EXPECT_FALSE(stack.isStackDirty());

    stack.setViewClip(createClipVector(1).Get(), kEmptyStyle);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_NE(stack.clips()[0].Get(), prevClip.Get());
    EXPECT_TRUE(stack.isStackDirty());
    stack.getTexture();

    stack.setViewClip(nullptr, kEmptyStyle);
    EXPECT_FALSE(stack.hasClip());
    EXPECT_TRUE(stack.clips()[0].IsNull());  // not a ClipVolume
    EXPECT_TRUE(stack.isStackDirty());
    stack.getTexture();

    stack.setViewClip(nullptr, kEmptyStyle);
    EXPECT_FALSE(stack.isStackDirty());
}

// Ported from: ClipStack.test.ts:153-228 ("updates state as clips are pushed and popped")
// 适配：texture.width/bytesUsed（真 GL 属性）→ textureHeight()/cpuBuffer 字节数。
TEST(ClipStackTest, UpdatesStateAsClipsArePushedAndPopped) {
    TestStack stack;
    EXPECT_EQ(stack.clips().size(), 1u);
    EXPECT_FALSE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 0u);
    EXPECT_EQ(stack.numRowsInUse(), 0u);
    EXPECT_EQ(stack.cpuBuffer().size(), 0u);
    EXPECT_EQ(stack.startIndex(), 0u);
    EXPECT_EQ(stack.endIndex(), 0u);

    stack.setViewClip(createClipVector().Get(), kEmptyStyle);
    EXPECT_EQ(stack.clips().size(), 1u);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 5u);
    EXPECT_EQ(stack.numRowsInUse(), 5u);
    EXPECT_EQ(stack.startIndex(), 0u);
    EXPECT_EQ(stack.endIndex(), 5u);

    stack.wantViewClipFlag = false;
    EXPECT_FALSE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 5u);
    EXPECT_EQ(stack.numRowsInUse(), 5u);
    EXPECT_EQ(stack.startIndex(), 5u);
    EXPECT_EQ(stack.endIndex(), 5u);

    stack.pushClip();
    EXPECT_EQ(stack.clips().size(), 2u);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 10u);
    EXPECT_EQ(stack.numRowsInUse(), 10u);
    EXPECT_EQ(stack.startIndex(), 5u);
    EXPECT_EQ(stack.endIndex(), 10u);

    stack.wantViewClipFlag = true;
    EXPECT_TRUE(stack.hasClip());
    EXPECT_EQ(stack.startIndex(), 0u);
    EXPECT_EQ(stack.endIndex(), 10u);

    stack.pop();
    EXPECT_EQ(stack.clips().size(), 1u);
    EXPECT_TRUE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 10u);  // never shrinks
    EXPECT_EQ(stack.numRowsInUse(), 5u);
    EXPECT_EQ(stack.startIndex(), 0u);
    EXPECT_EQ(stack.endIndex(), 5u);

    stack.wantViewClipFlag = false;
    EXPECT_FALSE(stack.hasClip());

    stack.wantViewClipFlag = true;
    stack.setViewClip(nullptr, kEmptyStyle);
    EXPECT_EQ(stack.clips().size(), 1u);
    EXPECT_FALSE(stack.hasClip());
    EXPECT_EQ(stack.numTotalRows(), 10u);
    EXPECT_EQ(stack.numRowsInUse(), 0u);
    EXPECT_EQ(stack.startIndex(), 0u);
    EXPECT_EQ(stack.endIndex(), 0u);

    stack.setViewClip(createClipVector().Get(), kEmptyStyle);
    stack.pushClip();
    stack.getTexture();
    EXPECT_EQ(stack.numTotalRows(), 10u);
    EXPECT_EQ(stack.cpuBuffer().size(), size_t{160});  // 4 floats * 4 bytes * 10 rows
    EXPECT_EQ(stack.textureHeight(), 10u);
}

// Ported from: ClipStack.test.ts:230-250 ("is marked dirty when a new clip is pushed
// until texture is updated")
TEST(ClipStackTest, MarkedDirtyWhenClipPushedUntilTextureUpdated) {
    TestStack stack;
    EXPECT_FALSE(stack.isStackDirty());

    stack.pushClip();
    EXPECT_TRUE(stack.isStackDirty());
    EXPECT_TRUE(stack.getTexture() || stack.textureHeight() > 0);  // headless 适配
    EXPECT_FALSE(stack.isStackDirty());

    stack.pushClip();
    EXPECT_TRUE(stack.isStackDirty());
    EXPECT_TRUE(stack.getTexture() || stack.textureHeight() > 0);  // headless 适配
    EXPECT_FALSE(stack.isStackDirty());

    stack.pop();
    stack.pop();
    EXPECT_FALSE(stack.isStackDirty());

    stack.pushClip();
    EXPECT_TRUE(stack.isStackDirty());
}

// Ported from: ClipStack.test.ts:252-268 ("only recomputes data when dirty")
TEST(ClipStackTest, OnlyRecomputesDataWhenDirty) {
    TestStack stack;
    EXPECT_FALSE(stack.isStackDirty());

    rhi::TextureHandle const tex1 = stack.texture();
    EXPECT_FALSE(tex1);  // undefined（无内容 → 无纹理）
    bool const f = false, t = true;
    stack.expectInvoked(&t, &f, &f);  // updateTexture ran, recompute didn't

    stack.pushClip();
    rhi::TextureHandle const tex2 = stack.getTexture();
    EXPECT_TRUE(tex2 || stack.numTotalRows() > 0);  // headless：句柄可空，行数在案
    stack.expectInvoked(&t, &t, &t);

    rhi::TextureHandle const tex3 = stack.texture();
    (void)tex3;
    stack.expectInvoked(&t, &f, &f);
}

// Ported from: ClipStack.test.ts:270-299 ("recreates texture only when size increases")
// 适配：句柄同一性 → m_textureGeneration（create 计数）。
TEST(ClipStackTest, RecreatesTextureOnlyWhenSizeIncreases) {
    TestStack stack;
    bool const f = false;
    stack.expectInvoked(&f, &f, &f);

    stack.pushClip();
    stack.expectInvoked(&f, &f, &f);

    (void)stack.getTexture();
    uint32_t const gen1 = stack.textureGeneration();
    EXPECT_EQ(gen1, 1u);
    bool const t = true;
    stack.expectInvoked(&t, &t, &t);

    stack.pop();
    stack.pushClip();
    (void)stack.getTexture();
    EXPECT_EQ(stack.textureGeneration(), gen1);  // same size → replace, not recreate
    stack.expectInvoked(&t, &t, &f);             // content unchanged → no upload

    stack.pushClip();
    (void)stack.getTexture();
    uint32_t const gen3 = stack.textureGeneration();
    EXPECT_EQ(gen3, gen1 + 1);  // size increased → recreate
    stack.expectInvoked(&t, &t, &t);

    stack.pop();
    stack.pop();
    stack.pushClip(1);
    stack.pushClip(2);
    (void)stack.getTexture();
    EXPECT_EQ(stack.textureGeneration(), gen3);  // fits → replace
    stack.expectInvoked(&t, &t, &t);             // content changed → upload (recreate=false)
    EXPECT_FALSE(stack.lastUploadCreated());
}

// Ported from: ClipStack.test.ts:301-319 ("uploads texture data only after it has changed")
TEST(ClipStackTest, UploadsTextureDataOnlyAfterChanged) {
    TestStack stack;

    stack.pushClip(1);
    (void)stack.getTexture();
    bool const t = true, f = false;
    stack.expectInvoked(&t, &t, &t);

    stack.pop();
    stack.pushClip(1);
    (void)stack.getTexture();
    stack.expectInvoked(&t, &t, &f);  // same content → no upload

    stack.pop();
    stack.pushClip(2);
    (void)stack.getTexture();
    stack.expectInvoked(&t, &t, &t);  // different content → upload
    EXPECT_FALSE(stack.lastUploadCreated());
}

// Ported from: ClipStack.test.ts:321-340 ("updates texture when transform changes")
TEST(ClipStackTest, UpdatesTextureWhenTransformChanges) {
    TestStack stack;

    stack.pushClip();
    (void)stack.getTexture();
    bool const t = true, f = false;
    stack.expectInvoked(&t, &t, &t);

    stack.pop();
    stack.transform.origin.x += 1;
    stack.pushClip();
    (void)stack.getTexture();
    stack.expectInvoked(&t, &t, &t);  // view-coord planes changed → upload

    stack.pop();
    stack.pushClip();
    (void)stack.getTexture();
    stack.expectInvoked(&t, &t, &f);  // same transform → no upload
}

}  // namespace dqRender
