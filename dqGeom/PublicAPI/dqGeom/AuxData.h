// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — AuxData (per-vertex auxiliary analysis channels for IndexedPolyface)
//
// Ported from: itwinjs-core core/geometry/src/polyface/AuxData.ts
//
// PolyfaceAuxData holds one or more analytical data channels for each vertex of a
// polyface, allowing the polyface to be styled using an AnalysisStyle (dqCommon).
// All data channels are indexed by the same indices, which must have the same length
// and structure as the other Polyface index arrays.
//
// EQUIVALENCE: 参考源=AuxData.ts:191-201（indices 为 0-based 无符号、parallel to
// 参考 pointIndex）；发散=DanQing 的 auxData.indices 采用 DanQing PolyfaceData
// pointIndex 约定（signed 1-based，符号=边可见性），与参考存取值域不同但信息等价；
// JSON I/O 边界经 SerializationHelpers 转换（W-a2 IModelJson）；消费面经
// abs(idx)-1 取通道数据（与 DanQing 现有点索引消费一致）。验证法=
// PolyfaceAuxDataTest 的 TryTransformInPlacePerDataType/PolyfaceTransformTransforms
// AuxData + W-a2 round-trip 锁 + W-a4 装配链锁。
#pragma once

#include "Point3d.h"
#include "Range3d.h"
#include "Transform.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// AuxChannelDataType — 1:1 AuxData.ts:21-38
// ---------------------------------------------------------------------------
// The types of data that can be represented by an AuxChannelData. Each type of data
// contributes differently to the animation applied by an AnalysisStyle and responds
// differently when the host PolyfaceAuxData is transformed.
enum class AuxChannelDataType : int {
    // General-purpose scalar values like stress, temperature, etc. Scalar values remain
    // unmodified when the host Polyface is transformed.
    Scalar = 0,
    // Distances in meters. The Transform's scale is applied to the distances.
    Distance = 1,
    // (X, Y, Z) displacement vectors added to the Polyface's vertex positions resulting in
    // deformation of the mesh. Displacements are rotated and scaled accordingly.
    Vector = 2,
    // (X, Y, Z) unit normal vectors that replace the host Polyface's own normals.
    // Normals are rotated and renormalized accordingly.
    Normal = 3,
};

// ---------------------------------------------------------------------------
// AuxChannelData — the AuxChannel data at a single input value (1:1 AuxData.ts:43-77)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT AuxChannelData {
public:
    // The input value for this data.
    double input;
    // The vertex values for this data. A single value per vertex for scalar and distance
    // types and 3 values (x,y,z) for normal or vector channels.
    // （参考的 `number[] | Float64Array` 构造双形态归一为 vector<double>——
    // TS 的 Float64Array 捕捉语义与 C++ 移动语义等价。）
    std::vector<double> values;

    // Constructor. Values are captured (moved).
    AuxChannelData(double input_, std::vector<double> values_)
        : input(input_)
        , values(std::move(values_))
    {}

    // Copy blocks of size `blockSize` from (blocked index) `thisIndex` in this
    // AuxChannelData to (blockIndex) `otherIndex` of `other`（1:1 AuxData.ts:60-63——
    // 注意参考语义：this.values[thisIndex] ← other.values[otherIndex]，即从 other 拷到 this）。
    void copyValues(AuxChannelData const& other, size_t thisIndex, size_t otherIndex, size_t blockSize);

    // Return a deep copy（1:1 AuxData.ts:66-68；C++ 值拷贝即深拷贝）。
    AuxChannelData clone() const { return *this; }

    // Toleranced comparison of the `input` and `value` fields（1:1 AuxData.ts:73-76——
    // 默认容差 1.0e-8；参考的 `tol ? tol : 1.0E-8` truthiness 怪癖原样：tol==0 时回落默认）。
    bool isAlmostEqual(AuxChannelData const& other, double tol = 1.0e-8) const;
};

// ---------------------------------------------------------------------------
// AuxChannel — a single PolyfaceAuxData channel (1:1 AuxData.ts:82-177)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT AuxChannel {
public:
    // An array of AuxChannelData that represents the vertex data at one or more input values.
    std::vector<AuxChannelData> data;
    // The type of data stored in this channel.
    AuxChannelDataType dataType;
    // The channel name. Used to present the AuxChannel to the user and also to select the
    // AuxChannel for display from AnalysisStyle.
    std::optional<std::string> name;
    // The input name.
    std::optional<std::string> inputName;

    // Constructor with CAPTURED inputs（1:1 AuxData.ts:93-98——vector 移动即捕捉）。
    AuxChannel(std::vector<AuxChannelData> data_, AuxChannelDataType dataType_,
               std::optional<std::string> name_ = std::nullopt,
               std::optional<std::string> inputName_ = std::nullopt)
        : data(std::move(data_))
        , dataType(dataType_)
        , name(std::move(name_))
        , inputName(std::move(inputName_))
    {}

    // Return a deep copy（1:1 AuxData.ts:101-105；C++ 值拷贝即深拷贝）。
    AuxChannel clone() const { return *this; }

    // Toleranced comparison of contents（1:1 AuxData.ts:108-120——name/inputName 的
    // !== 语义由 optional 不等承载：缺席 vs 空串不等，与参考 undefined vs "" 同构）。
    bool isAlmostEqual(AuxChannel const& other, double tol = 1.0e-8) const;

    // True if the data type is 1-dimensional（1:1 AuxData.ts:123-125 静态面）。
    static bool isScalar(AuxChannelDataType dataType_)
    {
        return dataType_ == AuxChannelDataType::Distance || dataType_ == AuxChannelDataType::Scalar;
    }

    // True if the data stored in this AuxChannel is 1-dimensional（1:1 AuxData.ts:128-130）。
    bool isScalar() const { return isScalar(dataType); }

    // The dimension (1D or 3D) of each datum of an AuxChannel of the given type
    // （1:1 AuxData.ts:133-135 静态面）。
    static size_t entriesPerValue(AuxChannelDataType dataType_)
    {
        return isScalar(dataType_) ? 1u : 3u;
    }

    // The dimension (1D or 3D) of each datum in the data arrays of this AuxChannel
    // （1:1 AuxData.ts:138-140）。
    size_t entriesPerValue() const { return entriesPerValue(dataType); }

    // The number of data stored in each data array of this AuxChannel, equal to the length
    // of the array divided by `entriesPerValue`（1:1 AuxData.ts:143-145）。
    size_t valueCount() const
    {
        return data.empty() ? 0u : data[0].values.size() / entriesPerValue();
    }

    // The minimum and maximum values in `data.values`, or nullopt if isScalar is false
    // （1:1 AuxData.ts:148-157——Range1d.createNull + 逐 data extendArray）。
    std::optional<Range1d> scalarRange() const;

    // Compute the range of this channel's displacement values, if dataType is Vector
    // （1:1 AuxData.ts:164-176——非 Vector 型返回 null range；逐点 extendXYZ(x*s,y*s,z*s)）。
    Range3d computeDisplacementRange(double scale = 1.0, Range3d* result = nullptr) const;
};

// ---------------------------------------------------------------------------
// PolyfaceAuxData — auxiliary data channels for a Polyface (1:1 AuxData.ts:191-293)
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT PolyfaceAuxData {
public:
    // Array with one or more channels of auxiliary data for the associated polyface.
    std::vector<AuxChannel> channels;
    // The indices (shared by all data in all channels) mapping the data to the mesh facets.
    // DanQing 约定：signed 1-based，parallel to PolyfaceData::pointIndex（见文件头 EQUIVALENCE）。
    std::vector<int32_t> indices;

    // Constructor with CAPTURED inputs（1:1 AuxData.ts:198-201）。
    PolyfaceAuxData(std::vector<AuxChannel> channels_, std::vector<int32_t> indices_)
        : channels(std::move(channels_))
        , indices(std::move(indices_))
    {}

    // Return a deep copy（1:1 AuxData.ts:204-207；C++ 值拷贝即深拷贝）。
    PolyfaceAuxData clone() const { return *this; }

    // Returns true if `this` is equivalent to `other` within `tolerance`. The indices are
    // compared for exact equality. The data in the channels are compared using `tolerance`,
    // which defaults to 1.0e-8（1:1 AuxData.ts:212-221）。
    bool isAlmostEqual(PolyfaceAuxData const& other, double tol = 1.0e-8) const;

    // Returns true if both `left` and `right` are null, or both are defined and equivalent
    // within `tolerance` (default: 1.0e-8)（1:1 AuxData.ts:224-230——双 undefined 捕获由
    // nullptr==nullptr 承载）。
    static bool isAlmostEqual(PolyfaceAuxData const* left, PolyfaceAuxData const* right,
                              double tol = 1.0e-8)
    {
        if (left == right)  // This catches double undefined !!!
            return true;
        if (left && right)
            return left->isAlmostEqual(*right, tol);
        return false;
    }

    // Create a PolyfaceAuxData for use by a PolyfaceVisitor（1:1 AuxData.ts:233-245——
    // 值数组留空骨架，indices 空）。
    PolyfaceAuxData createForVisitor() const;

    // Apply `transform` to the data in each channel（1:1 AuxData.ts:252-292）。
    // scale = |det|^(1/3) × sign(det)；Normal 型 inverseRot 懒建 + multiplyTranspose
    // VectorInPlace + 重规范化（tol 1.0e-15 怪癖 :276 原样）；不可逆 → false。
    bool tryTransformInPlace(Transform const& transform);
};

END_DQ_GEOM_NAMESPACE
