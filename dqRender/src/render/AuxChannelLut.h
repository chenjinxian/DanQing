// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — AuxChannelLUT (GPU texture + name→channel maps for aux channels)
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/VertexLUT.ts
//              (AuxChannelLUT class :21-66)
//
// 落点裁决（W-a4，计划委托代理定）：DanQing 无参考的 VertexLUT 类层级（MeshData.lut
// 的容器）——VertexLutTexture 是 VertexLUT 的部分同构体（纯打包纹理+量化参数，无
// auxChannels 字段）。AuxChannelLUT 自立为 per-geometry 的 owning 类（与参考
// MeshData.lut.auxChannels 的 per-geometry 生命周期同位），由 SurfaceGeometry 以
// unique_ptr 持有（W-a5 的 addAnimation 经 params.geometry 的 LUT 面消费）。
#pragma once

#include "AuxChannelTable.h"

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/Handle.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// AuxChannelLUT — 1:1 VertexLUT.ts:21-66 AuxChannelLUT
// ---------------------------------------------------------------------------
class AuxChannelLUT {
public:
    // 三族 name→channel 映射（1:1 :25-27——参考 TS Map<string, channel> 为哈希+插入序，
    // std::unordered_map 承载（§8.3 注：TS Map 的忠实 C++ 对应物）。
    std::unordered_map<std::string, AuxDisplacementChannel> displacements;
    std::unordered_map<std::string, AuxChannel> normals;
    std::unordered_map<std::string, AuxParamChannel> params;

    AuxChannelLUT() = default;
    AuxChannelLUT(AuxChannelLUT const&) = delete;
    AuxChannelLUT& operator=(AuxChannelLUT const&) = delete;

    // 1:1 字段（:22-24）。
    rhi::TextureHandle texture() const noexcept { return m_texture; }
    uint32_t numVertices() const noexcept { return m_numVertices; }
    uint32_t numBytesPerVertex() const noexcept { return m_numBytesPerVertex; }

    // 1:1 hasScalarAnimation（:54——params 表存在性）。
    bool hasScalarAnimation() const noexcept { return !params.empty(); }

    // 1:1 create（:62-65——TextureHandle.createForData ↔ AuxChannelTable::createLutTexture
    // 既有缝；纹理创建失败 → nullptr）。
    static std::unique_ptr<AuxChannelLUT> create(rhi::Driver& driver, AuxChannelTable const& table)
    {
        auto lut = std::unique_ptr<AuxChannelLUT>(new AuxChannelLUT());
        lut->m_texture = table.createLutTexture(driver);
        if (lut->m_texture == rhi::TextureHandle{})
            return nullptr;
        lut->m_numVertices = table.numVertices();
        lut->m_numBytesPerVertex = table.numBytesPerVertex();
        // initChannels 泛型（:39-51——逐族填图；空族 → 空 map 保持缺席语义）。
        for (auto const& channel : table.displacements())
            lut->displacements.emplace(channel.name(), channel);
        for (auto const& channel : table.normals())
            lut->normals.emplace(channel.name(), channel);
        for (auto const& channel : table.params())
            lut->params.emplace(channel.name(), channel);
        return lut;
    }

    // 纹理由 driver 销毁（rhi 句柄的既有生命周期模式——VertexLutTexture::destroy 同族）。
    void destroy(rhi::Driver& driver)
    {
        if (m_texture != rhi::TextureHandle{}) {
            driver.destroyTexture(m_texture);
            m_texture = rhi::TextureHandle{};
        }
    }

private:
    rhi::TextureHandle m_texture;
    uint32_t m_numVertices = 0;
    uint32_t m_numBytesPerVertex = 0;
};

END_DQ_RENDER_NAMESPACE
