// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/DVec3d.h
// DanQing dqGeom — 3D 向量（值类型，POD）
//
// 保真依据（CLAUDE.md §7.2）：API 以 imodel-native GeomLibs `DVec3d` (C++) 为主权威；
// 类型名用 itwinjs `Vector3d`。语义逐位对齐 GeomLibs：原地修改、From* 工厂、
// Normalize 原地（零向量→(1,0,0)，返回 0）、TryNormalize bool+out、IsPositiveParallelTo、
// IsParallelTo(v, radians) 容差重载、无运算符重载。
// 单向 include（本头 → Point3d.h）；Point3d 的跨类型成员在此内联定义。
// 溯源：imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dvec3d.h
#pragma once

#include "Point3d.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

struct DQ_GEOM_EXPORT Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // --- 静态工厂（对齐 GeomLibs DVec3d::From*） ---
    static Vector3d From(double ax, double ay, double az = 0.0) noexcept { return Vector3d{ax, ay, az}; }
    static Vector3d From(const Point3d& point) noexcept { return Vector3d{point.x, point.y, point.z}; }
    static Vector3d FromZero() noexcept { return Vector3d{0.0, 0.0, 0.0}; }
    static Vector3d UnitX() noexcept { return Vector3d{1.0, 0.0, 0.0}; }
    static Vector3d UnitY() noexcept { return Vector3d{0.0, 1.0, 0.0}; }
    static Vector3d UnitZ() noexcept { return Vector3d{0.0, 0.0, 1.0}; }
    static Vector3d FromArray(const double* pXyz) noexcept {
        if (pXyz == nullptr) return FromZero();
        return Vector3d{pXyz[0], pXyz[1], pXyz[2]};
    }
    static Vector3d FromStartEnd(const Point3d& start, const Point3d& end) noexcept {
        return Vector3d{end.x - start.x, end.y - start.y, end.z - start.z};
    }
    static Vector3d FromCrossProduct(const Vector3d& v0, const Vector3d& v1) noexcept {
        return Vector3d{v0.y * v1.z - v0.z * v1.y, v0.z * v1.x - v0.x * v1.z, v0.x * v1.y - v0.y * v1.x};
    }
    static Vector3d FromCrossProduct(double x0, double y0, double z0, double x1, double y1, double z1) noexcept {
        return Vector3d{y0 * z1 - z0 * y1, z0 * x1 - x0 * z1, x0 * y1 - y0 * x1};
    }
    static Vector3d FromNormalizedCrossProduct(const Vector3d& v0, const Vector3d& v1) noexcept {
        Vector3d r = FromCrossProduct(v0, v1);
        r.Normalize();
        return r;
    }
    static Vector3d FromInterpolate(const Vector3d& v0, double fraction, const Vector3d& v1) noexcept {
        return Vector3d{v0.x + fraction * (v1.x - v0.x), v0.y + fraction * (v1.y - v0.y), v0.z + fraction * (v1.z - v0.z)};
    }
    static Vector3d FromScale(const Vector3d& source, double scale) noexcept {
        return Vector3d{source.x * scale, source.y * scale, source.z * scale};
    }
    // XY 平面极坐标（对齐 GeomLibs FromXYAngleAndMagnitude）
    static Vector3d FromXYAngleAndMagnitude(double theta, double magnitude) noexcept {
        return Vector3d{magnitude * std::cos(theta), magnitude * std::sin(theta), 0.0};
    }
    // FromSumOf —— GeomLibs "尾随缩放" 约定：首向量不缩放，后续 (vi, si) 配对
    static Vector3d FromSumOf(const Vector3d& v0, const Vector3d& v1) noexcept {
        return Vector3d{v0.x + v1.x, v0.y + v1.y, v0.z + v1.z};
    }
    static Vector3d FromSumOf(const Vector3d& v0, const Vector3d& v1, double s1) noexcept {
        return Vector3d{v0.x + s1 * v1.x, v0.y + s1 * v1.y, v0.z + s1 * v1.z};
    }
    static Vector3d FromSumOf(const Vector3d& v0, const Vector3d& v1, double s1,
                              const Vector3d& v2, double s2) noexcept {
        return Vector3d{v0.x + s1 * v1.x + s2 * v2.x, v0.y + s1 * v1.y + s2 * v2.y, v0.z + s1 * v1.z + s2 * v2.z};
    }
    static Vector3d FromSumOf(const Vector3d& v0, const Vector3d& v1, double s1, const Vector3d& v2, double s2,
                              const Vector3d& v3, double s3) noexcept {
        return Vector3d{v0.x + s1 * v1.x + s2 * v2.x + s3 * v3.x,
                        v0.y + s1 * v1.y + s2 * v2.y + s3 * v3.y,
                        v0.z + s1 * v1.z + s2 * v2.z + s3 * v3.z};
    }

    // --- 初始化 / 状态（对齐 GeomLibs Init/Zero/One） ---
    void Init(double ax, double ay, double az) noexcept { x = ax; y = ay; z = az; }
    void Init(double ax, double ay) noexcept { x = ax; y = ay; z = 0.0; }
    void Init(const Point3d& point) noexcept { x = point.x; y = point.y; z = point.z; }
    void InitFromArray(const double* pXyz) noexcept {
        if (pXyz == nullptr) { Zero(); return; }
        x = pXyz[0]; y = pXyz[1]; z = pXyz[2];
    }
    void InitFromXYAngleAndMagnitude(double theta, double magnitude) noexcept {
        x = magnitude * std::cos(theta);
        y = magnitude * std::sin(theta);
        z = 0.0;
    }
    void Zero() noexcept { x = y = z = 0.0; }
    void One() noexcept { x = y = z = 1.0; }
    void SetComponent(double a, int index) noexcept {
        switch (Cyclic3dAxis(index)) {
            case 0: x = a; break;
            case 1: y = a; break;
            default: z = a; break;
        }
    }
    double GetComponent(int index) const noexcept {
        switch (Cyclic3dAxis(index)) {
            case 0: return x;
            case 1: return y;
            default: return z;
        }
    }
    void GetComponents(double& xCoord, double& yCoord, double& zCoord) const noexcept {
        xCoord = x; yCoord = y; zCoord = z;
    }

    // --- 取反 / 缩放（原地；对齐 GeomLibs Negate/Scale） ---
    void Negate() noexcept { x = -x; y = -y; z = -z; }
    void Negate(const Vector3d& source) noexcept { x = -source.x; y = -source.y; z = -source.z; }
    void Scale(double s) noexcept { x *= s; y *= s; z *= s; }
    void Scale(const Vector3d& source, double s) noexcept { x = source.x * s; y = source.y * s; z = source.z * s; }

    // --- 归一化 / 缩放至长度（原地；对齐 GeomLibs Normalize/ScaleToLength） ---
    // DVec3d：零向量 → (1,0,0)，返回 0（与 DPoint3d 的零向量→零 不同）
    double Normalize() noexcept {
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double inv = 1.0 / mag;
            x *= inv; y *= inv; z *= inv;
        } else {
            x = 1.0; y = 0.0; z = 0.0;
        }
        return mag;
    }
    double Normalize(const Vector3d& source) noexcept {
        *this = source;
        return Normalize();
    }
    // this = source 归一化；成功返回 true 并写 magnitude；零源返回 false
    bool TryNormalize(const Vector3d& source, double& magnitude) noexcept {
        magnitude = source.Magnitude();
        if (magnitude <= kSmallMetricDistance) return false;
        const double inv = 1.0 / magnitude;
        x = source.x * inv; y = source.y * inv; z = source.z * inv;
        return true;
    }
    double ScaleToLength(double length) noexcept {
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double s = length / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }
    double ScaleToLength(const Vector3d& source, double length) noexcept {
        *this = source;
        return ScaleToLength(length);
    }
    // this = unit(target - origin)；返回原距离（对齐 GeomLibs NormalizedDifference）
    double NormalizedDifference(const Point3d& target, const Point3d& origin) noexcept {
        *this = Vector3d{target.x - origin.x, target.y - origin.y, target.z - origin.z};
        return Normalize();
    }

    // --- 加减 / 差 / 和（原地；对齐 GeomLibs Add/Subtract/DifferenceOf/SumOf/Interpolate） ---
    void Add(const Vector3d& v) noexcept { x += v.x; y += v.y; z += v.z; }
    void Subtract(const Vector3d& v) noexcept { x -= v.x; y -= v.y; z -= v.z; }
    void DifferenceOf(const Vector3d& v1, const Vector3d& v2) noexcept {
        x = v1.x - v2.x; y = v1.y - v2.y; z = v1.z - v2.z;
    }
    void DifferenceOf(const Point3d& target, const Point3d& base) noexcept {
        x = target.x - base.x; y = target.y - base.y; z = target.z - base.z;
    }
    void Interpolate(const Vector3d& v0, double fraction, const Vector3d& v1) noexcept {
        x = v0.x + fraction * (v1.x - v0.x);
        y = v0.y + fraction * (v1.y - v0.y);
        z = v0.z + fraction * (v1.z - v0.z);
    }

    // --- 点积 / 叉积 / 三重积（对齐 GeomLibs DotProduct/CrossProduct/TripleProduct） ---
    double DotProduct(const Vector3d& other) const noexcept { return x * other.x + y * other.y + z * other.z; }
    double DotProduct(double ax, double ay, double az) const noexcept { return x * ax + y * ay + z * az; }
    double DotProduct(const Point3d& other) const noexcept { return x * other.x + y * other.y + z * other.z; }
    double DotProductXY(const Vector3d& other) const noexcept { return x * other.x + y * other.y; }
    double TripleProduct(const Vector3d& v1, const Vector3d& v2) const noexcept {
        const double cx = v1.y * v2.z - v1.z * v2.y;
        const double cy = v1.z * v2.x - v1.x * v2.z;
        const double cz = v1.x * v2.y - v1.y * v2.x;
        return x * cx + y * cy + z * cz;
    }
    double CrossProductXY(const Vector3d& other) const noexcept { return x * other.y - y * other.x; }
    double CrossProductMagnitude(const Vector3d& other) const noexcept {
        const double cx = y * other.z - z * other.y;
        const double cy = z * other.x - x * other.z;
        const double cz = x * other.y - y * other.x;
        return std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    // this = v1 × v2（原地）
    void CrossProduct(const Vector3d& v1, const Vector3d& v2) noexcept {
        const double nx = v1.y * v2.z - v1.z * v2.y;
        const double ny = v1.z * v2.x - v1.x * v2.z;
        const double nz = v1.x * v2.y - v1.y * v2.x;
        x = nx; y = ny; z = nz;
    }
    // this = unit(v1 × v2)；返回 |v1 × v2|
    double NormalizedCrossProduct(const Vector3d& v1, const Vector3d& v2) noexcept {
        CrossProduct(v1, v2);
        const double mag = Magnitude();
        Normalize();
        return mag;
    }
    // this = (v1 × v2) 缩放至 productLength；返回 |v1 × v2|
    double SizedCrossProduct(const Vector3d& v1, const Vector3d& v2, double productLength) noexcept {
        CrossProduct(v1, v2);
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double s = productLength / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }
    double GeometricMeanCrossProduct(const Vector3d& v1, const Vector3d& v2) noexcept {
        CrossProduct(v1, v2);
        const double mag = Magnitude();
        const double target = std::sqrt(v1.MagnitudeSquared()) * std::sqrt(v2.MagnitudeSquared());
        if (mag > 0.0) {
            const double s = target / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }

    // --- 模长 / 距离 / 分量（对齐 GeomLibs Magnitude/Distance/MaxAbs） ---
    double MagnitudeSquared() const noexcept { return x * x + y * y + z * z; }
    double Magnitude() const noexcept { return std::sqrt(MagnitudeSquared()); }
    double MagnitudeSquaredXY() const noexcept { return x * x + y * y; }
    double MagnitudeXY() const noexcept { return std::sqrt(MagnitudeSquaredXY()); }
    double MaxAbs() const noexcept {
        const double ax = std::fabs(x);
        const double ay = std::fabs(y);
        const double az = std::fabs(z);
        double m = ax > ay ? ax : ay;
        return az > m ? az : m;
    }
    double Distance(const Vector3d& other) const noexcept { return Point3d::From(x - other.x, y - other.y, z - other.z).Magnitude(); }
    double DistanceSquared(const Vector3d& other) const noexcept {
        const double dx = x - other.x;
        const double dy = y - other.y;
        const double dz = z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // --- 相等 / 零（对齐 GeomLibs IsEqual/AlmostEqual/IsZero） ---
    bool IsEqual(const Vector3d& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
    bool IsEqual(const Vector3d& other, double tolerance) const noexcept {
        if (tolerance < 0.0) return false;
        return std::fabs(x - other.x) <= tolerance
            && std::fabs(y - other.y) <= tolerance
            && std::fabs(z - other.z) <= tolerance;
    }
    bool AlmostEqual(const Vector3d& other) const noexcept {
        const double tol = kSmallMetricDistance * (1.0 + MaxAbs());
        return IsEqual(other, tol);
    }
    bool IsZero() const noexcept { return x == 0.0 && y == 0.0 && z == 0.0; }

    // --- 平行 / 垂直（对齐 GeomLibs DVec3d IsParallelTo(v)/IsParallelTo(v,radians)/IsPositiveParallelTo/IsPerpendicularTo） ---
    bool IsParallelTo(const Vector3d& other) const noexcept {
        const double aa = MagnitudeSquared();
        const double bb = other.MagnitudeSquared();
        if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared) return false;
        return CrossProductMagnitude(other) <= 1.0e-15 * std::sqrt(aa * bb); // 默认小容差
    }
    // 显式弧度容差（对齐 GeomLibs IsParallelTo(v, double radians)）
    bool IsParallelTo(const Vector3d& other, double radians) const noexcept {
        const double aa = MagnitudeSquared();
        const double bb = other.MagnitudeSquared();
        if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared) return false;
        return CrossProductMagnitude(other) <= std::fabs(std::sin(radians)) * std::sqrt(aa * bb);
    }
    // 反向不算平行（对齐 GeomLibs IsPositiveParallelTo）
    bool IsPositiveParallelTo(const Vector3d& other) const noexcept {
        return IsParallelTo(other) && DotProduct(other) > 0.0;
    }
    bool IsPerpendicularTo(const Vector3d& other) const noexcept {
        const double aa = MagnitudeSquared();
        const double bb = other.MagnitudeSquared();
        if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared) return false;
        const double dot = DotProduct(other);
        return std::fabs(dot) <= 1.0e-15 * std::sqrt(aa * bb);
    }

    // --- 角度（弧度；对齐 GeomLibs AngleTo/AngleToXY/.../AngleFromPerpendicular） ---
    double AngleTo(const Vector3d& other) const noexcept {
        return std::atan2(CrossProductMagnitude(other), DotProduct(other));
    }
    double AngleToXY(const Vector3d& other) const noexcept {
        return std::atan2(CrossProductXY(other), DotProductXY(other));
    }
    double SmallerUnorientedAngleTo(const Vector3d& other) const noexcept {
        const double a = AngleTo(other);
        return a > M_PI_2 ? (M_PI - a) : a;
    }
    double SignedAngleTo(const Vector3d& other, const Vector3d& orientationVector) const noexcept {
        const Vector3d c = FromCrossProduct(*this, other);
        return std::atan2(c.DotProduct(orientationVector), DotProduct(other));
    }
    double PlanarAngleTo(const Vector3d& other, const Vector3d& planeNormal) const noexcept {
        return SignedAngleTo(other, planeNormal);
    }
    double AngleFromPerpendicular(const Vector3d& planeNormal) const noexcept {
        return std::asin(DotProduct(planeNormal) / (Magnitude() * planeNormal.Magnitude()));
    }

    // --- XY 旋转（原地；对齐 GeomLibs RotateXY） ---
    void RotateXY(double theta) noexcept {
        const double c = std::cos(theta);
        const double s = std::sin(theta);
        const double nx = x * c - y * s;
        const double ny = x * s + y * c;
        x = nx; y = ny;
    }
    void RotateXY(const Vector3d& source, double theta) noexcept {
        *this = source;
        RotateXY(theta);
    }
    // this = source 在 XY 的单位垂直向量（对齐 GeomLibs UnitPerpendicularXY）
    bool UnitPerpendicularXY(const Vector3d& source) noexcept {
        const double mag = source.MagnitudeXY();
        if (mag <= kSmallMetricDistance) return false;
        x = -source.y / mag;
        y = source.x / mag;
        z = 0.0;
        return true;
    }

    // --- 安全除法（对齐 GeomLibs SafeDivide） ---
    bool SafeDivide(const Vector3d& source, double denominator) noexcept {
        if (denominator == 0.0) return false;
        x = source.x / denominator;
        y = source.y / denominator;
        z = source.z / denominator;
        return true;
    }

    // --- 投影（对齐 GeomLibs ProjectToVector） ---
    bool ProjectToVector(const Vector3d& target, double& fraction) const noexcept {
        const double bb = target.MagnitudeSquared();
        if (bb < kSmallMetricDistanceSquared) return false;
        fraction = DotProduct(target) / bb;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Point3d 跨类型成员的内联定义（需 Vector3d 完整类型）
// ---------------------------------------------------------------------------
inline void Point3d::Add(const Vector3d& vector) noexcept { x += vector.x; y += vector.y; z += vector.z; }
inline void Point3d::Subtract(const Vector3d& vector) noexcept { x -= vector.x; y -= vector.y; z -= vector.z; }
inline void Point3d::Subtract(const Point3d& other) noexcept { x -= other.x; y -= other.y; z -= other.z; }
inline void Point3d::DifferenceOf(const Point3d& point1, const Point3d& point2) noexcept {
    x = point1.x - point2.x; y = point1.y - point2.y; z = point1.z - point2.z;
}
inline void Point3d::SumOf(const Point3d& origin, const Vector3d& vector) noexcept {
    x = origin.x + vector.x; y = origin.y + vector.y; z = origin.z + vector.z;
}
inline void Point3d::SumOf(const Point3d& origin, const Vector3d& vector, double scaleFactor) noexcept {
    x = origin.x + scaleFactor * vector.x; y = origin.y + scaleFactor * vector.y; z = origin.z + scaleFactor * vector.z;
}
inline void Point3d::SumOf(const Point3d& origin, const Vector3d& v0, double s0, const Vector3d& v1, double s1) noexcept {
    x = origin.x + s0 * v0.x + s1 * v1.x;
    y = origin.y + s0 * v0.y + s1 * v1.y;
    z = origin.z + s0 * v0.z + s1 * v1.z;
}
inline void Point3d::SumOf(const Point3d& origin, const Vector3d& v0, double s0, const Vector3d& v1, double s1,
                           const Vector3d& v2, double s2) noexcept {
    x = origin.x + s0 * v0.x + s1 * v1.x + s2 * v2.x;
    y = origin.y + s0 * v0.y + s1 * v1.y + s2 * v2.y;
    z = origin.z + s0 * v0.z + s1 * v1.z + s2 * v2.z;
}

END_DQ_GEOM_NAMESPACE
