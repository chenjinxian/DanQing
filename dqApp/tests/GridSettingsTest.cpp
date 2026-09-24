// GridSettingsTest — ViewState3d grid-settings API (D8.1/D8.2)
// Authored: no reference test exists in itwinjs-core for the grid-settings wrapper
// API (getGridOrientation/getGridsPerRef/getGridSpacing/setGridSettings/getGridSettings);
// the only upstream coverage (full-stack-tests/core/src/frontend/standalone/ViewState
// .test.ts:67-68) reads seeded iModel JSON values, not these accessors or their defaults.
#include <gtest/gtest.h>

#include <dqApp/StandardView.h>
#include <dqApp/ViewState.h>

#include <dqCommon/GridOrientationType.h>

// Authored: no reference test exists in itwinjs-core for the grid-settings defaults
TEST(GridSettings, DefaultsMatchReference)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    // ViewDetails.ts:24-31 — orientation=WorldXY, gridsPerRef=10, spacing={1,1}.
    EXPECT_EQ(v3->getGridOrientation(), dqCommon::GridOrientationType::WorldXY);
    EXPECT_EQ(v3->getGridsPerRef(), 10);
    EXPECT_NEAR(v3->getGridSpacing().x, 1.0, 1e-12);
    EXPECT_NEAR(v3->getGridSpacing().y, 1.0, 1e-12);
}

// Authored: no reference test exists in itwinjs-core for setGridSettings
TEST(GridSettings, SetGridSettingsPersists)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    v3->setGridSettings(dqCommon::GridOrientationType::WorldYZ,
                        dqGeom::Point2d{2.0, 3.0}, 5);
    EXPECT_EQ(v3->getGridOrientation(), dqCommon::GridOrientationType::WorldYZ);
    EXPECT_EQ(v3->getGridsPerRef(), 5);
    EXPECT_NEAR(v3->getGridSpacing().x, 2.0, 1e-12);
    EXPECT_NEAR(v3->getGridSpacing().y, 3.0, 1e-12);
}

// Authored: no reference test exists in itwinjs-core for getGridSettings matrix output
TEST(GridSettings, GetGridSettingsWorldXYIsIdentity)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    dqGeom::Point3d origin{5, 5, 5};
    dqGeom::Matrix3d rMatrix = dqGeom::Matrix3d::CreateRowValues(
        9, 9, 9, 9, 9, 9, 9, 9, 9);  // sentinel — must be overwritten
    v3->getGridSettings(origin, rMatrix, dqCommon::GridOrientationType::WorldXY);
    // WorldXY: identity matrix (ViewState.ts:985-986).
    EXPECT_TRUE(rMatrix.IsExactEqual(dqGeom::Matrix3d::CreateIdentity()));
}

// Authored: no reference test exists in itwinjs-core for getGridSettings WorldYZ row cycle
TEST(GridSettings, GetGridSettingsWorldYZRowCycle)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    dqGeom::Point3d origin{0, 0, 0};
    dqGeom::Matrix3d rMatrix = dqGeom::Matrix3d::CreateIdentity();
    v3->getGridSettings(origin, rMatrix, dqCommon::GridOrientationType::WorldYZ);
    // ViewState.ts:987-994 — (X,Y,Z) -> (Y,Z,X): rows become (0,1,0),(0,0,1),(1,0,0).
    auto expected = dqGeom::Matrix3d::CreateRowValues(
        0, 1, 0,
        0, 0, 1,
        1, 0, 0);
    EXPECT_TRUE(rMatrix.IsExactEqual(expected));
}

// Authored: no reference test exists in itwinjs-core for getGridSettings WorldXZ row cycle
TEST(GridSettings, GetGridSettingsWorldXZRowCycle)
{
    auto view = dqApp::SpatialViewState::CreateBlank(
        nullptr, dqGeom::Point3d::From(0, 0, 0),
        dqGeom::Vector3d::From(1000, 1000, 1000));
    auto* v3 = view->AsViewState3d();
    ASSERT_NE(v3, nullptr);
    dqGeom::Point3d origin{0, 0, 0};
    dqGeom::Matrix3d rMatrix = dqGeom::Matrix3d::CreateIdentity();
    v3->getGridSettings(origin, rMatrix, dqCommon::GridOrientationType::WorldXZ);
    // ViewState.ts:996-1003 — (X,Y,Z) -> (X,Z,Y): rows become (1,0,0),(0,0,1),(0,1,0).
    auto expected = dqGeom::Matrix3d::CreateRowValues(
        1, 0, 0,
        0, 0, 1,
        0, 1, 0);
    EXPECT_TRUE(rMatrix.IsExactEqual(expected));
}
