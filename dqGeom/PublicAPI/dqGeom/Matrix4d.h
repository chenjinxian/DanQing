// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — Double-precision 4x4 matrix
// Ported from: itwinjs-core core/geometry/src/geometry4d/Matrix4d.ts
//
// Double-precision 4x4 matrix used for shadow projection, atmosphere
// ellipsoid transforms, and other high-precision computations.
#pragma once

#include "Point3d.h"
#include "Point4d.h"
#include "Vector3d.h"
#include "Transform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// Matrix4d — double-precision 4x4 matrix
// Ported from: itwinjs-core Matrix4d.ts
//
// Row-major layout: _coffs[row*4 + col]
// Row 0: [0]  [1]  [2]  [3]
// Row 1: [4]  [5]  [6]  [7]
// Row 2: [8]  [9]  [10] [11]
// Row 3: [12] [13] [14] [15]
// ---------------------------------------------------------------------------
class Matrix4d {
public:
    /// Default constructor — zero matrix.
    Matrix4d() noexcept { m_coffs.fill(0.0); }

    /// Construct from 16 row-major values.
    Matrix4d(double cxx, double cxy, double cxz, double cxw,
             double cyx, double cyy, double cyz, double cyw,
             double czx, double czy, double czz, double czw,
             double cwx, double cwy, double cwz, double cww) noexcept
    {
        m_coffs[0] = cxx;  m_coffs[1] = cxy;  m_coffs[2] = cxz;  m_coffs[3] = cxw;
        m_coffs[4] = cyx;  m_coffs[5] = cyy;  m_coffs[6] = cyz;  m_coffs[7] = cyw;
        m_coffs[8] = czx;  m_coffs[9] = czy;  m_coffs[10] = czz; m_coffs[11] = czw;
        m_coffs[12] = cwx; m_coffs[13] = cwy; m_coffs[14] = cwz; m_coffs[15] = cww;
    }

    // --- Factory methods ---
    // Ported from: itwinjs-core Matrix4d.ts

    /// Create zero matrix.
    static Matrix4d CreateZero() noexcept { return Matrix4d(); }

    /// Create identity matrix.
    static Matrix4d CreateIdentity() noexcept {
        return Matrix4d(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);
    }

    /// Create from row values.
    /// Ported from: itwinjs-core Matrix4d.createRowValues()
    static Matrix4d CreateRowValues(
        double cxx, double cxy, double cxz, double cxw,
        double cyx, double cyy, double cyz, double cyw,
        double czx, double czy, double czz, double czw,
        double cwx, double cwy, double cwz, double cww) noexcept
    {
        return Matrix4d(cxx, cxy, cxz, cxw,
                        cyx, cyy, cyz, cyw,
                        czx, czy, czz, czw,
                        cwx, cwy, cwz, cww);
    }

    /// Create from Transform (4x3 → 4x4, last row = [0,0,0,1]).
    /// Ported from: itwinjs-core Matrix4d.createTransform()
    static Matrix4d CreateTransform(Transform const& xf) noexcept {
        auto const& m = xf.GetMatrix();
        auto const& o = xf.GetOrigin();
        return Matrix4d(
            m.coffs[0], m.coffs[1], m.coffs[2], o.x,
            m.coffs[3], m.coffs[4], m.coffs[5], o.y,
            m.coffs[6], m.coffs[7], m.coffs[8], o.z,
            0,          0,          0,          1);
    }

    /// Create scale+translate (axis-aligned, no rotation).
    /// Ported from: itwinjs-core Matrix4d.createTranslationAndScaleXYZ()
    static Matrix4d CreateTranslationAndScaleXYZ(
        double tx, double ty, double tz,
        double sx, double sy, double sz) noexcept
    {
        return CreateRowValues(sx, 0, 0, tx,  0, sy, 0, ty,  0, 0, sz, tz,  0, 0, 0, 1);
    }

    /// Create an axis-aligned box-to-box scale+translate map (A→B).
    /// Ported from: itwinjs-core Matrix4d.createBoxToBox(). Returns nullopt if any
    /// axis of box A is zero-sized (conditionalDivideFraction undefined on zero divisor).
    static std::optional<Matrix4d> CreateBoxToBox(
        Point3d const& lowA, Point3d const& highA,
        Point3d const& lowB, Point3d const& highB) noexcept
    {
        double ax = highA.x - lowA.x, ay = highA.y - lowA.y, az = highA.z - lowA.z;
        if (ax == 0.0 || ay == 0.0 || az == 0.0)
            return std::nullopt;
        double bx = highB.x - lowB.x, by = highB.y - lowB.y, bz = highB.z - lowB.z;
        double abx = bx / ax, aby = by / ay, abz = bz / az;
        return CreateTranslationAndScaleXYZ(
            lowB.x - abx * lowA.x, lowB.y - aby * lowA.y, lowB.z - abz * lowA.z,
            abx, aby, abz);
    }

    // --- Element access ---
    // Ported from: itwinjs-core Matrix4d.at()

    /// Get element at (row, col).
    double at(int row, int col) const noexcept { return m_coffs[row * 4 + col]; }

    /// Set element at (row, col).
    void set(int row, int col, double value) noexcept { m_coffs[row * 4 + col] = value; }

    /// Get raw coefficients (row-major, 16 doubles).
    double const* GetCoeffs() const noexcept { return m_coffs.data(); }
    double* GetCoeffs() noexcept { return m_coffs.data(); }

    // --- Mutation ---
    // Ported from: itwinjs-core Matrix4d.setZero/setIdentity/setFrom

    /// Set to zero.
    void SetZero() noexcept { m_coffs.fill(0.0); }

    /// Set to identity.
    void SetIdentity() noexcept {
        m_coffs.fill(0.0);
        m_coffs[0] = m_coffs[5] = m_coffs[10] = m_coffs[15] = 1.0;
    }

    /// Copy from another Matrix4d.
    void SetFrom(Matrix4d const& other) noexcept {
        m_coffs = other.m_coffs;
    }

    // --- clone ---
    // Ported from: itwinjs-core Matrix4d.clone()

    /// Return a deep clone.
    Matrix4d clone() const noexcept { return *this; }

    // --- Comparison ---
    // Ported from: itwinjs-core Matrix4d.isExactEqual/maxDiff

    /// Check exact equality.
    bool IsExactEqual(Matrix4d const& other) const noexcept {
        return m_coffs == other.m_coffs;
    }

    /// Compute max absolute difference.
    double MaxDiff(Matrix4d const& other) const noexcept {
        double maxDiff = 0.0;
        for (int i = 0; i < 16; ++i)
            maxDiff = std::max(maxDiff, std::abs(m_coffs[i] - other.m_coffs[i]));
        return maxDiff;
    }

    /// Check approximate equality.
    bool IsAlmostEqual(Matrix4d const& other, double tol = 1.0e-10) const noexcept {
        return MaxDiff(other) <= tol;
    }

    /// Test if this is (approximately) the identity matrix.
    /// Ported from: itwinjs-core Matrix4d.isIdentity()
    bool IsIdentity(double tol = 1.0e-10) const noexcept {
        return IsAlmostEqual(CreateIdentity(), tol);
    }

    // --- Matrix multiplication ---
    // Ported from: itwinjs-core Matrix4d.multiplyMatrixMatrix()

    /// Compute this * other.
    Matrix4d MultiplyMatrixMatrix(Matrix4d const& other) const noexcept {
        Matrix4d result;
        for (int i0 = 0; i0 < 16; i0 += 4) {
            for (int k = 0; k < 4; ++k)
                result.m_coffs[i0 + k] =
                    m_coffs[i0]     * other.m_coffs[k]      +
                    m_coffs[i0 + 1] * other.m_coffs[k + 4]  +
                    m_coffs[i0 + 2] * other.m_coffs[k + 8]  +
                    m_coffs[i0 + 3] * other.m_coffs[k + 12];
        }
        return result;
    }

    /// Compute this * transpose(other).
    Matrix4d MultiplyMatrixMatrixTranspose(Matrix4d const& other) const noexcept {
        Matrix4d result;
        int j = 0;
        for (int i0 = 0; i0 < 16; i0 += 4) {
            for (int k = 0; k < 16; k += 4)
                result.m_coffs[j++] =
                    m_coffs[i0]     * other.m_coffs[k]      +
                    m_coffs[i0 + 1] * other.m_coffs[k + 1]  +
                    m_coffs[i0 + 2] * other.m_coffs[k + 2]  +
                    m_coffs[i0 + 3] * other.m_coffs[k + 3];
        }
        return result;
    }

    /// Compute transpose(this) * other.
    Matrix4d MultiplyTransposeMatrixMatrix(Matrix4d const& other) const noexcept {
        Matrix4d result;
        int j = 0;
        for (int i0 = 0; i0 < 4; ++i0) {
            for (int k0 = 0; k0 < 4; ++k0)
                result.m_coffs[j++] =
                    m_coffs[i0]      * other.m_coffs[k0]      +
                    m_coffs[i0 + 4]  * other.m_coffs[k0 + 4]  +
                    m_coffs[i0 + 8]  * other.m_coffs[k0 + 8]  +
                    m_coffs[i0 + 12] * other.m_coffs[k0 + 12];
        }
        return result;
    }

    // --- Point/vector multiplication ---
    // Ported from: itwinjs-core Matrix4d.multiplyXYZW/multiplyPoint3d

    /// Multiply matrix * [x,y,z,w], return as Point4d (not normalized).
    Point4d MultiplyXYZW(double x, double y, double z, double w) const noexcept {
        return Point4d::From(
            m_coffs[0]*x + m_coffs[1]*y + m_coffs[2]*z  + m_coffs[3]*w,
            m_coffs[4]*x + m_coffs[5]*y + m_coffs[6]*z  + m_coffs[7]*w,
            m_coffs[8]*x + m_coffs[9]*y + m_coffs[10]*z + m_coffs[11]*w,
            m_coffs[12]*x + m_coffs[13]*y + m_coffs[14]*z + m_coffs[15]*w);
    }

    /// Multiply matrix * point3d with given w, return as Point4d.
    Point4d MultiplyPoint3d(Point3d const& pt, double w = 1.0) const noexcept {
        return MultiplyXYZW(pt.x, pt.y, pt.z, w);
    }

    /// Multiply matrix * [x,y,z,w], then perspective-divide into a Point3d. If the
    /// resulting w is zero, leave xyz un-renormalized (the "Quiet" semantic).
    /// Ported from: itwinjs-core Matrix4d.multiplyXYZWQuietRenormalize()
    Point3d MultiplyXYZWQuietRenormalize(double x, double y, double z, double w) const noexcept
    {
        Point3d result = Point3d::From(
            m_coffs[0]*x + m_coffs[1]*y + m_coffs[2]*z  + m_coffs[3]*w,
            m_coffs[4]*x + m_coffs[5]*y + m_coffs[6]*z  + m_coffs[7]*w,
            m_coffs[8]*x + m_coffs[9]*y + m_coffs[10]*z + m_coffs[11]*w);
        double w1 = m_coffs[12]*x + m_coffs[13]*y + m_coffs[14]*z + m_coffs[15]*w;
        if (w1 != 0.0) {
            double inv = 1.0 / w1;
            result = Point3d::From(result.x * inv, result.y * inv, result.z * inv);
        }
        return result;
    }

    /// Multiply matrix * point3d (w=1), perspective-divide, return Point3d.
    /// Ported from: itwinjs-core Matrix4d.multiplyPoint3dQuietNormalize()
    Point3d MultiplyPoint3dQuietNormalize(Point3d const& pt) const noexcept {
        return MultiplyXYZWQuietRenormalize(pt.x, pt.y, pt.z, 1.0);
    }

    /// In-place: multiply each point (w=1) + perspective-divide.
    /// Ported from: itwinjs-core Matrix4d.multiplyPoint3dArrayQuietNormalize()
    void MultiplyPoint3dArrayQuietNormalize(std::vector<Point3d>& points) const noexcept {
        for (auto& p : points)
            p = MultiplyXYZWQuietRenormalize(p.x, p.y, p.z, 1.0);
    }

    /// Multiply each point3d (weight w) → Point4d array (NOT normalized).
    /// Ported from: itwinjs-core Matrix4d.multiplyPoint3dArray()
    void MultiplyPoint3dArray(std::vector<Point3d> const& pts, std::vector<Point4d>& results,
                              double w = 1.0) const
    {
        results.resize(pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
            results[i] = MultiplyXYZW(pts[i].x, pts[i].y, pts[i].z, w);
    }

    /// Multiply each Point4d → Point3d array (perspective-divided).
    /// Ported from: itwinjs-core Matrix4d.multiplyPoint4dArrayQuietRenormalize()
    void MultiplyPoint4dArrayQuietRenormalize(std::vector<Point4d> const& pts,
                                              std::vector<Point3d>& results) const
    {
        results.resize(pts.size());
        for (size_t i = 0; i < pts.size(); ++i)
            results[i] = MultiplyXYZWQuietRenormalize(pts[i].x, pts[i].y, pts[i].z, pts[i].w);
    }

    // --- Transpose ---
    // Ported from: itwinjs-core Matrix4d.cloneTransposed()

    /// Return transposed matrix.
    Matrix4d CloneTransposed() const noexcept {
        return Matrix4d(
            m_coffs[0], m_coffs[4], m_coffs[8],  m_coffs[12],
            m_coffs[1], m_coffs[5], m_coffs[9],  m_coffs[13],
            m_coffs[2], m_coffs[6], m_coffs[10], m_coffs[14],
            m_coffs[3], m_coffs[7], m_coffs[11], m_coffs[15]);
    }

    // --- Origin and vectors ---
    // Ported from: itwinjs-core Matrix4d.setOriginAndVectors()

    /// Set columns from origin and three vectors.
    void SetOriginAndVectors(Point3d const& origin,
                              Vector3d const& vectorX,
                              Vector3d const& vectorY,
                              Vector3d const& vectorZ) noexcept
    {
        m_coffs[0] = vectorX.x;  m_coffs[1] = vectorY.x;  m_coffs[2] = vectorZ.x;  m_coffs[3] = origin.x;
        m_coffs[4] = vectorX.y;  m_coffs[5] = vectorY.y;  m_coffs[6] = vectorZ.y;  m_coffs[7] = origin.y;
        m_coffs[8] = vectorX.z;  m_coffs[9] = vectorY.z;  m_coffs[10] = vectorZ.z; m_coffs[11] = origin.z;
        m_coffs[12] = 0.0;       m_coffs[13] = 0.0;       m_coffs[14] = 0.0;       m_coffs[15] = 1.0;
    }

    // --- Inverse (for 4x4 matrix) ---
    // Ported from: itwinjs-core Matrix4d.inverse()

    /// Compute inverse. Returns true if successful (non-singular).
    bool Inverse(Matrix4d& result) const noexcept {
        // Gauss-Jordan elimination with partial pivoting
        double augmented[4][8];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                augmented[i][j] = m_coffs[i * 4 + j];
                augmented[i][j + 4] = (i == j) ? 1.0 : 0.0;
            }
        }

        for (int col = 0; col < 4; ++col) {
            // Find pivot
            int pivot = col;
            double maxVal = std::abs(augmented[col][col]);
            for (int row = col + 1; row < 4; ++row) {
                double val = std::abs(augmented[row][col]);
                if (val > maxVal) {
                    maxVal = val;
                    pivot = row;
                }
            }
            if (maxVal < 1.0e-15) return false;  // Singular

            // Swap rows
            if (pivot != col) {
                for (int j = 0; j < 8; ++j)
                    std::swap(augmented[col][j], augmented[pivot][j]);
            }

            // Scale pivot row
            double scale = augmented[col][col];
            for (int j = 0; j < 8; ++j)
                augmented[col][j] /= scale;

            // Eliminate column
            for (int row = 0; row < 4; ++row) {
                if (row == col) continue;
                double factor = augmented[row][col];
                for (int j = 0; j < 8; ++j)
                    augmented[row][j] -= factor * augmented[col][j];
            }
        }

        // Extract result
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result.m_coffs[i * 4 + j] = augmented[i][j + 4];

        return true;
    }

private:
    std::array<double, 16> m_coffs;
};

END_DQ_GEOM_NAMESPACE
