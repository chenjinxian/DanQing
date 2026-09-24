// StandardViewTest — Verify standard view rotation matrices and SetStandardView
// Ported from: itwinjs-core core/frontend/src/test/StandardView.test.ts
#include <gtest/gtest.h>

#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>

#include <cmath>

// Ported from: itwinjs-core StandardView.test.ts — verify rotation matrices are orthonormal
TEST(StandardView, MatricesAreOrthonormal)
{
    auto checkOrthonormal = [](dqGeom::Matrix3d const& mat, const char* name) {
        auto rowX = mat.RowX();
        auto rowY = mat.RowY();
        auto rowZ = mat.RowZ();

        // Each row should be unit length
        double lenX = std::sqrt(rowX.x * rowX.x + rowX.y * rowX.y + rowX.z * rowX.z);
        double lenY = std::sqrt(rowY.x * rowY.x + rowY.y * rowY.y + rowY.z * rowY.z);
        double lenZ = std::sqrt(rowZ.x * rowZ.x + rowZ.y * rowZ.y + rowZ.z * rowZ.z);

        EXPECT_NEAR(lenX, 1.0, 1e-10) << name << " rowX not unit length";
        EXPECT_NEAR(lenY, 1.0, 1e-10) << name << " rowY not unit length";
        EXPECT_NEAR(lenZ, 1.0, 1e-10) << name << " rowZ not unit length";

        // Rows should be orthogonal
        double dotXY = rowX.x * rowY.x + rowX.y * rowY.y + rowX.z * rowY.z;
        double dotXZ = rowX.x * rowZ.x + rowX.y * rowZ.y + rowX.z * rowZ.z;
        double dotYZ = rowY.x * rowZ.x + rowY.y * rowZ.y + rowY.z * rowZ.z;

        EXPECT_NEAR(dotXY, 0.0, 1e-10) << name << " rowX not orthogonal to rowY";
        EXPECT_NEAR(dotXZ, 0.0, 1e-10) << name << " rowX not orthogonal to rowZ";
        EXPECT_NEAR(dotYZ, 0.0, 1e-10) << name << " rowY not orthogonal to rowZ";
    };

    checkOrthonormal(dqApp::StandardView::Top(), "Top");
    checkOrthonormal(dqApp::StandardView::Bottom(), "Bottom");
    checkOrthonormal(dqApp::StandardView::Left(), "Left");
    checkOrthonormal(dqApp::StandardView::Right(), "Right");
    checkOrthonormal(dqApp::StandardView::Front(), "Front");
    checkOrthonormal(dqApp::StandardView::Back(), "Back");
    checkOrthonormal(dqApp::StandardView::Iso(), "Iso");
    checkOrthonormal(dqApp::StandardView::RightIso(), "RightIso");
}

// Ported from: itwinjs-core StandardView.test.ts — verify Top is identity
TEST(StandardView, TopIsIdentity)
{
    auto top = dqApp::StandardView::Top();
    auto identity = dqGeom::Matrix3d::CreateIdentity();

    auto topX = top.RowX(); auto idX = identity.RowX();
    auto topY = top.RowY(); auto idY = identity.RowY();
    auto topZ = top.RowZ(); auto idZ = identity.RowZ();

    EXPECT_NEAR(topX.x, idX.x, 1e-10);
    EXPECT_NEAR(topX.y, idX.y, 1e-10);
    EXPECT_NEAR(topX.z, idX.z, 1e-10);
    EXPECT_NEAR(topY.x, idY.x, 1e-10);
    EXPECT_NEAR(topY.y, idY.y, 1e-10);
    EXPECT_NEAR(topY.z, idY.z, 1e-10);
    EXPECT_NEAR(topZ.x, idZ.x, 1e-10);
    EXPECT_NEAR(topZ.y, idZ.y, 1e-10);
    EXPECT_NEAR(topZ.z, idZ.z, 1e-10);
}

// Ported from: itwinjs-core StandardView.test.ts — verify GetStandardRotation matches named accessors
TEST(StandardView, GetStandardRotationMatchesAccessors)
{
    auto checkMatch = [](dqGeom::Matrix3d const& a, dqGeom::Matrix3d const& b, const char* name) {
        EXPECT_NEAR(a.RowX().x, b.RowX().x, 1e-10) << name;
        EXPECT_NEAR(a.RowX().y, b.RowX().y, 1e-10) << name;
        EXPECT_NEAR(a.RowX().z, b.RowX().z, 1e-10) << name;
        EXPECT_NEAR(a.RowY().x, b.RowY().x, 1e-10) << name;
        EXPECT_NEAR(a.RowY().y, b.RowY().y, 1e-10) << name;
        EXPECT_NEAR(a.RowY().z, b.RowY().z, 1e-10) << name;
        EXPECT_NEAR(a.RowZ().x, b.RowZ().x, 1e-10) << name;
        EXPECT_NEAR(a.RowZ().y, b.RowZ().y, 1e-10) << name;
        EXPECT_NEAR(a.RowZ().z, b.RowZ().z, 1e-10) << name;
    };

    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Top), dqApp::StandardView::Top(), "Top");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Bottom), dqApp::StandardView::Bottom(), "Bottom");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Front), dqApp::StandardView::Front(), "Front");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Back), dqApp::StandardView::Back(), "Back");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Left), dqApp::StandardView::Left(), "Left");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Right), dqApp::StandardView::Right(), "Right");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso), dqApp::StandardView::Iso(), "Iso");
    checkMatch(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::RightIso), dqApp::StandardView::RightIso(), "RightIso");
}

// Verify SetStandardView sets the correct rotation on ViewState3d
TEST(StandardView, SetStandardViewSetsRotation)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));

    auto* view3d = view->AsViewState3d();
    ASSERT_NE(view3d, nullptr);

    // Set to Front view
    view3d->SetStandardView(static_cast<int>(dqApp::StandardViewId::Front));
    auto frontRot = dqApp::StandardView::Front();
    auto actualRot = view3d->getRotation();

    EXPECT_NEAR(actualRot.RowX().x, frontRot.RowX().x, 1e-10);
    EXPECT_NEAR(actualRot.RowX().y, frontRot.RowX().y, 1e-10);
    EXPECT_NEAR(actualRot.RowX().z, frontRot.RowX().z, 1e-10);
    EXPECT_NEAR(actualRot.RowY().x, frontRot.RowY().x, 1e-10);
    EXPECT_NEAR(actualRot.RowY().y, frontRot.RowY().y, 1e-10);
    EXPECT_NEAR(actualRot.RowY().z, frontRot.RowY().z, 1e-10);
    EXPECT_NEAR(actualRot.RowZ().x, frontRot.RowZ().x, 1e-10);
    EXPECT_NEAR(actualRot.RowZ().y, frontRot.RowZ().y, 1e-10);
    EXPECT_NEAR(actualRot.RowZ().z, frontRot.RowZ().z, 1e-10);

    // Set to Iso view
    view3d->SetStandardView(static_cast<int>(dqApp::StandardViewId::Iso));
    auto isoRot = dqApp::StandardView::Iso();
    actualRot = view3d->getRotation();

    EXPECT_NEAR(actualRot.RowX().x, isoRot.RowX().x, 1e-10);
    EXPECT_NEAR(actualRot.RowY().y, isoRot.RowY().y, 1e-10);
    EXPECT_NEAR(actualRot.RowZ().z, isoRot.RowZ().z, 1e-10);
}

// Verify SetStandardView preserves the center point
TEST(StandardView, SetStandardViewPreservesCenter)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(100, 200, 50),
        dqGeom::Vector3d::From(1000, 1000, 1000));

    auto* view3d = view->AsViewState3d();
    ASSERT_NE(view3d, nullptr);

    auto centerBefore = view3d->getCenter();

    // Change to a different standard view
    view3d->SetStandardView(static_cast<int>(dqApp::StandardViewId::Right));

    auto centerAfter = view3d->getCenter();

    // Center should be approximately preserved (within floating point tolerance)
    EXPECT_NEAR(centerAfter.x, centerBefore.x, 1e-6);
    EXPECT_NEAR(centerAfter.y, centerBefore.y, 1e-6);
    EXPECT_NEAR(centerAfter.z, centerBefore.z, 1e-6);
}

// Verify invalid index defaults to Top
TEST(StandardView, InvalidIndexDefaultsToTop)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));

    auto* view3d = view->AsViewState3d();
    ASSERT_NE(view3d, nullptr);

    // Set an invalid index
    view3d->SetStandardView(99);

    auto topRot = dqApp::StandardView::Top();
    auto actualRot = view3d->getRotation();

    EXPECT_NEAR(actualRot.RowX().x, topRot.RowX().x, 1e-10);
    EXPECT_NEAR(actualRot.RowY().y, topRot.RowY().y, 1e-10);
    EXPECT_NEAR(actualRot.RowZ().z, topRot.RowZ().z, 1e-10);
}

// ---------------------------------------------------------------------------
// adjustToStandardRotation — snap a rotation matrix to the nearest standard view.
// Authored: no reference test exists in itwinjs-core for StandardView
// .adjustToStandardRotation (the function's sole caller is ViewState.setupFromFrustum
// at ViewState.ts:751; upstream tests never assert the snap behavior directly).
// ----------------------------------------------------------------------------

// Authored: no reference test exists in itwinjs-core for StandardView.adjustToStandardRotation
TEST(StandardView, AdjustToStandardRotationSnapsExactStandardToItself)
{
    // A matrix already exactly equal to a standard rotation (maxDiff == 0 <= 1e-7) snaps
    // to that same rotation (idempotent).
    auto verify = [](dqApp::StandardViewId id) {
        auto m = dqApp::StandardView::GetStandardRotation(id);
        auto canonical = m;
        dqApp::StandardView::adjustToStandardRotation(m);
        EXPECT_TRUE(m.IsExactEqual(canonical)) << "id=" << static_cast<int>(id);
    };
    verify(dqApp::StandardViewId::Top);
    verify(dqApp::StandardViewId::Bottom);
    verify(dqApp::StandardViewId::Left);
    verify(dqApp::StandardViewId::Right);
    verify(dqApp::StandardViewId::Front);
    verify(dqApp::StandardViewId::Back);
    verify(dqApp::StandardViewId::Iso);
    verify(dqApp::StandardViewId::RightIso);
}

// Authored: no reference test exists in itwinjs-core for StandardView.adjustToStandardRotation
TEST(StandardView, AdjustToStandardRotationSnapsWithinThreshold)
{
    // Identity perturbed by 5e-8 in one entry (< 1e-7) snaps back to exact Top
    // (StandardView.ts:87 threshold is the literal 1e-7).
    auto nearTop = dqGeom::Matrix3d::CreateRowValues(
        1.0 + 5e-8, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0);
    dqApp::StandardView::adjustToStandardRotation(nearTop);
    EXPECT_TRUE(nearTop.IsExactEqual(dqApp::StandardView::Top()));
}

// Authored: no reference test exists in itwinjs-core for StandardView.adjustToStandardRotation
TEST(StandardView, AdjustToStandardRotationLeavesOverThresholdUnchanged)
{
    // Identity perturbed by 1e-6 in one entry (> 1e-7) is NOT snapped (and is far from
    // every other standard rotation, so it is left bit-identical).
    auto overTop = dqGeom::Matrix3d::CreateRowValues(
        1.0 + 1e-6, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0);
    auto before = overTop;
    dqApp::StandardView::adjustToStandardRotation(overTop);
    EXPECT_TRUE(overTop.IsExactEqual(before));
}

// Authored: no reference test exists in itwinjs-core for StandardView.adjustToStandardRotation
TEST(StandardView, AdjustToStandardRotationLeavesNonStandardUnchanged)
{
    // A 30-degree rotation about Z is far from every standard rotation → unchanged.
    auto rot30 = dqGeom::Matrix3d::CreateRowValues(
        0.8660254037844386, -0.5, 0.0,
        0.5, 0.8660254037844386, 0.0,
        0.0, 0.0, 1.0);
    auto before = rot30;
    dqApp::StandardView::adjustToStandardRotation(rot30);
    EXPECT_TRUE(rot30.IsExactEqual(before));
}
