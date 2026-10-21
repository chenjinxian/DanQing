// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Contours implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Contours.ts
#include "Contours.h"

#include "RenderSystemImpl.h"
#include "TargetImpl.h"
#include "UniformHandle.h"
#include "VertexTableBuilder.h"

#include <dqBase/CompressedId64Set.h>
#include <dqBase/DqId.h>

#include <cassert>
#include <cmath>
#include <unordered_map>

BEGIN_DQ_RENDER_NAMESPACE

Contours::Contours(TargetImpl& target)
    : m_target(&target)
    , m_contours(target.getCurrentContours()
                     ? std::optional<dqCommon::ContourDisplay>(*target.getCurrentContours())
                     : std::nullopt)
{
}

// Ported from: itwinjs-core Contours.createFromTarget（:117-119）。
std::unique_ptr<Contours> Contours::createFromTarget(TargetImpl& target)
{
    return std::unique_ptr<Contours>(new Contours(target));
}

// Ported from: itwinjs-core Contours.matchesTargetAndFeatureCount（:37-40）。
bool Contours::matchesTargetAndFeatureCount(TargetImpl const& target,
                                            dqCommon::PackedFeatureTable const& map) const noexcept
{
    return &target == m_target && m_numFeatures == map.getNumFeatures();
}

// Ported from: itwinjs-core Contours.matchesSubCategories（:42-54——
//   undefined 双态矩阵 + 组数等 + 逐组 subCategoriesEqual[截断于
//   maxContourGroups]）。
bool Contours::matchesSubCategories() const
{
    auto const* current = m_target->getCurrentContours();
    if (!m_contours.has_value() && nullptr == current)
        return true;
    if (!m_contours.has_value() || nullptr == current)
        return false;
    if (m_contours->groups.size() != current->groups.size())
        return false;
    for (size_t index = 0, len = m_contours->groups.size();
         index < len && static_cast<int>(index) < dqCommon::ContourDisplay::MaxContourGroups;
         ++index) {
        if (!m_contours->groups[index].subCategoriesEqual(current->groups[index]))
            return false;
    }
    return true;
}

// Ported from: itwinjs-core Contours._initialize（:56-69）。
void Contours::initialize(dqCommon::PackedFeatureTable const& map)
{
    assert(0 < map.getNumFeatures());
    m_numFeatures = map.getNumFeatures();
    // computeDimensions(numFeatures, 1/8, 0, maxTextureSize)——每 RGBA texel
    // 装 8 特征（Contours.ts:59）。
    uint32_t width = 0, height = 0;
    VertexTableBuilder::computeDimensions(m_numFeatures, 1.0 / 8.0, 0,
                                          m_target->getSystem().getMaxTextureSize(),
                                          width, height);
    assert(static_cast<size_t>(width) * height * 8 >= m_numFeatures);

    m_data.assign(static_cast<size_t>(width) * height * 4, 0);
    buildLookupTable(map);
    // TextureHandle.createForData(w, h, data, true, ClampToEdge)（:67）。
    m_lut = TextureHandle::create2D(m_target->getDriver(), width, height,
                                    rhi::TextureFormat::RGBA8,
                                    m_data.data(),
                                    static_cast<uint32_t>(m_data.size()));
    m_lutWidth = width;
}

// Ported from: itwinjs-core Contours._update（:71-76——同尺寸就地重传；
//   replaceTextureData 语义）。
void Contours::updateLut(dqCommon::PackedFeatureTable const& map)
{
    assert(m_numFeatures == map.getNumFeatures());
    buildLookupTable(map);
    if (m_lut.isValid()) {
        // setTextureData 就地重传（ThematicSensors::uploadTexture replace 臂
        // 先例——GL_RGBA/UNSIGNED_BYTE 客户端格式）。
        rhi::PixelBufferDescriptor pbd(
            m_data.data(), m_data.size(),
            0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */,
            0, 1, 0, 0, m_lut.getWidth(), m_lut.getHeight(), 1);
        m_target->getDriver().setTextureData(m_lut.getRhiHandle(), 0, 0, 0, 0,
                                             m_lut.getWidth(), m_lut.getHeight(), 1,
                                             std::move(pbd));
    }
}

// Ported from: itwinjs-core Contours.buildLookupTable（:78-109）。
void Contours::buildLookupTable(dqCommon::PackedFeatureTable const& map)
{
    // setup an efficient way to compare feature subcategories with lists in groups
    // EQUIVALENCE（§11.10）：参考 Id64.Uint32Map（lower,upper 双键）→ uint64
    // 单键（DqId 的 lower|upper<<32 打包恒等）；发散=无；验证法=
    // ContoursTest.LookupTablePacksNibblesPerFeaturePair 字节钉值。
    std::unordered_map<uint64_t, uint32_t> subCatMap;
    uint32_t defaultNdx = 0xf;  // default for unmatched subcategories is to not show contours
    auto const* current = m_target->getCurrentContours();
    if (nullptr != current) {
        // NB: index also has to be a max of 14 - has to fit in 4 bits with value
        // 15 reserved for no group def（Contours.ts:82-83 注）。
        for (size_t index = 0, len = current->groups.size();
             index < len && static_cast<int>(index) < dqCommon::ContourDisplay::MaxContourGroups;
             ++index) {
            auto const& subCats = current->groups[index].subCategories;
            if (subCats.empty()) {
                defaultNdx = static_cast<uint32_t>(index);  // change default for unmatched subcategories to this definition
            } else {
                for (auto const subCat : dqBase::CompressedId64Set::DecompressArray(subCats))
                    subCatMap[subCat.GetValue()] = static_cast<uint32_t>(index);
            }
        }
    }

    // NB: We currently use 1/2 of one component of RGBA value per feature as follows:
    //   [0] R/G/B/A = index pair - lower 4 bits = ndx n, upper 4 bits = ndx n+1
    bool even = false;
    uint8_t byteOut = 0;
    size_t dataIndex = 0;
    for (uint32_t i = 0; i < map.getNumFeatures(); ++i) {
        auto const feature = map.getFeature(i);
        dataIndex = static_cast<size_t>(i / 2);
        even = (i & 1u) == 0u;
        auto const it = subCatMap.find(feature.subCategoryId);
        uint8_t const groupNdx = static_cast<uint8_t>(
            it != subCatMap.end() ? it->second : defaultNdx);
        if (even)
            byteOut = groupNdx;
        else
            m_data[dataIndex] = static_cast<uint8_t>((groupNdx << 4) | byteOut);
    }
    if (even)  // not written
        m_data[dataIndex] = byteOut;
}

// Ported from: itwinjs-core Contours.initFromMap（:128-131——dispose 旧 LUT
//   后重建）。
void Contours::initFromMap(dqCommon::PackedFeatureTable const& map)
{
    m_lut = TextureHandle{};
    initialize(map);
}

// Ported from: itwinjs-core Contours.update（:133-142）。
void Contours::update(dqCommon::PackedFeatureTable const& map)
{
    if (matchesSubCategories())
        return;
    m_contours = m_target->getCurrentContours()
        ? std::optional<dqCommon::ContourDisplay>(*m_target->getCurrentContours())
        : std::nullopt;
    updateLut(map);
}

// Ported from: itwinjs-core Contours.bindContourLUTWidth（:144-146）。
void Contours::bindContourLUTWidth(UniformHandle& uniform) const
{
    uniform.setUniform1ui(m_lutWidth);
}

END_DQ_RENDER_NAMESPACE
