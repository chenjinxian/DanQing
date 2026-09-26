// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Feature table
//
// Ported from: itwinjs-core core/common/src/FeatureTable.ts
// Defines Feature, FeatureTable, BatchType for identifying elements in rendered graphics.
#pragma once

#include "Export.h"
#include "GeometryClass.h"
#include "DqCommon.h"

#include <dqBase/IndexMap.h>
#include <dqBase/DqId.h>

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declaration — PackedFeatureTable.h includes this header.
class PackedFeatureTable;

// Describes the type of a 'batch' of graphics.
// Ported from: itwinjs-core core/common/src/FeatureTable.ts
enum class BatchType : uint8_t {
    /** Graphics derived from a model's visible geometry. */
    Primary = 0,
    /** Color volumes used to classify a model's visible geometry. */
    VolumeClassifier = 1,
    /** Planar graphics used to classify a model's visible geometry. */
    PlanarClassifier = 2,
};

// A discrete entity within a batched RenderGraphic.
// Ported from: itwinjs-core core/common/src/FeatureTable.ts
class DQ_COMMON_EXPORT Feature {
public:
    dqBase::DqId elementId;
    dqBase::DqId subCategoryId;
    GeometryClass geometryClass = GeometryClass::Primary;

    constexpr Feature() noexcept = default;
    Feature(const dqBase::DqId& element, const dqBase::DqId& subCategory = dqBase::DqId{},
            GeometryClass geomClass = GeometryClass::Primary) noexcept
        : elementId(element), subCategoryId(subCategory), geometryClass(geomClass)
    {
    }

    // True if this feature has at least one non-default property.
    // Ported from: itwinjs-core Feature.isDefined
    bool isDefined() const noexcept
    {
        return elementId.isValid() || subCategoryId.isValid() || geometryClass != GeometryClass::Primary;
    }

    // Equality.
    // Ported from: itwinjs-core Feature.equals()
    bool equals(const Feature& other) const noexcept { return compare(other) == 0; }

    // Ordinal comparison.
    // Ported from: itwinjs-core Feature.compare()
    int compare(const Feature& rhs) const noexcept
    {
        if (this == &rhs)
            return 0;
        if (geometryClass != rhs.geometryClass)
            return static_cast<int>(geometryClass) < static_cast<int>(rhs.geometryClass) ? -1 : 1;
        if (elementId != rhs.elementId)
            return elementId < rhs.elementId ? -1 : 1;
        if (subCategoryId != rhs.subCategoryId)
            return subCategoryId < rhs.subCategoryId ? -1 : 1;
        return 0;
    }
};

// A Feature with a modelId identifying the containing model.
// Ported from: itwinjs-core ModelFeature
struct DQ_COMMON_EXPORT ModelFeature {
    dqBase::DqId modelId;
    dqBase::DqId elementId;
    dqBase::DqId subCategoryId;
    GeometryClass geometryClass = GeometryClass::Primary;
};

// Defines a look-up table for Features within a batched RenderGraphic.
// Ported from: itwinjs-core core/common/src/FeatureTable.ts
class DQ_COMMON_EXPORT FeatureTable {
public:
    using IndexedFeature = dqBase::IndexedValue<Feature>;

    // Construct an empty FeatureTable.
    // Ported from: itwinjs-core FeatureTable constructor
    FeatureTable(int maxFeatures, const dqBase::DqId& modelId = dqBase::DqId{},
                 BatchType type = BatchType::Primary);

    // The model Id.
    const dqBase::DqId& getModelId() const noexcept { return m_modelId; }

    // The batch type.
    BatchType getType() const noexcept { return m_type; }

    // Maximum number of features.
    int getMaxFeatures() const noexcept { return m_maxFeatures; }

    // Number of features currently in the table.
    int getSize() const noexcept { return m_size; }

    // True if at least one feature has a valid element/subcategory Id.
    // Ported from: itwinjs-core FeatureTable.anyDefined
    bool isAnyDefined() const noexcept;

    // True if table contains exactly one feature.
    // Ported from: itwinjs-core FeatureTable.isUniform
    bool isUniform() const noexcept { return m_size == 1; }

    // If uniform, returns the single feature; otherwise nullopt.
    // Ported from: itwinjs-core FeatureTable.uniform
    std::optional<Feature> getUniform() const noexcept;

    // True if associated with VolumeClassifier geometry.
    bool isVolumeClassifier() const noexcept { return m_type == BatchType::VolumeClassifier; }

    // True if associated with PlanarClassifier geometry.
    bool isPlanarClassifier() const noexcept { return m_type == BatchType::PlanarClassifier; }

    // Insert a feature, returns its index.
    // Ported from: itwinjs-core IndexMap.insert (inherited)
    int insert(const Feature& feature);

    // Find feature by index.
    // Ported from: itwinjs-core FeatureTable.findFeature
    std::optional<Feature> findFeature(int index) const;

    // Insert a feature at a specific index.
    // Ported from: itwinjs-core FeatureTable.insertWithIndex
    void insertWithIndex(const Feature& feature, int index);

    // Access the underlying array.
    const IndexedFeature* getArray() const noexcept { return m_array; }
    int getArraySize() const noexcept { return m_size; }

    // pack into a flat uint32 array for GPU upload.
    // Ported from: itwinjs-core FeatureTable.pack()
    PackedFeatureTable pack() const;

    // Rule-of-5 (TD-21): the reference is GC'd; DanQing owns m_array.
    // Ported from: itwinjs-core FeatureTable (value-object semantics —
    // copy is deep, move steals, destructor releases).
    ~FeatureTable();
    FeatureTable(FeatureTable const& rhs);
    FeatureTable& operator=(FeatureTable const& rhs);
    FeatureTable(FeatureTable&& rhs) noexcept;
    FeatureTable& operator=(FeatureTable&& rhs) noexcept;

private:
    // Binary search for feature in sorted array
    int lowerBound(const Feature& feature) const;

    // Copy-and-swap helper for operator= (TD-21): exchanges the full state.
    void swap(FeatureTable& other) noexcept;

    dqBase::DqId m_modelId;
    BatchType m_type = BatchType::Primary;
    int m_maxFeatures;
    int m_size = 0;
    IndexedFeature* m_array;
};

END_DQ_COMMON_NAMESPACE
