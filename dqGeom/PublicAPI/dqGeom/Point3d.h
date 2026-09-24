// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/DPoint3d.h
// DanQing dqGeom — 3D 点（值类型，POD）
//
// 保真依据（CLAUDE.md §7.2）：API 以 imodel-native GeomLibs `DPoint3d` (C++) 为主权威；
// 类型名用 itwinjs `Point3d`（弃 D 前缀）。方法名/签名/语义逐位对齐 GeomLibs：
//   - 原地修改（Zero/Add/Subtract/CrossProduct/SumOf/Interpolate/Scale 改 *this）
//   - `From*` 静态工厂返回新对象
//   - `Normalize`/`ScaleToLength` 原地、返回原/目标模长
//   - 无运算符重载（§7.2 禁）；比较走 IsEqual/AlmostEqual
//   - 跨类型（Vector3d）成员在此声明、在 Vector3d.h 内联定义（单向 include 避免循环）
// 溯源：imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dpoint3d.h
#pragma once

#include "DqGeom.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

// 数值容差（取自 itwinjs Geometry.* 精确值，CLAUDE.md §7.2）
inline constexpr double kSmallMetricDistance = 1.0e-6;          // Geometry.smallMetricDistance
inline constexpr double kSmallMetricDistanceSquared = 1.0e-12;  // Geometry.smallMetricDistanceSquared
inline constexpr double kSmallAngleRadiansSquared = 1.0e-24;    // Geometry.smallAngleRadiansSquared

struct Vector3d; // 前向；跨类型方法定义见 Vector3d.h

// 轴下标循环归约（对齐 GeomLibs Angle::Cyclic3dAxis / itwinjs Geometry.cyclic3dAxis）
inline int Cyclic3dAxis(int axis) noexcept { return ((axis % 3) + 3) % 3; }

struct DQ_GEOM_EXPORT Point3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    // --- 静态工厂（返回新对象；对齐 GeomLibs DPoint3d::From*） ---
    static Point3d From(double ax, double ay, double az = 0.0) noexcept { return Point3d{ax, ay, az}; }
    static Point3d FromZero() noexcept { return Point3d{0.0, 0.0, 0.0}; }
    static Point3d FromArray(const double* pXyz) noexcept {
        if (pXyz == nullptr) return FromZero();
        return Point3d{pXyz[0], pXyz[1], pXyz[2]};
    }
    static Point3d FromScale(const Point3d& source, double scale) noexcept {
        return Point3d{source.x * scale, source.y * scale, source.z * scale};
    }
    static Point3d FromInterpolate(const Point3d& pointA, double fraction, const Point3d& pointB) noexcept {
        return Point3d{pointA.x + fraction * (pointB.x - pointA.x),
                       pointA.y + fraction * (pointB.y - pointA.y),
                       pointA.z + fraction * (pointB.z - pointA.z)};
    }

    // --- 初始化 / 状态（原地；对齐 GeomLibs Init/Zero/One/Swap） ---
    void Init(double ax, double ay, double az) noexcept { x = ax; y = ay; z = az; }
    void Init(double ax, double ay) noexcept { x = ax; y = ay; z = 0.0; }
    void InitFromArray(const double* pXyz) noexcept {
        if (pXyz == nullptr) { Zero(); return; }
        x = pXyz[0]; y = pXyz[1]; z = pXyz[2];
    }
    void Zero() noexcept { x = y = z = 0.0; }
    void One() noexcept { x = y = z = 1.0; }
    void Swap(Point3d& other) noexcept {
        const Point3d tmp = *this;
        *this = other;
        other = tmp;
    }
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

    // --- 缩放 / 取反 / 插值（原地；对齐 GeomLibs Scale/Negate/Interpolate） ---
    void Scale(double s) noexcept { x *= s; y *= s; z *= s; }
    void Scale(const Point3d& source, double s) noexcept { x = source.x * s; y = source.y * s; z = source.z * s; }
    void Negate() noexcept { x = -x; y = -y; z = -z; }
    void Negate(const Point3d& source) noexcept { x = -source.x; y = -source.y; z = -source.z; }
    // this = point0 + fraction * (point1 - point0)
    void Interpolate(const Point3d& point0, double fraction, const Point3d& point1) noexcept {
        x = point0.x + fraction * (point1.x - point0.x);
        y = point0.y + fraction * (point1.y - point0.y);
        z = point0.z + fraction * (point1.z - point0.z);
    }

    // --- 距离 / 模长（DPoint3d 兼作向量；对齐 GeomLibs Distance/Magnitude*） ---
    double DistanceSquared(const Point3d& other) const noexcept {
        const double dx = x - other.x;
        const double dy = y - other.y;
        const double dz = z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }
    double Distance(const Point3d& other) const noexcept { return std::sqrt(DistanceSquared(other)); }
    double DistanceSquaredXY(const Point3d& other) const noexcept {
        const double dx = x - other.x;
        const double dy = y - other.y;
        return dx * dx + dy * dy;
    }
    double DistanceXY(const Point3d& other) const noexcept { return std::sqrt(DistanceSquaredXY(other)); }
    double MagnitudeSquared() const noexcept { return x * x + y * y + z * z; }
    double Magnitude() const noexcept { return std::sqrt(MagnitudeSquared()); }
    double MagnitudeSquaredXY() const noexcept { return x * x + y * y; }
    double MagnitudeXY() const noexcept { return std::sqrt(MagnitudeSquaredXY()); }

    // --- 分量分析（对齐 GeomLibs MaxAbs/MinAbs/MaxAbsIndex） ---
    double MaxAbs() const noexcept {
        const double ax = std::fabs(x);
        const double ay = std::fabs(y);
        const double az = std::fabs(z);
        double m = ax > ay ? ax : ay;
        return az > m ? az : m;
    }
    double MinAbs() const noexcept {
        const double ax = std::fabs(x);
        const double ay = std::fabs(y);
        const double az = std::fabs(z);
        double m = ax < ay ? ax : ay;
        return az < m ? az : m;
    }
    int MaxAbsIndex() const noexcept {
        const double ax = std::fabs(x);
        const double ay = std::fabs(y);
        const double az = std::fabs(z);
        if (ay > ax && ay >= az) return 1;
        if (az > ax && az > ay) return 2;
        return 0;
    }

    // --- 相等（对齐 GeomLibs IsEqual/AlmostEqual） ---
    bool IsEqual(const Point3d& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
    // 负容差返回 false（对齐 GeomLibs 语义）
    bool IsEqual(const Point3d& other, double tolerance) const noexcept {
        if (tolerance < 0.0) return false;
        return std::fabs(x - other.x) <= tolerance
            && std::fabs(y - other.y) <= tolerance
            && std::fabs(z - other.z) <= tolerance;
    }
    bool AlmostEqual(const Point3d& other, double abstol) const noexcept { return IsEqual(other, abstol); }
    bool AlmostEqual(const Point3d& other) const noexcept {
        // 相对容差：与分量幅度成正比（近似 GeomLibs AlmostEqual 默认行为）
        const double tol = kSmallMetricDistance * (1.0 + MaxAbs());
        return IsEqual(other, tol);
    }

    // --- 点积 / 叉积 / 三重积（对齐 GeomLibs DotProduct/CrossProduct/TripleProduct） ---
    double DotProduct(const Point3d& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }
    double DotProduct(double ax, double ay, double az) const noexcept {
        return x * ax + y * ay + z * az;
    }
    double DotProductXY(const Point3d& other) const noexcept { return x * other.x + y * other.y; }
    // (target1 - this) · (target2 - this)
    double DotProductToPoints(const Point3d& target1, const Point3d& target2) const noexcept {
        return VectorDeltaDot(target1, target2);
    }
    double DotProductToPointsXY(const Point3d& target1, const Point3d& target2) const noexcept {
        const double ax = target1.x - x;
        const double ay = target1.y - y;
        const double bx = target2.x - x;
        const double by = target2.y - y;
        return ax * bx + ay * by;
    }
    // this_xy × other_xy 的 z 分量
    double CrossProductXY(const Point3d& other) const noexcept { return x * other.y - y * other.x; }
    // (target1 - this) × (target2 - this) 的 z 分量
    double CrossProductToPointsXY(const Point3d& target1, const Point3d& target2) const noexcept {
        const double ax = target1.x - x;
        const double ay = target1.y - y;
        const double bx = target2.x - x;
        const double by = target2.y - y;
        return ax * by - ay * bx;
    }
    double TripleProduct(const Point3d& point2, const Point3d& point3) const noexcept {
        // this · (point2 × point3)
        const double cx = point2.y * point3.z - point2.z * point3.y;
        const double cy = point2.z * point3.x - point2.x * point3.z;
        const double cz = point2.x * point3.y - point2.y * point3.x;
        return x * cx + y * cy + z * cz;
    }
    double TripleProductToPoints(const Point3d& target1, const Point3d& target2, const Point3d& target3) const noexcept {
        Point3d a{target1.x - x, target1.y - y, target1.z - z};
        Point3d b{target2.x - x, target2.y - y, target2.z - z};
        Point3d c{target3.x - x, target3.y - y, target3.z - z};
        return a.TripleProduct(b, c);
    }

    // --- 原地叉积（写入 *this；对齐 GeomLibs CrossProduct/NormalizedCrossProduct/...） ---
    // this = point1 × point2
    void CrossProduct(const Point3d& point1, const Point3d& point2) noexcept {
        const double nx = point1.y * point2.z - point1.z * point2.y;
        const double ny = point1.z * point2.x - point1.x * point2.z;
        const double nz = point1.x * point2.y - point1.y * point2.x;
        x = nx; y = ny; z = nz;
    }
    // this = unit(point1 × point2)；返回 |point1 × point2|（归一化前）
    double NormalizedCrossProduct(const Point3d& point1, const Point3d& point2) noexcept {
        CrossProduct(point1, point2);
        return Normalize();
    }
    // this = (point1 × point2) 缩放至 productLength；返回 |point1 × point2|
    double SizedCrossProduct(const Point3d& point1, const Point3d& point2, double productLength) noexcept {
        CrossProduct(point1, point2);
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double s = productLength / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }
    // this = sqrt(|point1|) * sqrt(|point2|) 方向的 (point1 × point2)；返回 |point1 × point2|
    double GeometricMeanCrossProduct(const Point3d& point1, const Point3d& point2) noexcept {
        CrossProduct(point1, point2);
        const double mag = Magnitude();
        const double target = std::sqrt(point1.MagnitudeSquared()) * std::sqrt(point2.MagnitudeSquared());
        if (mag > 0.0) {
            const double s = target / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }

    // --- 归一化 / 缩放至长度（原地；返回原模长；对齐 GeomLibs Normalize/ScaleToLength） ---
    // DPoint3d：零向量 → 零向量，返回 0
    double Normalize() noexcept {
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double inv = 1.0 / mag;
            x *= inv; y *= inv; z *= inv;
        }
        return mag;
    }
    double Normalize(const Point3d& source) noexcept {
        *this = source;
        return Normalize();
    }
    // 原地缩放至 length；返回缩放前的原模长
    double ScaleToLength(double length) noexcept {
        const double mag = Magnitude();
        if (mag > 0.0) {
            const double s = length / mag;
            x *= s; y *= s; z *= s;
        }
        return mag;
    }
    double ScaleToLength(const Point3d& source, double length) noexcept {
        *this = source;
        return ScaleToLength(length);
    }

    // --- 平行 / 垂直（DPoint3d：无容差重载；对齐 GeomLibs IsParallelTo/IsPerpendicularTo） ---
    bool IsParallelTo(const Point3d& other) const noexcept {
        const double aa = MagnitudeSquared();
        const double bb = other.MagnitudeSquared();
        if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared) return false;
        Point3d c;
        c.CrossProduct(*this, other);
        return c.MagnitudeSquared() <= kSmallAngleRadiansSquared * aa * bb;
    }
    bool IsPerpendicularTo(const Point3d& other) const noexcept {
        const double aa = MagnitudeSquared();
        const double bb = other.MagnitudeSquared();
        if (aa < kSmallMetricDistanceSquared || bb < kSmallMetricDistanceSquared) return false;
        const double dot = DotProduct(other);
        return dot * dot <= kSmallAngleRadiansSquared * aa * bb;
    }

    // --- 角度（弧度，对齐 GeomLibs AngleTo/AngleToXY/AngleXY/...；均为 double） ---
    double AngleTo(const Point3d& other) const noexcept {
        return std::atan2(CrossProductMagnitude(other), DotProduct(other)); // [0, PI]
    }
    double AngleToXY(const Point3d& other) const noexcept {
        return std::atan2(CrossProductXY(other), DotProductXY(other)); // [-PI, PI]
    }
    double AngleXY() const noexcept { return std::atan2(y, x); } // [-PI, PI]
    double SmallerUnorientedAngleTo(const Point3d& other) const noexcept {
        const double a = AngleTo(other);
        return a > M_PI_2 ? (M_PI - a) : a; // [0, PI/2]
    }
    double SignedAngleTo(const Point3d& other, const Point3d& orientationVector) const noexcept {
        Point3d c;
        c.CrossProduct(*this, other);
        return std::atan2(c.DotProduct(orientationVector), DotProduct(other)); // [-PI, PI]
    }
    double PlanarAngleTo(const Point3d& other, const Point3d& planeNormal) const noexcept {
        return SignedAngleTo(other, planeNormal);
    }

    // --- XY 旋转（原地；对齐 GeomLibs RotateXY） ---
    void RotateXY(double theta) noexcept {
        const double c = std::cos(theta);
        const double s = std::sin(theta);
        const double nx = x * c - y * s;
        const double ny = x * s + y * c;
        x = nx; y = ny;
    }
    void RotateXY(const Point3d& origin, double theta) noexcept {
        const double dx = x - origin.x;
        const double dy = y - origin.y;
        const double c = std::cos(theta);
        const double s = std::sin(theta);
        x = origin.x + dx * c - dy * s;
        y = origin.y + dx * s + dy * c;
    }

    // --- 安全除法（对齐 GeomLibs SafeDivide） ---
    bool SafeDivide(const Point3d& source, double denominator) noexcept {
        if (denominator == 0.0) return false;
        x = source.x / denominator;
        y = source.y / denominator;
        z = source.z / denominator;
        return true;
    }

    // --- 跨类型（Vector3d）；声明于本头，定义于 Vector3d.h ---
    void Add(const Vector3d& vector) noexcept;
    void Subtract(const Vector3d& vector) noexcept;
    void Subtract(const Point3d& other) noexcept;                       // this -= other
    void DifferenceOf(const Point3d& point1, const Point3d& point2) noexcept; // this = point1 - point2
    void SumOf(const Point3d& origin, const Vector3d& vector) noexcept;
    void SumOf(const Point3d& origin, const Vector3d& vector, double scaleFactor) noexcept;
    void SumOf(const Point3d& origin, const Vector3d& v0, double s0, const Vector3d& v1, double s1) noexcept;
    void SumOf(const Point3d& origin, const Vector3d& v0, double s0, const Vector3d& v1, double s1,
               const Vector3d& v2, double s2) noexcept;

private:
    // (target1 - this) · (target2 - this)
    double VectorDeltaDot(const Point3d& target1, const Point3d& target2) const noexcept {
        const double ax = target1.x - x;
        const double ay = target1.y - y;
        const double az = target1.z - z;
        const double bx = target2.x - x;
        const double by = target2.y - y;
        const double bz = target2.z - z;
        return ax * bx + ay * by + az * bz;
    }
    double CrossProductMagnitude(const Point3d& other) const noexcept {
        const double cx = y * other.z - z * other.y;
        const double cy = z * other.x - x * other.z;
        const double cz = x * other.y - y * other.x;
        return std::sqrt(cx * cx + cy * cy + cz * cz);
    }
};

END_DQ_GEOM_NAMESPACE
