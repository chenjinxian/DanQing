// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Decoration Geometry Example implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/DecorationGeometryExample.ts
//
// Rendering-channel note (APPROVED mapping): the reference builds graphics via
// DecorateContext.createGraphic (GraphicBuilder per decoration); DanQing routes
// the same geometry through the verified GltfDecoration channel
// (tessellate → PolyfaceBuilder → createGraphicFromPolyface with the
// color/normalMap texture pair → createBatch for pickability) — the channel
// that already renders textured, normal-mapped, pickable decorations.
// UV 坐标经参考的 MeshBuilder 机制生成（MeshBuilder.ts:136-139 →
// TextureMapping.computeUVParams），逐 facet 写回 polyface 参数。
#include "DecorationGeometryExample.h"

#include "View3DInventor.h"

#include <dqApp/DecorateContext.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewManager.h>
#include <dqApp/ViewingSpace.h>
#include <dqApp/Application.h>

#include <dqRender/CreateTextureArgs.h>
#include <dqRender/GraphicBranch.h>
#include <dqRender/RenderSystem.h>

#include <dqCommon/ColorDef.h>
#include <dqCommon/Environment.h>
#include <dqCommon/FeatureTable.h>
#include <dqCommon/Image.h>
#include <dqCommon/RenderMode.h>
#include <dqCommon/TextureMapping.h>
#include <dqCommon/ViewFlags.h>

#include <dqGeom/Box.h>
#include <dqGeom/Cone.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/Sphere.h>

#include <dqBase/DqId.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <fstream>
#include <vector>

#ifndef DANQING_SPRITE_ASSETS_DIR
#define DANQING_SPRITE_ASSETS_DIR "."
#endif

namespace Gui {

using namespace dqRender;

// transientId 序列（参考 iModel.transientIds.getNext()，DecorationGeometryExample.ts
// :138/:142/:164-166——DanQing 的 blank connection 无 transient id 序列，用静态计数器）。
static uint32_t s_nextPickableId = 900000;

uint32_t GeometryDecorator::NextPickableId() { return ++s_nextPickableId; }

// ---------------------------------------------------------------------------
// PolyfaceUVVisitor — dqCommon::PolyfaceVisitor 的 IndexedPolyface 桥接
// （参考 MeshBuilder.ts:136-139 的 computeUVParams 调用点以 PolyfaceVisitor 读
// facet；DanQing 的 visitor 接口在 dqCommon，实现放在使用点）。
// ---------------------------------------------------------------------------
namespace {

class PolyfaceUVVisitor : public dqCommon::PolyfaceVisitor {
public:
    PolyfaceUVVisitor(dqGeom::IndexedPolyface const& pf, size_t facet)
        : m_data(pf.Data())
    {
        m_i0 = pf.FacetIndex0(facet);
        m_numEdge = pf.FacetIndex1(facet) - m_i0;
    }

    int numEdgesThisFacet() const noexcept override { return static_cast<int>(m_numEdge); }

    dqGeom::Point3d point(int i) const override
    {
        return m_data.GetPoint(std::abs(m_data.pointIndex[m_i0 + static_cast<size_t>(i)]));
    }

    std::optional<dqGeom::Vector3d> normal(int i) const override
    {
        auto const idx = normalIndex(i);
        if (!idx.has_value() || *idx <= 0) return std::nullopt;
        return m_data.GetNormal(*idx);
    }

    std::optional<int> normalIndex(int i) const override
    {
        size_t const k = m_i0 + static_cast<size_t>(i);
        if (m_data.normals.empty() || k >= m_data.normalIndex.size()) return std::nullopt;
        return m_data.normalIndex[k];
    }

    // 距离/归一化参数依赖 FacetFaceData（IndexedPolyfaceVisitor.ts:109-141）——
    // PolyfaceBuilder 产出的 polyface 无 face 数据，参考同样落回 getParam。
    std::optional<dqCommon::TextureUVPoint> tryGetDistanceParameter(int) const override { return std::nullopt; }
    std::optional<dqCommon::TextureUVPoint> tryGetNormalizedParameter(int) const override { return std::nullopt; }

    dqCommon::TextureUVPoint getParam(int i) const override
    {
        size_t const k = m_i0 + static_cast<size_t>(i);
        if (m_data.params.empty() || k >= m_data.paramIndex.size())
            return dqCommon::TextureUVPoint(0.0, 0.0);
        int32_t const idx = m_data.paramIndex[k];
        if (idx <= 0) return dqCommon::TextureUVPoint(0.0, 0.0);
        dqGeom::Point2d const p = m_data.GetParam(idx);
        return dqCommon::TextureUVPoint(p.x, p.y);
    }

private:
    dqGeom::PolyfaceData const& m_data;
    size_t m_i0 = 0;
    size_t m_numEdge = 0;
};

// applyTextureMappingUVs — 参考 MeshBuilder.createTriangleVertices 的 UV 来源
// （MeshBuilder.ts:130-142：mappedTexture.computeUVParams(visitor, ...) 逐 facet
// 计算，经 textureMatrix 变换）。DanQing 通道把最终 UV 写回 polyface 参数数组
// （PolyfaceGraphic 消费 params/paramIndex）。
void ApplyTextureMappingUVs(dqGeom::IndexedPolyface& pf)
{
    // ← DecorationGeometryExample.ts:77-79: Trans2x3(2,0,0,0,2,0) + Planar + worldMapping
    dqCommon::TextureMappingParams params;
    params.textureMatrix = dqCommon::TextureTrans2x3(2.0, 0.0, 0.0, 0.0, 2.0, 0.0);
    params.mode = dqCommon::TextureMappingMode::Planar;
    params.worldMapping = true;

    auto& data = pf.Data();
    std::vector<dqGeom::Point2d> newParams;
    std::vector<int32_t> newParamIndex;
    newParamIndex.reserve(data.pointIndex.size());
    for (size_t facet = 0; facet < pf.FacetCount(); ++facet) {
        PolyfaceUVVisitor visitor(pf, facet);
        auto uvs = params.computeUVParams(visitor);
        size_t const numEdge = pf.FacetIndex1(facet) - pf.FacetIndex0(facet);
        if (!uvs.has_value() || uvs->size() < numEdge) {
            // computeUVParams 失败（退化法线）——参考 assert(undefined !== params)；
            // 这里保底写 0（不会被命中：该 facet 同时法线退化，渲染侧同样跳过）。
            for (size_t i = 0; i < numEdge; ++i) newParamIndex.push_back(0);
            continue;
        }
        for (size_t i = 0; i < numEdge; ++i) {
            newParams.push_back(dqGeom::Point2d::From((*uvs)[i].x, (*uvs)[i].y));
            newParamIndex.push_back(static_cast<int32_t>(newParams.size()));  // 1-based
        }
    }
    data.params = std::move(newParams);
    data.paramIndex = std::move(newParamIndex);
}

}  // namespace

// TessellatePiece — 每条几何一个 builder（参考每条 decoration 一个 GraphicBuilder）。
// 公差 = 像素尺寸 × 0.25（GraphicBuilder.ts:169-181 默认 computeChordTolerance：
// getPixelSizeAtPoint(range.center) × 0.25；adjustPixelSizeForLOD 为 tile LOD
// 钩子，DanQing 无对应物——恒等）。aspectRatioSkew 分支：本视图无 skew（未接线）。
dqBase::RefPtr<dqGeom::IndexedPolyface> GeometryDecorator::TessellatePiece(dqApp::Viewport& vp,
                                                                           PieceDesc const& piece,
                                                                           bool textured)
{
    dqGeom::StrokeOptions options = dqGeom::StrokeOptions::CreateForFacets();
    // needParams iff displayParams.isTextured（GeometryPrimitives.ts:65-66）；
    // needNormals iff !ignoreLighting（:68——本示例 lighting=true）。
    options.needParams = textured;
    options.needNormals = true;
    // shape 为非平面五点多边形：参考经 Loop→SweepContour 三角化后逐三角 facet；
    // DanQing 以 fan 三角化对齐（每三角独立法线 → planar UV 逐三角）。
    options.shouldTriangulate = (piece.kind == GeometryDecorator::PieceDesc::Kind::Shape);

    dqGeom::Point3d const center = dqGeom::Point3d::From(piece.cx + 0.5, piece.cy + 0.5, 0.5);
    double const pixelSize = vp.GetViewingSpace().getPixelSizeAtPoint(&center);
    options.chordTol = pixelSize * 0.25;

    auto builder = dqGeom::PolyfaceBuilder::create(options);
    switch (piece.kind) {
        case GeometryDecorator::PieceDesc::Kind::Sphere: {
            // ← :152-153 addSphere — Sphere.createCenterRadius((cx+0.5, cy+0.5, 0.5), 0.5)
            auto sphere = dqGeom::Sphere::CreateCenterRadius(center, 0.5,
                                                             dqGeom::Sphere::FullLatitudeSweep(), /*capped=*/true);
            if (sphere) builder->AddSphere(*sphere);
            break;
        }
        case GeometryDecorator::PieceDesc::Kind::Box: {
            // ← :145-149 addBox — Box.createRange((cx,cy,0)→(cx+1,cy+1,1), true)
            auto box = dqGeom::Box::CreateRange(
                dqGeom::Range3d(piece.cx, piece.cy, 0.0, piece.cx + 1, piece.cy + 1, 1.0), true);
            if (box) builder->AddBox(*box);
            break;
        }
        case GeometryDecorator::PieceDesc::Kind::Cone: {
            // ← :156-160 addCone — Cone.createAxisPoints((cx+.5,cy+.5,0)→(z=1), 0.5, 0.25, true)
            auto cone = dqGeom::Cone::CreateBaseAndTarget(
                dqGeom::Point3d::From(piece.cx + 0.5, piece.cy + 0.5, 0.0),
                dqGeom::Point3d::From(piece.cx + 0.5, piece.cy + 0.5, 1.0),
                dqGeom::Vector3d(1, 0, 0), dqGeom::Vector3d(0, 1, 0), 0.5, 0.25, true);
            if (cone) builder->AddCone(*cone);
            break;
        }
        case GeometryDecorator::PieceDesc::Kind::Shape: {
            // ← :134-139 addShape — 5 点闭合非平面多边形
            std::vector<dqGeom::Point3d> const points = {
                dqGeom::Point3d::From(piece.cx, piece.cy, 0), dqGeom::Point3d::From(piece.cx + 1, piece.cy, 0),
                dqGeom::Point3d::From(piece.cx + 1, piece.cy + 1, 1), dqGeom::Point3d::From(piece.cx, piece.cy + 1, 1),
                dqGeom::Point3d::From(piece.cx, piece.cy, 0),
            };
            builder->AddPolygon(points);
            break;
        }
    }
    return builder->ClaimPolyface();
}

// ---------------------------------------------------------------------------
// GeometryDecorator
// ---------------------------------------------------------------------------
GeometryDecorator::GeometryDecorator(dqApp::Viewport& vp,
                                     rhi::TextureHandle texture,
                                     rhi::TextureHandle normalMap)
    : m_texture(texture)
    , m_normalMap(normalMap)
    , m_viewport(&vp)
{
    // ← :24-42 (constructor): 4 rows × (sphere | box | cone | shape)。Row y
    // values {6, 3, 0, -3}; column x offsets {0, 3, 6, 9}。每条 decoration 一个
    // pickable id（:87 pickable: { id: key }——key = transientIds.getNext()）。
    int entryIndex = 0;
    for (double y : { 6.0, 3.0, 0.0, -3.0 }) {
        for (double x : { 0.0, 3.0, 6.0, 9.0 }) {
            PieceDesc piece;
            piece.cx = x; piece.cy = y;
            piece.pickId = NextPickableId();
            piece.kind = (x == 0.0) ? PieceDesc::Kind::Sphere
                       : (x == 3.0) ? PieceDesc::Kind::Box
                       : (x == 6.0) ? PieceDesc::Kind::Cone
                                    : PieceDesc::Kind::Shape;
            EntryDesc entry;
            entry.pieces.push_back(piece);
            entry.colorIndex = entryIndex;
            entry.textureRow = entryIndex / 4;
            m_entries.push_back(std::move(entry));
            ++entryIndex;
        }
    }

    // ← :162-185 (addMultiFeatureDecoration)：y=9 行，一个 builder 内 4 个几何、
    // 4 个独立 pickable id（shape=builder 基 id；box/sphere/cone 各 activatePickableId/
    // activateFeature——sphere 为 GeometryClass.Construction）。
    {
        EntryDesc entry;
        entry.colorIndex = entryIndex;   // 16 → blue；4 个几何共享同一 setSymbology 颜色（:95）
        entry.textureRow = 4;            // ndx=4 → 无纹理（:97-99 越界）
        PieceDesc shape; shape.cx = 0; shape.cy = 9; shape.kind = PieceDesc::Kind::Shape; shape.pickId = NextPickableId();
        PieceDesc box;   box.cx = 3;   box.cy = 9; box.kind = PieceDesc::Kind::Box;       box.pickId = NextPickableId();
        PieceDesc sphere; sphere.cx = 6; sphere.cy = 9; sphere.kind = PieceDesc::Kind::Sphere;
        sphere.pickId = NextPickableId();
        sphere.geomClass = dqCommon::GeometryClass::Construction;   // ← :176
        PieceDesc cone;  cone.cx = 9;  cone.cy = 9; cone.kind = PieceDesc::Kind::Cone;    cone.pickId = NextPickableId();
        entry.pieces = { shape, box, sphere, cone };
        m_entries.push_back(std::move(entry));
    }
    m_pickableId = m_entries.empty() ? 0 : m_entries.front().pieces.front().pickId;
    BuildGraphic(vp);   // initial build（参考首个 decorate 即建图）
}

GeometryDecorator::~GeometryDecorator()
{
    // ← :52-63 ([Symbol.dispose]): drop the registration. The graphics are
    // owned by the frame's decoration set (each Decorate builds them fresh and
    // hands ownership to the viewport's decoration pipeline — the reference's
    // useCachedDecorations=true contract, :12); the decorator deletes nothing.
    m_graphic = nullptr;
}

void GeometryDecorator::BuildGraphic(dqApp::Viewport& vp)
{
    // ← :81 颜色循环：blue/red/green/yellow 逐装饰轮换（:91-95）。
    static dqCommon::ColorDef const kColors[4] = {
        dqCommon::ColorDef::blue, dqCommon::ColorDef::red,
        dqCommon::ColorDef::green, dqCommon::ColorDef::fromString("yellow"),
    };

    std::vector<RenderGraphic*> graphics;
    std::vector<std::pair<uint32_t, dqCommon::GeometryClass>> featureElems;  // 与 graphics 平行
    m_range = dqGeom::Range3d::CreateNull();

    for (auto const& entry : m_entries) {
        dqCommon::ColorDef const color = kColors[entry.colorIndex % 4];

        // 纹理组合（:74-99：textures=[_,_,tex,tex]、nMaps=[_,nm,_,nm]，ndx=floor(i/4)）
        bool const useTexture = (entry.textureRow == 2 || entry.textureRow == 3) && m_texture != rhi::TextureHandle{};
        bool const useNormalMap = (entry.textureRow == 1 || entry.textureRow == 3) && m_normalMap != rhi::TextureHandle{};
        bool const textured = useTexture || useNormalMap;

        for (auto const& piece : entry.pieces) {
            auto pf = TessellatePiece(vp, piece, textured);
            if (pf.IsNull() || pf->Data().PointCount() == 0)
                continue;
            if (textured)
                ApplyTextureMappingUVs(*pf);

            // PolyfaceGraphic 的颜色解包约定（buildFromPolyface：R=(c>>24)&FF …
            // A=(c>>0)&FF——GltfDecoration.cpp:118-122 同款）。
            dqCommon::ColorComponents const cc = color.getColors();
            uint32_t const rgba =
                (static_cast<uint32_t>(cc.r) << 24) |
                (static_cast<uint32_t>(cc.g) << 16) |
                (static_cast<uint32_t>(cc.b) << 8) |
                static_cast<uint32_t>(255);

            // a_featureId 携带 batch-local 特征下标（参考顶点表 featureIndex——
            // GltfDecoration.cpp:123-134 同机制）。
            uint32_t const featureIndex = static_cast<uint32_t>(graphics.size());
            // 法线图 greenUp 绑定（Surface.ts:546-550——参考 normalMapParams 无
            // scale，greenUp 在 uniform 取负；GltfDecoration 通道同值）。
            float const kNormalMapScale = -1.0f;
            RenderGraphic* graphic = vp.createGraphicFromPolyface(
                pf.Get(), rgba, featureIndex,
                useTexture ? m_texture : rhi::TextureHandle{},
                useNormalMap ? m_normalMap : rhi::TextureHandle{},
                kNormalMapScale);
            if (graphic) {
                graphics.push_back(graphic);
                featureElems.emplace_back(piece.pickId, piece.geomClass);
                for (auto const& p : pf->Data().points)
                    m_range.ExtendPoint(p);
            }
        }
    }

    if (graphics.empty())
        return;

    RenderGraphic* list = vp.createGraphicList(std::move(graphics));

    // Pickable batch：参考每条 builder finish() 产一个单特征 batch
    // （pickable.id → FeatureTable(1)）；DanQing 以单个多特征 batch 承载同一组
    // （featureIndex → elementId 映射等价：BatchState.getElementId 按
    // featureId-batchId 反查表项，Batch.cpp:245-261）。modelId ≠ 任何 feature id
    // （Viewport.isPixelSelectable 拒绝 modelId==elementId，Viewport.ts:2788-2796）。
    if (auto* system = vp.renderSystem()) {
        uint32_t const modelId = NextPickableId();
        auto featureTable = std::make_unique<dqCommon::FeatureTable>(
            static_cast<int>(featureElems.size()), dqBase::DqId(static_cast<uint64_t>(modelId)),
            dqCommon::BatchType::Primary);
        for (size_t i = 0; i < featureElems.size(); ++i) {
            // multi-feature 行 sphere 的 Construction 类（:176）随表项携带。
            featureTable->insertWithIndex(
                dqCommon::Feature(dqBase::DqId(static_cast<uint64_t>(featureElems[i].first)),
                                  dqBase::DqId(), featureElems[i].second),
                static_cast<int>(i));
        }
        m_graphic = system->createBatch(list, featureTable.get(), m_range);
    } else {
        m_graphic = list;
    }
    // Ownership: the frame's decoration set takes the graphic on AddDecoration
    // (Decorations normal list — Viewport's per-frame decoration pipeline owns
    // it; the reference's useCachedDecorations=true contract, :12). The
    // decorator keeps only a non-owning pointer for Decorate/TestDecorationHit.
}

void GeometryDecorator::Decorate(dqApp::DecorateContext& context)
{
    // ← :70-131 + :130-131: the branch as a Scene decoration. Rebuilt per
    // Decorate — the previous frame's graphic belongs to the previous frame's
    // decoration set (the viewport disposes it on the next CollectDecorations).
    if (m_shutdown || !m_viewport)
        return;
    BuildGraphic(*m_viewport);
    if (m_graphic)
        context.AddDecoration(dqRender::GraphicType::Scene, m_graphic);
}

bool GeometryDecorator::TestDecorationHit(uint32_t featureId) const
{
    // ← :87/:164-166：认领全部 20 个 pickable id（参考由 transientIds 集合承载）。
    if (m_shutdown)
        return false;
    for (auto const& entry : m_entries)
        for (auto const& piece : entry.pieces)
            if (piece.pickId == featureId)
                return true;
    return false;
}

QString GeometryDecorator::GetDecorationToolTip(uint32_t) const
{
    return QStringLiteral("Decoration Geometry Example");
}

// ---------------------------------------------------------------------------
// openDecorationGeometryExample
// Ported from: DecorationGeometryExample.ts:188-219
// ---------------------------------------------------------------------------
static rhi::TextureHandle LoadExampleTexture(dqApp::Viewport& vp, char const* file)
{
    std::string const path = std::string(DANQING_SPRITE_ASSETS_DIR "/") + file;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        fprintf(stderr, "[DECgeo] failed to open %s\n", path.c_str());
        return {};
    }
    size_t const size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(size);
    f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    auto decoded = dqCommon::DecodeImage({ bytes, dqCommon::ImageSourceFormat::Jpeg });
    if (!decoded.has_value()) {
        fprintf(stderr, "[DECgeo] failed to decode %s\n", path.c_str());
        return {};
    }
    CreateTextureArgs args{};
    args.imageBuffer = std::move(*decoded);
    rhi::TextureHandle const tex = vp.createTexture(args);
    if (tex == rhi::TextureHandle{}) {
        fprintf(stderr, "[DECgeo] failed to create texture from %s\n", path.c_str());
    } else if (getenv("DANQING_NM_TRACE")) {
        printf("[DECgeo] loaded %s\n", path.c_str());
    }
    return tex;
}

void openDecorationGeometryExample(View3DInventor& view)
{
    dqApp::Viewport* vp = view.getUeViewport();
    if (!vp)
        return;

    // ← Surface.ts:155-163：示例运行在自己 extents 的 blank connection 上
    // （Range3d(-1,-1,-1,13,2,2)）——projectExtents 创建后不可变，Fit/初始取景
    // 都读它；先换绑再装装饰（参考 openBlankConnection → 新 viewer 的语义）。
    view.resetBlankConnection(dqGeom::Range3d(-1, -1, -1, 13, 2, 2),
                              "Decoration Geometry Example");

    // ← :212-216: load brick05baseColor.jpg / brick05normal.jpg and hand both
    // to the decorator (setTextures equivalent — DanQing passes them at
    // construction; the scene has not rendered yet so no invalidation dance
    // is needed).
    rhi::TextureHandle texture = LoadExampleTexture(*vp, "brick05baseColor.jpg");
    rhi::TextureHandle normalMap = LoadExampleTexture(*vp, "brick05normal.jpg");

    // ← :190-191: the decorator is owned by the view (the reference ties its
    // lifetime to iModel.onClose, :49 — DanQing's View3DInventor holds it and
    // drops/disposes it on close, the same discipline as GltfDecoration).
    auto gd = std::make_unique<GeometryDecorator>(*vp, texture, normalMap);
    dqApp::Application::Get().GetViewManager().AddDecorator(gd.get());
    view.setGeoDecorator(std::move(gd));

    auto* view3d = vp->GetView() ? vp->GetView()->AsViewState3d() : nullptr;

    // ← :194-196: setStandardRotation(Iso) + turnCameraOn +
    //   zoomToVolume(iModel.projectExtents)（换绑后 projectExtents 即
    //   (-1,-1,-1,13,2,2)，与参考逐字一致）。
    if (view3d) {
        view3d->SetStandardView(6 /*Iso — StandardViewId mapping, ViewState.h:478*/);
        view3d->EnableCamera();
        if (auto* imodel = vp->GetIModel())
            view3d->LookAtVolume(imodel->GetProjectExtents());
    }

    // ← :198-204: viewFlags = copy({ renderMode: SmoothShade, lighting: true,
    //   visibleEdges: true, whiteOnWhiteReversal: false, backgroundMap: true })
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        dqCommon::ViewFlagsProperties p = style.getViewFlags().Properties();
        p.renderMode = dqCommon::RenderMode::SmoothShade;
        p.lighting = true;
        p.visibleEdges = true;
        p.whiteOnWhiteReversal = false;
        p.backgroundMap = true;
        style.setViewFlags(dqCommon::ViewFlags(p));
    }

    // ← :206-210: environment = clone({ displaySky: true, sky: SkyBox.fromJSON({
    //   twoColor: true, nadirColor: 0xdfefff, zenithColor: 0xffefdf }) })
    {
        auto& style = vp->GetView()->GetDisplayStyle();
        dqCommon::Environment env = style.getEnvironment();
        env.displaySky = true;
        // SkyBox.fromJSON({twoColor, nadirColor, zenithColor}) — DanQing's
        // SkyGradient carries the same fields (SkyBox.h:38-66; colors as tbgr).
        env.sky.gradient.twoColor = true;
        env.sky.gradient.nadirColor = dqCommon::ColorDef::fromTbgr(0xdfefff);
        env.sky.gradient.zenithColor = dqCommon::ColorDef::fromTbgr(0xffefdf);
        style.setEnvironment(env);
    }

    vp->InvalidateRenderPlan();
    vp->RequestRedraw();
}

}  // namespace Gui
