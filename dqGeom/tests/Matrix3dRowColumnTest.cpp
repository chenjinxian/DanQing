// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Matrix3d createColumns/createRows regression tests
// Ported from: itwinjs-core core/geometry/src/test/geometry3d/Matrix3d.test.ts
//              describe("Matrix3d.RowColumn") it("Matrix3d.RowColumn") (:969-1000, subset)
//
// 子集说明：normalizeRowsInPlace / maxAbs 演化 / setAt 部分未移植（对应 API 未移植）；
// 本文件覆盖 createColumns 布局断言（getColumn↔getRow 相等、转置元素相等、
// matrixByRow == matrixByColumn.transpose()）——即 ueBIM 初始导入的
// CreateColumns 誊录笔误（coffs[5] 误写 colZ.z）的回归守卫。
#include <gtest/gtest.h>

#include <dqGeom/Matrix3d.h>

using namespace dqGeom;

namespace {
void expectVector3dEqual(Vector3d const& a, Vector3d const& b, char const* msg)
{
    // ck.testVector3d → Geometry.isSameVector3d（逐分量 isSameCoordinate，容差 1e-6）
    EXPECT_NEAR(a.x, b.x, 1.0e-6) << msg;
    EXPECT_NEAR(a.y, b.y, 1.0e-6) << msg;
    EXPECT_NEAR(a.z, b.z, 1.0e-6) << msg;
}
}  // namespace

// Ported from: Matrix3d.test.ts describe("Matrix3d.RowColumn") (:969-1000 的
// createColumns 部分，:985-997)
TEST(Matrix3dRowColumnTest, CreateColumnsLayout)
{
    Vector3d const vectorX = Vector3d::From(1, 2, 4);
    Vector3d const vectorY = Vector3d::From(3, 9, 27);
    Vector3d const vectorZ = Vector3d::From(5, 25, 125);
    // createRows(vectorX, vectorY, vectorZ)：行依次为三个向量
    Matrix3d const matrixByRow = Matrix3d::CreateRowValues(
        vectorX.x, vectorX.y, vectorX.z,
        vectorY.x, vectorY.y, vectorY.z,
        vectorZ.x, vectorZ.y, vectorZ.z);

    Matrix3d const matrixByColumn = Matrix3d::CreateColumns(vectorX, vectorY, vectorZ);
    for (int i = 0; i < 3; ++i) {
        // ck.testVector3d(matrixByColumn.getColumn(i), matrixByRow.getRow(i))
        expectVector3dEqual(matrixByColumn.ColumnX(), matrixByRow.RowX(), "col0 == row0");
        expectVector3dEqual(matrixByColumn.ColumnY(), matrixByRow.RowY(), "col1 == row1");
        expectVector3dEqual(matrixByColumn.ColumnZ(), matrixByRow.RowZ(), "col2 == row2");
        for (int j = 0; j < 3; ++j) {
            // ck.testExactNumber(matrixByRow.at(i, j), matrixByColumn.at(j, i))
            EXPECT_DOUBLE_EQ(matrixByRow.at(i, j), matrixByColumn.at(j, i))
                << "transposed elements equal at (" << i << "," << j << ")";
        }
    }
    // ck.testMatrix3d(matrixByRow, matrixByColumn.transpose())
    EXPECT_TRUE(matrixByRow.IsAlmostEqual(matrixByColumn.Transpose()))
        << "matrixByRow is transpose of matrixByColumn";
}
