// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/DisplayParams.ts
// DanQing dqRender — DisplayParams method bodies (factories / regionEdgeType / equals / compareForMerge)
#include "DisplayParams.h"

#include <dqCommon/RenderTexture.h>

BEGIN_DQ_RENDER_NAMESPACE

namespace {
// 1:1 core-bentley compareNumbers / compareBooleans / comparePossiblyUndefined.
template <typename T> int compareOrdered(T a, T b) { return a < b ? -1 : (a > b ? 1 : 0); }

int compareMaterials(const dqBase::RefPtr<dqCommon::RenderMaterial>& lhs,
                     const dqBase::RefPtr<dqCommon::RenderMaterial>& rhs) {
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return lhs->compare(*rhs);
}

int compareTextureMappings(const std::optional<dqCommon::TextureMapping>& lhs,
                           const std::optional<dqCommon::TextureMapping>& rhs) {
    if (!lhs && !rhs) return 0;
    if (!lhs) return -1;
    if (!rhs) return 1;
    return lhs->compare(*rhs);
}
}  // namespace

// 1:1 DisplayParams constructor. Reference asserts material+textureMapping are not both set; DanQing
// (-fno-exceptions) leaves that as a caller invariant documented in the header.
DisplayParams::DisplayParams(Type type, dqCommon::ColorDef lineColor, dqCommon::ColorDef fillColor, double width,
                             dqCommon::LinePixels linePixels, dqCommon::FillFlags fillFlags,
                             dqBase::RefPtr<dqCommon::RenderMaterial> material,
                             std::optional<dqCommon::GradientSymb> gradient, bool ignoreLighting,
                             std::optional<dqCommon::TextureMapping> textureMapping)
    : m_type(type),
      m_material(std::move(material)),
      m_gradient(std::move(gradient)),
      m_textureMapping(std::move(textureMapping)),
      m_lineColor(adjustTransparency(lineColor)),
      m_fillColor(adjustTransparency(fillColor)),
      m_width(width),
      m_linePixels(linePixels),
      m_fillFlags(fillFlags),
      m_ignoreLighting(ignoreLighting) {}

// Ported from: DisplayParams.createForType (53-70).
DisplayParams DisplayParams::createForType(Type type, const dqCommon::GraphicParams& gf,
                                           GradientResolver resolveGradient, bool ignoreLighting) {
    const dqCommon::ColorDef lineColor = adjustTransparency(gf.lineColor);
    switch (type) {
        case Type::Mesh: {
            std::optional<dqCommon::TextureMapping> gradientMapping;
            if (gf.gradient.has_value() && resolveGradient) {
                auto gradientTexture = resolveGradient(*gf.gradient);
                if (gradientTexture)
                    gradientMapping = dqCommon::TextureMapping(gradientTexture, dqCommon::TextureMappingParams{});
            }
            return DisplayParams(type, lineColor, adjustTransparency(gf.fillColor),
                                 static_cast<double>(gf.rasterWidth), gf.linePixels, gf.fillFlags,
                                 gf.material, gf.gradient, ignoreLighting, std::move(gradientMapping));
        }
        case Type::Linear:
            return DisplayParams(type, lineColor, lineColor, static_cast<double>(gf.rasterWidth), gf.linePixels);
        default: { // Type::Text
            return DisplayParams(type, lineColor, lineColor, 0.0, dqCommon::LinePixels::Solid,
                                 dqCommon::FillFlags::Always, nullptr, std::nullopt, /*ignoreLighting=*/true);
        }
    }
}

// Ported from: DisplayParams.textureMapping getter (108).
const std::optional<dqCommon::TextureMapping>& DisplayParams::textureMapping() const noexcept {
    return m_material ? m_material->textureMapping : m_textureMapping;
}

// Ported from: DisplayParams.regionEdgeType getter (87-99).
RegionEdgeType DisplayParams::regionEdgeType() const noexcept {
    if (hasBlankingFill())
        return RegionEdgeType::None;

    if (m_gradient.has_value()) {
        // Even if the gradient is not outlined, produce an outline to display as the region's edges
        // when the fill ViewFlag is off.
        if (m_gradient->isOutlined() ||
            dqCommon::FillFlags::None == (m_fillFlags & dqCommon::FillFlags::Always))
            return RegionEdgeType::Outline;
        return RegionEdgeType::None;
    }
    return (!m_fillColor.equals(m_lineColor)) ? RegionEdgeType::Outline : RegionEdgeType::Default;
}

// Ported from: DisplayParams.equals (112-141). The reference uses reference-identity (!==) for
// material/textureMapping; DanQing stores them by value, so the faithful equivalent is value comparison
// via the ordered compare helpers (null/null → equal, matching the reference test expectations).
bool DisplayParams::equals(const DisplayParams& rhs, ComparePurpose purpose) const {
    if (ComparePurpose::Merge == purpose)
        return 0 == compareForMerge(rhs);
    if (this == &rhs)
        return true;

    if (m_type != rhs.m_type) return false;
    if (m_ignoreLighting != rhs.m_ignoreLighting) return false;
    if (m_width != rhs.m_width) return false;
    if (m_linePixels != rhs.m_linePixels) return false;
    if (m_fillFlags != rhs.m_fillFlags) return false;
    if (wantRegionOutline() != rhs.wantRegionOutline()) return false;
    if (compareMaterials(m_material, rhs.m_material) != 0) return false;
    if (compareTextureMappings(textureMapping(), rhs.textureMapping()) != 0) return false;
    if (!m_fillColor.equals(rhs.m_fillColor)) return false;
    if (!m_lineColor.equals(rhs.m_lineColor)) return false;
    return true;
}

// Ported from: DisplayParams.compareForMerge (143-177).
int DisplayParams::compareForMerge(const DisplayParams& rhs) const noexcept {
    if (this == &rhs)
        return 0;

    int diff = compareOrdered(static_cast<int>(m_type), static_cast<int>(rhs.m_type));
    if (0 == diff) {
        diff = compareOrdered(m_ignoreLighting, rhs.m_ignoreLighting);
        if (0 == diff) {
            diff = compareOrdered(m_width, rhs.m_width);
            if (0 == diff) {
                diff = compareOrdered(static_cast<int>(m_linePixels), static_cast<int>(rhs.m_linePixels));
                if (0 == diff) {
                    diff = compareOrdered(static_cast<uint32_t>(m_fillFlags), static_cast<uint32_t>(rhs.m_fillFlags));
                    if (0 == diff) {
                        diff = compareOrdered(wantRegionOutline(), rhs.wantRegionOutline());
                        if (0 == diff) {
                            diff = compareOrdered(hasFillTransparency(), rhs.hasFillTransparency());
                            if (0 == diff) {
                                diff = compareOrdered(hasLineTransparency(), rhs.hasLineTransparency());
                                if (0 == diff) {
                                    diff = compareMaterials(m_material, rhs.m_material);
                                    if (0 == diff && !m_material && isTextured())
                                        diff = compareTextureMappings(textureMapping(), rhs.textureMapping());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return diff;
}

END_DQ_RENDER_NAMESPACE
