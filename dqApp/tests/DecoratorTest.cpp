// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Decorator and GltfDecoration tests
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
#include <gtest/gtest.h>

#include <dqApp/GltfDecoration.h>
#include <dqApp/ViewManager.h>

using namespace dqApp;

// ---------------------------------------------------------------------------
// GltfDecoration tests
// Ported from: itwinjs-core GltfDecoration.ts
// ---------------------------------------------------------------------------
TEST(GltfDecorationTest, TestDecorationHitMatchesId)
{
    GltfDecoration dec(42, "test-model.gltf");

    EXPECT_TRUE(dec.TestDecorationHit(42));
    EXPECT_FALSE(dec.TestDecorationHit(1));
    EXPECT_FALSE(dec.TestDecorationHit(0));
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(GltfDecorationTest, GetToolTipReturnsName)
TEST(GltfDecorationTest, GetToolTipReturnsName)
{
    GltfDecoration dec(42, "test-model.gltf");

    EXPECT_EQ(dec.GetDecorationToolTip(42), "test-model.gltf");
    EXPECT_TRUE(dec.GetDecorationToolTip(1).isEmpty());
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(GltfDecorationTest, GetPickableId)
TEST(GltfDecorationTest, GetPickableId)
{
    GltfDecoration dec(100, "model");
    EXPECT_EQ(dec.GetPickableId(), 100u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(GltfDecorationTest, GetName)
TEST(GltfDecorationTest, GetName)
{
    GltfDecoration dec(1, "my-model.glb");
    EXPECT_EQ(dec.GetName(), "my-model.glb");
}

// ---------------------------------------------------------------------------
// ViewManager decorator tests
// Ported from: itwinjs-core ViewManager.ts
// ---------------------------------------------------------------------------
TEST(ViewManagerDecoratorTest, AddAndFindDecorator)
{
    ViewManager mgr;
    GltfDecoration dec(42, "test");

    mgr.AddDecorator(&dec);

    auto* found = mgr.FindDecoratorForHit(42);
    EXPECT_EQ(found, &dec);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(ViewManagerDecoratorTest, FindReturnsNullForUnknown)
TEST(ViewManagerDecoratorTest, FindReturnsNullForUnknown)
{
    ViewManager mgr;
    GltfDecoration dec(42, "test");

    mgr.AddDecorator(&dec);

    auto* found = mgr.FindDecoratorForHit(99);
    EXPECT_EQ(found, nullptr);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(ViewManagerDecoratorTest, DropDecoratorRemoves)
TEST(ViewManagerDecoratorTest, DropDecoratorRemoves)
{
    ViewManager mgr;
    GltfDecoration dec(42, "test");

    mgr.AddDecorator(&dec);
    mgr.DropDecorator(&dec);

    auto* found = mgr.FindDecoratorForHit(42);
    EXPECT_EQ(found, nullptr);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(ViewManagerDecoratorTest, GetDecorationToolTip)
TEST(ViewManagerDecoratorTest, GetDecorationToolTip)
{
    ViewManager mgr;
    GltfDecoration dec(42, "my-model.gltf");

    mgr.AddDecorator(&dec);

    EXPECT_EQ(mgr.GetDecorationToolTip(42), "my-model.gltf");
    EXPECT_TRUE(mgr.GetDecorationToolTip(99).isEmpty());
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(ViewManagerDecoratorTest, MultipleDecorators)
TEST(ViewManagerDecoratorTest, MultipleDecorators)
{
    ViewManager mgr;
    GltfDecoration dec1(1, "model1.gltf");
    GltfDecoration dec2(2, "model2.gltf");

    mgr.AddDecorator(&dec1);
    mgr.AddDecorator(&dec2);

    EXPECT_EQ(mgr.FindDecoratorForHit(1), &dec1);
    EXPECT_EQ(mgr.FindDecoratorForHit(2), &dec2);
    EXPECT_EQ(mgr.FindDecoratorForHit(3), nullptr);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(ViewManagerDecoratorTest, DuplicateAddIsNoOp)
TEST(ViewManagerDecoratorTest, DuplicateAddIsNoOp)
{
    ViewManager mgr;
    GltfDecoration dec(42, "test");

    mgr.AddDecorator(&dec);
    mgr.AddDecorator(&dec);  // duplicate

    // Should still find it
    EXPECT_EQ(mgr.FindDecoratorForHit(42), &dec);
}

// ---------------------------------------------------------------------------
// Scene tests (public API)
// Ported from: itwinjs-core Scene.ts
// ---------------------------------------------------------------------------
#include <dqRender/Scene.h>
#include <dqRender/RenderGraphic.h>
#include <dqRender/GraphicBranch.h>

TEST(SceneTest, EmptyScene)
{
    dqRender::Scene scene;
    EXPECT_TRUE(scene.isEmpty());
    EXPECT_EQ(scene.size(), 0u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(SceneTest, AddToForeground)
TEST(SceneTest, AddToForeground)
{
    dqRender::Scene scene;
    dqRender::GraphicBranch branch;
    scene.foreground.push_back(&branch);

    EXPECT_FALSE(scene.isEmpty());
    EXPECT_EQ(scene.size(), 1u);
    EXPECT_EQ(scene.foreground.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(SceneTest, ThreeLists)
TEST(SceneTest, ThreeLists)
{
    dqRender::Scene scene;
    dqRender::GraphicBranch b1, b2, b3;
    scene.foreground.push_back(&b1);
    scene.background.push_back(&b2);
    scene.overlay.push_back(&b3);

    EXPECT_EQ(scene.size(), 3u);
    EXPECT_EQ(scene.foreground.size(), 1u);
    EXPECT_EQ(scene.background.size(), 1u);
    EXPECT_EQ(scene.overlay.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(SceneTest, clear)
TEST(SceneTest, clear)
{
    dqRender::Scene scene;
    dqRender::GraphicBranch b1, b2;
    scene.foreground.push_back(&b1);
    scene.background.push_back(&b2);

    scene.clear();

    EXPECT_TRUE(scene.isEmpty());
    EXPECT_EQ(scene.foreground.size(), 0u);
    EXPECT_EQ(scene.background.size(), 0u);
    EXPECT_EQ(scene.overlay.size(), 0u);
}

// ---------------------------------------------------------------------------
// Decorations tests (public API)
// Ported from: itwinjs-core Decorations.ts
// ---------------------------------------------------------------------------
#include <dqRender/Decorations.h>

TEST(DecorationsTest, EmptyDecorations)
{
    dqRender::Decorations decos;
    EXPECT_TRUE(decos.isEmpty());
    EXPECT_EQ(decos.skyBox, nullptr);
    EXPECT_EQ(decos.viewBackground, nullptr);
    EXPECT_TRUE(decos.normal.empty());
    EXPECT_TRUE(decos.world.empty());
    EXPECT_TRUE(decos.worldOverlay.empty());
    EXPECT_TRUE(decos.viewOverlay.empty());
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, AddNormal)
TEST(DecorationsTest, AddNormal)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch branch;
    decos.add(dqRender::GraphicType::Scene, &branch);

    EXPECT_FALSE(decos.isEmpty());
    EXPECT_EQ(decos.normal.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, AddWorld)
TEST(DecorationsTest, AddWorld)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch branch;
    decos.add(dqRender::GraphicType::WorldDecoration, &branch);

    EXPECT_EQ(decos.world.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, AddWorldOverlay)
TEST(DecorationsTest, AddWorldOverlay)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch branch;
    decos.add(dqRender::GraphicType::WorldOverlay, &branch);

    EXPECT_EQ(decos.worldOverlay.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, AddViewOverlay)
TEST(DecorationsTest, AddViewOverlay)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch branch;
    decos.add(dqRender::GraphicType::ViewOverlay, &branch);

    EXPECT_EQ(decos.viewOverlay.size(), 1u);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, SetSkyBox)
TEST(DecorationsTest, SetSkyBox)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch branch;
    decos.skyBox = &branch;

    EXPECT_FALSE(decos.isEmpty());
    EXPECT_EQ(decos.skyBox, &branch);
}

// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/GltfDecoration.ts
//              TEST(DecorationsTest, clear)
TEST(DecorationsTest, clear)
{
    dqRender::Decorations decos;
    dqRender::GraphicBranch b1, b2;
    decos.add(dqRender::GraphicType::Scene, &b1);
    decos.add(dqRender::GraphicType::WorldDecoration, &b2);
    decos.skyBox = &b1;

    decos.clear();

    EXPECT_TRUE(decos.isEmpty());
    EXPECT_EQ(decos.skyBox, nullptr);
    EXPECT_TRUE(decos.normal.empty());
    EXPECT_TRUE(decos.world.empty());
}
