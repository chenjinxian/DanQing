// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Graphic template implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/GraphicTemplateImpl.ts
//
// Concrete implementation of GraphicTemplate.
#pragma once

// 公开 API 的 GraphicTemplate（抽象接口，GraphicTemplate.ts 忠实移植）——
// 此前继承的是 src/render/GraphicTemplate.h 的自创具体类（setGeometry/
// getFeatureCount 布局），与公开抽象版同名不同布局构成 ODR 双定义，已删除。
#include "dqRender/GraphicTemplate.h"
#include "MeshGraphic.h"
#include "dqRender/RenderGraphic.h"

#include <memory>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GraphicTemplateImpl — concrete graphic template
// (Ported from: itwinjs-core GraphicTemplateImpl.ts)
// ---------------------------------------------------------------------------
class GraphicTemplateImpl : public GraphicTemplate {
public:
    GraphicTemplateImpl() = default;
    ~GraphicTemplateImpl() = default;

    /// Whether the graphics in this template can be instanced.
    /// Ported from: itwinjs-core GraphicTemplateImpl.ts:57-66 —— 参考在构造时
    /// 逐 geometry 检查 isInstanceable 并累计（glTF 已实例化几何/不可实例化
    /// builder 几何 → false）；DanQing 简化实现恒 true（无不可实例化源接入）。
    bool isInstanceable() const noexcept override { return true; }

    GraphicTemplateImpl(GraphicTemplateImpl const&) = delete;
    GraphicTemplateImpl& operator=(GraphicTemplateImpl const&) = delete;

    /// Set the mesh graphic for this template.
    void setMeshGraphic(std::unique_ptr<MeshGraphic> mesh)
    {
        m_meshGraphic = std::move(mesh);
    }

    /// Get the mesh graphic.
    MeshGraphic* getMeshGraphic() const noexcept { return m_meshGraphic.get(); }

    /// Create a RenderGraphic from this template (instanced).
    RenderGraphic* createInstance(float const* transform);

private:
    std::unique_ptr<MeshGraphic> m_meshGraphic;
};

END_DQ_RENDER_NAMESPACE
