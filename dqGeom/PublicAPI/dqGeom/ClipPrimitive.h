// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipPrimitive / ClipShape (clip volume primitive + swept shape)
// Ported from: itwinjs-core core/geometry/src/clipping/ClipPrimitive.ts
//
// M-P（Sectioning 剖切）P-A 落地。ClipPrimitive = UnionOfConvexClipPlaneSets 容器 +
// invisible 位；ClipShape = xy 多边形扫掠体（zLow/zHigh + 局部↔世界变换 + isMask 洞位）。
// §3.4 适配（族先例）：bvector → std::vector；TS union Props → 双 optional 成员结构；
/// result 复用形参缺席（RefPtr 世界重新绑定即等价）；Matrix4d 族未移植（见 TODO）。
//
// TODO（参考有、依赖未移植，随依赖落地补齐）：
//  - ClipShape.parsePolygonPlanes（凹多边形与 mask 洞解析，ClipPrimitive.ts:783-824）：
//    依赖 PolylineOps.compressDanglers + Triangulator.createTriangulatedGraphFromSingleLoop
//    + flipTriangles + announceFaceLoops + HalfEdgeGraph +（mask 分支）
//    AlternatingCCTreeNode.createHullAndInletsForPolygon —— 均未移植。当前行为 = 参考
//    三角化不可用时的失败形态（不加任何凸集，整个形状视为空裁剪），视图剖切四种定义
//    （Plane/Range/Shape-凸/Element）不触及该路径。
//  - multiplyPlanesByMatrix4d（Matrix4d 未移植）。
#pragma once

#include <dqBase/RefCounted.h>

#include <dqGeom/ClipPlane.h>
#include <dqGeom/ConvexClipPlaneSet.h>
#include <dqGeom/Export.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>
#include <dqGeom/UnionOfConvexClipPlaneSets.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

class ClipShape;  // 后置完整定义（ClipPrimitive::asClipShape 返回类型）

/// Bit mask type for referencing subsets of 6 planes of range box.
/// Ported from: itwinjs-core ClipMaskXYZRangePlanes (ClipPrimitive.ts:37-56)
enum class ClipMaskXYZRangePlanes : uint32_t {
    None = 0x00,
    XLow = 0x01,
    XHigh = 0x02,
    YLow = 0x04,
    YHigh = 0x08,
    ZLow = 0x10,
    ZHigh = 0x20,
    XAndY = 0x0f,
    All = 0x3f,
};
// §3.4 适配：enum class 位运算（参考 TS number 掩码）。
inline constexpr ClipMaskXYZRangePlanes operator&(ClipMaskXYZRangePlanes a, ClipMaskXYZRangePlanes b) noexcept {
    return static_cast<ClipMaskXYZRangePlanes>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr ClipMaskXYZRangePlanes operator|(ClipMaskXYZRangePlanes a, ClipMaskXYZRangePlanes b) noexcept {
    return static_cast<ClipMaskXYZRangePlanes>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/// Wire transform rows (3 rows × 4 cols: matrix row + origin col) — the array
/// form of TransformProps consumed by ClipShape trans.
/// Ported from: itwinjs-core TransformProps (array form, Transform.ts)
struct ClipShapeTransformProps {
    std::array<std::array<double, 4>, 3> rows{};
};

/// Inner wire part of a ClipShape: { points, trans?, zlow?, zhigh?, mask?,
/// invisible? }.
/// Ported from: itwinjs-core ClipPrimitiveShapeProps.shape (ClipPrimitive.ts:75-91)
struct ClipPrimitiveShapePart {
    std::vector<Point3d> points;                        // XYZProps[]（宿主归一为分量）
    std::optional<ClipShapeTransformProps> trans;
    std::optional<double> zlow;
    std::optional<double> zhigh;
    std::optional<bool> mask;
    std::optional<bool> invisible;
};

/// Wire format describing a ClipShape: { shape: {...} }.
/// Ported from: itwinjs-core ClipPrimitiveShapeProps (ClipPrimitive.ts:75-91)
struct ClipPrimitiveShapeProps {
    std::optional<ClipPrimitiveShapePart> shape;
};

/// Inner wire part of a planes-defined ClipPrimitive: { clips?, invisible? }.
/// Ported from: itwinjs-core ClipPrimitivePlanesProps.planes (ClipPrimitive.ts:62-69)
struct ClipPrimitivePlanesPart {
    std::optional<UnionOfConvexClipPlaneSetsProps> clips;
    std::optional<bool> invisible;
};

/// Wire format describing a ClipPrimitive defined by clip planes:
/// { planes: {...} }.
/// Ported from: itwinjs-core ClipPrimitivePlanesProps (ClipPrimitive.ts:62-69)
struct ClipPrimitivePlanesProps {
    std::optional<ClipPrimitivePlanesPart> planes;
};

/// Wire format describing a ClipPrimitive (planes | shape union).
/// Ported from: itwinjs-core ClipPrimitiveProps (ClipPrimitive.ts:93-96)
// §3.4 适配：TS union → 双 optional 臂（宿主 JSON 层按实际线形态填充其一）。
struct ClipPrimitiveProps {
    std::optional<ClipPrimitivePlanesPart> planes;
    std::optional<ClipPrimitiveShapePart> shape;
};

/// ClipPrimitive is a base class for clipping implementations that use a
/// UnionOfConvexClipPlaneSets ("clipPlanes") plus an "invisible" flag. The
/// derived ClipShape carries a swept shape that generates the planes.
/// Ported from: itwinjs-core ClipPrimitive (ClipPrimitive.ts:116-324)
class DQ_GEOM_EXPORT ClipPrimitive : public dqBase::RefCounted<ClipPrimitive> {
public:
    using Ptr = dqBase::RefPtr<ClipPrimitive>;

    ~ClipPrimitive() override = default;

    /// Get a pointer to the UnionOfConvexClipPlaneSets (triggers lazy
    /// construction via ensurePlaneSets; null when absent).
    /// Ported from: ClipPrimitive.fetchClipPlanesRef (:126-129)
    UnionOfConvexClipPlaneSets* fetchClipPlanesRef();
    UnionOfConvexClipPlaneSets const* fetchClipPlanesRef() const;

    /// Return whether this primitive is invisible.
    /// Ported from: ClipPrimitive.invisible (:131-133)
    bool invisible() const noexcept { return m_invisible; }

    /// Create a ClipPrimitive, capturing the supplied plane set as the clip
    /// planes (a single ConvexClipPlaneSet is wrapped into a union).
    /// Ported from: ClipPrimitive.createCapture (:143-152)
    static Ptr createCapture(UnionOfConvexClipPlaneSets planes, bool isInvisible = false);
    static Ptr createCapture(ConvexClipPlaneSet const& planes, bool isInvisible = false);

    /// Emit json form of the clip planes.
    /// Ported from: ClipPrimitive.toJSON (:154-161)
    virtual ClipPrimitiveProps toJSON() const;

    /// Returns true if the planes are present (a ClipShape holding a polygon
    /// but not yet asked to construct planes returns false).
    /// Ported from: ClipPrimitive.arePlanesDefined (:167-169)
    bool arePlanesDefined() const noexcept { return m_clipPlanes.has_value(); }

    /// Return a deep clone.
    /// Ported from: ClipPrimitive.clone (:171-175) — base clone returns a base
    /// ClipPrimitive; ClipShape overrides.
    virtual Ptr clone() const;

    /// Trigger (if needed) computation of plane sets in the derived class.
    /// Base class is a no op.
    /// Ported from: ClipPrimitive.ensurePlaneSets (:182)
    // §3.4 适配：TS 在（逻辑上 const 的）查询路径里懒建平面集；C++ 以 mutable
    // 缓存 + const 虚方法承载同一懒建语义。
    virtual void ensurePlaneSets() const {}

    /// True if the point lies inside/on this clipper (mask semantics belong
    /// to the ClipShape override).
    /// Ported from: ClipPrimitive.pointInside (:189-195)
    bool pointInside(Point3d const& point, double onTolerance = 1.0e-12) const;   /// ← Geometry.smallMetricDistanceSquared（Geometry.ts:258——2026-10-07 审计 B1：原 1e-14 偏 100×）

    /// Method from the Clipper interface.
    /// Ported from: ClipPrimitive.isPointOnOrInside (:200-206)
    bool isPointOnOrInside(Point3d const& point, double onTolerance = 1.0e-12) const;   /// 同上

    /// Announce the fractional interval of a segment inside this clipper.
    /// Ported from: ClipPrimitive.announceClippedSegmentIntervals (:211-219)
    bool announceClippedSegmentIntervals(double f0, double f1, Point3d const& pointA,
                                         Point3d const& pointB,
                                         AnnounceNumberNumber const& announce = nullptr) const;

    /// Apply a transform to the clipper (transform all planes).
    /// Ported from: ClipPrimitive.transformInPlace (:264-269)
    bool transformInPlace(Transform const& transform);

    /// Sets the primitive visibility.
    /// Ported from: ClipPrimitive.setInvisible (:271-273)
    void setInvisible(bool invis) noexcept { m_invisible = invis; }

    /// True if any plane of the primary clipPlanes has a non-zero z normal
    /// component and finite distance.
    /// Ported from: ClipPrimitive.containsZClip (:278-287)
    bool containsZClip() const;

    /// Quick test of whether the given points fall completely inside or
    /// outside (ClipShape override inverts for masks).
    /// Ported from: ClipPrimitive.classifyPointContainment (:293-300)
    virtual ClipPlaneContainment classifyPointContainment(std::vector<Point3d> const& points,
                                                           bool ignoreMasks = true) const;

    /// Promote json to a class instance: first try ClipShape, then the base
    /// ClipPrimitive. Returns null when neither parses.
    /// Ported from: ClipPrimitive.fromJSON (:306-313)
    static Ptr fromJSON(ClipPrimitiveProps const* json);
    /// Specific converter producing the base class ClipPrimitive.
    /// Ported from: ClipPrimitive.fromJSONClipPrimitive (:315-323)
    static Ptr fromJSONClipPrimitive(ClipPrimitiveProps const* json);

    /// §3.4 适配：TS `instanceof ClipShape` 的 no-RTTI 等价物（-fno-rtti 禁
    /// dynamic_cast）。ClipShape 覆写返回 this；消费方如
    /// ClipVector.extractBoundaryLoops 的多态分派。
    virtual ClipShape const* asClipShape() const noexcept { return nullptr; }

protected:
    /// Ported from: ClipPrimitive ctor (:134-137)
    ClipPrimitive(std::optional<UnionOfConvexClipPlaneSets> planeSet = std::nullopt,
                  bool isInvisible = false);

    mutable std::optional<UnionOfConvexClipPlaneSets> m_clipPlanes;  // mutable：懒建缓存（见 ensurePlaneSets）
    bool m_invisible = false;
};

/// A clipping volume defined by a shape (array of 3d points using only the x
/// and y dimensions), swept between optional zLow/zHigh in the local frame.
/// Ported from: itwinjs-core ClipShape (ClipPrimitive.ts:358-903)
class DQ_GEOM_EXPORT ClipShape : public ClipPrimitive {
public:
    using Ptr = dqBase::RefPtr<ClipShape>;

    ~ClipShape() override = default;

    /// Return local to world transform (identity when none supplied).
    /// Ported from: ClipShape.transformFromClip (:385-387)
    Transform const* transformFromClip() const noexcept { return m_transformFromClip ? &*m_transformFromClip : nullptr; }
    /// Return world to local transform (identity when none supplied).
    /// Ported from: ClipShape.transformToClip (:389-391)
    Transform const* transformToClip() const noexcept { return m_transformToClip ? &*m_transformToClip : nullptr; }
    /// True if this ClipShape has a local to world transform.
    /// Ported from: ClipShape.transformValid (:396-398)
    bool transformValid() const noexcept { return m_transformFromClip.has_value(); }
    /// True if this ClipShape's lower z boundary is set.
    /// Ported from: ClipShape.zLowValid (:403-405)
    bool zLowValid() const noexcept { return m_zLow.has_value(); }
    /// True if this ClipShape's upper z boundary is set.
    /// Ported from: ClipShape.zHighValid (:412-414)
    bool zHighValid() const noexcept { return m_zHigh.has_value(); }
    /// Type guard for zLow.
    /// Ported from: ClipShape.hasZLow (:422-424)
    bool hasZLow() const noexcept { return m_zLow.has_value(); }
    /// Type guard for zHigh.
    /// Ported from: ClipShape.hasZHigh (:427-429)
    bool hasZHigh() const noexcept { return m_zHigh.has_value(); }
    /// Type guard for transformFromClip.
    /// Ported from: ClipShape.hasTransformFromClip (:433-435)
    bool hasTransformFromClip() const noexcept { return m_transformFromClip.has_value(); }
    /// Type guard for both transforms.
    /// Ported from: ClipShape.hasTransforms (:447-449)
    bool hasTransforms() const noexcept { return m_transformFromClip.has_value() && m_transformToClip.has_value(); }

    /// Return this zLow (nullopt when unset).
    /// Ported from: ClipShape.zLow (:451-453)
    std::optional<double> zLow() const noexcept { return m_zLow; }
    /// Return this zHigh (nullopt when unset).
    /// Ported from: ClipShape.zHigh (:455-457)
    std::optional<double> zHigh() const noexcept { return m_zHigh; }
    /// Returns a reference to this ClipShape's polygon array.
    /// Ported from: ClipShape.polygon (:459-461)
    std::vector<Point3d> const& polygon() const noexcept { return m_polygon; }
    std::vector<Point3d>& polygon() noexcept { return m_polygon; }
    /// Returns true if this ClipShape is a masking set.
    /// Ported from: ClipShape.isMask (:463-465)
    bool isMask() const noexcept { return m_isMask; }

    /// Sets the polygon points array of this ClipShape (adds closure point).
    /// Ported from: ClipShape.setPolygon (:467-472)
    void setPolygon(std::vector<Point3d> polygon);

    /// If the UnionOfConvexClipPlaneSets is undefined, generate it from the
    /// ClipShape and transform.
    /// Ported from: ClipShape.ensurePlaneSets (:478-485, override)
    void ensurePlaneSets() const override;

    /// Initialize members that may at times be undefined.
    /// Ported from: ClipShape.initSecondaryProps (:490-502)
    void initSecondaryProps(bool isMask, std::optional<double> zLow, std::optional<double> zHigh,
                            Transform const* transform);

    /// Emit json object form.
    /// Ported from: ClipShape.toJSON (:504-519, override)
    ClipPrimitiveProps toJSON() const override;

    /// Parse json to a clip shape (inner shape part).
    /// Ported from: ClipShape.fromClipShapeJSON (:521-533)
    static ClipShape::Ptr fromClipShapeJSON(ClipPrimitiveShapePart const* shape);

    /// Returns a new ClipShape that is a deep copy of the ClipShape given.
    /// Ported from: ClipShape.createFrom (:535-547)
    static ClipShape::Ptr createFrom(ClipShape const& other);

    /// Create a ClipShape from an array of points making up a 2d shape (deep
    /// copy; closure point enforced). Returns null when fewer than 3 points.
    /// Ported from: ClipShape.createShape (:549-575)
    static ClipShape::Ptr createShape(std::vector<Point3d> const& polygon,
                                      std::optional<double> zLow = std::nullopt,
                                      std::optional<double> zHigh = std::nullopt,
                                      Transform const* transform = nullptr, bool isMask = false,
                                      bool invisible = false);

    /// Create a ClipShape that exists as a 3d box of the range given,
    /// optionally storing zLow/zHigh from the range per the mask.
    /// Ported from: ClipShape.createBlock (:580-608)
    static ClipShape::Ptr createBlock(Range3d const& extremities, ClipMaskXYZRangePlanes clipMask,
                                      bool isMask = false, bool invisible = false,
                                      Transform const* transform = nullptr);

    /// Creates a new ClipShape with undefined members and empty polygon.
    /// Ported from: ClipShape.createEmpty (:610-621)
    static ClipShape::Ptr createEmpty(bool isMask = false, bool invisible = false,
                                      Transform const* transform = nullptr);

    /// Checks that the member polygon has an area and is closed.
    /// Ported from: ClipShape.isValidPolygon (:623-629)
    bool isValidPolygon() const noexcept;

    /// Returns a deep copy of this ClipShape.
    /// Ported from: ClipShape.clone (:631-633, override)
    ClipPrimitive::Ptr clone() const override;

    /// Apply transform to the local-to-world transform and the clip planes.
    /// Ported from: ClipShape.transformInPlace (:845-855, override)
    bool transformInPlace(Transform const& transform);

    /// True when the local frame z column is parallel to global z (xy polygon).
    /// Ported from: ClipShape.isXYPolygon (:863-870)
    bool isXYPolygon() const noexcept;

    /// Transform the point in place using transformToClip.
    /// Ported from: ClipShape.performTransformToClip (:872-875)
    void performTransformToClip(Point3d& point) const noexcept;
    /// Transform the point in place using transformFromClip.
    /// Ported from: ClipShape.performTransformFromClip (:877-880)
    void performTransformFromClip(Point3d& point) const noexcept;

    /// Quick containment test; inverts for masked instances unless ignoreMasks.
    /// Ported from: ClipShape.classifyPointContainment (:886-902, override)
    ClipPlaneContainment classifyPointContainment(std::vector<Point3d> const& points,
                                                  bool ignoreMasks) const override;

    /// §3.4 适配：TS `instanceof ClipShape` —— 见 ClipPrimitive::asClipShape。
    ClipShape const* asClipShape() const noexcept override { return this; }

protected:
    /// Ported from: ClipShape ctor (:372-379)
    ClipShape(std::vector<Point3d> polygon, std::optional<double> zLow,
              std::optional<double> zHigh, Transform const* transform, bool isMask, bool invisible);

private:
    /// Given the current polygon data, parse clip planes that form the shape.
    /// Ported from: ClipShape.parseClipPlanes (:638-655)
    bool parseClipPlanes(UnionOfConvexClipPlaneSets& set) const;
    /// Two-point (linear, select-by-line) region planes.
    /// Ported from: ClipShape.parseLinearPlanes (:661-696)
    bool parseLinearPlanes(UnionOfConvexClipPlaneSets& set, Point3d const& start, Point3d const& end,
                           std::optional<double> cameraFocalLength = std::nullopt) const;
    /// Convex polygon planes (interior region or exterior fans per
    /// buildExteriorClipper).
    /// Ported from: ClipShape.parseConvexPolygonPlanes (:708-778)
    bool parseConvexPolygonPlanes(UnionOfConvexClipPlaneSets& set, std::vector<Point3d> const& polygon,
                                  double direction, bool buildExteriorClipper,
                                  std::optional<double> cameraFocalLength = std::nullopt) const;
    /// Non-convex polygon and mask-hole planes. TODO — see file header.
    /// Ported from: ClipShape.parsePolygonPlanes (:783-824)
    bool parsePolygonPlanes(UnionOfConvexClipPlaneSets& set, std::vector<Point3d> const& polygon,
                            bool isMask, std::optional<double> cameraFocalLength = std::nullopt) const;

    std::vector<Point3d> m_polygon;
    std::optional<double> m_zLow;
    std::optional<double> m_zHigh;
    bool m_isMask = false;
    std::optional<Transform> m_transformFromClip;
    std::optional<Transform> m_transformToClip;
};

END_DQ_GEOM_NAMESPACE
