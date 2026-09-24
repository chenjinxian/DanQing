// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Geometry stream types
//
// Ported from: itwinjs-core core/common/src/geometry/GeometryStream.ts
//              core/common/src/geometry/ElementGeometry.ts
// Types for geometry stream entries, appearance, and placement.
#pragma once

#include "ColorDef.h"
#include "Export.h"
#include "FillFlags.h"
#include "GeometryClass.h"
#include "Gradient.h"
#include "GraphicParams.h"
#include "DqCommon.h"

#include <dqBase/DqId.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Transform.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Type of entry in a geometry stream.
// Ported from: itwinjs-core core/common/src/geometry/ElementGeometry.ts ElementGeometryOpcode (values verified 1:1)
enum class ElementGeometryOpcode : uint8_t {
    SubGraphicRange = 2,
    PartReference = 3,
    BasicSymbology = 4,
    PointPrimitive = 5,
    PointPrimitive2d = 6,
    ArcPrimitive = 7,
    CurveCollection = 8,
    Polyface = 9,
    CurvePrimitive = 10,
    SolidPrimitive = 11,
    BsplineSurface = 12,
    Fill = 19,
    Pattern = 20,
    Material = 21,
    TextString = 22,
    LineStyleModifiers = 23,
    BRep = 25,
    Image = 28,
};

// Flags for geometry stream.
// Ported from: itwinjs-core GeometryStreamFlags
enum class GeometryStreamFlags : uint8_t {
    None = 0,
    ViewIndependent = 1 << 0,
};

// Establish non-default SubCategory or override appearance.
// Ported from: itwinjs-core GeometryAppearanceProps
struct DQ_COMMON_EXPORT GeometryAppearanceProps {
    std::optional<dqBase::DqId> subCategory;
    std::optional<ColorDefProps> color;
    std::optional<int> weight;
    std::optional<dqBase::DqId> style;
    std::optional<double> transparency;
    std::optional<int> displayPriority;
    std::optional<GeometryClass> geometryClass;
};

// Fill properties for planar regions.
// Ported from: itwinjs-core AreaFillProps
struct DQ_COMMON_EXPORT AreaFillProps {
    FillDisplay display = FillDisplay::Never;
    std::optional<double> transparency;
    std::optional<BackgroundFill> backgroundFill;
    std::optional<ColorDefProps> color;
    std::optional<GradientSymbProps> gradient;
};

// Material override for surfaces.
// Ported from: itwinjs-core MaterialProps
struct DQ_COMMON_EXPORT GeometryMaterialProps {
    std::optional<dqBase::DqId> materialId;
    // @internal: texture mapping origin, size, rotation
    std::optional<dqGeom::Point3d> origin;
    std::optional<dqGeom::Point3d> size;
    double rotationDegrees = 0.0;
};

// BRep entity type.
// Ported from: itwinjs-core BRepEntity.Type
enum class BRepType : uint8_t {
    Solid = 0,
    Sheet = 1,
    Wire = 2,
};

// BRep face symbology.
// Ported from: itwinjs-core BRepEntity.FaceSymbologyProps
struct BRepFaceSymbologyProps {
    std::optional<ColorDefProps> color;
    std::optional<double> transparency;
    std::optional<dqBase::DqId> materialId;
};

// BRep data entry.
// Ported from: itwinjs-core BRepEntity.DataProps
struct DQ_COMMON_EXPORT BRepDataProps {
    std::string data;  // Base64 encoded
    BRepType type = BRepType::Solid;
    // Body transform (optional, for positioned BRep data)
    std::optional<dqGeom::Transform> transform;
    std::vector<BRepFaceSymbologyProps> faceSymbology;
};

// GeometryPart instance reference.
// Ported from: itwinjs-core GeometryPartInstanceProps
struct DQ_COMMON_EXPORT GeometryPartInstanceProps {
    dqBase::DqId partId;
    double originX = 0.0, originY = 0.0, originZ = 0.0;
    double yawDegrees = 0.0, pitchDegrees = 0.0, rollDegrees = 0.0;
    double scale = 1.0;
};

// Geometry stream header.
// Ported from: itwinjs-core GeometryStreamHeaderProps
struct GeometryStreamHeaderProps {
    GeometryStreamFlags flags = GeometryStreamFlags::None;
};

// A geometry stream entry.
// Ported from: itwinjs-core GeometryStreamEntryProps (union of all entry types)
struct DQ_COMMON_EXPORT GeometryStreamEntry {
    ElementGeometryOpcode opcode = ElementGeometryOpcode::PointPrimitive;

    // Stream header (first entry, if present)
    std::optional<GeometryStreamHeaderProps> header;
    // Optional data based on opcode
    std::optional<GeometryAppearanceProps> appearance;
    std::optional<AreaFillProps> fill;
    std::optional<GeometryMaterialProps> material;
    std::optional<BRepDataProps> brep;
    std::optional<GeometryPartInstanceProps> partRef;
    // Sub-range for nested geometry
    std::optional<dqGeom::Range3d> subRange;
};

// Placement of a GeometricElement3d.
// Ported from: itwinjs-core Placement3d
class DQ_COMMON_EXPORT Placement3d {
public:
    dqGeom::Point3d origin;
    double yawDegrees = 0.0, pitchDegrees = 0.0, rollDegrees = 0.0;
    dqGeom::Range3d bbox;

    Placement3d() noexcept = default;
    Placement3d(const dqGeom::Point3d& origin_, double yaw, double pitch, double roll,
                const dqGeom::Range3d& bbox_) noexcept
        : origin(origin_), yawDegrees(yaw), pitchDegrees(pitch), rollDegrees(roll), bbox(bbox_)
    {
    }

    // Whether this is a 3d placement.
    bool is3d() const noexcept { return true; }

    // Whether the placement is valid (bbox not null).
    // Ported from: itwinjs-core Placement3d.isValid
    bool isValid() const noexcept { return !bbox.isNull(); }

    // Set from another placement.
    void setFrom(const Placement3d& other) noexcept
    {
        origin = other.origin;
        yawDegrees = other.yawDegrees;
        pitchDegrees = other.pitchDegrees;
        rollDegrees = other.rollDegrees;
        bbox = other.bbox;
    }
};

// Placement of a GeometricElement2d.
// Ported from: itwinjs-core Placement2d
// Note: bbox uses Range3d with z=0 for 2d elements (Range2d not yet in dqGeom).
class DQ_COMMON_EXPORT Placement2d {
public:
    double originX = 0.0, originY = 0.0;
    double angleDegrees = 0.0;
    dqGeom::Range3d bbox;  // z components are 0 for 2d

    Placement2d() noexcept = default;

    // Whether this is a 3d placement.
    bool is3d() const noexcept { return false; }

    // Whether the placement is valid.
    bool isValid() const noexcept { return !bbox.isNull(); }
};

END_DQ_COMMON_NAMESPACE
