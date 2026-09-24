// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Quantized point types
//
// Ported from: itwinjs-core core/common/src/QPoint.ts
// Quantization of floating point values to 16-bit unsigned integers.
// Used to reduce space for RenderGraphic coordinates.
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Quantization utilities.
// Ported from: itwinjs-core Quantization namespace
namespace Quantization {
    inline constexpr uint32_t rangeScale16 = 0xFFFF;
    inline constexpr uint32_t rangeScale8 = 0xFF;

    // Compute scale factor to quantize `extent` to `rangeScale` discrete values.
    // Ported from: itwinjs-core Quantization.computeScale()
    inline double computeScale(double extent, uint32_t rangeScale = rangeScale16) noexcept
    {
        return 0.0 == extent ? 0.0 : static_cast<double>(rangeScale) / extent;
    }

    // Returns true if quantized value fits within range.
    // Ported from: itwinjs-core Quantization.isInRange()
    inline bool isInRange(double qpos, uint32_t rangeScale = rangeScale16) noexcept
    {
        return qpos >= 0.0 && qpos < static_cast<double>(rangeScale) + 1.0;
    }

    // quantize a position to [0, rangeScale].
    // Ported from: itwinjs-core Quantization.quantize()
    inline uint16_t quantize(double pos, double origin, double scale, uint32_t rangeScale = rangeScale16) noexcept
    {
        return static_cast<uint16_t>(
            std::floor(std::max(0.0, std::min(static_cast<double>(rangeScale), 0.5 + (pos - origin) * scale))));
    }

    // Check if value is quantizable.
    // Ported from: itwinjs-core Quantization.isQuantizable()
    inline bool isQuantizable(double pos, double origin, double scale, uint32_t rangeScale = rangeScale16) noexcept
    {
        return isInRange(quantize(pos, origin, scale, rangeScale), rangeScale);
    }

    // unquantize a value.
    // Ported from: itwinjs-core Quantization.unquantize()
    inline double unquantize(uint16_t qpos, double origin, double scale) noexcept
    {
        return 0.0 == scale ? origin : origin + static_cast<double>(qpos) / scale;
    }

    // Check if value is a valid quantized 16-bit value.
    // Ported from: itwinjs-core Quantization.isQuantized()
    inline bool isQuantized(double qpos) noexcept
    {
        return isInRange(qpos) && qpos == std::floor(qpos);
    }
}  // namespace Quantization

// Parameters for quantizing 3d points to 16-bit unsigned integers.
// Ported from: itwinjs-core QParams3d
class DQ_COMMON_EXPORT QParams3d {
public:
    dqGeom::Point3d origin;
    dqGeom::Point3d scale;

    QParams3d() noexcept = default;

    // Set from origin and scale.
    void setFromOriginAndScale(const dqGeom::Point3d& o, const dqGeom::Point3d& s) noexcept
    {
        origin = o;
        scale = s;
    }

    // Initialize for quantization of values within the specified range.
    // Ported from: itwinjs-core QParams3d.setFromRange()
    void setFromRange(const dqGeom::Range3d& range, uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        if (!range.isNull()) {
            origin = range.low;
            scale = dqGeom::Point3d::From(
                Quantization::computeScale(range.high.x - range.low.x, rangeScale),
                Quantization::computeScale(range.high.y - range.low.y, rangeScale),
                Quantization::computeScale(range.high.z - range.low.z, rangeScale));
        } else {
            origin = dqGeom::Point3d::FromZero();
            scale = dqGeom::Point3d::FromZero();
        }
    }

    // create from range.
    // Ported from: itwinjs-core QParams3d.fromRange()
    static QParams3d fromRange(const dqGeom::Range3d& range, uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        QParams3d params;
        params.setFromRange(range, rangeScale);
        return params;
    }

    // create from origin and scale.
    // Ported from: itwinjs-core QParams3d.fromOriginAndScale()
    static QParams3d fromOriginAndScale(const dqGeom::Point3d& o, const dqGeom::Point3d& s) noexcept
    {
        QParams3d params;
        params.setFromOriginAndScale(o, s);
        return params;
    }

    // create for range [-1, 1].
    // Ported from: itwinjs-core QParams3d.fromNormalizedRange()
    static QParams3d fromNormalizedRange(uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        return fromRange(dqGeom::Range3d::CreateXYZXYZ(-1, -1, -1, 1, 1, 1), rangeScale);
    }

    // create for range [0, 1].
    // Ported from: itwinjs-core QParams3d.fromZeroToOne()
    static QParams3d fromZeroToOne(uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        return fromRange(dqGeom::Range3d::CreateXYZXYZ(0, 0, 0, 1, 1, 1), rangeScale);
    }

    // copy from another.
    void copyFrom(const QParams3d& src) noexcept
    {
        origin = src.origin;
        scale = src.scale;
    }

    // clone.
    QParams3d clone() const noexcept { return *this; }

    // unquantize a point.
    // Ported from: itwinjs-core QParams3d.unquantize()
    dqGeom::Point3d unquantize(uint16_t x, uint16_t y, uint16_t z) const noexcept
    {
        return dqGeom::Point3d::From(
            Quantization::unquantize(x, origin.x, scale.x),
            Quantization::unquantize(y, origin.y, scale.y),
            Quantization::unquantize(z, origin.z, scale.z));
    }

    // The diagonal of the unquantized range.
    // Ported from: itwinjs-core QParams3d.rangeDiagonal
    dqGeom::Vector3d rangeDiagonal() const noexcept
    {
        return dqGeom::Vector3d::From(
            scale.x == 0.0 ? 0.0 : Quantization::rangeScale16 / scale.x,
            scale.y == 0.0 ? 0.0 : Quantization::rangeScale16 / scale.y,
            scale.z == 0.0 ? 0.0 : Quantization::rangeScale16 / scale.z);
    }

    // Check if point is quantizable.
    // Ported from: itwinjs-core QParams3d.isQuantizable()
    bool isQuantizable(const dqGeom::Point3d& point) const noexcept
    {
        return Quantization::isQuantizable(point.x, origin.x, scale.x) &&
               Quantization::isQuantizable(point.y, origin.y, scale.y) &&
               Quantization::isQuantizable(point.z, origin.z, scale.z);
    }

    // Compute the range to which these parameters quantize.
    // Ported from: itwinjs-core QParams3d.computeRange()
    dqGeom::Range3d computeRange() const noexcept
    {
        auto range = dqGeom::Range3d::CreateNull();
        range.ExtendPoint(origin);
        const auto diag = rangeDiagonal();
        range.ExtendPoint(dqGeom::Point3d::From(origin.x + diag.x, origin.y + diag.y, origin.z + diag.z));
        return range;
    }
};

// A Point3d compressed to 16-bit unsigned integers per component.
// Ported from: itwinjs-core QPoint3d
class DQ_COMMON_EXPORT QPoint3d {
public:
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t z = 0;

    QPoint3d() noexcept = default;
    QPoint3d(uint16_t x_, uint16_t y_, uint16_t z_) noexcept : x(x_), y(y_), z(z_) {}

    // create by quantizing a Point3d.
    // Ported from: itwinjs-core QPoint3d.create()
    static QPoint3d create(const dqGeom::Point3d& pos, const QParams3d& params) noexcept
    {
        return QPoint3d(
            Quantization::quantize(pos.x, params.origin.x, params.scale.x),
            Quantization::quantize(pos.y, params.origin.y, params.scale.y),
            Quantization::quantize(pos.z, params.origin.z, params.scale.z));
    }

    // create from scalars.
    // Ported from: itwinjs-core QPoint3d.fromScalars()
    static QPoint3d fromScalars(uint16_t x_, uint16_t y_, uint16_t z_) noexcept
    {
        return QPoint3d(x_, y_, z_);
    }

    // Set from scalars.
    void setFromScalars(uint16_t x_, uint16_t y_, uint16_t z_) noexcept
    {
        x = x_;
        y = y_;
        z = z_;
    }

    // Initialize by quantizing a Point3d.
    // Ported from: itwinjs-core QPoint3d.init()
    void init(const dqGeom::Point3d& pos, const QParams3d& params) noexcept
    {
        *this = create(pos, params);
    }

    // copy from another.
    void copyFrom(const QPoint3d& src) noexcept
    {
        x = src.x;
        y = src.y;
        z = src.z;
    }

    // clone.
    QPoint3d clone() const noexcept { return *this; }

    // unquantize to Point3d.
    // Ported from: itwinjs-core QPoint3d.unquantize()
    dqGeom::Point3d unquantize(const QParams3d& params) const noexcept
    {
        return params.unquantize(x, y, z);
    }

    // Equality.
    // Ported from: itwinjs-core QPoint3d.equals()
    bool equals(const QPoint3d& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }

    // Ordinal comparison.
    // Ported from: itwinjs-core QPoint3d.compare()
    int compare(const QPoint3d& rhs) const noexcept
    {
        if (x != rhs.x) return x < rhs.x ? -1 : 1;
        if (y != rhs.y) return y < rhs.y ? -1 : 1;
        if (z != rhs.z) return z < rhs.z ? -1 : 1;
        return 0;
    }
};

// A list of QPoint3d all quantized to the same range.
// Ported from: itwinjs-core QPoint3dList
class DQ_COMMON_EXPORT QPoint3dList {
public:
    QParams3d params;

    QPoint3dList() = default;
    explicit QPoint3dList(const QParams3d& p) : params(p) {}

    // Construct from points.
    // Ported from: itwinjs-core QPoint3dList.fromPoints()
    static QPoint3dList fromPoints(const dqGeom::Point3d* points, int count)
    {
        auto range = dqGeom::Range3d::CreateNull();
        for (int i = 0; i < count; ++i)
            range.ExtendPoint(points[i]);
        QPoint3dList list(QParams3d::fromRange(range));
        for (int i = 0; i < count; ++i)
            list.add(points[i]);
        return list;
    }

    // Remove all points.
    void clear() noexcept { m_list.clear(); }

    // clear and change parameters.
    void reset(const QParams3d& p) noexcept
    {
        clear();
        params = p;
    }

    // quantize and append a point.
    // Ported from: itwinjs-core QPoint3dList.add()
    void add(const dqGeom::Point3d& pt) noexcept
    {
        m_list.push_back(QPoint3d::create(pt, params));
    }

    // add a previously-quantized point.
    void push(const QPoint3d& qpt) noexcept
    {
        m_list.push_back(qpt);
    }

    // Number of points.
    int length() const noexcept { return static_cast<int>(m_list.size()); }

    // Get quantized point at index.
    const QPoint3d& get(int index) const noexcept { return m_list[index]; }

    // unquantize point at index.
    // Ported from: itwinjs-core QPoint3dList.unquantize()
    dqGeom::Point3d unquantize(int index) const noexcept
    {
        return m_list[index].unquantize(params);
    }

    // requantize all points to new parameters.
    // Ported from: itwinjs-core QPoint3dList.requantize()
    void requantize(const QParams3d& newParams) noexcept
    {
        for (auto& pt : m_list) {
            const auto unq = pt.unquantize(params);
            pt = QPoint3d::create(unq, newParams);
        }
        params = newParams;
    }

    // Convert to Uint16Array (x,y,z triples).
    std::vector<uint16_t> toTypedArray() const noexcept
    {
        std::vector<uint16_t> array(m_list.size() * 3);
        for (size_t i = 0; i < m_list.size(); ++i) {
            array[i * 3 + 0] = m_list[i].x;
            array[i * 3 + 1] = m_list[i].y;
            array[i * 3 + 2] = m_list[i].z;
        }
        return array;
    }

private:
    std::vector<QPoint3d> m_list;
};

// Simple 2d point type (dqGeom does not yet have Point2d).
struct Point2d {
    double x = 0.0;
    double y = 0.0;
    static Point2d from(double x_, double y_) noexcept { return {x_, y_}; }
};

// Parameters for quantizing 2d points to 16-bit unsigned integers.
// Ported from: itwinjs-core QParams2d
class DQ_COMMON_EXPORT QParams2d {
public:
    Point2d origin;
    Point2d scale;

    QParams2d() noexcept = default;

    // Set from origin and scale.
    void setFromOriginAndScale(const Point2d& o, const Point2d& s) noexcept
    {
        origin = o;
        scale = s;
    }

    // Initialize for quantization of values within the specified range.
    // Ported from: itwinjs-core QParams2d.setFromRange()
    void setFromRange(const dqGeom::Range2d& range, uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        if (!range.isNull()) {
            origin = {range.low.x, range.low.y};
            scale = Point2d::from(
                Quantization::computeScale(range.high.x - range.low.x, rangeScale),
                Quantization::computeScale(range.high.y - range.low.y, rangeScale));
        } else {
            origin = {0.0, 0.0};
            scale = {0.0, 0.0};
        }
    }

    // create from range.
    // Ported from: itwinjs-core QParams2d.fromRange()
    static QParams2d fromRange(const dqGeom::Range2d& range, uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        QParams2d params;
        params.setFromRange(range, rangeScale);
        return params;
    }

    // create from origin and scale.
    // Ported from: itwinjs-core QParams2d.fromOriginAndScale()
    static QParams2d fromOriginAndScale(const Point2d& o, const Point2d& s) noexcept
    {
        QParams2d params;
        params.setFromOriginAndScale(o, s);
        return params;
    }

    // create for range [-1, 1].
    // Ported from: itwinjs-core QParams2d.fromNormalizedRange()
    static QParams2d fromNormalizedRange(uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        return fromRange(dqGeom::Range2d::CreateXYXY(-1.0, -1.0, 1.0, 1.0), rangeScale);
    }

    // create for range [0, 1].
    // Ported from: itwinjs-core QParams2d.fromZeroToOne()
    static QParams2d fromZeroToOne(uint32_t rangeScale = Quantization::rangeScale16) noexcept
    {
        return fromRange(dqGeom::Range2d::CreateXYXY(0.0, 0.0, 1.0, 1.0), rangeScale);
    }

    // copy from another.
    void copyFrom(const QParams2d& src) noexcept
    {
        origin = src.origin;
        scale = src.scale;
    }

    // clone.
    QParams2d clone() const noexcept { return *this; }

    // unquantize a point.
    // Ported from: itwinjs-core QParams2d.unquantize()
    Point2d unquantize(uint16_t x, uint16_t y) const noexcept
    {
        return Point2d::from(
            Quantization::unquantize(x, origin.x, scale.x),
            Quantization::unquantize(y, origin.y, scale.y));
    }

    // The diagonal of the unquantized range.
    // Ported from: itwinjs-core QParams2d.rangeDiagonal
    Point2d rangeDiagonal() const noexcept
    {
        return Point2d::from(
            scale.x == 0.0 ? 0.0 : Quantization::rangeScale16 / scale.x,
            scale.y == 0.0 ? 0.0 : Quantization::rangeScale16 / scale.y);
    }

    // Check if point is quantizable.
    // Ported from: itwinjs-core QParams2d.isQuantizable()
    bool isQuantizable(const Point2d& point) const noexcept
    {
        return Quantization::isQuantizable(point.x, origin.x, scale.x) &&
               Quantization::isQuantizable(point.y, origin.y, scale.y);
    }
};

// A Point2d compressed to 16-bit unsigned integers per component.
// Ported from: itwinjs-core QPoint2d
class DQ_COMMON_EXPORT QPoint2d {
public:
    uint16_t x = 0;
    uint16_t y = 0;

    QPoint2d() noexcept = default;
    QPoint2d(uint16_t x_, uint16_t y_) noexcept : x(x_), y(y_) {}

    // create by quantizing a Point2d.
    // Ported from: itwinjs-core QPoint2d.create()
    static QPoint2d create(const Point2d& pos, const QParams2d& params) noexcept
    {
        return QPoint2d(
            Quantization::quantize(pos.x, params.origin.x, params.scale.x),
            Quantization::quantize(pos.y, params.origin.y, params.scale.y));
    }

    // create from scalars.
    // Ported from: itwinjs-core QPoint2d.fromScalars()
    static QPoint2d fromScalars(uint16_t x_, uint16_t y_) noexcept
    {
        return QPoint2d(x_, y_);
    }

    // Set from scalars.
    void setFromScalars(uint16_t x_, uint16_t y_) noexcept
    {
        x = x_;
        y = y_;
    }

    // Initialize by quantizing a Point2d.
    // Ported from: itwinjs-core QPoint2d.init()
    void init(const Point2d& pos, const QParams2d& params) noexcept
    {
        *this = create(pos, params);
    }

    // copy from another.
    void copyFrom(const QPoint2d& src) noexcept
    {
        x = src.x;
        y = src.y;
    }

    // clone.
    QPoint2d clone() const noexcept { return *this; }

    // unquantize to Point2d.
    // Ported from: itwinjs-core QPoint2d.unquantize()
    Point2d unquantize(const QParams2d& params) const noexcept
    {
        return params.unquantize(x, y);
    }

    // Equality.
    bool equals(const QPoint2d& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

// A list of QPoint2d all quantized to the same range.
// Ported from: itwinjs-core QPoint2dList
class DQ_COMMON_EXPORT QPoint2dList {
public:
    QParams2d params;

    QPoint2dList() = default;
    explicit QPoint2dList(const QParams2d& p) : params(p) {}

    // Construct from points.
    // Ported from: itwinjs-core QPoint2dList.fromPoints()
    static QPoint2dList fromPoints(const Point2d* points, int count)
    {
        auto range = dqGeom::Range2d::CreateXY(points[0].x, points[0].y);
        for (int i = 1; i < count; ++i)
            range.ExtendXY(points[i].x, points[i].y);
        QPoint2dList list(QParams2d::fromRange(range));
        for (int i = 0; i < count; ++i)
            list.add(points[i]);
        return list;
    }

    // Remove all points.
    void clear() noexcept { m_list.clear(); }

    // clear and change parameters.
    void reset(const QParams2d& p) noexcept
    {
        clear();
        params = p;
    }

    // quantize and append a point.
    // Ported from: itwinjs-core QPoint2dList.add()
    void add(const Point2d& pt) noexcept
    {
        m_list.push_back(QPoint2d::create(pt, params));
    }

    // add a previously-quantized point.
    void push(const QPoint2d& qpt) noexcept
    {
        m_list.push_back(qpt);
    }

    // Number of points.
    int length() const noexcept { return static_cast<int>(m_list.size()); }

    // Get quantized point at index.
    const QPoint2d& get(int index) const noexcept { return m_list[index]; }

    // unquantize point at index.
    // Ported from: itwinjs-core QPoint2dList.unquantize()
    Point2d unquantize(int index) const noexcept
    {
        return m_list[index].unquantize(params);
    }

    // requantize all points to new parameters.
    // Ported from: itwinjs-core QPoint2dList.requantize()
    void requantize(const QParams2d& newParams) noexcept
    {
        for (auto& pt : m_list) {
            const auto unq = pt.unquantize(params);
            pt = QPoint2d::create(unq, newParams);
        }
        params = newParams;
    }

    // Convert to Uint16Array (x,y pairs).
    std::vector<uint16_t> toTypedArray() const noexcept
    {
        std::vector<uint16_t> array(m_list.size() * 2);
        for (size_t i = 0; i < m_list.size(); ++i) {
            array[i * 2 + 0] = m_list[i].x;
            array[i * 2 + 1] = m_list[i].y;
        }
        return array;
    }

private:
    std::vector<QPoint2d> m_list;
};

END_DQ_COMMON_NAMESPACE
