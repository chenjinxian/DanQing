// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dpoint2d.h
// DanQing dqGeom — 2D 点（值类型，POD）
//
// 保真依据（CLAUDE.md §7.2）：API 以 imodel-native GeomLibs `DPoint2d` (C++) 为主权威；
// 类型名用 itwinjs `Point2d`（弃 D 前缀）。方法名/签名/语义逐位对齐 GeomLibs
// (geom/src/structs/cpp/refmethods/refdpoint2d.cpp)：
//   - 原地修改（Zero/Add/Subtract/DifferenceOf/SumOf/Interpolate/Scale/Rotate 改 *this）
//   - `From*` 静态工厂返回新对象
//   - `Normalize`/`ScaleToLength` 原地、返回原/目标模长
//   - `SetComponent`/`GetComponent` 用 `index & 0x01` 循环归约（对齐 GeomLibs）
//   - 无运算符重载（§9 禁）；比较走 IsEqual/AlmostEqual
//   - 跨类型（Vector2d）成员在此声明、在 Vector2d.h 内联定义（单向 include 避免循环，
//     对齐 Point3d↔Vector3d 模式；Vector2d 待移植，定义随之补）
// 溯源：imodel-native iModelCore/GeomLibs/PublicAPI/Geom/dpoint2d.h
#pragma once

#include "DqGeom.h"
#include "Point3d.h"   // kSmallMetricDistance* / kSmallAngleRadiansSquared + From(Point3d)
#include "Angle.h"     // Angle::kSmallAngleRadians (IsConvexPair 容差)

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

struct Vector2d; // 前向；跨类型方法定义见 Vector2d.h（待移植），对齐 Point3d↔Vector3d 模式

// 2d point coordinates.（对齐 GeomLibs DPoint2d）
struct DQ_GEOM_EXPORT Point2d {
    double x = 0.0;
    double y = 0.0;

    // --- 静态工厂（返回新对象；对齐 GeomLibs DPoint2d::From*） ---
    static Point2d From(double ax, double ay) noexcept { return Point2d{ax, ay}; }
    static Point2d From(const Point3d& source) noexcept { return Point2d{source.x, source.y}; } // 丢 z
    static Point2d FromArray(const double* pXy) noexcept {
        Point2d point;
        point.InitFromArray(pXy);
        return point;
    }
    static Point2d FromZero() noexcept { Point2d xy; xy.Zero(); return xy; }
    static Point2d FromOne() noexcept { Point2d xy; xy.One(); return xy; }
    static Point2d FromScale(const Point2d& point, double scale) noexcept {
        return Point2d{point.x * scale, point.y * scale};
    }
    static Point2d FromInterpolate(const Point2d& pointA, double fraction, const Point2d& pointB) noexcept {
        Point2d result;
        result.Interpolate(pointA, fraction, pointB);
        return result;
    }
    // point0 + tangentFraction*(point1-point0) 沿弦，再左偏 leftFraction 沿 90°CCW 法向。
    // Ported from: refdpoint2d.cpp DPoint2d::FromForwardLeftInterpolate
    static Point2d FromForwardLeftInterpolate(const Point2d& point0, double tangentFraction,
                                              double leftFraction, const Point2d& point1) noexcept {
        const double dx = point1.x - point0.x;
        const double dy = point1.y - point0.y;
        return Point2d{point0.x + tangentFraction * dx - leftFraction * dy,
                       point0.y + tangentFraction * dy + leftFraction * dx};
    }
    // 四角双线性插值。Ported from: refdpoint2d.cpp DPoint2d::FromInterpolateBilinear
    static Point2d FromInterpolateBilinear(const Point2d& data00, const Point2d& data10,
                                           const Point2d& data01, const Point2d& data11,
                                           double u, double v) noexcept {
        const double a00 = (1.0 - u) * (1.0 - v);
        const double a10 = u * (1.0 - v);
        const double a01 = (1.0 - u) * v;
        const double a11 = u * v;
        return Point2d{a00 * data00.x + a10 * data10.x + a01 * data01.x + a11 * data11.x,
                       a00 * data00.y + a10 * data10.y + a01 * data01.y + a11 * data11.y};
    }
    // 线性组合 point0*scale0 + point1*scale1。Ported from: refdpoint2d.cpp DPoint2d::FromSumOf
    static Point2d FromSumOf(const Point2d& point0, double scale0, const Point2d& point1, double scale1) noexcept {
        return Point2d{point0.x * scale0 + point1.x * scale1,
                       point0.y * scale0 + point1.y * scale1};
    }
    static Point2d FromSumOf(const Point2d& point0, double scale0, const Point2d& point1, double scale1,
                             const Point2d& point2, double scale2) noexcept {
        return Point2d{point0.x * scale0 + point1.x * scale1 + point2.x * scale2,
                       point0.y * scale0 + point1.y * scale1 + point2.y * scale2};
    }

    // --- 初始化 / 状态（原地；对齐 GeomLibs Init/Zero/One/InitFromArray） ---
    void Init(double ax, double ay) noexcept { x = ax; y = ay; }
    // 从 Point3d 取 xy（丢 z）。Ported from: refdpoint2d.cpp DPoint2d::Init(DPoint3dCR)
    void Init(const Point3d& source) noexcept { x = source.x; y = source.y; }
    // Ported from: refdpoint2d.cpp DPoint2d::InitFromArray（逐位，无 null 防御——对齐参考）
    void InitFromArray(const double* pXy) noexcept { x = pXy[0]; y = pXy[1]; }
    void Zero() noexcept { x = 0.0; y = 0.0; }
    void One() noexcept { x = 1.0; y = 1.0; }
    void Swap(Point2d& other) noexcept {
        const Point2d temp = *this;
        *this = other;
        other = temp;
    }
    // true 若任一分量为 NaN。Ported from: refdpoint2d.cpp DPoint2d::IsNan
    bool IsNan() const noexcept { return std::isnan(x) || std::isnan(y); }

    // 循环下标访问（index & 0x01；对齐 GeomLibs DPoint2d::SetComponent/GetComponent）
    void SetComponent(double a, int index) noexcept {
        index = index & 0x01;
        if (index == 0) x = a;
        else y = a;
    }
    double GetComponent(int index) const noexcept {
        index = index & 0x01;
        return index == 0 ? x : y;
    }
    void GetComponents(double& xCoord, double& yCoord) const noexcept { xCoord = x; yCoord = y; }

    // --- 缩放 / 取反 / 插值（原地；对齐 GeomLibs Scale/Negate/Interpolate） ---
    void Scale(double s) noexcept { x *= s; y *= s; }
    // Ported from: refdpoint2d.cpp DPoint2d::Scale(DPoint2dCR, double)
    void Scale(const Point2d& source, double s) noexcept { x = source.x * s; y = source.y * s; }
    // Ported from: refdpoint2d.cpp DPoint2d::Negate（取反 source 写入 *this）
    void Negate(const Point2d& source) noexcept { x = -source.x; y = -source.y; }
    // 分段插值（fraction≤0.5 以 point0 为基，否则以 point1 为基；最优末位行为）。
    // Ported from: refdpoint2d.cpp DPoint2d::Interpolate
    void Interpolate(const Point2d& point0, double s, const Point2d& point1) noexcept {
        if (s <= 0.5) {
            x = point0.x + s * (point1.x - point0.x);
            y = point0.y + s * (point1.y - point0.y);
        } else {
            const double t = s - 1.0;
            x = point1.x + t * (point1.x - point0.x);
            y = point1.y + t * (point1.y - point0.y);
        }
    }

    // --- 求和 / 差（原地；对齐 GeomLibs SumOf/Add/Subtract/DifferenceOf） ---
    // this = point1 + point2
    void SumOf(const Point2d& point1, const Point2d& point2) noexcept {
        x = point1.x + point2.x;
        y = point1.y + point2.y;
    }
    // this = point + vector*s
    void SumOf(const Point2d& point, const Point2d& vector, double s) noexcept {
        x = point.x + vector.x * s;
        y = point.y + vector.y * s;
    }
    // this = origin + vector1*scale1 + vector2*scale2
    void SumOf(const Point2d& origin, const Point2d& vector1, double scale1,
               const Point2d& vector2, double scale2) noexcept {
        x = origin.x + vector1.x * scale1 + vector2.x * scale2;
        y = origin.y + vector1.y * scale1 + vector2.y * scale2;
    }
    // this = origin + vector1*scale1 + vector2*scale2 + vector3*scale3
    void SumOf(const Point2d& origin, const Point2d& vector1, double scale1,
               const Point2d& vector2, double scale2, const Point2d& vector3, double scale3) noexcept {
        x = origin.x + vector1.x * scale1 + vector2.x * scale2 + vector3.x * scale3;
        y = origin.y + vector1.y * scale1 + vector2.y * scale2 + vector3.y * scale3;
    }
    // this = vector1*scale1 + vector2*scale2
    void SumOf(const Point2d& vector1, double scale1, const Point2d& vector2, double scale2) noexcept {
        x = vector1.x * scale1 + vector2.x * scale2;
        y = vector1.y * scale1 + vector2.y * scale2;
    }
    // this = vector1*scale1 + vector2*scale2 + vector3*scale3
    void SumOf(const Point2d& vector1, double scale1, const Point2d& vector2, double scale2,
               const Point2d& vector3, double scale3) noexcept {
        x = vector1.x * scale1 + vector2.x * scale2 + vector3.x * scale3;
        y = vector1.y * scale1 + vector2.y * scale2 + vector3.y * scale3;
    }
    void Add(const Point2d& vector) noexcept { x += vector.x; y += vector.y; }
    void Subtract(const Point2d& vector) noexcept { x -= vector.x; y -= vector.y; }
    // this = point1 - point2
    void DifferenceOf(const Point2d& point1, const Point2d& point2) noexcept {
        x = point1.x - point2.x;
        y = point1.y - point2.y;
    }
    // this = normalize(point1 - point2)，返回归一化前模长。Ported from: refdpoint2d.cpp NormalizedDifferenceOf
    double NormalizedDifferenceOf(const Point2d& point1, const Point2d& point2) noexcept {
        DifferenceOf(point1, point2);
        return Normalize();
    }

    // --- 归一化 / 缩放至长度（原地；返回原模长；对齐 GeomLibs Normalize/ScaleToLength） ---
    // DPoint2d：零向量 → 零向量，返回 0
    double Normalize() noexcept {
        const double magnitude = std::sqrt(x * x + y * y);
        if (magnitude > 0.0) {
            const double f = 1.0 / magnitude;
            x *= f;
            y *= f;
        }
        return magnitude;
    }
    // 归一化 vector1 存入 *this；零向量则原样复制。Ported from: refdpoint2d.cpp DPoint2d::Normalize(DPoint2dCR)
    double Normalize(const Point2d& vector1) noexcept {
        const double magnitude = std::sqrt(vector1.x * vector1.x + vector1.y * vector1.y);
        *this = vector1;
        if (magnitude > 0.0) {
            const double f = 1.0 / magnitude;
            x *= f;
            y *= f;
        }
        return magnitude;
    }
    // 原地缩放至 length；返回归一化前模长。Ported from: refdpoint2d.cpp DPoint2d::ScaleToLength(length)
    double ScaleToLength(double length) noexcept { return ScaleToLength(*this, length); }
    // 缩放 vector 至 length；零向量 → (length,0)。Ported from: refdpoint2d.cpp DPoint2d::ScaleToLength(vector,length)
    double ScaleToLength(const Point2d& vector, double length) noexcept {
        const double magnitude = std::sqrt(vector.x * vector.x + vector.y * vector.y);
        if (magnitude > 0.0) {
            const double f = length / magnitude;
            x = vector.x * f;
            y = vector.y * f;
        } else {
            x = length;
            y = 0.0;
        }
        return magnitude;
    }

    // --- 旋转（原地；对齐 GeomLibs Rotate90/RotateCCW） ---
    // this = vec 旋转 90° CCW（(-y, x)）。Ported from: refdpoint2d.cpp DPoint2d::Rotate90
    void Rotate90(const Point2d& vec) noexcept {
        const double xx = -vec.y; // 本地副本，允许 in-place
        const double yy = vec.x;
        x = xx;
        y = yy;
    }
    // this = vec 逆时针旋转 radians。Ported from: refdpoint2d.cpp DPoint2d::RotateCCW
    void RotateCCW(const Point2d& vec, double radians) noexcept {
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        const double xx = vec.x; // 本地副本，允许 in-place
        const double yy = vec.y;
        x = c * xx - s * yy;
        y = s * xx + c * yy;
    }

    // --- 距离 / 模长（DPoint2d 兼作向量；对齐 GeomLibs Distance/Magnitude*/MaxDiff） ---
    double DistanceSquared(const Point2d& point2) const noexcept {
        const double dx = point2.x - x;
        const double dy = point2.y - y;
        return dx * dx + dy * dy;
    }
    double Distance(const Point2d& point1) const noexcept {
        return std::sqrt(DistanceSquared(point1));
    }
    double MagnitudeSquared() const noexcept { return x * x + y * y; }
    double Magnitude() const noexcept { return std::sqrt(x * x + y * y); }
    double MaxAbs() const noexcept {
        double maxVal = std::fabs(x);
        if (std::fabs(y) > maxVal) maxVal = std::fabs(y);
        return maxVal;
    }
    // 对应分量绝对差的最大值。Ported from: refdpoint2d.cpp DPoint2d::MaxDiff
    double MaxDiff(const Point2d& other) const noexcept {
        double maxVal = std::fabs(x - other.x);
        const double dy = std::fabs(y - other.y);
        if (dy > maxVal) maxVal = dy;
        return maxVal;
    }

    // --- 点积 / 叉积（对齐 GeomLibs DotProduct/CrossProduct*ToPoints） ---
    double CrossProduct(const Point2d& vector1) const noexcept { return x * vector1.y - y * vector1.x; }
    // (target1 - this) × (target2 - this) 的 z 分量
    double CrossProductToPoints(const Point2d& target1, const Point2d& target2) const noexcept {
        const double ax = target1.x - x;
        const double ay = target1.y - y;
        const double bx = target2.x - x;
        const double by = target2.y - y;
        return ax * by - ay * bx;
    }
    double DotProduct(const Point2d& vector2) const noexcept { return x * vector2.x + y * vector2.y; }
    // (target1 - this) · (target2 - this)
    double DotProductToPoints(const Point2d& target1, const Point2d& target2) const noexcept {
        const double ax = target1.x - x;
        const double ay = target1.y - y;
        const double bx = target2.x - x;
        const double by = target2.y - y;
        return ax * bx + ay * by;
    }

    // --- 角度（弧度；对齐 GeomLibs AngleTo = Atan2(cross, dot)，范围 [-PI, PI]） ---
    double AngleTo(const Point2d& vector2) const noexcept {
        return std::atan2(CrossProduct(vector2), DotProduct(vector2));
    }

    // --- 平行 / 垂直（对齐 GeomLibs IsParallelTo/IsPerpendicularTo；eps = Angle::SmallAngle()，
    //     eps*eps == kSmallAngleRadiansSquared，与 Point3d 同形式） ---
    bool IsParallelTo(const Point2d& vector2) const noexcept {
        const double a2 = DotProduct(*this);
        const double b2 = vector2.DotProduct(vector2);
        const double cross = CrossProduct(vector2);
        return cross * cross <= kSmallAngleRadiansSquared * a2 * b2;
    }
    bool IsPerpendicularTo(const Point2d& vector2) const noexcept {
        const double a2 = DotProduct(*this);
        const double b2 = vector2.DotProduct(vector2);
        const double dot = DotProduct(vector2);
        return dot * dot <= kSmallAngleRadiansSquared * a2 * b2;
    }

    // --- 相等（对齐 GeomLibs IsEqual/AlmostEqual） ---
    bool IsEqual(const Point2d& vector2) const noexcept { return x == vector2.x && y == vector2.y; }
    // 负容差返回 false（逐分量绝对差 ≤ tolerance；对齐 GeomLibs 语义）
    bool IsEqual(const Point2d& vector2, double tolerance) const noexcept {
        if (tolerance < 0.0) return false;
        return std::fabs(x - vector2.x) <= tolerance && std::fabs(y - vector2.y) <= tolerance;
    }
    // 相对容差近似（对齐 Point3d::AlmostEqual 默认行为；GeomLibs AlmostEqual 以小角度容差为基准）
    bool AlmostEqual(const Point2d& other) const noexcept {
        const double tol = kSmallMetricDistance * (1.0 + MaxAbs());
        return IsEqual(other, tol);
    }
    // x+y 是否在 Angle::SmallAngle() 内等于 1（合法插值权重对），allowExtrapolation 控制是否允许外插。
    // Ported from: refdpoint2d.cpp DPoint2d::IsConvexPair
    bool IsConvexPair(bool allowExtrapolation = false) const noexcept {
        const double tol = Angle::kSmallAngleRadians;
        if (std::fabs(1.0 - x - y) > tol) return false;
        if (allowExtrapolation) return true;
        return x >= -tol && y >= -tol;
    }

    // 字典序：a 在 b 左方，或 x 相等时 a.y < b.y。Ported from: refdpoint2d.cpp DPoint2d::LexicalXYLessThan
    static bool LexicalXYLessThan(const Point2d& a, const Point2d& b) noexcept {
        if (a.x < b.x) return true;
        if (a.x > b.x) return false;
        return a.y < b.y;
    }

    // --- 跨类型（Vector2d）；声明于本头，定义于 Vector2d.h（待移植），对齐 Point3d↔Vector3d 模式 ---
    // point 沿 vector 前 tangentFraction、左 leftFraction 偏移。
    Point2d AddForwardLeft(const Vector2d& vector, double tangentFraction, double leftFraction) const noexcept;
    // origin + vector*scale
    static Point2d FromSumOf(const Point2d& origin, const Vector2d& vector, double scaleFactor) noexcept;
    static Point2d FromSumOf(const Point2d& origin, const Vector2d& vector0, double scaleFactor0,
                             const Vector2d& vector1, double scaleFactor1) noexcept;
    static Point2d FromSumOf(const Point2d& origin, const Vector2d& vector0, double scaleFactor0,
                             const Vector2d& vector1, double scaleFactor1,
                             const Vector2d& vector2, double scaleFactor2) noexcept;
    void SumOf(const Point2d& origin, const Vector2d& vector, double s) noexcept;
    void SumOf(const Point2d& origin, const Vector2d& vector1, double scale1,
               const Vector2d& vector2, double scale2) noexcept;
    void SumOf(const Point2d& origin, const Vector2d& vector1, double scale1,
               const Vector2d& vector2, double scale2, const Vector2d& vector3, double scale3) noexcept;
};

END_DQ_GEOM_NAMESPACE
