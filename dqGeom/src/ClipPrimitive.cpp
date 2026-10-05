// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — ClipPrimitive / ClipShape implementation
// Ported from: itwinjs-core core/geometry/src/clipping/ClipPrimitive.ts
//
// M-P（Sectioning 剖切）P-A 落地。凹多边形/mask 解析路径 TODO（见 ClipPrimitive.h 头注）。
#include <dqGeom/ClipPrimitive.h>

#include <dqGeom/CurvePrimitive.h>  // AnnounceNumberNumber
#include <dqGeom/Geometry.h>
#include <dqGeom/PolygonOps.h>

#include <cassert>
#include <cmath>
#include <limits>

namespace dqGeom {
namespace {

// Internal helper holding XYZ components that serves as a representation of
// polygon edges defined by clip planes.
// Ported from: ClipPrimitive.ts:327-350 (class PolyEdge)
struct PolyEdge {
    Point3d pointA;
    Point3d pointB;
    Vector3d normal;

    PolyEdge(Point3d const& origin, Point3d const& next, Vector3d const& normalIn, double z)
        : pointA(Point3d::From(origin.x, origin.y, z)),
          pointB(Point3d::From(next.x, next.y, z)),
          normal(normalIn) {}

    // Assume both normals are unit length.
    // Ported from: PolyEdge.makeUnitPerpendicularToBisector (ClipPrimitive.ts:339-349)
    static std::optional<Vector3d> makeUnitPerpendicularToBisector(PolyEdge const& edgeA, PolyEdge const& edgeB,
                                                                   bool reverse) {
        // candidate = edgeB.normal.minus(edgeA.normal)
        Vector3d candidate = Vector3d::From(edgeB.normal.x - edgeA.normal.x,
                                            edgeB.normal.y - edgeA.normal.y,
                                            edgeB.normal.z - edgeA.normal.z);
        if (candidate.Normalize() <= kSmallFloatingPoint) {
            // adjacent edges are parallel: try chord as bisector normal
            candidate = Vector3d::FromStartEnd(edgeA.pointA, edgeB.pointB);
            if (candidate.Normalize() <= kSmallFloatingPoint)
                return std::nullopt;  // no chord => backtracking edge => fail
        }
        if (reverse)
            candidate.Scale(-1.0);
        return candidate;
    }
};

// Transform <-> wire rows (3 rows x 4 cols: matrix row + origin col).
// Ported from: Transform.fromJSON/toJSON array form (Transform.ts)
Transform transformFromRows(ClipShapeTransformProps const& rows) {
    Matrix3d const matrix = Matrix3d::CreateRowValues(
        rows.rows[0][0], rows.rows[0][1], rows.rows[0][2],
        rows.rows[1][0], rows.rows[1][1], rows.rows[1][2],
        rows.rows[2][0], rows.rows[2][1], rows.rows[2][2]);
    Point3d const origin = Point3d::From(rows.rows[0][3], rows.rows[1][3], rows.rows[2][3]);
    return Transform(origin, matrix);
}

ClipShapeTransformProps transformToRows(Transform const& transform) {
    ClipShapeTransformProps rows;
    for (int r = 0; r < 3; ++r) {
        rows.rows[r][0] = transform.matrix.coffs[r * 3 + 0];
        rows.rows[r][1] = transform.matrix.coffs[r * 3 + 1];
        rows.rows[r][2] = transform.matrix.coffs[r * 3 + 2];
    }
    rows.rows[0][3] = transform.origin.x;
    rows.rows[1][3] = transform.origin.y;
    rows.rows[2][3] = transform.origin.z;
    return rows;
}

}  // namespace

// ---------------------------------------------------------------------------
// ClipPrimitive (base)
// ---------------------------------------------------------------------------

ClipPrimitive::ClipPrimitive(std::optional<UnionOfConvexClipPlaneSets> planeSet, bool isInvisible)
    : m_clipPlanes(std::move(planeSet)), m_invisible(isInvisible) {}

UnionOfConvexClipPlaneSets* ClipPrimitive::fetchClipPlanesRef() {
    ensurePlaneSets();
    return m_clipPlanes ? &*m_clipPlanes : nullptr;
}

UnionOfConvexClipPlaneSets const* ClipPrimitive::fetchClipPlanesRef() const {
    ensurePlaneSets();  // 1:1 参考语义：读路径同样触发懒建（mutable 缓存）
    return m_clipPlanes ? &*m_clipPlanes : nullptr;
}

ClipPrimitive::Ptr ClipPrimitive::createCapture(UnionOfConvexClipPlaneSets planes, bool isInvisible) {
    return Ptr(new ClipPrimitive(std::move(planes), isInvisible));
}

ClipPrimitive::Ptr ClipPrimitive::createCapture(ConvexClipPlaneSet const& planes, bool isInvisible) {
    return createCapture(UnionOfConvexClipPlaneSets::createConvexSets({planes}), isInvisible);
}

ClipPrimitiveProps ClipPrimitive::toJSON() const {
    ClipPrimitiveProps props;
    ClipPrimitivePlanesPart& planes = props.planes.emplace();
    if (m_clipPlanes)
        planes.clips = m_clipPlanes->toJSON();
    if (m_invisible)
        planes.invisible = true;
    return props;
}

ClipPrimitive::Ptr ClipPrimitive::clone() const {
    std::optional<UnionOfConvexClipPlaneSets> newPlanes;
    if (m_clipPlanes)
        newPlanes = m_clipPlanes->clone();
    return Ptr(new ClipPrimitive(std::move(newPlanes), m_invisible));
}

bool ClipPrimitive::pointInside(Point3d const& point, double onTolerance) const {
    ensurePlaneSets();
    bool inside = true;
    if (m_clipPlanes)
        inside = m_clipPlanes->isPointOnOrInside(point, onTolerance);
    return inside;
}

bool ClipPrimitive::isPointOnOrInside(Point3d const& point, double onTolerance) const {
    ensurePlaneSets();
    bool inside = true;
    if (m_clipPlanes)
        inside = m_clipPlanes->isPointOnOrInside(point, onTolerance);
    return inside;
}

bool ClipPrimitive::announceClippedSegmentIntervals(double f0, double f1, Point3d const& pointA,
                                                    Point3d const& pointB,
                                                    AnnounceNumberNumber const& announce) const {
    ensurePlaneSets();
    bool hasInsideParts = false;
    if (m_clipPlanes)
        hasInsideParts = m_clipPlanes->announceClippedSegmentIntervals(f0, f1, pointA, pointB, announce);
    return hasInsideParts;
}

bool ClipPrimitive::transformInPlace(Transform const& transform) {
    if (m_clipPlanes)
        m_clipPlanes->transformInPlace(transform);
    return true;
}

bool ClipPrimitive::containsZClip() const {
    UnionOfConvexClipPlaneSets const* clipPlanes = fetchClipPlanesRef();
    if (clipPlanes != nullptr) {
        for (ConvexClipPlaneSet const& convexSet : clipPlanes->convexSets())
            for (ClipPlane const& plane : convexSet.planes)
                if (std::abs(plane.inwardNormal.z) > 1.0e-6
                    && std::abs(plane.getDistanceFromOrigin()) != std::numeric_limits<double>::max())
                    return true;
    }
    return false;
}

ClipPlaneContainment ClipPrimitive::classifyPointContainment(std::vector<Point3d> const& points,
                                                             bool /*ignoreMasks*/) const {
    ensurePlaneSets();
    ClipPlaneContainment inside = ClipPlaneContainment::StronglyInside;
    if (m_clipPlanes)
        inside = m_clipPlanes->classifyPointContainment(points, false);
    return inside;
}

ClipPrimitive::Ptr ClipPrimitive::fromJSON(ClipPrimitiveProps const* json) {
    if (json == nullptr)
        return nullptr;
    if (json->shape.has_value()) {
        ClipShape::Ptr const shape = ClipShape::fromClipShapeJSON(&*json->shape);
        if (shape)
            return shape;
    }
    return fromJSONClipPrimitive(json);
}

ClipPrimitive::Ptr ClipPrimitive::fromJSONClipPrimitive(ClipPrimitiveProps const* json) {
    if (json == nullptr || !json->planes.has_value())
        return nullptr;
    ClipPrimitivePlanesPart const& planes = *json->planes;
    std::optional<UnionOfConvexClipPlaneSets> clipPlanes;
    if (planes.clips.has_value()) {
        UnionOfConvexClipPlaneSetsProps const* clipsPtr = &*planes.clips;
        clipPlanes = UnionOfConvexClipPlaneSets::fromJSON(clipsPtr);
    }
    bool const invisible = planes.invisible.value_or(false);
    return Ptr(new ClipPrimitive(std::move(clipPlanes), invisible));
}

// ---------------------------------------------------------------------------
// ClipShape
// ---------------------------------------------------------------------------

ClipShape::ClipShape(std::vector<Point3d> polygon, std::optional<double> zLow,
                     std::optional<double> zHigh, Transform const* transform, bool isMask, bool invisible)
    : ClipPrimitive(std::nullopt, invisible) {
    m_isMask = false;
    m_polygon = std::move(polygon);
    initSecondaryProps(isMask, zLow, zHigh, transform);
}

void ClipShape::setPolygon(std::vector<Point3d> polygon) {
    // Add closure point
    if (!polygon.empty() && !polygon[0].AlmostEqual(polygon[polygon.size() - 1]))
        polygon.push_back(polygon[0]);
    m_polygon = std::move(polygon);
}

void ClipShape::ensurePlaneSets() const {
    if (m_clipPlanes.has_value())
        return;
    m_clipPlanes = UnionOfConvexClipPlaneSets::createEmpty();
    parseClipPlanes(*m_clipPlanes);
    if (m_transformFromClip)
        m_clipPlanes->transformInPlace(*m_transformFromClip);
}

void ClipShape::initSecondaryProps(bool isMask, std::optional<double> zLow, std::optional<double> zHigh,
                                   Transform const* transform) {
    m_isMask = isMask;
    m_zLow = zLow;
    m_zHigh = zHigh;

    if (transform != nullptr) {
        m_transformFromClip = *transform;
        Transform inverse;
        if (transform->Inverse(inverse))  // could fail (singular)
            m_transformToClip = inverse;
        else
            m_transformToClip = std::nullopt;
    } else {
        m_transformFromClip = Transform::CreateIdentity();
        m_transformToClip = Transform::CreateIdentity();
    }
}

ClipPrimitiveProps ClipShape::toJSON() const {
    ClipPrimitiveProps props;
    ClipPrimitiveShapePart& shape = props.shape.emplace();
    shape.points = m_polygon;
    if (m_invisible)
        shape.invisible = true;
    if (m_transformFromClip && !m_transformFromClip->IsIdentity())
        shape.trans = transformToRows(*m_transformFromClip);
    if (m_isMask)
        shape.mask = true;
    if (m_zLow.has_value() && *m_zLow != -std::numeric_limits<double>::max())
        shape.zlow = m_zLow;
    if (m_zHigh.has_value() && *m_zHigh != std::numeric_limits<double>::max())
        shape.zhigh = m_zHigh;
    return props;
}

ClipShape::Ptr ClipShape::fromClipShapeJSON(ClipPrimitiveShapePart const* shapeJson) {
    if (shapeJson == nullptr)
        return nullptr;
    std::vector<Point3d> const& points = shapeJson->points;
    Transform transform;
    Transform const* transformPtr = nullptr;
    if (shapeJson->trans.has_value()) {
        transform = transformFromRows(*shapeJson->trans);
        transformPtr = &transform;
    }
    std::optional<double> const zLow = shapeJson->zlow;
    std::optional<double> const zHigh = shapeJson->zhigh;
    bool const isMask = shapeJson->mask.value_or(false);
    bool const invisible = shapeJson->invisible.value_or(false);

    return createShape(points, zLow, zHigh, transformPtr, isMask, invisible);
}

ClipShape::Ptr ClipShape::createFrom(ClipShape const& other) {
    Ptr const retVal = createEmpty(false, false, nullptr);
    retVal->m_invisible = other.m_invisible;
    for (Point3d const& point : other.m_polygon)
        retVal->m_polygon.push_back(point);
    retVal->m_isMask = other.m_isMask;
    retVal->m_zLow = other.m_zLow;
    retVal->m_zHigh = other.m_zHigh;
    retVal->m_transformToClip = other.m_transformToClip;
    retVal->m_transformFromClip = other.m_transformFromClip;
    return retVal;
}

ClipShape::Ptr ClipShape::createShape(std::vector<Point3d> const& polygon, std::optional<double> zLow,
                                      std::optional<double> zHigh, Transform const* transform, bool isMask,
                                      bool invisible) {
    if (polygon.size() < 3)
        return nullptr;
    std::vector<Point3d> pPoints = polygon;
    // Add closure point (or refresh it as an exact copy of the first point).
    if (pPoints[0].AlmostEqual(pPoints[pPoints.size() - 1]))
        pPoints[pPoints.size() - 1] = pPoints[0];
    else
        pPoints.push_back(pPoints[0]);
    return Ptr(new ClipShape(std::move(pPoints), zLow, zHigh, transform, isMask, invisible));
}

ClipShape::Ptr ClipShape::createBlock(Range3d const& extremities, ClipMaskXYZRangePlanes clipMask,
                                      bool isMask, bool invisible, Transform const* transform) {
    Point3d const& low = extremities.low;
    Point3d const& high = extremities.high;
    std::vector<Point3d> blockPoints(5, Point3d::From(0, 0, 0));
    blockPoints[0].x = blockPoints[3].x = blockPoints[4].x = low.x;
    blockPoints[1].x = blockPoints[2].x = high.x;
    blockPoints[0].y = blockPoints[1].y = blockPoints[4].y = low.y;
    blockPoints[2].y = blockPoints[3].y = high.y;
    Ptr const shape = createShape(
        blockPoints,
        (ClipMaskXYZRangePlanes::None != (clipMask & ClipMaskXYZRangePlanes::ZLow)) ? std::optional<double>(low.z)
                                                                                     : std::nullopt,
        (ClipMaskXYZRangePlanes::None != (clipMask & ClipMaskXYZRangePlanes::ZHigh)) ? std::optional<double>(high.z)
                                                                                      : std::nullopt,
        transform, isMask, invisible);
    assert(shape);  // expect defined because blockPoints.length > 2
    return shape;
}

ClipShape::Ptr ClipShape::createEmpty(bool isMask, bool invisible, Transform const* transform) {
    return Ptr(new ClipShape({}, std::nullopt, std::nullopt, transform, isMask, invisible));
}

bool ClipShape::isValidPolygon() const noexcept {
    if (m_polygon.size() < 3)
        return false;
    if (!m_polygon[0].IsEqual(m_polygon[m_polygon.size() - 1]))
        return false;
    return true;
}

ClipPrimitive::Ptr ClipShape::clone() const {
    return createFrom(*this);
}

bool ClipShape::parseClipPlanes(UnionOfConvexClipPlaneSets& set) const {
    std::vector<Point3d> const& points = m_polygon;
    if (points.size() == 3 && !m_isMask && points[0].IsEqual(points[points.size() - 1])) {
        parseLinearPlanes(set, m_polygon[0], m_polygon[1]);
        return true;
    }
    if (!m_isMask) {
        double const direction = PolygonOps::testXYPolygonTurningDirections(m_polygon);
        if (direction != 0) {
            parseConvexPolygonPlanes(set, m_polygon, direction, false);
            return true;
        }
    }
    // REMARK: Pass all polygons to non-convex case. It will funnel to concave
    // case as appropriate.
    return parsePolygonPlanes(set, m_polygon, m_isMask);
}

bool ClipShape::parseLinearPlanes(UnionOfConvexClipPlaneSets& set, Point3d const& start, Point3d const& end,
                                  std::optional<double> cameraFocalLength) const {
    // Vector2d.createStartEnd(start, end) — xy only!
    double nx = end.x - start.x;
    double ny = end.y - start.y;
    double const mag = std::sqrt(nx * nx + ny * ny);
    if (mag <= kSmallFloatingPoint)  // filter out trivial edge (Vector2d.normalize failure)
        return false;
    nx /= mag;
    ny /= mag;
    if (cameraFocalLength == 0.0)
        cameraFocalLength = std::nullopt;  // ensure when camera is defined, vecStart/End3d are nonzero
    ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createEmpty();
    if (!cameraFocalLength.has_value()) {
        // normal and perp are nonzero, so ClipPlane creation can only fail on out-of-memory
        double const px = -ny;
        double const py = nx;
        if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(nx, ny, 0.0), start, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(-nx, -ny, 0.0), end, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(px, py, 0.0), start, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        if (auto p = ClipPlane::createNormalAndPoint(Vector3d::From(-px, -py, 0.0), start, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
    } else {
        double const focal = *cameraFocalLength;
        Vector3d const vecStart3d = Vector3d::From(start.x, start.y, -focal);
        Vector3d const vecEnd3d = Vector3d::From(end.x, end.y, -focal);
        Vector3d perpendicular;
        Vector3d endNormal;
        // vecEnd3d.crossProduct(vecStart3d, perpendicular).normalize(perpendicular)
        perpendicular = Vector3d::FromCrossProduct(vecEnd3d, vecStart3d);
        perpendicular.Normalize();
        endNormal = Vector3d::FromCrossProduct(vecStart3d, perpendicular);
        endNormal.Normalize();
        if (auto p = ClipPlane::createNormalAndDistance(perpendicular, 0.0, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        if (auto p = ClipPlane::createNormalAndDistance(endNormal, 0.0, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        perpendicular.Negate();
        endNormal = Vector3d::FromCrossProduct(vecEnd3d, perpendicular);
        endNormal.Normalize();
        if (auto p = ClipPlane::createNormalAndDistance(perpendicular, 0.0, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
        if (auto p = ClipPlane::createNormalAndDistance(endNormal, 0.0, m_invisible))
            convexSet.addPlaneToConvexSet(*p);
    }
    convexSet.addZClipPlanes(m_invisible, m_zLow, m_zHigh);
    set.addConvexSet(convexSet);
    return true;
}

bool ClipShape::parseConvexPolygonPlanes(UnionOfConvexClipPlaneSets& set, std::vector<Point3d> const& polygon,
                                         double direction, bool buildExteriorClipper,
                                         std::optional<double> cameraFocalLength) const {
    std::vector<PolyEdge> edges;
    bool const reverse = direction < 0;
    if (cameraFocalLength == 0.0)
        cameraFocalLength = std::nullopt;  // ensure when camera is defined, edges[].pointA/B are nonzero
    double const z = cameraFocalLength.has_value() ? -*cameraFocalLength : 0.0;
    for (size_t i = 0; i + 1 < polygon.size(); ++i) {
        // Vector2d.createStartEnd(polygon[i], polygon[i+1]) — xy only!
        double const dx = polygon[i + 1].x - polygon[i].x;
        double const dy = polygon[i + 1].y - polygon[i].y;
        double const mag = std::sqrt(dx * dx + dy * dy);
        if (mag > kSmallFloatingPoint) {  // filter out trivial edges
            double const ux = dx / mag;
            double const uy = dy / mag;
            Vector3d const normal = Vector3d::From(reverse ? uy : -uy, reverse ? -ux : ux, 0.0);
            edges.push_back(PolyEdge(polygon[i], polygon[i + 1], normal, z));
        }
    }
    if (edges.size() < 3)
        return false;
    if (buildExteriorClipper) {
        assert(!cameraFocalLength.has_value());  // TODO: implement camera logic for exterior clipper creation
        size_t const last = edges.size() - 1;
        for (size_t i = 0; i <= last; ++i) {
            PolyEdge const& edge = edges[i];
            PolyEdge const& prevEdge = edges[i != 0 ? (i - 1) : last];
            PolyEdge const& nextEdge = edges[(i == last) ? 0 : (i + 1)];
            ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createEmpty();
            std::optional<Vector3d> const previousPerpendicular =
                PolyEdge::makeUnitPerpendicularToBisector(prevEdge, edge, !reverse);
            std::optional<Vector3d> const nextPerpendicular =
                PolyEdge::makeUnitPerpendicularToBisector(edge, nextEdge, reverse);
            // Create three-sided fans from each edge; silently ignore undefined
            // bisector normal (backtracking edges violate the convexity assumption).
            if (previousPerpendicular.has_value()) {
                if (auto p = ClipPlane::createNormalAndPoint(*previousPerpendicular, edge.pointA, m_invisible, true))
                    convexSet.addPlaneToConvexSet(*p);
            }
            if (auto p = ClipPlane::createNormalAndPoint(edge.normal, edge.pointB, m_invisible, false))
                convexSet.addPlaneToConvexSet(*p);
            if (nextPerpendicular.has_value()) {
                if (auto p = ClipPlane::createNormalAndPoint(*nextPerpendicular, nextEdge.pointA, m_invisible, true))
                    convexSet.addPlaneToConvexSet(*p);
            }
            set.addConvexSet(convexSet);
        }
        set.addOutsideZClipSets(m_invisible, m_zLow, m_zHigh);
    } else {
        ConvexClipPlaneSet convexSet = ConvexClipPlaneSet::createEmpty();
        if (!cameraFocalLength.has_value()) {
            for (PolyEdge const& edge : edges) {
                if (auto p = ClipPlane::createNormalAndPoint(edge.normal, edge.pointA))
                    convexSet.addPlaneToConvexSet(*p);
            }
        } else {
            for (PolyEdge const& edge : edges) {
                Vector3d const a = Vector3d::From(edge.pointA.x, edge.pointA.y, edge.pointA.z);
                Vector3d const b = Vector3d::From(edge.pointB.x, edge.pointB.y, edge.pointB.z);
                if (reverse) {
                    Vector3d cross = Vector3d::FromCrossProduct(a, b);
                    cross.Normalize();
                    if (auto p = ClipPlane::createNormalAndDistance(cross, 0.0))
                        convexSet.addPlaneToConvexSet(*p);
                } else {
                    Vector3d cross = Vector3d::FromCrossProduct(b, a);
                    cross.Normalize();
                    if (auto p = ClipPlane::createNormalAndDistance(cross, 0.0))
                        convexSet.addPlaneToConvexSet(*p);
                }
            }
        }
        convexSet.addZClipPlanes(m_invisible, m_zLow, m_zHigh);
        set.addConvexSet(convexSet);
    }
    return true;
}

bool ClipShape::parsePolygonPlanes(UnionOfConvexClipPlaneSets& /*set*/,
                                   std::vector<Point3d> const& /*polygon*/, bool /*isMask*/,
                                   std::optional<double> /*cameraFocalLength*/) const {
    // TODO(M-P 登记): 凹多边形与 mask 洞解析未移植 —— 依赖 PolylineOps.compressDanglers +
    // Triangulator.createTriangulatedGraphFromSingleLoop/flipTriangles/announceFaceLoops +
    // HalfEdgeGraph +（mask 分支）AlternatingCCTreeNode.createHullAndInletsForPolygon
    // （ClipPrimitive.ts:783-824）。当前返回参考三角化不可用时的失败形态（不加凸集）。
    return false;
}

bool ClipShape::transformInPlace(Transform const& transform) {
    if (transform.IsIdentity())
        return true;
    ClipPrimitive::transformInPlace(transform);
    if (m_transformFromClip)
        m_transformFromClip = transform.MultiplyTransform(*m_transformFromClip);
    else
        m_transformFromClip = transform;
    Transform inverse;
    if (m_transformFromClip->Inverse(inverse))
        m_transformToClip = inverse;
    else
        m_transformToClip = std::nullopt;
    return true;
}

bool ClipShape::isXYPolygon() const noexcept {
    if (m_polygon.empty())  // lenient check, as in reference
        return false;
    if (!m_transformFromClip.has_value())
        return true;
    Vector3d const zVector = m_transformFromClip->matrix.ColumnZ();
    double const magnitudeXY = std::sqrt(zVector.x * zVector.x + zVector.y * zVector.y);
    return magnitudeXY < 1.0e-8;
}

void ClipShape::performTransformToClip(Point3d& point) const noexcept {
    if (m_transformToClip.has_value())
        point = m_transformToClip->MultiplyPoint3d(point);
}

void ClipShape::performTransformFromClip(Point3d& point) const noexcept {
    if (m_transformFromClip.has_value())
        point = m_transformFromClip->MultiplyPoint3d(point);
}

ClipPlaneContainment ClipShape::classifyPointContainment(std::vector<Point3d> const& points,
                                                         bool ignoreMasks) const {
    ensurePlaneSets();
    ClipPlaneContainment inside = ClipPlaneContainment::StronglyInside;
    if (m_clipPlanes)
        inside = m_clipPlanes->classifyPointContainment(points, false);
    if (m_isMask && !ignoreMasks) {
        switch (inside) {
            case ClipPlaneContainment::StronglyInside:
                return ClipPlaneContainment::StronglyOutside;
            case ClipPlaneContainment::StronglyOutside:
                return ClipPlaneContainment::StronglyInside;
            case ClipPlaneContainment::Ambiguous:
                return ClipPlaneContainment::Ambiguous;
        }
    }
    return inside;
}

}  // namespace dqGeom
