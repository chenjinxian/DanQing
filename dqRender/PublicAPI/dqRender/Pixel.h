// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Pixel readback API
// Ported from: itwinjs-core core/frontend/src/render/Pixel.ts
//
// Public API for reading pixel data from the rendered image.
// Used for picking/identification: reads feature ID, depth, and
// geometry type from the pick buffer.
#pragma once

#include "Export.h"

#include <cstdint>
#include <optional>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Pixel namespace — pixel readback types
// (Ported from: itwinjs-core Pixel.ts)
// ---------------------------------------------------------------------------
namespace Pixel {

// ---------------------------------------------------------------------------
// GeometryType — what kind of geometry produced the pixel
// Ported from: itwinjs-core Pixel.ts GeometryType (line 207-220)
// ---------------------------------------------------------------------------
enum class GeometryType : uint8_t {
    Unknown = 0,    // Not selected or undetermined
    None = 1,       // No geometry rendered
    Surface = 2,    // Surface geometry
    Linear = 3,     // point primitive or polyline
    Edge = 4,       // Edge of a surface
    Silhouette = 5, // Silhouette edge of curved surface
};

// ---------------------------------------------------------------------------
// Planarity — whether the geometry is planar
// Ported from: itwinjs-core Pixel.ts Planarity (line 223-232)
// ---------------------------------------------------------------------------
enum class Planarity : uint8_t {
    Unknown = 0,   // Not determined
    None = 1,      // No geometry
    Planar = 2,    // Planar geometry
    NonPlanar = 3, // Non-planar geometry
};

// ---------------------------------------------------------------------------
// Selector — what pixel data to read
// Ported from: itwinjs-core Pixel.ts Selector (line 238-246)
// ---------------------------------------------------------------------------
enum Selector : uint8_t {
    None = 0,
    Feature = 1 << 0,
    GeometryAndDistance = 1 << 2,
    All = GeometryAndDistance | Feature,
};

// ---------------------------------------------------------------------------
// BatchType — type of batch for classifier identification
// Ported from: itwinjs-core BatchType
// ---------------------------------------------------------------------------
enum class BatchType : uint8_t {
    Primary = 0,
    Classifier = 1,
    PlanarClassifier = 2,
};

// ---------------------------------------------------------------------------
// Feature — identifies the element/subcategory/geometry class of a pixel
// Ported from: itwinjs-core core-common Feature
// ---------------------------------------------------------------------------
struct PixelFeature {
    uint64_t elementId = 0;       // Element ID (packed as uint64)
    uint64_t subCategoryId = 0;   // SubCategory ID
    uint32_t geometryClass = 0;   // GeometryClass enum value
    uint64_t modelId = 0;         // Model ID

    PixelFeature() noexcept = default;
    PixelFeature(uint64_t elemId, uint64_t subCatId, uint32_t geomClass, uint64_t mdlId = 0) noexcept
        : elementId(elemId), subCategoryId(subCatId), geometryClass(geomClass), modelId(mdlId) {}
};

// Note: Cannot alias as 'Feature' because Selector::Feature enum value conflicts.

// ---------------------------------------------------------------------------
// HitPriority — priority of a hit for selection
// Ported from: itwinjs-core HitDetail.ts HitPriority
// ---------------------------------------------------------------------------
enum class HitPriority : uint8_t {
    Unknown = 0,
    WireEdge = 1,
    SilhouetteEdge = 2,
    NonPlanarEdge = 3,
    PlanarEdge = 4,
    NonPlanarSurface = 5,
    PlanarSurface = 6,
};

// ---------------------------------------------------------------------------
// HitProps — describes aspects of a hit for constructing a HitDetail
// Ported from: itwinjs-core Pixel.ts HitProps (line 173-204)
// ---------------------------------------------------------------------------
struct HitProps {
    uint64_t sourceId = 0;              // Element ID or transient ID
    HitPriority priority = HitPriority::Unknown;
    float distFraction = -1.0f;
    std::optional<uint64_t> subCategoryId;
    std::optional<uint32_t> geometryClass;
    std::optional<uint64_t> modelId;
    std::optional<std::string> tileId;
    bool isClassifier = false;
};

// ---------------------------------------------------------------------------
// Data — pixel data from a single pixel
// Ported from: itwinjs-core Pixel.ts Data (line 23-158)
// ---------------------------------------------------------------------------
struct Data {
    // The feature that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.feature
    std::optional<PixelFeature> feature;

    // The pixel's depth in NPC coordinates (0 to 1), or -1 if not written.
    // Ported from: itwinjs-core Pixel.Data.distanceFraction
    float distanceFraction = -1.0f;

    // The type of geometry that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.type
    GeometryType type = GeometryType::Unknown;

    // The planarity of the geometry that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.planarity
    Planarity planarity = Planarity::Unknown;

    // The batch type (Primary, Classifier, PlanarClassifier).
    // Ported from: itwinjs-core Pixel.Data.batchType
    std::optional<BatchType> batchType;

    // The tile ID from which the pixel originated.
    // Ported from: itwinjs-core Pixel.Data.tileId
    std::optional<std::string> tileId;

    // True if the pixel originated from a section drawing attachment.
    // Ported from: itwinjs-core Pixel.Data.inSectionDrawingAttachment
    bool inSectionDrawingAttachment = false;

    // --- Computed properties ---

    // Check if this pixel has valid geometry.
    bool hasGeometry() const noexcept {
        return type != GeometryType::None && type != GeometryType::Unknown;
    }

    // True if the pixel originated from a classifier.
    // Ported from: itwinjs-core Pixel.Data.isClassifier
    bool isClassifier() const noexcept {
        return batchType.has_value() && batchType.value() != BatchType::Primary;
    }

    // The element ID that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.elementId
    std::optional<uint64_t> elementId() const {
        return feature.has_value() ? std::optional<uint64_t>(feature->elementId) : std::nullopt;
    }

    // The subcategory ID that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.subCategoryId
    std::optional<uint64_t> subCategoryId() const {
        return feature.has_value() ? std::optional<uint64_t>(feature->subCategoryId) : std::nullopt;
    }

    // The geometry class that produced the pixel.
    // Ported from: itwinjs-core Pixel.Data.geometryClass
    std::optional<uint32_t> geometryClass() const {
        return feature.has_value() ? std::optional<uint32_t>(feature->geometryClass) : std::nullopt;
    }

    // The model ID from which the pixel originated.
    // Ported from: itwinjs-core Pixel.Data.modelId
    std::optional<uint64_t> modelId() const {
        return feature.has_value() ? std::optional<uint64_t>(feature->modelId) : std::nullopt;
    }

    // Compute the hit priority based on type and planarity.
    // Ported from: itwinjs-core Pixel.Data.computeHitPriority()
    HitPriority computeHitPriority() const {
        switch (type) {
            case GeometryType::Surface:
                return planarity == Planarity::Planar ? HitPriority::PlanarSurface : HitPriority::NonPlanarSurface;
            case GeometryType::Linear:
                return HitPriority::WireEdge;
            case GeometryType::Edge:
                return planarity == Planarity::Planar ? HitPriority::PlanarEdge : HitPriority::NonPlanarEdge;
            case GeometryType::Silhouette:
                return HitPriority::SilhouetteEdge;
            default:
                return HitPriority::Unknown;
        }
    }

    // Convert to HitProps for constructing a HitDetail.
    // Ported from: itwinjs-core Pixel.Data.toHitProps()
    HitProps toHitProps() const {
        HitProps props;
        props.sourceId = feature.has_value() ? feature->elementId : 0;
        props.priority = computeHitPriority();
        props.distFraction = distanceFraction;
        if (feature.has_value()) {
            props.subCategoryId = feature->subCategoryId;
            props.geometryClass = feature->geometryClass;
            props.modelId = feature->modelId;
        }
        props.tileId = tileId;
        props.isClassifier = isClassifier();
        return props;
    }
};

// ---------------------------------------------------------------------------
// Buffer — interface for reading pixels
// Ported from: itwinjs-core Pixel.ts Buffer (line 252-255)
// ---------------------------------------------------------------------------
class Buffer {
public:
    virtual ~Buffer() = default;

    /// Get the pixel data at (x, y).
    virtual Data getPixel(uint32_t x, uint32_t y) const = 0;
};

// ---------------------------------------------------------------------------
// Receiver — callback for readPixels results
// Ported from: itwinjs-core Pixel.ts Receiver (line 260)
// ---------------------------------------------------------------------------
using Receiver = void(*)(Buffer* buffer);

}  // namespace Pixel

END_DQ_RENDER_NAMESPACE
