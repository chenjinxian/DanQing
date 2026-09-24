// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderCommands implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/RenderCommands.ts
#include "RenderCommands.h"
#include "Batch.h"
#include "CachedGeometry.h"
#include "Graphic.h"
#include "Matrix.h"
#include "RenderGraphicAdapter.h"
#include "TargetGraphics.h"
#include "TargetImpl.h"

#include <algorithm>
#include <array>
#include <cassert>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// branchEffectiveTransform — fold a Branch's local-to-world (model) transform
// into its mv/mvp for the draw path.
//
// The branch recording sites (pushAndPopBranchInternal / pushAndPopBranchForPass)
// historically pushed getMv()/getMvp() — which stay IDENTITY for branches that use
// setLocalToWorld (PrimitiveBuilder.finish's recentering-recovery branch,
// 1:1 with itwinjs PrimitiveBuilder.ts:122-123 createBranch(branch, transform)).
// itwinjs folds that model transform into the draw mv via BranchUniforms
// (mv = view · currentTransform, BranchUniforms.ts:235). Without the equivalent,
// non-quantized Mesh vertices — pre-transformed relative to the shared range
// center (Mesh ctor sets Point3dList.center = range.Center()) — projected offset
// by the range center, so e.g. the ACS Z-axis PointString tip landed off the disc.
//
// Branches that use setTransform carry identity localToWorld, so this is a no-op
// for them; only setLocalToWorld branches (decoration/batch graphics) are affected.
// ---------------------------------------------------------------------------
// branchEffectiveTransform — 分支命令携带的变换 = 分支的 local-to-world（模型变换）。
//
// 参考机制（BranchState.fromBranch, BranchState.ts:101-104）：BranchState 只合成
// `transform = prev.transform × branch.localToWorldTransform`——**没有 mvp**；mvp
// 在绘制时经 BranchUniforms（view × transform，BranchUniforms.ts:215-226）现算。
// 即分支只贡献 localToWorld；视图矩阵来自分支栈底（setViewportTransform 每帧推的
// 新鲜 viewport mv/mvp，栈 push 相乘语义）。
//
// 旧实现把 branch.getMv()/getMvp()（**分支创建时算好的陈旧矩阵**——对 glTF 装饰
// 是初始俯视）乘上 localToWorld 推入栈——pushState 的替换语义让栈底新鲜 viewport
// mvp 被整个替换 → 视图旋转（StandardViewTool/相机）后渲染矩阵不更新，全标准
// 视图渲染同一初始朝向（2026-09-16 对拍 harness 抓到）。
static void branchEffectiveTransform(Branch const& branch,
                                     std::array<float, 16>& mv,
                                     std::array<float, 16>& mvp)
{
    Matrix4 const ltw = Matrix4::fromTransform(branch.getLocalToWorld());
    for (int i = 0; i < 16; ++i) {
        mv[i]  = ltw.data[i];
        mvp[i] = ltw.data[i];
    }
}

// ---------------------------------------------------------------------------
// Construction / reset
// Ported from: itwinjs-core RenderCommands constructor (line 97-105)
// ---------------------------------------------------------------------------
RenderCommands::RenderCommands(TargetImpl& target, BranchStack& stack,
                               BatchState& batchState)
    : m_target(&target)
    , m_stack(&stack)
    , m_batchState(&batchState)
{
    for (auto& cmds : m_commands)
        cmds.clear();
}

void RenderCommands::reset(TargetImpl& target, BranchStack& stack,
                           BatchState& batchState)
{
    m_target = &target;
    m_stack = &stack;
    m_batchState = &batchState;
    clear();
}

// ---------------------------------------------------------------------------
// addGraphics — iterate a graphic list and call addCommands on each
// Ported from: itwinjs-core RenderCommands.addGraphics() (line 124-128)
// ---------------------------------------------------------------------------
void RenderCommands::addGraphics(std::vector<Graphic*>& scene,
                                 RenderPass forcedPass)
{
    m_forcedRenderPass = forcedPass;
    for (auto* entry : scene) {
        if (entry)
            entry->addCommands(*this);
    }
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addPrimitiveCommand — the core pass routing logic
// Ported from: itwinjs-core RenderCommands.addPrimitiveCommand() (line 203-280)
//
// This is the central routing function that determines which render pass(es)
// a primitive command should be added to.  It handles:
// 1. Forced render pass (background, skybox, overlays)
// 2. Feature-less remapping (move to general opaque to avoid pick data)
// 3. Translucent-as-opaque (for readPixels)
// 4. Double-pass rendering (opaque-translucent, opaque-planar-translucent)
// 5. Opaque/translucent overrides (feature symbology)
// ---------------------------------------------------------------------------
void RenderCommands::addPrimitiveCommand(PrimitiveCommand& command,
                                         GL::Pass pass)
{
    // If pass not specified, get it from the geometry.
    if (pass == GL::Pass::None) {
        auto const* geom = command.getGeometry();
        if (!geom) return;
        pass = geom->getPass();
    }

    // "none" pass means edges not visible — skip entirely.
    if (pass == GL::Pass::None)
        return;

    // 1. Forced render pass: add directly and return.
    if (m_forcedRenderPass != RenderPass::None) {
        m_commands[static_cast<size_t>(m_forcedRenderPass)].push_back(
            std::make_unique<PrimitiveCommand>(command.getGeometry()));
        return;
    }

    // 2. Feature-less remapping: move linear/planar to general opaque
    //    so they don't appear in pick data.
    // Ported from: itwinjs-core addPrimitiveCommand (line 216-228)
    if (!command.getGeometry()->hasFeatures()) {
        switch (pass) {
            case GL::Pass::OpaqueLinear:
            case GL::Pass::OpaquePlanar:
                pass = GL::Pass::Opaque;
                break;
            case GL::Pass::OpaquePlanarTranslucent:
                pass = GL::Pass::OpaqueTranslucent;
                break;
            default:
                break;
        }
    }

    // 3. Translucent-as-opaque: force translucent geometry into opaque pass
    //    (used during readPixels).
    // Ported from: itwinjs-core addPrimitiveCommand (line 231-246)
    bool haveFeatureOverrides = (m_opaqueOverrides || m_translucentOverrides)
                                && command.getGeometry()->hasFeatures();

    if (GL::rendersTranslucent(pass) && m_addTranslucentAsOpaque) {
        auto order = command.getGeometry()->getRenderOrder();
        switch (order) {
            case GL::RenderOrder::PlanarLitSurface:
            case GL::RenderOrder::PlanarUnlitSurface:
            case GL::RenderOrder::BlankingRegion:
                pass = GL::Pass::OpaquePlanar;
                break;
            case GL::RenderOrder::LitSurface:
            case GL::RenderOrder::UnlitSurface:
                pass = GL::Pass::Opaque;
                break;
            default:
                pass = GL::Pass::OpaqueLinear;
                break;
        }
    }

    // 4. Double-pass and opaque/translucent override logic.
    // Ported from: itwinjs-core addPrimitiveCommand (line 248-279)
    bool isDoublePass = GL::rendersOpaqueAndTranslucent(pass);
    bool renderTranslucentDuringOpaque = isDoublePass
        || (m_opaqueOverrides && haveFeatureOverrides);

    if (renderTranslucentDuringOpaque && GL::rendersTranslucent(pass)) {
        // Determine the opaque pass to also render into.
        RenderPass opaquePass;
        if (isDoublePass) {
            opaquePass = GL::toOpaquePass(pass);
        } else {
            auto order = command.getGeometry()->getRenderOrder();
            switch (order) {
                case GL::RenderOrder::PlanarLitSurface:
                case GL::RenderOrder::PlanarUnlitSurface:
                case GL::RenderOrder::BlankingRegion:
                    opaquePass = RenderPass::OpaquePlanar;
                    break;
                case GL::RenderOrder::LitSurface:
                case GL::RenderOrder::UnlitSurface:
                    opaquePass = RenderPass::OpaqueGeneral;
                    break;
                default:
                    opaquePass = RenderPass::OpaqueLinear;
                    break;
            }
        }
        m_commands[static_cast<size_t>(opaquePass)].push_back(
            std::make_unique<PrimitiveCommand>(command.getGeometry()));
    }

    bool renderOpaqueDuringTranslucent = isDoublePass
        || (m_translucentOverrides && haveFeatureOverrides);
    if (renderOpaqueDuringTranslucent && GL::rendersOpaque(pass)
        && !m_addTranslucentAsOpaque) {
        m_commands[static_cast<size_t>(RenderPass::Translucent)].push_back(
            std::make_unique<PrimitiveCommand>(command.getGeometry()));
    }

    // 5. add to the primary pass (unless double-pass already handled both).
    if (!isDoublePass) {
        m_commands[static_cast<size_t>(GL::toRenderPass(pass))].push_back(
            std::make_unique<PrimitiveCommand>(command.getGeometry()));
    }
}

// ---------------------------------------------------------------------------
// addPrimitive — create a PrimitiveCommand and route it
// Ported from: itwinjs-core RenderCommands.addPrimitive() (line 577-602)
// ---------------------------------------------------------------------------
void RenderCommands::addPrimitive(Primitive& prim)
{
    // Frustum culling: skip primitives outside the frustum.
    // Ported from: itwinjs-core RenderCommands.addPrimitive() (line 582-592)
    // Note: Primitive doesn't have a bounding box yet, so we skip culling
    // for individual primitives. Culling is done at the batch level.

    auto command = std::make_unique<PrimitiveCommand>(prim.getGeometry());
    addPrimitiveCommand(*command);

    // Hidden edge pass: in non-wireframe modes with hidden edges enabled,
    // edges also draw in the HiddenEdge pass.
    // Ported from: itwinjs-core addPrimitive (line 597-601)
    if (m_forcedRenderPass == RenderPass::None && prim.isEdge()) {
        auto const& vf = m_target->getCurrentViewFlags();
        if (vf.renderMode != RenderMode::Wireframe && vf.hiddenEdges) {
            m_commands[static_cast<size_t>(RenderPass::HiddenEdge)].push_back(
                std::make_unique<PrimitiveCommand>(prim.getGeometry()));
        }
    }
}

// ---------------------------------------------------------------------------
// addPrimitive (CachedGeometry*) — convenience overload
// Ported from: itwinjs-core RenderCommands.addPrimitive()
// ---------------------------------------------------------------------------
void RenderCommands::addPrimitive(CachedGeometry* geometry)
{
    if (!geometry) return;
    PrimitiveCommand command(geometry);
    addPrimitiveCommand(command);
}

// ---------------------------------------------------------------------------
// addBranch — push/pop branch with child commands
// Ported from: itwinjs-core RenderCommands.addBranch() (line 604-608)
// ---------------------------------------------------------------------------
void RenderCommands::addBranch(Branch& branch)
{
    pushAndPopBranch(branch, [&branch, this]() {
        // Ported from: itwinjs-core RenderCommands.addBranch() (line 604-608)
        // branch.branch.entries.forEach((entry) => entry.addCommands(this));
        // In DanQing, Branch has a single child (not a GraphicBranch.entries list).
        auto* child = branch.getChild();
        if (child)
            child->addCommands(*this);
    });
}

// ---------------------------------------------------------------------------
// addBatch — visibility overrides, culling, hilite routing
// Ported from: itwinjs-core RenderCommands.addBatch() (line 634-701)
// ---------------------------------------------------------------------------
void RenderCommands::addBatch(Batch& batch)
{
    // TEMP-DIAG（拾取 saga，env 门控）
    static bool const s_unbuf = [] { setvbuf(stdout, nullptr, _IONBF, 0); return true; }();
    (void)s_unbuf;
    if (getenv("DANQING_PICK_TRACE")) printf("[PICK] addBatch enter: features=%u table=%d\n",
                                           batch.getFeatureCount(),
                                           batch.getFeatureTable() ? 1 : 0);
    // Locate-only batches only render during readPixels.
    // Ported from: itwinjs-core addBatch (line 635-636)
    if (batch.isLocateOnly() && !m_addTranslucentAsOpaque)
        return;

    // Get overrides from target and check visibility.
    // Ported from: itwinjs-core addBatch (line 647-651)
    if (batch.hasFeatureOverrides()) {
        auto const* lut = batch.getFeatureOverrideLUT();
        if (lut && lut->allHidden())
            return;
    }

    // Frustum culling: skip batches outside the frustum.
    // Ported from: itwinjs-core addBatch (line 655-662)
    if (m_frustumPlanes.isValid()) {
        auto range = dqGeom::Range3d::CreateNull();
        batch.unionRange(range);
        if (!range.isNull() && !m_frustumPlanes.computeBoxContainment(range))
            return;
    }

    // Assign the batch id BEFORE pushing — the reference does it inside
    // BatchState.push(batch, allowAdd=true) (RenderCommands.ts:667). Without
    // this, batch.getBatchId() is 0 here and the PushBatchCommand stores 0;
    // the compositor's PushBatchCommand lookup (findBatch) then fails and the
    // batch stack is never entered (hilite uniforms u_hiliteColor/u_featureOverrides
    // stay unset → the hilite draw renders unrecolored).
    m_batchState->assignBatchId(batch);
    m_batchState->push(batch);

    // Create push/pop commands and route them through the pass system.
    auto push = std::make_unique<PushBatchCommand>(batch.getBatchId());
    auto pop = std::make_unique<PopBatchCommand>();

    pushAndPop(*push, *pop, [&batch, this]() {
        // Set opaque/translucent overrides from feature overrides.
        // Ported from: itwinjs-core addBatch (line 669-677)
        if (batch.hasFeatureOverrides()) {
            m_opaqueOverrides = true;
            m_translucentOverrides = true;
        }

        // add child graphic commands.
        // Ported from: itwinjs-core addBatch (line 685-687)
        auto* child = batch.getChild();
        if (getenv("DANQING_PICK_TRACE")) printf("[PICK] addBatch child=%p\n", (void*)child);
        if (child)
            child->addCommands(*this);
        if (getenv("DANQING_PICK_TRACE")) printf("[PICK] addBatch child done\n");
        // add hilite commands if any features are hilited.
        // Ported from: itwinjs-core addBatch (line 692-696)
        if (child && batch.hasFeatureOverrides()) {
            auto const& lut = batch.getFeatureOverrideLUT();
            if (lut && lut->anyHilited()) {
                child->addHiliteCommands(*this, computeBatchHiliteRenderPass(batch));
            }
        }
    });

    m_opaqueOverrides = false;
    m_translucentOverrides = false;
    m_batchState->pop();
}

// ---------------------------------------------------------------------------
// addBackgroundMapGraphics — add background map to its own pass
// Ported from: itwinjs-core RenderCommands.addBackgroundMapGraphics() (line 131-135)
// ---------------------------------------------------------------------------
void RenderCommands::addBackgroundMapGraphics(
    std::vector<Graphic*>& backgroundMapGraphics)
{
    m_forcedRenderPass = RenderPass::BackgroundMap;
    for (auto* entry : backgroundMapGraphics) {
        if (entry)
            entry->addCommands(*this);
    }
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addOverlayGraphics — add overlays to world overlay pass
// Ported from: itwinjs-core RenderCommands.addOverlayGraphics() (line 137-141)
// ---------------------------------------------------------------------------
void RenderCommands::addOverlayGraphics(
    std::vector<Graphic*>& overlayGraphics)
{
    m_forcedRenderPass = RenderPass::WorldOverlay;
    for (auto* entry : overlayGraphics) {
        if (entry)
            entry->addCommands(*this);
    }
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addDecorations — add decorations to a specific pass
// Ported from: itwinjs-core RenderCommands.addDecorations() (line 143-150)
// ---------------------------------------------------------------------------
void RenderCommands::addDecorations(std::vector<Graphic*>& dec,
                                    RenderPass forcedPass)
{
    m_forcedRenderPass = forcedPass;
    for (auto* entry : dec) {
        if (entry)
            entry->addCommands(*this);
    }
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addWorldDecorations — wrap in WorldDecorations branch
// Ported from: itwinjs-core RenderCommands.addWorldDecorations() (line 152-159)
// ---------------------------------------------------------------------------
void RenderCommands::addWorldDecorations(std::vector<Graphic*>& decs)
{
    if (decs.empty())
        return;

    // Ported from: itwinjs-core RenderCommands.addWorldDecorations (line 152-159)
    // Create a WorldDecorations branch with identity transform and default view flags.
    WorldDecorations world;
    auto arr = std::make_unique<GraphicsArray>();
    for (auto* dec : decs) {
        if (dec)
            arr->addNonOwning(dec);
    }
    world.setChild(std::move(arr));
    addBranch(world);
}

// ---------------------------------------------------------------------------
// addBackground — add view background graphic
// Ported from: itwinjs-core RenderCommands.addBackground() (line 181-190)
// ---------------------------------------------------------------------------
void RenderCommands::addBackground(Graphic* gf)
{
    if (!gf) return;
    assert(m_forcedRenderPass == RenderPass::None);

    m_forcedRenderPass = RenderPass::Background;
    // Ported from: itwinjs-core RenderCommands.addBackground (line 181-190)
    pushAndPopState(m_target->getDecorationsState(), [&gf, this]() {
        gf->addCommands(*this);
    });
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addSkyBox — add sky box graphic
// Ported from: itwinjs-core RenderCommands.addSkyBox() (line 192-201)
// ---------------------------------------------------------------------------
void RenderCommands::addSkyBox(Graphic* gf)
{
    if (!gf) return;
    assert(m_forcedRenderPass == RenderPass::None);

    m_forcedRenderPass = RenderPass::SkyBox;
    // Ported from: itwinjs-core RenderCommands.addSkyBox (line 192-201)
    pushAndPopState(m_target->getDecorationsState(), [&gf, this]() {
        gf->addCommands(*this);
    });
    m_forcedRenderPass = RenderPass::None;
}

// ---------------------------------------------------------------------------
// addHiliteBranch — add hilite commands for a branch
// Ported from: itwinjs-core RenderCommands.addHiliteBranch() (line 297-301)
// ---------------------------------------------------------------------------
void RenderCommands::addHiliteBranch(Branch& branch, RenderPass pass)
{
    pushAndPopBranchForPass(pass, branch, [&branch, pass, this]() {
        // Ported from: itwinjs-core RenderCommands.addHiliteBranch (line 297-301)
        // In itwinjs-core, branch.branch.entries.forEach(entry => entry.addHiliteCommands(this, pass)).
        // In DanQing, Branch has a single child (not a GraphicBranch.entries list).
        auto* child = branch.getChild();
        if (child)
            child->addHiliteCommands(*this, pass);
    });
}

// ---------------------------------------------------------------------------
// computeBatchHiliteRenderPass — determine hilite pass for a batch
// Ported from: itwinjs-core RenderCommands.computeBatchHiliteRenderPass() (line 610-632)
// ---------------------------------------------------------------------------
RenderPass RenderCommands::computeBatchHiliteRenderPass(Batch& /*batch*/)
{
    // Ported from: itwinjs-core computeBatchHiliteRenderPass (line 610-632)
    // If the batch's graphic is a MeshGraphic with VolumeClassifier surface type,
    // return RenderPass::HiliteClassification. Requires VolumeClassifier integration.
    // For now, default to Hilite.
    return RenderPass::Hilite;
}

// ---------------------------------------------------------------------------
// initForRender — initialize commands for normal rendering
// Ported from: itwinjs-core RenderCommands.initForRender() (line 542-575)
// ---------------------------------------------------------------------------
void RenderCommands::initForRender(TargetGraphics& gfx)
{
    clear();

    // Ported from: itwinjs-core RenderCommands.initForRender() (line 542-575)

    // add scene graphics.
    auto foreground = gfx.getForeground();
    if (!foreground.empty())
        addGraphics(foreground);

    auto background = gfx.getBackground();
    if (!background.empty())
        addBackgroundMapGraphics(background);

    auto overlays = gfx.getOverlays();
    if (!overlays.empty())
        addOverlayGraphics(overlays);

    // add dynamics.
    // Note: dynamics are non-owning vectors, need to copy for addGraphics signature.
    auto const& fgDynamics = gfx.getForegroundDynamics();
    if (!fgDynamics.empty()) {
        std::vector<Graphic*> dynamics(fgDynamics.begin(), fgDynamics.end());
        addGraphics(dynamics);
    }

    auto const& ovDynamics = gfx.getOverlayDynamics();
    if (!ovDynamics.empty()) {
        std::vector<Graphic*> dynamics(ovDynamics.begin(), ovDynamics.end());
        addOverlayGraphics(dynamics);
    }

    // add decorations.
    // Note: Decoration GraphicLists are std::vector<RenderGraphic*>, but
    // internal pipeline methods expect std::vector<Graphic*>. We create
    // temporary converted vectors for the method calls.
    auto const* dec = gfx.getDecorations();
    if (dec) {
        addBackground(static_cast<Graphic*>(dec->viewBackground));
        addSkyBox(static_cast<Graphic*>(dec->skyBox));

        if (!dec->normal.empty()) {
            std::vector<Graphic*> normal;
            normal.reserve(dec->normal.size());
            for (auto* rg : dec->normal) {
                if (!rg) continue;
                if (getenv("DANQING_GLTF_TRACE")) printf("[RGC] normal elem isBranch=%d ptr=%p\n", rg->isBranch()?1:0, (void*)rg);
                if (rg->isBranch()) {
                    // GraphicBranch（仅继承 RenderGraphic，非 Graphic）：裸 cast 会
                    // 类型双关（-fno-rtti 无检查）→ SEH。经 adapter 桥接遍历 entries。
                    normal.push_back(new RenderGraphicAdapter(rg));
                } else {
                    // Primitive 等内部 Graphic（继承 Graphic）——裸 cast 合法（原路径）。
                    normal.push_back(static_cast<Graphic*>(rg));
                }
            }
            addGraphics(normal);
            // adapter 是即时桥（addGraphics 同步执行 addCommands，命令记录内部
            // graphic 而非 adapter）——但 delete 时机契约未完全验证，保守泄漏
            // （每帧数个小对象），独立 pass 再定所有权。
        }

        if (!dec->world.empty()) {
            std::vector<Graphic*> world;
            world.reserve(dec->world.size());
            for (auto* rg : dec->world)
                world.push_back(static_cast<Graphic*>(rg));
            addWorldDecorations(world);
        }

        pushAndPopState(m_target->getDecorationsState(), [&dec, this]() {
            if (!dec->viewOverlay.empty()) {
                std::vector<Graphic*> viewOverlay;
                viewOverlay.reserve(dec->viewOverlay.size());
                for (auto* rg : dec->viewOverlay)
                    viewOverlay.push_back(static_cast<Graphic*>(rg));
                addDecorations(viewOverlay, RenderPass::ViewOverlay);
            }
            if (!dec->worldOverlay.empty()) {
                std::vector<Graphic*> worldOverlay;
                worldOverlay.reserve(dec->worldOverlay.size());
                for (auto* rg : dec->worldOverlay)
                    worldOverlay.push_back(static_cast<Graphic*>(rg));
                addDecorations(worldOverlay, RenderPass::WorldOverlay);
            }
        });
    }

    setupClassificationByVolume();
}

// ---------------------------------------------------------------------------
// initForReadPixels — initialize for readPixels (translucent as opaque)
// Ported from: itwinjs-core RenderCommands.initForReadPixels() (line 520-540)
// ---------------------------------------------------------------------------
void RenderCommands::initForReadPixels(TargetGraphics& gfx)
{
    clear();

    // Force translucent geometry into opaque pass.
    // Ported from: itwinjs-core RenderCommands.initForReadPixels() (line 520-540)
    m_addTranslucentAsOpaque = true;

    // add scene foreground graphics.
    auto foreground = gfx.getForeground();
    if (!foreground.empty())
        addGraphics(foreground);

    // add pickable decorations only（参考 :529-531 — 拾取路径不含非 pickable
    // 装饰：网格（PlanarGridGeometry 非 pickable）不进拾取缓冲；ACS 取放点
    // （pickable 接线后）经此入列）。
    auto const* dec = gfx.getDecorations();
    if (dec) {
        if (!dec->normal.empty()) {
            for (auto* rg : dec->normal) {
                auto* gf = static_cast<Graphic*>(rg);
                if (gf && gf->isPickable())
                    gf->addCommands(*this);
            }
        }
        if (!dec->world.empty()) {
            std::vector<Graphic*> world;
            world.reserve(dec->world.size());
            for (auto* rg : dec->world) {
                auto* gf = static_cast<Graphic*>(rg);
                if (gf && gf->isPickable())
                    world.push_back(gf);
            }
            addWorldDecorations(world);
        }
    }

    // Background map is also pickable.
    auto background = gfx.getBackground();
    if (!background.empty())
        addBackgroundMapGraphics(background);

    m_addTranslucentAsOpaque = false;

    setupClassificationByVolume();
}

// ---------------------------------------------------------------------------
// initForPickOverlays — initialize for overlay picking
// Ported from: itwinjs-core RenderCommands.initForPickOverlays() (line 497-518)
// ---------------------------------------------------------------------------
void RenderCommands::initForPickOverlays(
    std::vector<Graphic*>& sceneOverlays,
    std::vector<Graphic*>* worldOverlayDecorations,
    std::vector<Graphic*>* viewOverlayDecorations)
{
    clearCommands();

    m_addTranslucentAsOpaque = true;

    for (auto* gf : sceneOverlays) {
        if (gf)
            gf->addCommands(*this);
    }

    // Ported from: itwinjs-core initForPickOverlays (line 505-515)
    if (worldOverlayDecorations && !worldOverlayDecorations->empty()) {
        pushAndPopState(m_target->getDecorationsState(), [&]() {
            for (auto* gf : *worldOverlayDecorations) {
                if (gf) gf->addCommands(*this);
            }
        });
    }

    if (viewOverlayDecorations && !viewOverlayDecorations->empty()) {
        pushAndPopState(m_target->getDecorationsState().withViewCoords(), [&]() {
            for (auto* gf : *viewOverlayDecorations) {
                if (gf) gf->addCommands(*this);
            }
        });
    }

    m_addTranslucentAsOpaque = false;
}

// ---------------------------------------------------------------------------
// pushAndPopBranch — push/pop with animation check
// Ported from: itwinjs-core RenderCommands.pushAndPopBranch() (line 449-458)
// ---------------------------------------------------------------------------
void RenderCommands::pushAndPopBranch(Branch& branch, std::function<void()> func)
{
    // Ported from: itwinjs-core RenderCommands.pushAndPopBranch() (line 451-455)
    // Animation branch state: omit branches that are not visible in the
    // current animation frame, and clip branches that have animation clips.
    // Requires the animation system to be ported from itwinjs-core.

    pushAndPopBranchInternal(branch, std::move(func));
}

// ---------------------------------------------------------------------------
// pushAndPopBranchForPass — push/pop for a specific render pass
// Ported from: itwinjs-core RenderCommands.pushAndPopBranchForPass() (line 357-394)
// ---------------------------------------------------------------------------
void RenderCommands::pushAndPopBranchForPass(RenderPass pass, Branch& branch,
                                              std::function<void()> func)
{
    // Ported from: itwinjs-core pushAndPopBranchForPass (line 360-362)
    // Animation state check: omit branches not visible in current frame.
    // Requires animation system to be ported.

    assert(pass != RenderPass::None);

    std::array<float, 16> mv, mvp;
    branchEffectiveTransform(branch, mv, mvp);
    m_stack->pushTransform(mv.data(), mvp.data());
    // Ported from: itwinjs-core pushAndPopBranchForPass (line 368-371)
    // Planar classifier push: if the branch has a planar classifier,
    // push it onto the classifier stack for classified rendering.
    // Requires PlanarClassifier type to be ported.

    auto& cmds = m_commands[static_cast<size_t>(pass)];

    // Ported from: itwinjs-core pushAndPopBranchForPass (line 374-377)
    // Animation clip: if the branch has an animation clip, wrap commands
    // in a PushClip/PopClip pair. Requires animation system.

    auto push = std::make_unique<PushBranchCommand>();
    push->setTransform(mv.data(), mvp.data());
    if (branch.hasViewFlags())
        push->setViewFlags(branch.getViewFlags());
    cmds.push_back(std::move(push));

    func();

    m_stack->pop();

    // Check if push is still the last command (no commands added between push/pop).
    // Ported from: itwinjs-core pushAndPopBranchForPass (line 385-393)
    // Note: we need to compare by pointer identity, same as itwinjs.
    // For now, always add pop (conservative approach).
    cmds.push_back(std::make_unique<PopBranchCommand>());
}

// ---------------------------------------------------------------------------
// pushAndPopState — push/pop a BranchState
// Ported from: itwinjs-core RenderCommands.pushAndPopState() (line 473-477)
// ---------------------------------------------------------------------------
void RenderCommands::pushAndPopState(BranchState const& state,
                                      std::function<void()> func)
{
    m_stack->pushState(state);
    auto push = std::make_unique<PushStateCommand>(state);
    auto pop = std::make_unique<PopBranchCommand>();
    pushAndPop(*push, *pop, std::move(func));
    m_stack->pop();
}

// ---------------------------------------------------------------------------
// pushAndPop — generic push/pop with callback
// Ported from: itwinjs-core RenderCommands.pushAndPop() (line 396-447)
//
// Adds push command to all passes, executes callback, then for each pass:
// - If push is still the last command (nothing was added), remove it.
// - Otherwise, add the pop command.
// ---------------------------------------------------------------------------
void RenderCommands::pushAndPop(DrawCommand& push, DrawCommand& pop,
                                 std::function<void()> func)
{
    if (isDrawingLayers()) {
        // Ported from: itwinjs-core pushAndPop (line 397-407)
        // Layer handling: when drawing layers, add commands to the layer
        // list instead of the pass lists. Requires Layer system to be ported.
        return;
    }

    if (m_forcedRenderPass == RenderPass::None) {
        // add push to all passes.
        for (auto& cmds : m_commands) {
            // We need to clone the push command for each pass.
            // Use the type to create appropriate clones.
            switch (push.getType()) {
                case DrawCommandType::PushBranch: {
                    auto& src = static_cast<PushBranchCommand&>(push);
                    auto clone = std::make_unique<PushBranchCommand>();
                    clone->setTransform(src.getMv(), src.getMvp());
                    if (src.hasViewFlags())
                        clone->setViewFlags(src.getViewFlags());
                    cmds.push_back(std::move(clone));
                    break;
                }
                case DrawCommandType::PushBatch: {
                    auto& src = static_cast<PushBatchCommand&>(push);
                    cmds.push_back(std::make_unique<PushBatchCommand>(src.getBatchId()));
                    break;
                }
                case DrawCommandType::PushClip: {
                    cmds.push_back(std::make_unique<PushClipCommand>());
                    break;
                }
                case DrawCommandType::PushState: {
                    auto& src = static_cast<PushStateCommand&>(push);
                    cmds.push_back(std::make_unique<PushStateCommand>(src.getState()));
                    break;
                }
                default:
                    break;
            }
        }
    } else {
        // add push only to forced pass and hilite pass.
        auto addPushToPass = [&](RenderPass p) {
            auto& cmds = m_commands[static_cast<size_t>(p)];
            switch (push.getType()) {
                case DrawCommandType::PushBranch: {
                    auto& src = static_cast<PushBranchCommand&>(push);
                    auto clone = std::make_unique<PushBranchCommand>();
                    clone->setTransform(src.getMv(), src.getMvp());
                    if (src.hasViewFlags())
                        clone->setViewFlags(src.getViewFlags());
                    cmds.push_back(std::move(clone));
                    break;
                }
                case DrawCommandType::PushBatch: {
                    auto& src = static_cast<PushBatchCommand&>(push);
                    cmds.push_back(std::make_unique<PushBatchCommand>(src.getBatchId()));
                    break;
                }
                case DrawCommandType::PushClip: {
                    cmds.push_back(std::make_unique<PushClipCommand>());
                    break;
                }
                case DrawCommandType::PushState: {
                    auto& src = static_cast<PushStateCommand&>(push);
                    cmds.push_back(std::make_unique<PushStateCommand>(src.getState()));
                    break;
                }
                default:
                    break;
            }
        };
        addPushToPass(m_forcedRenderPass);
        addPushToPass(RenderPass::Hilite);
    }

    func();

    // For each pass: remove push if nothing was added, otherwise add pop.
    // Ported from: itwinjs-core pushAndPop (line 422-446)
    if (m_forcedRenderPass == RenderPass::None) {
        for (auto& cmds : m_commands) {
            if (cmds.empty()) continue;
            // Check if the last command is a push of the same type.
            // If so, nothing was added between push and pop — remove push.
            bool lastIsPush = false;
            switch (push.getType()) {
                case DrawCommandType::PushBranch:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushBranch;
                    break;
                case DrawCommandType::PushBatch:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushBatch;
                    break;
                case DrawCommandType::PushClip:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushClip;
                    break;
                case DrawCommandType::PushState:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushState;
                    break;
                default:
                    break;
            }

            if (lastIsPush) {
                cmds.pop_back();
            } else {
                // add the pop command.
                switch (pop.getType()) {
                    case DrawCommandType::PopBranch:
                        cmds.push_back(std::make_unique<PopBranchCommand>());
                        break;
                    case DrawCommandType::PopBatch:
                        cmds.push_back(std::make_unique<PopBatchCommand>());
                        break;
                    case DrawCommandType::PopClip:
                        cmds.push_back(std::make_unique<PopClipCommand>());
                        break;
                    default:
                        break;
                }
            }
        }
    } else {
        // Same logic for forced pass and hilite pass.
        auto cleanPass = [&](RenderPass p) {
            auto& cmds = m_commands[static_cast<size_t>(p)];
            if (cmds.empty()) return;

            bool lastIsPush = false;
            switch (push.getType()) {
                case DrawCommandType::PushBranch:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushBranch;
                    break;
                case DrawCommandType::PushBatch:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushBatch;
                    break;
                case DrawCommandType::PushClip:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushClip;
                    break;
                case DrawCommandType::PushState:
                    lastIsPush = cmds.back()->getType() == DrawCommandType::PushState;
                    break;
                default:
                    break;
            }

            if (lastIsPush) {
                cmds.pop_back();
            } else {
                switch (pop.getType()) {
                    case DrawCommandType::PopBranch:
                        cmds.push_back(std::make_unique<PopBranchCommand>());
                        break;
                    case DrawCommandType::PopBatch:
                        cmds.push_back(std::make_unique<PopBatchCommand>());
                        break;
                    case DrawCommandType::PopClip:
                        cmds.push_back(std::make_unique<PopClipCommand>());
                        break;
                    default:
                        break;
                }
            }
        };
        cleanPass(m_forcedRenderPass);
        cleanPass(RenderPass::Hilite);
    }
}

// ---------------------------------------------------------------------------
// pushAndPopBranchInternal — push branch state and add commands
// Ported from: itwinjs-core RenderCommands._pushAndPopBranch() (line 460-471)
// ---------------------------------------------------------------------------
void RenderCommands::pushAndPopBranchInternal(Branch& branch,
                                               std::function<void()> func)
{
    std::array<float, 16> mv, mvp;
    branchEffectiveTransform(branch, mv, mvp);
    m_stack->pushTransform(mv.data(), mvp.data());
    // Ported from: itwinjs-core _pushAndPopBranch (line 463-466)
    // Planar classifier push: if the branch has a planar classifier,
    // push it onto the classifier stack. Requires PlanarClassifier type.

    auto push = std::make_unique<PushBranchCommand>();
    push->setTransform(mv.data(), mvp.data());
    if (branch.hasViewFlags())
        push->setViewFlags(branch.getViewFlags());
    auto pop = std::make_unique<PopBranchCommand>();
    pushAndPop(*push, *pop, std::move(func));

    m_stack->pop();
}

// ---------------------------------------------------------------------------
// setupClassificationByVolume — split classification commands
// Ported from: itwinjs-core RenderCommands.setupClassificationByVolume() (line 708-740)
// ---------------------------------------------------------------------------
void RenderCommands::setupClassificationByVolume()
{
    auto& groupedCmds = m_commands[static_cast<size_t>(RenderPass::Classification)];
    auto& byIndexCmds = m_commands[static_cast<size_t>(RenderPass::ClassificationByIndex)];

    // Track current push commands for building per-index groups.
    std::vector<DrawCommand*> pushCommands;

    for (auto& cmd : groupedCmds) {
        switch (cmd->getType()) {
            case DrawCommandType::PushBranch:
            case DrawCommandType::PushBatch:
            case DrawCommandType::PushState:
                pushCommands.push_back(cmd.get());
                break;
            case DrawCommandType::Primitive: {
                // add all pushes, then the primitive, then matching pops.
                for (auto* pushCmd : pushCommands) {
                    // Clone the push command.
                    switch (pushCmd->getType()) {
                        case DrawCommandType::PushBranch: {
                            auto& src = *static_cast<PushBranchCommand*>(pushCmd);
                            auto clone = std::make_unique<PushBranchCommand>();
                            clone->setTransform(src.getMv(), src.getMvp());
                            if (src.hasViewFlags())
                                clone->setViewFlags(src.getViewFlags());
                            byIndexCmds.push_back(std::move(clone));
                            break;
                        }
                        case DrawCommandType::PushBatch: {
                            auto& src = *static_cast<PushBatchCommand*>(pushCmd);
                            byIndexCmds.push_back(
                                std::make_unique<PushBatchCommand>(src.getBatchId()));
                            break;
                        }
                        case DrawCommandType::PushState: {
                            auto& src = *static_cast<PushStateCommand*>(pushCmd);
                            byIndexCmds.push_back(
                                std::make_unique<PushStateCommand>(src.getState()));
                            break;
                        }
                        default:
                            break;
                    }
                }
                byIndexCmds.push_back(std::make_unique<PrimitiveCommand>(
                    static_cast<PrimitiveCommand*>(cmd.get())->getGeometry()));
                // add matching pops in reverse order.
                for (int i = static_cast<int>(pushCommands.size()) - 1; i >= 0; --i) {
                    if (pushCommands[i]->getType() == DrawCommandType::PushBatch)
                        byIndexCmds.push_back(std::make_unique<PopBatchCommand>());
                    else
                        byIndexCmds.push_back(std::make_unique<PopBranchCommand>());
                }
                break;
            }
            case DrawCommandType::PopBatch:
            case DrawCommandType::PopBranch:
                if (!pushCommands.empty())
                    pushCommands.pop_back();
                break;
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Command buffer access
// ---------------------------------------------------------------------------

std::vector<std::unique_ptr<DrawCommand>> const&
RenderCommands::getCommands(RenderPass pass) const
{
    return m_commands[static_cast<size_t>(pass)];
}

std::vector<std::unique_ptr<DrawCommand>>&
RenderCommands::getCommands(RenderPass pass)
{
    return m_commands[static_cast<size_t>(pass)];
}

void RenderCommands::replaceCommands(
    RenderPass pass, std::vector<std::unique_ptr<DrawCommand>>& cmds)
{
    auto idx = static_cast<size_t>(pass);
    m_commands[idx].swap(cmds);
}

void RenderCommands::clear()
{
    clearCommands();
}

void RenderCommands::clearCommands()
{
    for (auto& cmds : m_commands)
        cmds.clear();
    // Ported from: itwinjs-core RenderCommands._clearCommands() (line 484-487)
    // m_layers.clear() — requires Layer system to be ported.
}

size_t RenderCommands::getCommandCount() const
{
    size_t total = 0;
    for (auto const& cmds : m_commands)
        total += cmds.size();
    return total;
}

// Ported from: itwinjs-core RenderCommands.dump() (RenderCommands.ts:742-771).
RenderCommands::CommandCount RenderCommands::dump() const
{
    CommandCount out;
    for (auto const& cmds : m_commands) {
        for (auto const& cmd : cmds) {
            if (!cmd) continue;
            switch (cmd->getType()) {
                case DrawCommandType::Primitive:  ++out.primitives; break;
                case DrawCommandType::PushBatch:  ++out.batches;    break;
                case DrawCommandType::PushBranch: ++out.branches;   break;
                default: break;
            }
        }
    }
    return out;
}

bool RenderCommands::isEmpty() const
{
    for (auto const& cmds : m_commands)
        if (!cmds.empty()) return false;
    return true;
}

bool RenderCommands::hasCommands(RenderPass pass) const
{
    auto idx = static_cast<size_t>(pass);
    if (idx >= kPassCount) return false;
    return !m_commands[idx].empty();
}

bool RenderCommands::isDrawingLayers() const
{
    switch (m_forcedRenderPass) {
        case RenderPass::OpaqueLayers:
        case RenderPass::TranslucentLayers:
        case RenderPass::OverlayLayers:
            return true;
        default:
            return false;
    }
}

uint8_t RenderCommands::getCompositeFlags() const
{
    uint8_t flags = 0;
    if (hasCommands(RenderPass::Translucent))
        flags |= static_cast<uint8_t>(GL::CompositeFlags::Translucent);
    if (hasCommands(RenderPass::Hilite)
        || hasCommands(RenderPass::HiliteClassification)
        || hasCommands(RenderPass::HilitePlanarClassification))
        flags |= static_cast<uint8_t>(GL::CompositeFlags::Hilite);
    // Ported from: itwinjs-core RenderCommands.compositeFlags (line 86-89)
    if (m_target->wantAmbientOcclusion())
        flags |= static_cast<uint8_t>(GL::CompositeFlags::AmbientOcclusion);
    return flags;
}

bool RenderCommands::isOpaquePass(RenderPass pass)
{
    return pass >= RenderPass::OpaqueLinear
        && pass <= RenderPass::OpaqueGeneral;
}

void RenderCommands::setCheckRange(float const* mvp16)
{
    // Ported from: itwinjs-core RenderCommands.setCheckRange() (line 704)
    m_frustumPlanes = FrustumPlanes(mvp16);
}

void RenderCommands::clearCheckRange()
{
    // Ported from: itwinjs-core RenderCommands.clearCheckRange() (line 706)
    m_frustumPlanes.invalidate();
}

END_DQ_RENDER_NAMESPACE
