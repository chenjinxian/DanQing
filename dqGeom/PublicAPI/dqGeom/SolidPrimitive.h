// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/solid/SolidPrimitive.ts
// DanQing dqGeom — SolidPrimitive (abstract base for solid variants)
//
// 保真依据：逐位移植 SolidPrimitive.ts。SolidPrimitive : GeometryQuery（abstract）持 _capped
// 标志 + 抽象 solidPrimitiveType/constantVSection/getConstructiveFrame/isClosedVolume。
// 具体子类型（Box/Cone/Sphere/sweeps/TorusPipe）derive 自本类。clone/Range/IsAlmostEqual 等保持
// 抽象（子类覆写），对齐参考（参考 SolidPrimitive 不覆写这些）。
#pragma once

#include "CurveCollection.h"
#include "GeometryQuery.h"
#include "Transform.h"

#include <optional>

BEGIN_DQ_GEOM_NAMESPACE

// SolidPrimitiveType — concrete type discriminator (1:1 SolidPrimitive.ts SolidPrimitiveType).
enum class SolidPrimitiveType : int {
    Box,
    Cone,
    Sphere,
    LinearSweep,
    RotationalSweep,
    RuledSweep,
    TorusPipe,
};

// SolidPrimitive — abstract base for solid variants (1:1 SolidPrimitive.ts).
// Base class holds the capped flag for all derived classes.
class DQ_GEOM_EXPORT SolidPrimitive : public GeometryQuery {
public:
    ~SolidPrimitive() override;

    // --- GeometryQuery ---
    GeometryCategory Category() const noexcept final { return GeometryCategory::Solid; }

    // --- Type discriminator (1:1 solidPrimitiveType) ---
    virtual SolidPrimitiveType GetSolidPrimitiveType() const noexcept = 0;

    // --- capped flag (1:1 SolidPrimitive.capped get/set) ---
    bool Capped() const noexcept { return m_capped; }
    void SetCapped(bool capped) noexcept { m_capped = capped; }

    // --- Abstract solid queries ---
    // Cross-section curves at fractional v position (1:1 constantVSection).
    virtual dqBase::RefPtr<CurveCollection> ConstantVSection(double vFraction) const = 0;
    // Local-to-world rigid frame (1:1 getConstructiveFrame).
    virtual std::optional<Transform> GetConstructiveFrame() const = 0;
    // True if this is a closed volume (1:1 isClosedVolume).
    virtual bool IsClosedVolume() const = 0;

protected:
    explicit SolidPrimitive(bool capped) : m_capped(capped) {}

    bool m_capped; // 1:1 SolidPrimitive._capped
};

using SolidPrimitivePtr = dqBase::RefPtr<SolidPrimitive>;

END_DQ_GEOM_NAMESPACE
