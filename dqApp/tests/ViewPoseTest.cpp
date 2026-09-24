// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewPose tests
//
// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
#include <gtest/gtest.h>

#include <dqApp/StandardView.h>
#include <dqApp/ViewPose.h>
#include <dqApp/ViewState.h>

using namespace dqApp;
using namespace dqGeom;
using namespace dqCommon;

// ViewPose3d tests
// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, DefaultConstruction)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto extents = Vector3d::From(1000, 1000, 1000);
    const auto rotation = Matrix3d::CreateIdentity();
    const Camera camera;

    const ViewPose3d pose(origin, extents, rotation, camera, false);

    EXPECT_TRUE(pose.GetOrigin().IsEqual(origin));
    EXPECT_TRUE(pose.GetExtents().IsEqual(extents));
    EXPECT_FALSE(pose.IsCameraOn());
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, Center)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto extents = Vector3d::From(100, 200, 300);
    const auto rotation = Matrix3d::CreateIdentity();
    const Camera camera;

    const ViewPose3d pose(origin, extents, rotation, camera, false);
    const auto center = pose.getCenter();

    // center = origin + extents * 0.5
    EXPECT_NEAR(center.x, 50.0, 1e-10);
    EXPECT_NEAR(center.y, 100.0, 1e-10);
    EXPECT_NEAR(center.z, 150.0, 1e-10);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, TargetWithoutCamera)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto extents = Vector3d::From(100, 100, 100);
    const auto rotation = Matrix3d::CreateIdentity();
    const Camera camera;

    const ViewPose3d pose(origin, extents, rotation, camera, false);
    const auto target = pose.GetTarget();

    // Without camera, target == center
    EXPECT_TRUE(target.AlmostEqual(pose.getCenter()));
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, TargetWithCamera)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto extents = Vector3d::From(100, 100, 100);
    const auto rotation = Matrix3d::CreateIdentity();

    Camera camera;
    camera.eye = Point3d::From(0, 0, 100);
    camera.focusDist = 50.0;

    const ViewPose3d pose(origin, extents, rotation, camera, true);
    const auto target = pose.GetTarget();

    // With camera: target = eye + zVec * -focusDist
    // zVec = (0, 0, 1), so target = (0, 0, 100) + (0, 0, -50) = (0, 0, 50)
    EXPECT_NEAR(target.x, 0.0, 1e-10);
    EXPECT_NEAR(target.y, 0.0, 1e-10);
    EXPECT_NEAR(target.z, 50.0, 1e-10);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, ZVec)
{
    const auto rotation = Matrix3d::CreateIdentity();
    const ViewPose3d pose(Point3d::FromZero(), Vector3d::From(100, 100, 100), rotation, Camera(), false);

    const auto zVec = pose.GetZVec();
    EXPECT_NEAR(zVec.x, 0.0, 1e-10);
    EXPECT_NEAR(zVec.y, 0.0, 1e-10);
    EXPECT_NEAR(zVec.z, 1.0, 1e-10);
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose3d, Equality)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto extents = Vector3d::From(100, 100, 100);
    const auto rotation = Matrix3d::CreateIdentity();
    const Camera camera;

    const ViewPose3d a(origin, extents, rotation, camera, false);
    const ViewPose3d b(origin, extents, rotation, camera, false);
    EXPECT_TRUE(a.equals(b));

    const ViewPose3d c(Point3d::From(10, 0, 0), extents, rotation, camera, false);
    EXPECT_FALSE(a.equals(c));
}

// ViewPose2d tests
// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose2d, DefaultConstruction)
{
    const auto origin = Point3d::From(0, 0, 0);
    const auto delta = Vector3d::From(1000, 1000, 0);

    const ViewPose2d pose(origin, delta, 0.0);
    EXPECT_TRUE(pose.GetOrigin().IsEqual(origin));
    EXPECT_TRUE(pose.GetExtents().IsEqual(delta));
    EXPECT_FALSE(pose.IsCameraOn());
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose2d, Rotation)
{
    const ViewPose2d pose(Point3d::FromZero(), Vector3d::From(100, 100, 0), 90.0);
    const auto rot = pose.getRotation();

    // 90 degree rotation around Z
    // Matrix3d stores row-major: coffs[0..2]=row0, coffs[3..5]=row1, coffs[6..8]=row2
    // Rotation matrix: [cos, -sin, 0; sin, cos, 0; 0, 0, 1]
    EXPECT_NEAR(rot.coffs[0], 0.0, 1e-10);   // cos(90) ≈ 0
    EXPECT_NEAR(rot.coffs[1], -1.0, 1e-10);  // -sin(90) = -1
    EXPECT_NEAR(rot.coffs[3], 1.0, 1e-10);   // sin(90) = 1
    EXPECT_NEAR(rot.coffs[4], 0.0, 1e-10);   // cos(90) ≈ 0
    EXPECT_NEAR(rot.coffs[8], 1.0, 1e-10);   // Z axis unchanged
}

// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose2d, Equality)
{
    const ViewPose2d a(Point3d::From(0, 0, 0), Vector3d::From(100, 100, 0), 0.0);
    const ViewPose2d b(Point3d::From(0, 0, 0), Vector3d::From(100, 100, 0), 0.0);
    EXPECT_TRUE(a.equals(b));

    const ViewPose2d c(Point3d::From(0, 0, 0), Vector3d::From(100, 100, 0), 45.0);
    EXPECT_FALSE(a.equals(c));
}

// Cross-type equality
// Ported from: itwinjs-core core/frontend/src/test/ViewPose.test.ts
TEST(ViewPose, CrossTypeEquality)
{
    const ViewPose3d pose3d(Point3d::FromZero(), Vector3d::From(100, 100, 100),
                            Matrix3d::CreateIdentity(), Camera(), false);
    const ViewPose2d pose2d(Point3d::FromZero(), Vector3d::From(100, 100, 0), 0.0);

    // Different types should not be equal
    EXPECT_FALSE(pose3d.equals(pose2d));
    EXPECT_FALSE(pose2d.equals(pose3d));
}

// Authored: no reference test exists in itwinjs-core for savePose/applyPose as an
//           isolated round-trip (reference exercises undo via live viewports);
//           scenario from ViewState.ts:1532-1547 (ViewState3d.savePose/applyPose).
TEST(ViewPoseTest, SavePoseApplyPoseRoundTrip)
{
    auto view = dqApp::SpatialViewState::CreateBlank(nullptr, {0, 0, 0}, {100, 100, 100});
    view->SetOrigin(dqGeom::Point3d::From(10, 20, 30));
    view->SetExtents(dqGeom::Vector3d::From(500, 600, 700));
    view->SetRotation(dqApp::StandardView::Iso());
    view->EnableCamera();
    view->setEyePoint(dqGeom::Point3d::From(1, 2, 3));

    auto pose = view->savePose();
    ASSERT_NE(pose, nullptr);

    // 破坏现场
    view->SetOrigin(dqGeom::Point3d::From(-1, -2, -3));
    view->SetExtents(dqGeom::Vector3d::From(1, 1, 1));
    view->SetRotation(dqGeom::Matrix3d::CreateIdentity());
    view->TurnCameraOff();

    view->applyPose(*pose);
    EXPECT_NEAR(view->GetOrigin().x, 10.0, 1e-9);
    EXPECT_NEAR(view->GetOrigin().y, 20.0, 1e-9);
    EXPECT_NEAR(view->GetExtents().x, 500.0, 1e-9);
    EXPECT_TRUE(view->getRotation().IsAlmostEqual(dqApp::StandardView::Iso()));
    EXPECT_TRUE(view->IsCameraOn());
    EXPECT_NEAR(view->GetCamera().eye.x, 1.0, 1e-9);
}

// Authored: no reference test exists in itwinjs-core for ViewPose.equalState in
//           isolation; scenario from ViewPose.ts:108-117 (ViewPose3d.equalState).
TEST(ViewPoseTest, EqualStateMatchesViewState)
{
    auto view = dqApp::SpatialViewState::CreateBlank(nullptr, {5, 6, 7}, {100, 200, 300});
    auto pose = view->savePose();
    EXPECT_TRUE(pose->equalState(*view));

    view->SetOrigin(dqGeom::Point3d::From(9, 6, 7));   // origin 变化 → 不等
    EXPECT_FALSE(pose->equalState(*view));
    view->SetOrigin(dqGeom::Point3d::From(5, 6, 7));
    EXPECT_TRUE(pose->equalState(*view));

    view->EnableCamera();                               // cameraOn 变化 → 不等
    EXPECT_FALSE(pose->equalState(*view));
}
