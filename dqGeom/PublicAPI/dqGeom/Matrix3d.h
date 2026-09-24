// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Matrix3d
//
// Ported from: itwinjs-core core/geometry/src/geometry3d/Matrix3d.ts
//   Matrix3d, PackedMatrix3dOps
//
// 3x3 matrix stored in row-major order: coffs[0..2] = row0, coffs[3..5] = row1, coffs[6..8] = row2.
// Used for rotations, scales, and general linear transforms.
#pragma once

#include "Export.h"
#include "Point3d.h"
#include "Point4d.h"
#include "Vector3d.h"

#include <array>
#include <cmath>
#include <cstring>
#include <optional>

namespace dqGeom {

/// Axis ordering (right-handed XYZ/YZX/ZXY, left-handed XZY/YXZ/ZYX).
/// Ported from: itwinjs-core Geometry.AxisOrder (Geometry.ts:25-39)
enum class AxisOrder : int {
    XYZ = 0,
    YZX = 1,
    ZXY = 2,
    XZY = 4,
    YXZ = 5,
    ZYX = 6,
};

/// Map (axisOrder, index 0..2) to an axis index 0=X, 1=Y, 2=Z.
/// Ported from: itwinjs-core Geometry.axisOrderToAxis (Geometry.ts:1075-1078)
inline int axisOrderToAxis(AxisOrder order, int index) noexcept {
    int const o = static_cast<int>(order);
    int axis = (o <= static_cast<int>(AxisOrder::ZXY)) ? (o + index)
                                                       : (o - static_cast<int>(AxisOrder::XZY)) - index;
    return ((axis % 3) + 3) % 3;  // Geometry.cyclic3dAxis
}

/// 3x3 matrix in row-major order.
/// Ported from: itwinjs-core Matrix3d
struct DQ_GEOM_EXPORT Matrix3d {
    /// Coefficients in row-major order: [row0col0, row0col1, row0col2, row1col0, ...]
    std::array<double, 9> coffs;

    /// Construct zero matrix.
    Matrix3d() : coffs{} {}

    /// Construct from row values.
    Matrix3d(
        double a00, double a01, double a02,
        double a10, double a11, double a12,
        double a20, double a21, double a22
    ) : coffs{a00, a01, a02, a10, a11, a12, a20, a21, a22}
    {}

    // --- Static factories ---

    /// Create identity matrix.
    /// Ported from: itwinjs-core Matrix3d.createIdentity
    static Matrix3d CreateIdentity()
    {
        return Matrix3d(
            1, 0, 0,
            0, 1, 0,
            0, 0, 1
        );
    }

    /// Create zero matrix.
    /// Ported from: itwinjs-core Matrix3d.createZero
    static Matrix3d CreateZero()
    {
        return Matrix3d();
    }

    /// Create scale matrix.
    /// Ported from: itwinjs-core Matrix3d.createScale
    static Matrix3d CreateScale(double sx, double sy, double sz)
    {
        return Matrix3d(sx, 0, 0, 0, sy, 0, 0, 0, sz);
    }

    /// Create uniform scale matrix.
    /// Ported from: itwinjs-core Matrix3d.createUniformScale
    static Matrix3d CreateUniformScale(double s)
    {
        return CreateScale(s, s, s);
    }

    /// Create rotation matrix from a quaternion (x, y, z, w).
    /// Ported from: itwinjs-core Matrix3d.createFromQuaternion (geometry3d/
    /// Matrix3d.ts:2879-2901). Returns the matrix that the reference's
    /// trsMatrix then **transposes in place** for the glTF/glTF-instancing
    /// convention — callers needing the glTF layout must apply
    /// `transposeInPlace()` (see GltfReader trsMatrix usage).
    static Matrix3d CreateFromQuaternion(double qx, double qy, double qz, double qw)
    {
        double const qqx = qx * qx, qqy = qy * qy, qqz = qz * qz, qqw = qw * qw;
        double const mag2 = qqx + qqy + qqz + qqw;
        if (mag2 == 0.0)
            return CreateIdentity();
        double const a = 1.0 / mag2;
        return CreateRowValues(
            a * (qqw + qqx - qqy - qqz),
            2.0 * a * (qw * qz + qx * qy),
            2.0 * a * (qx * qz - qw * qy),
            2.0 * a * (qx * qy - qw * qz),
            a * (qqw - qqx + qqy - qqz),
            2.0 * a * (qw * qx + qy * qz),
            2.0 * a * (qx * qz + qw * qy),
            2.0 * a * (qy * qz - qw * qx),
            a * (qqw - qqx - qqy + qqz));
    }

    /// Create from row values.
    /// Ported from: itwinjs-core Matrix3d.createRowValues
    static Matrix3d CreateRowValues(
        double a00, double a01, double a02,
        double a10, double a11, double a12,
        double a20, double a21, double a22
    )
    {
        return Matrix3d(a00, a01, a02, a10, a11, a12, a20, a21, a22);
    }

    /// Create from column vectors.
    /// Ported from: itwinjs-core Matrix3d.createColumns
    static Matrix3d CreateColumns(const Vector3d& colX, const Vector3d& colY, const Vector3d& colZ)
    {
        return Matrix3d(
            colX.x, colY.x, colZ.x,
            colX.y, colY.y, colZ.y,
            colX.z, colY.z, colZ.z
        );
    }

    /// Create a vector perpendicular to `vectorA`, favoring the XY plane.
    /// Ported from: itwinjs-core Matrix3d.createPerpendicularVectorFavorXYPlane
    static Vector3d CreatePerpendicularVectorFavorXYPlane(Vector3d const& vectorA) noexcept
    {
        double const a = vectorA.Magnitude();
        double const b = a / 64.0;  // "A constant from the dawn of time in the CAD industry"
        if (std::abs(vectorA.x) < b && std::abs(vectorA.y) < b) {
            // vectorA is close to the Z axis -> cross with (0,-1,0).
            return Vector3d::FromCrossProduct(vectorA.x, vectorA.y, vectorA.z, 0.0, -1.0, 0.0);
        }
        // otherwise cross with (0,0,1).
        return Vector3d::FromCrossProduct(0.0, 0.0, 1.0, vectorA.x, vectorA.y, vectorA.z);
    }

    /// Create a matrix with columns (vectorU, vectorV, vectorW) placed at axes per `axisOrder`.
    /// Ported from: itwinjs-core Matrix3d.createShuffledColumns
    static Matrix3d CreateShuffledColumns(Vector3d const& vectorU, Vector3d const& vectorV,
                                          Vector3d const& vectorW, AxisOrder axisOrder) noexcept
    {
        Matrix3d target = CreateIdentity();
        auto setCol = [&target](int axisIndex, Vector3d const& v) {
            if (axisIndex == 0) target.SetColumnX(v);
            else if (axisIndex == 1) target.SetColumnY(v);
            else target.SetColumnZ(v);
        };
        setCol(axisOrderToAxis(axisOrder, 0), vectorU);
        setCol(axisOrderToAxis(axisOrder, 1), vectorV);
        setCol(axisOrderToAxis(axisOrder, 2), vectorW);
        return target;
    }

    /// Create a rigid (orthonormal) matrix from two columns; vectorA is normalized, the
    /// other two axes are derived orthonormal. Returns nullopt if vectorA is (near) zero
    /// or vectorB is parallel to it. (setupInverseTranspose is a no-op for a rigid matrix:
    /// inverse-transpose == self.)
    /// Ported from: itwinjs-core Matrix3d.createRigidFromColumns
    static std::optional<Matrix3d> CreateRigidFromColumns(Vector3d vectorA, Vector3d const& vectorB,
                                                          AxisOrder axisOrder) noexcept
    {
        if (vectorA.Normalize() < 1.0e-15)
            return std::nullopt;  // vectorA near zero
        Vector3d vectorC1 = Vector3d::FromCrossProduct(vectorA, vectorB);
        if (vectorC1.Normalize() < 1.0e-15)
            return std::nullopt;  // parallel
        Vector3d vectorB1 = Vector3d::FromCrossProduct(vectorC1, vectorA);
        vectorB1.Normalize();  // inputs orthonormal -> already unit
        return CreateShuffledColumns(vectorA, vectorB1, vectorC1, axisOrder);
    }

    /// Create a rigid frame whose `axisOrder`-first axis aligns with `vectorA` (defaults
    /// ZXY -> Z axis = vectorA). Returns identity if `vectorA` is degenerate.
    /// Ported from: itwinjs-core Matrix3d.createRigidHeadsUp (Matrix3d.ts:733-743)
    static Matrix3d CreateRigidHeadsUp(Vector3d const& vectorA,
                                       AxisOrder axisOrder = AxisOrder::ZXY) noexcept
    {
        Vector3d const vectorB = CreatePerpendicularVectorFavorXYPlane(vectorA);
        auto matrix = CreateRigidFromColumns(vectorA, vectorB, axisOrder);
        return matrix ? *matrix : CreateIdentity();
    }

    /// Create rotation around arbitrary axis.
    /// Ported from: itwinjs-core Matrix3d.createRotationAroundVector
    static Matrix3d CreateRotationAroundAxis(const Vector3d& axis, double radians)
    {
        double c = std::cos(radians);
        double s = std::sin(radians);
        double t = 1.0 - c;
        double ax = axis.x, ay = axis.y, az = axis.z;
        return Matrix3d(
            t * ax * ax + c,      t * ax * ay - s * az, t * ax * az + s * ay,
            t * ax * ay + s * az, t * ay * ay + c,      t * ay * az - s * ax,
            t * ax * az - s * ay, t * ay * az + s * ax, t * az * az + c
        );
    }

    /// Create rotation around X axis.
    /// Ported from: itwinjs-core Matrix3d.createRotationAroundAxisIndex
    static Matrix3d CreateRotationAroundX(double radians)
    {
        double c = std::cos(radians);
        double s = std::sin(radians);
        return Matrix3d(1, 0, 0, 0, c, -s, 0, s, c);
    }

    /// Create rotation around Y axis.
    static Matrix3d CreateRotationAroundY(double radians)
    {
        double c = std::cos(radians);
        double s = std::sin(radians);
        return Matrix3d(c, 0, s, 0, 1, 0, -s, 0, c);
    }

    /// Create rotation around Z axis.
    static Matrix3d CreateRotationAroundZ(double radians)
    {
        double c = std::cos(radians);
        double s = std::sin(radians);
        return Matrix3d(c, -s, 0, s, c, 0, 0, 0, 1);
    }

    // --- State queries ---

    /// Set to identity.
    /// Ported from: itwinjs-core Matrix3d.setIdentity
    void SetIdentity()
    {
        coffs = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    }

    /// Set to zero.
    /// Ported from: itwinjs-core Matrix3d.setZero
    void SetZero()
    {
        coffs = {};
    }

    /// Set row values.
    /// Ported from: itwinjs-core Matrix3d.setRowValues
    void SetRowValues(
        double a00, double a01, double a02,
        double a10, double a11, double a12,
        double a20, double a21, double a22
    )
    {
        coffs = {a00, a01, a02, a10, a11, a12, a20, a21, a22};
    }

    /// Copy from other matrix.
    /// Ported from: itwinjs-core Matrix3d.setFrom
    void SetFrom(const Matrix3d& other)
    {
        coffs = other.coffs;
    }

    /// clone this matrix.
    /// Ported from: itwinjs-core Matrix3d.clone
    Matrix3d clone() const { return *this; }

    /// Test approximate equality.
    /// Ported from: itwinjs-core Matrix3d.isAlmostEqual
    bool IsAlmostEqual(const Matrix3d& other, double tol = kSmallMetricDistance) const
    {
        for (int i = 0; i < 9; ++i)
            if (std::abs(coffs[i] - other.coffs[i]) > tol)
                return false;
        return true;
    }

    /// Test exact equality.
    /// Ported from: itwinjs-core Matrix3d.isExactEqual
    bool IsExactEqual(const Matrix3d& other) const
    {
        return coffs == other.coffs;
    }

    // --- Column access ---

    /// Get column X (column 0).
    /// Ported from: itwinjs-core Matrix3d.columnX
    Vector3d ColumnX() const { return Vector3d::From(coffs[0], coffs[3], coffs[6]); }

    /// Get column Y (column 1).
    /// Ported from: itwinjs-core Matrix3d.columnY
    Vector3d ColumnY() const { return Vector3d::From(coffs[1], coffs[4], coffs[7]); }

    /// Get column Z (column 2).
    /// Ported from: itwinjs-core Matrix3d.columnZ
    Vector3d ColumnZ() const { return Vector3d::From(coffs[2], coffs[5], coffs[8]); }

    /// Set column X (column 0).
    void SetColumnX(const Vector3d& col)
    {
        coffs[0] = col.x; coffs[3] = col.y; coffs[6] = col.z;
    }

    /// Set column Y (column 1).
    void SetColumnY(const Vector3d& col)
    {
        coffs[1] = col.x; coffs[4] = col.y; coffs[7] = col.z;
    }

    /// Set column Z (column 2).
    void SetColumnZ(const Vector3d& col)
    {
        coffs[2] = col.x; coffs[5] = col.y; coffs[8] = col.z;
    }

    // --- Row access ---

    /// Get row X (row 0).
    /// Ported from: itwinjs-core Matrix3d.rowX
    Vector3d RowX() const { return Vector3d::From(coffs[0], coffs[1], coffs[2]); }

    /// Get row Y (row 1).
    /// Ported from: itwinjs-core Matrix3d.rowY
    Vector3d RowY() const { return Vector3d::From(coffs[3], coffs[4], coffs[5]); }

    /// Get row Z (row 2).
    /// Ported from: itwinjs-core Matrix3d.rowZ
    Vector3d RowZ() const { return Vector3d::From(coffs[6], coffs[7], coffs[8]); }

    // --- Magnitude queries ---

    /// Column X magnitude squared.
    double ColumnXMagnitudeSquared() const
    {
        return coffs[0] * coffs[0] + coffs[3] * coffs[3] + coffs[6] * coffs[6];
    }

    /// Column Y magnitude squared.
    double ColumnYMagnitudeSquared() const
    {
        return coffs[1] * coffs[1] + coffs[4] * coffs[4] + coffs[7] * coffs[7];
    }

    /// Column Z magnitude squared.
    double ColumnZMagnitudeSquared() const
    {
        return coffs[2] * coffs[2] + coffs[5] * coffs[5] + coffs[8] * coffs[8];
    }

    /// Column X magnitude.
    double ColumnXMagnitude() const { return std::sqrt(ColumnXMagnitudeSquared()); }

    /// Column Y magnitude.
    double ColumnYMagnitude() const { return std::sqrt(ColumnYMagnitudeSquared()); }

    /// Column Z magnitude.
    double ColumnZMagnitude() const { return std::sqrt(ColumnZMagnitudeSquared()); }

    // --- Dot products ---

    /// Dot product of column X and column Y.
    double ColumnXDotColumnY() const
    {
        return coffs[0] * coffs[1] + coffs[3] * coffs[4] + coffs[6] * coffs[7];
    }

    /// Dot product of column X and column Z.
    double ColumnXDotColumnZ() const
    {
        return coffs[0] * coffs[2] + coffs[3] * coffs[5] + coffs[6] * coffs[8];
    }

    /// Dot product of column Y and column Z.
    double ColumnYDotColumnZ() const
    {
        return coffs[1] * coffs[2] + coffs[4] * coffs[5] + coffs[7] * coffs[8];
    }

    // --- Matrix operations ---

    /// Compute transpose.
    /// Ported from: itwinjs-core PackedMatrix3dOps.copyTransposed
    Matrix3d Transpose() const
    {
        return Matrix3d(
            coffs[0], coffs[3], coffs[6],
            coffs[1], coffs[4], coffs[7],
            coffs[2], coffs[5], coffs[8]
        );
    }

    /// Transpose in place.
    void TransposeInPlace()
    {
        double q;
        q = coffs[1]; coffs[1] = coffs[3]; coffs[3] = q;
        q = coffs[2]; coffs[2] = coffs[6]; coffs[6] = q;
        q = coffs[5]; coffs[5] = coffs[7]; coffs[7] = q;
    }

    /// Compute determinant.
    double Determinant() const
    {
        return coffs[0] * (coffs[4] * coffs[8] - coffs[5] * coffs[7])
             - coffs[1] * (coffs[3] * coffs[8] - coffs[5] * coffs[6])
             + coffs[2] * (coffs[3] * coffs[7] - coffs[4] * coffs[6]);
    }

    /// Compute inverse. Returns false if singular.
    /// Ported from: itwinjs-core Matrix3d.computeCachedInverse
    bool Inverse(Matrix3d& result) const
    {
        double det = Determinant();
        if (std::abs(det) < 1e-15)
            return false;

        double invDet = 1.0 / det;
        result.coffs[0] = (coffs[4] * coffs[8] - coffs[5] * coffs[7]) * invDet;
        result.coffs[1] = (coffs[2] * coffs[7] - coffs[1] * coffs[8]) * invDet;
        result.coffs[2] = (coffs[1] * coffs[5] - coffs[2] * coffs[4]) * invDet;
        result.coffs[3] = (coffs[5] * coffs[6] - coffs[3] * coffs[8]) * invDet;
        result.coffs[4] = (coffs[0] * coffs[8] - coffs[2] * coffs[6]) * invDet;
        result.coffs[5] = (coffs[2] * coffs[3] - coffs[0] * coffs[5]) * invDet;
        result.coffs[6] = (coffs[3] * coffs[7] - coffs[4] * coffs[6]) * invDet;
        result.coffs[7] = (coffs[1] * coffs[6] - coffs[0] * coffs[7]) * invDet;
        result.coffs[8] = (coffs[0] * coffs[4] - coffs[1] * coffs[3]) * invDet;
        return true;
    }

    /// Multiply matrix * vector.
    /// Ported from: itwinjs-core Matrix3d.multiplyVector
    Vector3d MultiplyVector(const Vector3d& v) const
    {
        return Vector3d::From(
            coffs[0] * v.x + coffs[1] * v.y + coffs[2] * v.z,
            coffs[3] * v.x + coffs[4] * v.y + coffs[5] * v.z,
            coffs[6] * v.x + coffs[7] * v.y + coffs[8] * v.z
        );
    }

    /// Multiply transpose * vector.
    Vector3d MultiplyTransposeVector(const Vector3d& v) const
    {
        return Vector3d::From(
            coffs[0] * v.x + coffs[3] * v.y + coffs[6] * v.z,
            coffs[1] * v.x + coffs[4] * v.y + coffs[7] * v.z,
            coffs[2] * v.x + coffs[5] * v.y + coffs[8] * v.z
        );
    }

    /// Multiply matrix * matrix.
    /// Ported from: itwinjs-core PackedMatrix3dOps.multiplyMatrixMatrix
    Matrix3d MultiplyMatrix(const Matrix3d& other) const
    {
        return Matrix3d(
            coffs[0] * other.coffs[0] + coffs[1] * other.coffs[3] + coffs[2] * other.coffs[6],
            coffs[0] * other.coffs[1] + coffs[1] * other.coffs[4] + coffs[2] * other.coffs[7],
            coffs[0] * other.coffs[2] + coffs[1] * other.coffs[5] + coffs[2] * other.coffs[8],
            coffs[3] * other.coffs[0] + coffs[4] * other.coffs[3] + coffs[5] * other.coffs[6],
            coffs[3] * other.coffs[1] + coffs[4] * other.coffs[4] + coffs[5] * other.coffs[7],
            coffs[3] * other.coffs[2] + coffs[4] * other.coffs[5] + coffs[5] * other.coffs[8],
            coffs[6] * other.coffs[0] + coffs[7] * other.coffs[3] + coffs[8] * other.coffs[6],
            coffs[6] * other.coffs[1] + coffs[7] * other.coffs[4] + coffs[8] * other.coffs[7],
            coffs[6] * other.coffs[2] + coffs[7] * other.coffs[5] + coffs[8] * other.coffs[8]
        );
    }

    /// Scale all entries.
    void Scale(double s)
    {
        for (auto& c : coffs)
            c *= s;
    }

    /// add other matrix.
    void add(const Matrix3d& other)
    {
        for (int i = 0; i < 9; ++i)
            coffs[i] += other.coffs[i];
    }

    /// Subtract other matrix.
    void Subtract(const Matrix3d& other)
    {
        for (int i = 0; i < 9; ++i)
            coffs[i] -= other.coffs[i];
    }

    // --- Element access ---
    // Ported from: itwinjs-core Matrix3d.at/setAt

    /// Get element at (row, col).
    double at(int row, int col) const noexcept { return coffs[row * 3 + col]; }

    /// Set element at (row, col).
    void setAt(int row, int col, double value) noexcept { coffs[row * 3 + col] = value; }

    // --- Scale to result ---
    // Ported from: itwinjs-core Matrix3d.scale()

    /// Scale all entries and store in result.
    void Scale(double s, Matrix3d& result) const
    {
        for (int i = 0; i < 9; ++i)
            result.coffs[i] = coffs[i] * s;
    }

    // --- Multiply by inverse ---
    // Ported from: itwinjs-core Matrix3d.multiplyMatrixInverseMatrix()

    /// Compute this * inverse(other) and store in result.
    /// Returns false if other is singular.
    bool MultiplyMatrixInverseMatrix(const Matrix3d& other, Matrix3d& result) const
    {
        Matrix3d otherInv;
        if (!other.Inverse(otherInv))
            return false;
        result = MultiplyMatrix(otherInv);
        return true;
    }

    // --- itwinjs Matrix3d.ts additions (camelCase, 1:1) ---

    /// Create a Matrix3d whose columns are scaled copies of `this` Matrix3d.
    /// Ported from: itwinjs-core Matrix3d.scaleColumns (Matrix3d.ts:2491-2498)
    Matrix3d scaleColumns(double scaleX, double scaleY, double scaleZ) const noexcept
    {
        return Matrix3d(
            coffs[0] * scaleX, coffs[1] * scaleY, coffs[2] * scaleZ,
            coffs[3] * scaleX, coffs[4] * scaleY, coffs[5] * scaleZ,
            coffs[6] * scaleX, coffs[7] * scaleY, coffs[8] * scaleZ
        );
    }

    /// Return the max absolute value of all 9 coefficients.
    /// Ported from: itwinjs-core Matrix3d.maxAbs (Matrix3d.ts:2697-2702)
    double maxAbs() const noexcept
    {
        double max = 0.0;
        for (int i = 0; i < 9; ++i)
            max = std::fmax(max, std::abs(coffs[i]));
        return max;
    }

    /// Solve `matrix * result = vector` for an unknown `result` (i.e. multiply by
    /// the inverse). Returns nullopt if the matrix is singular.
    /// Ported from: itwinjs-core Matrix3d.multiplyInverse (Matrix3d.ts:1982-1996)
    std::optional<Vector3d> multiplyInverse(const Vector3d& vector) const noexcept
    {
        Matrix3d inv;
        if (!Inverse(inv))
            return std::nullopt;
        return inv.MultiplyVector(vector);
    }

    /// Solve `matrixTranspose * result = vector` for an unknown `result` (i.e.
    /// multiply by the inverse-transpose). Returns nullopt if singular.
    /// Ported from: itwinjs-core Matrix3d.multiplyInverseTranspose (Matrix3d.ts:2002-2016)
    std::optional<Vector3d> multiplyInverseTranspose(const Vector3d& vector) const noexcept
    {
        Matrix3d inv;
        if (!Inverse(inv))
            return std::nullopt;
        return Vector3d::From(
            inv.coffs[0] * vector.x + inv.coffs[3] * vector.y + inv.coffs[6] * vector.z,
            inv.coffs[1] * vector.x + inv.coffs[4] * vector.y + inv.coffs[7] * vector.z,
            inv.coffs[2] * vector.x + inv.coffs[5] * vector.y + inv.coffs[8] * vector.z
        );
    }

    /// Multiply `matrixInverse * [x,y,z]` and return the result as a `Point4d` with
    /// the given weight as the last coordinate. Returns nullopt if singular.
    /// Ported from: itwinjs-core Matrix3d.multiplyInverseXYZW (Matrix3d.ts:2041-2053)
    std::optional<Point4d> multiplyInverseXYZW(double x, double y, double z, double w) const noexcept
    {
        Matrix3d inv;
        if (!Inverse(inv))
            return std::nullopt;
        Vector3d const r = inv.MultiplyVector(Vector3d::From(x, y, z));
        return Point4d(r.x, r.y, r.z, w);
    }
};

} // namespace dqGeom
