// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/Matrix3d.test.ts
//              (createRigidHeadsUp / createRigidFromColumns / createPerpendicularVectorFavorXYPlane)
// DanQing dqGeom - rigid-frame construction unit tests
#include "dqGeom/Matrix3d.h"
#include "dqGeom/Vector3d.h"

#include <gtest/gtest.h>

using namespace dqGeom;

// Ported from: Matrix3d.createRigidHeadsUp (Matrix3d.ts:733-743) with +Z normal and
// ZXY order -> identity frame (Z axis = +Z).
TEST(Matrix3dRigidTest, CreateRigidHeadsUpZIsIdentity)
{
    Matrix3d const m = Matrix3d::CreateRigidHeadsUp(Vector3d::UnitZ(), AxisOrder::ZXY);
    EXPECT_NEAR(m.ColumnZ().x, 0.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().z, 1.0, 1e-12);
    // X and Y columns are +X and +Y (identity).
    EXPECT_NEAR(m.ColumnX().x, 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnY().y, 1.0, 1e-12);
}

// Ported from: Matrix3d.createRigidHeadsUp - the Z column aligns with the input normal
// and the frame is orthonormal.
TEST(Matrix3dRigidTest, CreateRigidHeadsUpAlignsZToNormal)
{
    Matrix3d const m = Matrix3d::CreateRigidHeadsUp(Vector3d::UnitX(), AxisOrder::ZXY);
    // Z column = +X (the input normal).
    EXPECT_NEAR(m.ColumnZ().x, 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().y, 0.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().z, 0.0, 1e-12);
    // Orthonormal columns.
    EXPECT_NEAR(m.ColumnX().Magnitude(), 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnY().Magnitude(), 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().Magnitude(), 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnX().DotProduct(m.ColumnY()), 0.0, 1e-12);
    EXPECT_NEAR(m.ColumnX().DotProduct(m.ColumnZ()), 0.0, 1e-12);
    EXPECT_NEAR(m.ColumnY().DotProduct(m.ColumnZ()), 0.0, 1e-12);
}

// Ported from: Matrix3d.createRigidHeadsUp - input is normalized.
TEST(Matrix3dRigidTest, CreateRigidHeadsUpNormalizesInput)
{
    Matrix3d const m = Matrix3d::CreateRigidHeadsUp(Vector3d::From(0, 0, 5), AxisOrder::ZXY);
    EXPECT_NEAR(m.ColumnZ().z, 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().Magnitude(), 1.0, 1e-12);
}

// Ported from: Matrix3d.createRigidHeadsUp - degenerate input falls back to identity.
TEST(Matrix3dRigidTest, CreateRigidHeadsUpZeroFallsBackToIdentity)
{
    Matrix3d const m = Matrix3d::CreateRigidHeadsUp(Vector3d::FromZero(), AxisOrder::ZXY);
    EXPECT_NEAR(m.ColumnX().x, 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnY().y, 1.0, 1e-12);
    EXPECT_NEAR(m.ColumnZ().z, 1.0, 1e-12);
}

// Ported from: Matrix3d.createPerpendicularVectorFavorXYPlane - result is perpendicular
// to the input.
TEST(Matrix3dRigidTest, CreatePerpendicularVectorFavorXYPlane)
{
    Vector3d const n = Vector3d::From(1, 2, 3);
    Vector3d const p = Matrix3d::CreatePerpendicularVectorFavorXYPlane(n);
    EXPECT_NEAR(p.DotProduct(n), 0.0, 1e-12);
}
