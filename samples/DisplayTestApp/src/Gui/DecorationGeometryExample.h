// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Decoration Geometry Example
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/DecorationGeometryExample.ts
//              GeometryDecorator (:11-186) + openDecorationGeometryExample (:188-219)
//
// Shows a 4×4 grid of decorated solids (sphere/box/cone/shape per row) cycling
// four texture combinations (none / normal-map only / color-map only / both)
// plus one multi-geometry row, in a new blank connection with a two-color sky.
#pragma once

#include <dqApp/Decorator.h>
#include <dqApp/Export.h>

#include <dqCommon/GeometryClass.h>

#include <dqGeom/Range3d.h>

#include <dqRender/rhi/Handle.h>

#include <QString>

#include <cstdint>
#include <vector>

namespace dqApp {
class Viewport;
class DecorateContext;
}

namespace dqGeom {
class IndexedPolyface;
}

namespace Gui {

class View3DInventor;

// ---------------------------------------------------------------------------
// GeometryDecorator — the decoration owning the example geometry
// Ported from: DecorationGeometryExample.ts:11-186
// ---------------------------------------------------------------------------
class GeometryDecorator : public dqApp::IDecorator {
public:
    // Ported from: DecorationGeometryExample.ts:20-50 (constructor: lays out the
    // 4×4 grid + multi-feature row; stores textures). DanQing tessellates per
    // BuildGraphic (the reference tessellates per builder.finish() with the
    // current chord tolerance — GeometryPrimitives.ts:62-69 + GraphicBuilder.ts
    // :169-181), so geometry is rebuilt on scene invalidation like the reference.
    GeometryDecorator(dqApp::Viewport& vp,
                      dqRender::rhi::TextureHandle texture,
                      dqRender::rhi::TextureHandle normalMap);
    ~GeometryDecorator() override;

    // ← :12 (useCachedDecorations = true): the graphic is built once and
    // cached; scene invalidation rebuilds it.
    bool UseCachedDecorations() const override { return true; }

    // ← :70-131 (decorate: build the branch of pickable graphics, add as a
    // Scene decoration).
    void Decorate(dqApp::DecorateContext& context) override;

    // ← :87 pickable ids: 17 decorations × own id + multi-feature row's 4 ids
    // (shape/box/sphere/cone, :162-185) = 20 个可拾取 id（transientIds.getNext
    // 等价物 = NextPickableId 静态序列）。
    bool TestDecorationHit(uint32_t featureId) const override;
    QString GetDecorationToolTip(uint32_t featureId) const override;

    static uint32_t NextPickableId();

    // Explicit teardown (the reference ties it to iModel.onClose, :49;
    // DanQing's iModel has no onClose event — the caller destroys the decorator
    // when the view it decorates goes away, before the ViewManager's
    // decorator list can dangle into the next viewport).
    void Shutdown() { m_shutdown = true; }

private:
    // 每条 decoration 的几何描述（参考 :134-160 的 add* 注册的 builder 回调；
    // DanQing 在 BuildGraphic 里按描述即时 tessellate）。
    struct PieceDesc {
        enum class Kind { Sphere, Box, Cone, Shape } kind;
        double cx = 0.0, cy = 0.0;      // grid cell origin（单位格）
        uint32_t pickId = 0;            // 该几何的 pickable id（:87/:164-166）
        dqCommon::GeometryClass geomClass = dqCommon::GeometryClass::Primary;  // :176 sphere=Construction
    };
    struct EntryDesc {
        std::vector<PieceDesc> pieces;  // 常规 1 个；multi-feature 行 4 个（:162-185）
        int colorIndex = 0;             // 装饰级颜色循环索引（:91-95）
        int textureRow = 0;             // floor(i/4) 纹理组合行（:97-99；4=multi 无纹理）
    };

    void BuildGraphic(dqApp::Viewport& vp);

    // 每条几何一个 builder 的即时 tessellate（参考每条 decoration 一个
    // GraphicBuilder；公差 = 像素尺寸 × 0.25，GraphicBuilder.ts:169-181）。
    static dqBase::RefPtr<dqGeom::IndexedPolyface> TessellatePiece(dqApp::Viewport& vp,
                                                                   PieceDesc const& piece,
                                                                   bool textured);

    std::vector<EntryDesc> m_entries;

    dqRender::rhi::TextureHandle m_texture;
    dqRender::rhi::TextureHandle m_normalMap;

    dqRender::RenderGraphic* m_graphic = nullptr;   // not owned — built per Decorate, owned by the frame's decoration set
    dqGeom::Range3d m_range;
    uint32_t m_pickableId;              // 保留：首个装饰 id（兼容既有语义）
    bool m_shutdown = false;
    dqApp::Viewport* m_viewport = nullptr;          // back-reference for per-Decorate rebuilds
};

// ---------------------------------------------------------------------------
// openDecorationGeometryExample — entry point
// Ported from: DecorationGeometryExample.ts:188-219
// ---------------------------------------------------------------------------
void openDecorationGeometryExample(View3DInventor& view);

}  // namespace Gui
