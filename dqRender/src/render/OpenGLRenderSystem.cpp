// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGLRenderSystem implementation
// Ported from: filament filament/src/details/Engine.cpp + itwinjs-core core/frontend/src/render/RenderSystem.ts
//
// Bridges public RenderSystem → internal RenderSystemImpl.
#include "OpenGLRenderSystem.h"
#include "OpenGLRenderTarget.h"
#include "TargetImpl.h"
#include "Graphic.h"
#include "RenderGraphicAdapter.h"
#include "GraphicTemplateImpl.h"
#include "PlanarGridGraphic.h"
#include "PolyfaceGraphic.h"
#include "TextureHandle.h"
#include "ViewportQuadGeometry.h"
#include "PrimitiveBuilder.h"

#include <dqGeom/IndexedPolyface.h>

BEGIN_DQ_RENDER_NAMESPACE

OpenGLRenderSystem::OpenGLRenderSystem(std::unique_ptr<rhi::Driver> driver)
    : m_impl(new RenderSystemImpl(std::move(driver)))
    , m_ownsImpl(true)
{
}

std::unique_ptr<RenderTarget> OpenGLRenderSystem::createTarget(void* /*nativeWindow*/, uint32_t width, uint32_t height)
{
    if (!m_techniques) return nullptr;

    // Create internal target with techniques
    ViewRect rect(0, 0, width, height);
    auto* targetImpl = m_impl->createTarget(rect, *m_techniques);

    // Wrap in OpenGLRenderTarget
    return std::make_unique<OpenGLRenderTarget>(*this, std::unique_ptr<TargetImpl>(targetImpl));
}

std::unique_ptr<GraphicBuilder> OpenGLRenderSystem::createGraphicBuilder(const GraphicBuilderOptions& options)
{
    // PrimitiveBuilder collects geometry and produces RenderGraphics.
    // Ported from: itwinjs-core PrimitiveBuilder.ts. PR F: pass the driver so finish() can upload
    // accumulated meshes via MeshRenderGeometry::create(driver, Mesh).
    return std::make_unique<PrimitiveBuilder>(*this, m_impl->getDriver(), options);
}

GraphicBranch* OpenGLRenderSystem::createBranch(bool ownsEntries)
{
    return new GraphicBranch(ownsEntries);
}

RenderGraphic* OpenGLRenderSystem::createBranchGraphic(GraphicBranch* branch)
{
    // The branch itself is a RenderGraphic
    return branch;
}

RenderGraphic* OpenGLRenderSystem::createGraphicList(std::vector<RenderGraphic*> graphics)
{
    if (graphics.empty())
        return nullptr;
    if (graphics.size() == 1)
        return graphics[0];

    // Wrap in a GraphicBranch (which is a RenderGraphic)
    auto* branch = new GraphicBranch(false);  // don't own — caller manages lifetime
    for (auto* g : graphics)
        branch->add(g);
    return branch;
}

RenderGraphicOwner* OpenGLRenderSystem::createGraphicOwner(RenderGraphic* owned)
{
    if (!owned)
        return nullptr;
    return new RenderGraphicOwner(owned);
}

bool OpenGLRenderSystem::isValid() const noexcept
{
    return m_impl != nullptr;
}

void OpenGLRenderSystem::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: System.collectStatistics (System.ts:255) — the system's
    // live texture walk. DanQing routes through the GL driver's texture
    // accounting (per-texture bytes → RenderMemory Textures consumer).
    if (m_impl)
        m_impl->getDriver().collectTextureStatistics(stats);
}

RenderGraphic* OpenGLRenderSystem::createGridGraphic(float extent, int gridLines)
{
    if (!m_impl) return nullptr;

    // Create the PlanarGridGraphic (CachedGeometry)
    auto* gridGeometry = new PlanarGridGraphic(m_impl->getDriver(), extent, gridLines);

    // Wrap in a Primitive (internal Graphic) which is also a RenderGraphic
    auto* primitive = new Primitive(gridGeometry);
    return primitive;
}

// Create a procedural planar grid from the current view frustum.
// Ported from: itwinjs-core RenderSystem.createPlanarGrid (System.ts:481) →
//               PlanarGridGeometry.create(frustum, grid, system) (PlanarGrid.ts:52).
RenderGraphic* OpenGLRenderSystem::createPlanarGrid(dqCommon::Frustum const& frustum,
                                                    PlanarGridProps const& grid)
{
    if (!m_impl) return nullptr;

    // PlanarGridGraphic builds the frustum∩grid-plane polygon + uploads GPU resources.
    // Faithful per-frame path — the caller (Viewport::CreateScene, mirroring
    // ViewContext.drawStandardGrid) rebuilds whenever the frustum changes.
    auto* gridGeometry = new PlanarGridGraphic(m_impl->getDriver(), frustum, grid);
    return new Primitive(gridGeometry);
}

// Update an existing planar grid's geometry from a new frustum, reusing GPU handles
// (in-place buffer update). Ported from: itwinjs-core ViewContext.drawStandardGrid
// re-gathering the grid decoration each frame (ViewContext.ts:348).
bool OpenGLRenderSystem::updatePlanarGridFrustum(RenderGraphic* grid,
                                                 dqCommon::Frustum const& frustum,
                                                 PlanarGridProps const& gridProps)
{
    if (!grid) return false;
    // grid is a Primitive wrapping a PlanarGridGraphic (created by createPlanarGrid).
    auto* primitive = static_cast<Primitive*>(grid);
    auto* geom = primitive->getGeometry();
    auto* planarGrid = geom ? geom->asPlanarGrid() : nullptr;
    if (!planarGrid) return false;
    planarGrid->updateFrustum(frustum, gridProps);
    return true;
}

// Update the sky sphere's per-frame worldPos (a_worldPos) + eye (u_worldEye) from
// the camera-consistent world frustum. Ported from: itwinjs-core
// SkySphereViewportQuadGeometry worldPos (CachedGeometry.ts:583-596 non-globe /
// :597-651 globe 分支) + SkySphere.ts u_worldEye (:237-265 ortho pseudo-camera /
// :240-250 perspective actual-camera)。
bool OpenGLRenderSystem::updateSkySphere(RenderGraphic* sky,
                                         dqCommon::Frustum const& worldFrustum,
                                         SkySphereGlobeParams const& globe)
{
    if (!sky) return false;
    auto* primitive = static_cast<Primitive*>(sky);
    auto* geom = primitive->getGeometry();
    auto* skyGeom = geom ? geom->asSkySphere() : nullptr;
    if (!skyGeom) return false;
    float worldPos[12];
    float worldEye[3];
    ComputeSkySphereWorldPosAndEye(worldFrustum, worldPos, worldEye, globe);
    skyGeom->setWorldPosAndEye(worldPos, worldEye);
    return true;
}

RenderGraphic* OpenGLRenderSystem::createGraphicFromPolyface(
    void const* polyface, uint32_t defaultColor, uint32_t featureId,
    rhi::TextureHandle texture)
{
    if (!m_impl || !polyface) return nullptr;

    // Cast to the actual type
    auto const& mesh = *static_cast<dqGeom::IndexedPolyface const*>(polyface);
    if (mesh.Data().PointCount() == 0 || mesh.FacetCount() == 0) return nullptr;

    // Create PolyfaceGraphic (CachedGeometry) from the polyface — feature id
    // baked at construction (the reference bakes featureIndex into the vertex
    // table at mesh build, MeshBuilder.ts:142-151).
    auto* polyfaceGeometry = new PolyfaceGraphic(m_impl->getDriver(), mesh, defaultColor, featureId);

    // Surface baseColor texture
    // ← itwinjs-core: SurfaceGeometry.setTexture (texture set at geometry creation)
    if (texture != rhi::TextureHandle{}) {
        polyfaceGeometry->setTexture(texture);
    }

    // Wrap in a Primitive (internal Graphic) which is also a RenderGraphic
    auto* primitive = new Primitive(polyfaceGeometry);
    return primitive;
}

// Ported from: itwinjs-core RenderSystem.createTexture (System.ts) — upload
// decoded RGBA8 via TextureHandle::create2D.
rhi::TextureHandle OpenGLRenderSystem::createTexture(CreateTextureArgs const& args)
{
    if (!m_impl) return {};
    // args.ownership 当前未处理（TODO）
    // img.format 契约为 RGBA8（ImageBuffer 类型文档），不校验
    if (!args.imageBuffer.has_value() || args.imageBuffer->data.empty())
        return {};  // TODO: decode-on-demand from imageData/format (compressed path, 未实现)
    auto const& img = *args.imageBuffer;
    // Wrap dispatch per texture type — 逐字对齐参考:
    // Ported from: itwinjs-core Texture.ts:300 getImageProperties —
    //   wrapMode = (RenderTexture.Type.Normal === type) ? Repeat : ClampToEdge
    // glTF baseColorTexture 走 Normal（glTF 规范默认 wrapS/T=REPEAT）——图集 UV
    // （如 BoxTextured 的 U∈[0,6]）依赖 REPEAT 才能每面平铺采样自己的格子。
    GL::Texture::WrapMode const wrap =
        (RenderTexture::Type::Normal == args.type) ? GL::Texture::WrapMode::Repeat
                                                   : GL::Texture::WrapMode::ClampToEdge;
    TextureHandle tex = TextureHandle::create2D(
        m_impl->getDriver(), static_cast<uint32_t>(img.width),
        static_cast<uint32_t>(img.getHeight()), rhi::TextureFormat::RGBA8,
        img.data.data(), static_cast<uint32_t>(img.data.size()), wrap);
    return tex.getRhiHandle();
}

// ---------------------------------------------------------------------------
// createRenderGraphic — wrap CachedGeometry in a Primitive
// Ported from: itwinjs-core System.createRenderGraphic()
// ---------------------------------------------------------------------------
RenderGraphic* OpenGLRenderSystem::createRenderGraphic(void* cachedGeometry)
{
    if (!cachedGeometry) return nullptr;
    auto* geom = static_cast<CachedGeometry*>(cachedGeometry);
    return new Primitive(geom);
}

// ---------------------------------------------------------------------------
// createBatch — wrap graphic with feature table for picking
// Ported from: itwinjs-core System.createBatch() (System.ts:445-463)
//
// The feature table maps per-vertex feature indices to element ids; the Batch
// owns a copy. (Since TD-21 FeatureTable has deep-copy value semantics; this
// fresh-table re-insert is the equivalent pre-existing form — kept as-is.)
// ---------------------------------------------------------------------------
RenderGraphic* OpenGLRenderSystem::createBatch(RenderGraphic* graphic,
                                                dqCommon::FeatureTable const* featureTable,
                                                dqGeom::Range3d const& range)
{
    if (!graphic) return nullptr;

    std::unique_ptr<dqCommon::FeatureTable> table;
    uint32_t featureCount = 1;
    if (featureTable) {
        auto t = std::make_unique<dqCommon::FeatureTable>(
            featureTable->getMaxFeatures(), featureTable->getModelId(),
            featureTable->getType());
        for (int i = 0; i < featureTable->getArraySize(); ++i) {
            auto const& indexed = featureTable->getArray()[i];
            t->insertWithIndex(indexed.value, indexed.index);
        }
        featureCount = static_cast<uint32_t>(t->getSize());
        table = std::move(t);
    }

    auto* batch = new Batch(featureCount, std::move(table));
    // The child adapts the caller's RenderGraphic — it may be an internal
    // Graphic (Primitive/GraphicsArray, single-mesh path) OR a public
    // GraphicBranch (createGraphicList 的多 mesh 返回，仅继承 RenderGraphic)。
    // 直接 static_cast<Graphic*> 对后者是无效下转型（-fno-rtti 无检查，错误
    // vtable 调用 → AV——多 mesh 模型崩溃的根因）；RenderGraphicAdapter 按
    // isInternalGraphic 判型后正确分派（内部直连 / branch 迭代）。
    batch->setChild(std::make_unique<RenderGraphicAdapter>(graphic));
    batch->setRange(range);
    return batch;
}

// ---------------------------------------------------------------------------
// createSkyBox — create sky box graphic
// Ported from: itwinjs-core System.createSkyBox()
// ---------------------------------------------------------------------------
RenderGraphic* OpenGLRenderSystem::createSkyBox(void const* skyBoxParams)
{
    if (!m_impl || !skyBoxParams) return nullptr;

    // The params is a RenderSkyBoxParams variant (RenderSkyGradientParams /
    // RenderSkySphereParams / RenderSkyCubeParams), discriminated by the leading
    // SkyBoxType field. Ported from: itwinjs-core System.createSkyBox(params).
    auto const type = static_cast<RenderSkyGradientParams const*>(skyBoxParams)->type;
    if (type == SkyBoxType::Gradient) {
        auto const& g = *static_cast<RenderSkyGradientParams const*>(skyBoxParams);
        auto* geom = SkySphereViewportQuadGeometry::createGeometry(g);
        return geom ? new Primitive(geom) : nullptr;
    }

    // Cube/sphere textured sky (texture-based; not used by the blank-connection
    // gradient sky). TODO: wire when textured sky is required.
    return nullptr;
}

// ---------------------------------------------------------------------------
// createGraphicFromTemplate — create a render graphic from a GraphicTemplate.
// Ported from: itwinjs-core RenderSystem.createGraphicFromTemplate()
// NOTE: Full instanced template path is deferred until InstancedGraphicParams
// is exposed through the public API. For now, returns the template's geometry.
// ---------------------------------------------------------------------------
RenderGraphic* OpenGLRenderSystem::createGraphicFromTemplate(
    void const* graphicTemplate,
    void const* /*instancedGraphicParams*/)
{
    if (!graphicTemplate) return nullptr;

    // Cast to the internal template type.
    // GraphicTemplateImpl owns a MeshGraphic; instances are lightweight
    // RenderGraphics sharing its GPU resources (GraphicTemplateImpl.createInstance)。
    auto* tmpl = const_cast<GraphicTemplateImpl*>(
        static_cast<GraphicTemplateImpl const*>(graphicTemplate));

    // TODO: When instancedGraphicParams is provided, create InstancedGeometry
    // wrapping the template's mesh with InstanceBuffers from the transforms.

    // 无实例化参数时返回共享模板网格的轻量实例（createInstance 自行处理空网格）。
    return tmpl->createInstance(nullptr);
}

END_DQ_RENDER_NAMESPACE
