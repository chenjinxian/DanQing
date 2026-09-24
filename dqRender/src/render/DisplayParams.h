// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/DisplayParams.ts
// DanQing dqRender — DisplayParams (symbology carrier used to determine mesh batching; @internal)
//
// 保真依据：逐字段/方法移植 DisplayParams.ts。DisplayParams 决定哪些几何可批处理合并显示。
// 依赖（均已 port 于 dqCommon）：GraphicParams/ColorDef/FillFlags/Gradient(GraphientSymb)/LinePixels/
// RenderMaterial/RenderTexture/TextureMapping。material(TS ?) → RefPtr<RenderMaterial>(null=absent)；
// gradient/_textureMapping(TS ?) → std::optional。
#pragma once

#include <dqBase/RefCounted.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/FillFlags.h>
#include <dqCommon/Gradient.h>
#include <dqCommon/GraphicParams.h>
#include <dqCommon/LinePixels.h>
#include <dqCommon/RenderMaterial.h>
#include <dqCommon/TextureMapping.h>

#include <functional>
#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 DisplayParams.Type / RegionEdgeType / ComparePurpose (flat enums + nested aliases so
// `DisplayParams::Type::Mesh` mirrors the reference `DisplayParams.Type.Mesh`).
enum class DisplayParamsType : uint8_t { Mesh = 0, Linear = 1, Text = 2 };
enum class RegionEdgeType : uint8_t { None = 0, Default = 1, Outline = 2 };
enum class ComparePurpose : uint8_t { Merge = 0, Strict = 1 };

// This class is used to determine if things can be batched together for display. @internal.
// Ported from: itwinjs-core core/frontend/src/common/internal/render/DisplayParams.ts
class DisplayParams {
public:
    using Type = DisplayParamsType;
    using GradientSymb = dqCommon::GradientSymb;

    // Resolve a gradient symbology to a texture (returns null RefPtr if unresolved).
    // 1:1 GraphicAssembler.resolveGradient: (grad: Gradient.Symb) => RenderTexture | undefined.
    using GradientResolver = std::function<dqBase::RefPtr<dqCommon::RenderTexture>(const dqCommon::GradientSymb&)>;

    // Threshold below which a color is considered fully opaque (1:1 DisplayParams.minTransparency).
    static constexpr int minTransparency = 15;

    DisplayParams(Type type, dqCommon::ColorDef lineColor, dqCommon::ColorDef fillColor, double width = 0.0,
                  dqCommon::LinePixels linePixels = dqCommon::LinePixels::Solid,
                  dqCommon::FillFlags fillFlags = dqCommon::FillFlags::None,
                  dqBase::RefPtr<dqCommon::RenderMaterial> material = nullptr,
                  std::optional<dqCommon::GradientSymb> gradient = std::nullopt,
                  bool ignoreLighting = false,
                  std::optional<dqCommon::TextureMapping> textureMapping = std::nullopt);

    // Default-constructed DisplayParams: a Type::Mesh with white line/fill (Authored — the TS reference
    // has no default ctor, but C++ aggregate value-types like Mesh::Props need one to allow incremental
    // field assignment; equivalent to DisplayParams(Type::Mesh, white, white)).
    DisplayParams() : DisplayParams(Type::Mesh, dqCommon::ColorDef::white, dqCommon::ColorDef::white) {}

    // --- Static factories (1:1 DisplayParams.createForType/Mesh/Linear/Text) ---
    static DisplayParams createForType(Type type, const dqCommon::GraphicParams& gf,
                                       GradientResolver resolveGradient = nullptr, bool ignoreLighting = false);
    static DisplayParams createForMesh(const dqCommon::GraphicParams& gf, bool ignoreLighting,
                                       GradientResolver resolveGradient = nullptr) {
        return createForType(Type::Mesh, gf, std::move(resolveGradient), ignoreLighting);
    }
    static DisplayParams createForLinear(const dqCommon::GraphicParams& gf) {
        return createForType(Type::Linear, gf);
    }
    static DisplayParams createForText(const dqCommon::GraphicParams& gf) {
        return createForType(Type::Text, gf);
    }

    // 1:1 DisplayParams.adjustTransparency — colors below minTransparency are treated fully opaque.
    static dqCommon::ColorDef adjustTransparency(dqCommon::ColorDef color) {
        return (color.getTransparency() < minTransparency) ? color.withTransparency(0) : color;
    }

    // --- Accessors ---
    Type type() const noexcept { return m_type; }
    const dqBase::RefPtr<dqCommon::RenderMaterial>& material() const noexcept { return m_material; }
    const std::optional<dqCommon::GradientSymb>& gradient() const noexcept { return m_gradient; }
    dqCommon::ColorDef lineColor() const noexcept { return m_lineColor; }
    dqCommon::ColorDef fillColor() const noexcept { return m_fillColor; }
    double width() const noexcept { return m_width; }
    dqCommon::LinePixels linePixels() const noexcept { return m_linePixels; }
    dqCommon::FillFlags fillFlags() const noexcept { return m_fillFlags; }
    bool ignoreLighting() const noexcept { return m_ignoreLighting; }

    // 1:1 DisplayParams.textureMapping getter: material.textureMapping if material set, else _textureMapping.
    const std::optional<dqCommon::TextureMapping>& textureMapping() const noexcept;
    bool isTextured() const noexcept { return textureMapping().has_value(); }

    // 1:1 DisplayParams.regionEdgeType getter.
    RegionEdgeType regionEdgeType() const noexcept;
    bool wantRegionOutline() const noexcept { return RegionEdgeType::Outline == regionEdgeType(); }
    bool hasBlankingFill() const noexcept {
        return dqCommon::FillFlags::Blanking == (m_fillFlags & dqCommon::FillFlags::Blanking);
    }
    bool hasFillTransparency() const noexcept { return 255 != m_fillColor.getAlpha(); }
    bool hasLineTransparency() const noexcept { return 255 != m_lineColor.getAlpha(); }

    // 1:1 DisplayParams.equals(rhs, purpose).
    bool equals(const DisplayParams& rhs, ComparePurpose purpose = ComparePurpose::Strict) const;

    // 1:1 DisplayParams.compareForMerge(rhs) — tiered ordered comparison for batching keys.
    int compareForMerge(const DisplayParams& rhs) const noexcept;

private:
    Type m_type = Type::Mesh;
    dqBase::RefPtr<dqCommon::RenderMaterial> m_material;          // meshes only
    std::optional<dqCommon::GradientSymb> m_gradient;             // gradient fill
    std::optional<dqCommon::TextureMapping> m_textureMapping;     // only if material is undefined
    dqCommon::ColorDef m_lineColor;                               // edge color for meshes
    dqCommon::ColorDef m_fillColor;                               // meshes only
    double m_width = 0.0;                                         // linear and mesh (edges)
    dqCommon::LinePixels m_linePixels = dqCommon::LinePixels::Solid;
    dqCommon::FillFlags m_fillFlags = dqCommon::FillFlags::None;  // meshes only
    bool m_ignoreLighting = false;
};

END_DQ_RENDER_NAMESPACE
