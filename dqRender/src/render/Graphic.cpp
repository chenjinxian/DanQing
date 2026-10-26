// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Scene graph node implementations
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Graphic.ts
#include "Graphic.h"
#include "CachedGeometry.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// Graphic — default addHiliteCommands
// Ported from: itwinjs-core Graphic.ts addHiliteCommands (line 38)
// ---------------------------------------------------------------------------
void Graphic::addHiliteCommands(RenderCommands& /*commands*/, RenderPass /*pass*/)
{
    // Default: do nothing. Only pickable graphics override this.
}

// ---------------------------------------------------------------------------
// Primitive
// ---------------------------------------------------------------------------
Primitive::~Primitive()
{
    if (m_ownsGeometry && m_geometry) {
        delete m_geometry;
        m_geometry = nullptr;
    }
}

void Primitive::addCommands(RenderCommands& commands)
{
    commands.addPrimitive(*this);
}

void Primitive::addHiliteCommands(RenderCommands& commands, RenderPass pass)
{
    // Ported from: itwinjs-core Primitive.ts addHiliteCommands (line 73-79)
    // add to hilite pass if not an edge (edges don't hilite).
    //
    // The caller's pass (RenderPass::Hilite from RenderCommands.addBatch) is a
    // RenderPass — addPrimitiveCommand takes a GL::Pass, and RenderPass::Hilite
    // has no GL::Pass equivalent (hilite is a compositor pass, not a geometry
    // class). Push the command DIRECTLY into the target pass bucket — the
    // forced-pass branch of addPrimitiveCommand routes by m_forcedRenderPass,
    // so we can't reach the Hilite bucket through it without a pass mapping.
    if (!isEdge()) {
        PrimitiveCommand cmd(m_geometry);
        commands.addHilitePrimitiveCommand(cmd, pass);
    }
}

// ---------------------------------------------------------------------------
// Primitive accessors — delegate to CachedGeometry
// Ported from: itwinjs-core Primitive.ts (line 42-68)
// ---------------------------------------------------------------------------
bool Primitive::hasFeatures() const
{
    // Ported from: itwinjs-core Primitive.ts hasFeatures (line 42)
    return m_geometry ? m_geometry->hasFeatures() : false;
}

bool Primitive::hasAnimation() const
{
    // Ported from: itwinjs-core Primitive.ts hasAnimation (line 45)
    // Animation not yet implemented; returns false.
    return false;
}

bool Primitive::isInstanced() const
{
    // Ported from: itwinjs-core Primitive.ts isInstanced (line 48)
    return m_geometry ? m_geometry->isInstanced() : false;
}

bool Primitive::isLit() const
{
    // Ported from: itwinjs-core Primitive.ts isLit (line 51)
    // Surface technique with SurfaceType != Unknown is lit.
    if (!m_geometry) return false;
    auto tech = m_geometry->getTechniqueId();
    if (tech == TechniqueId::Surface) {
        // SurfaceGeometry::isLit() checks surfaceType != Unknown.
        // Since we only have CachedGeometry*, we assume Surface is lit
        // (SurfaceGeometry always sets a SurfaceType).
        return true;
    }
    if (tech == TechniqueId::RealityMesh)
        return true;
    return false;
}

bool Primitive::isEdge() const
{
    // Ported from: itwinjs-core Primitive.ts isEdge (line 54)
    if (!m_geometry) return false;
    auto tech = m_geometry->getTechniqueId();
    return tech == TechniqueId::Edge
        || tech == TechniqueId::IndexedEdge
        || tech == TechniqueId::SilhouetteEdge;
}

uint8_t Primitive::getRenderOrder() const
{
    // Ported from: itwinjs-core Primitive.ts renderOrder (line 57)
    return m_geometry ? static_cast<uint8_t>(m_geometry->getRenderOrder()) : 0;
}

TechniqueId Primitive::getTechniqueId() const
{
    // Ported from: itwinjs-core Primitive.ts techniqueId (line 60)
    return m_geometry ? m_geometry->getTechniqueId() : TechniqueId::Surface;
}

// ---------------------------------------------------------------------------
// Branch — addCommands
// Ported from: itwinjs-core Graphic.ts Branch.addCommands (line 409-417)
//
// Delegates to RenderCommands.addBranch() which handles push/pop and
// animation state.  This matches itwinjs-core where Branch.addCommands
// calls commands.addBranch(this).
// ---------------------------------------------------------------------------
void Branch::addCommands(RenderCommands& commands)
{
    if (!shouldAddCommands())
        return;

    commands.addBranch(*this);
}

// ---------------------------------------------------------------------------
// Branch — addHiliteCommands
// Ported from: itwinjs-core Graphic.ts Branch.addHiliteCommands (line 419-427)
//
// Delegates to RenderCommands.addHiliteBranch() which handles push/pop.
// ---------------------------------------------------------------------------
void Branch::addHiliteCommands(RenderCommands& commands, RenderPass pass)
{
    if (!shouldAddCommands())
        return;

    commands.addHiliteBranch(*this, pass);
}

// ---------------------------------------------------------------------------
// Branch — shouldAddCommands
// Ported from: itwinjs-core Graphic.ts Branch.shouldAddCommands (line 429-436)
// ---------------------------------------------------------------------------
bool Branch::shouldAddCommands() const
{
    // Animation node filtering: if this branch has an animation node ID,
    // it should only be drawn when the animation system matches.
    // For now, always return true (animation system not yet implemented).
    return true;
}

// ---------------------------------------------------------------------------
// Primitive — unionRange
// Ported from: itwinjs-core Primitive.ts unionRange (line 76-78)
//   range.extendRange(this.cachedGeometry.computeRange())
// computeRange = 量化解码域（qOrigin .. qOrigin+0xFFFF*qScale，
// CachedGeometry.ts:170-184）。参考的全部几何恒量化（qparams 必在）；
// DanQing 的 VBO 形（PolyfaceGraphic——glTF/canvas 装饰）无 qparams——
// 守卫缺席即不并入（不造 (0,0,0) 幻影点；域缺席由 GltfDecoration 的
// GetGraphicRange workaround 承载，GltfDecoration.cpp 同源登记）。
// ---------------------------------------------------------------------------
void Primitive::unionRange(dqGeom::Range3d& range) const
{
    if (!m_geometry)
        return;
    float const* qo = m_geometry->getQOrigin();
    float const* qs = m_geometry->getQScale();
    if (qo && qs) {
        range.ExtendPoint(dqGeom::Point3d::From(qo[0], qo[1], qo[2]));
        range.ExtendPoint(dqGeom::Point3d::From(
            qo[0] + qs[0] * 65535.0f,
            qo[1] + qs[1] * 65535.0f,
            qo[2] + qs[2] * 65535.0f));
    }
}

// ---------------------------------------------------------------------------
// Branch — unionRange
// Ported from: itwinjs-core Graphic.ts Branch.unionRange (line 409-416)
//   子域收集 → localToWorldTransform.multiplyRange → 并入 range。
// 1:1：变换恒走 localToWorld（参考 Branch 无 mv/mvp 存储面——那些在
// PushBranchCommand 绘制期推导；此前 DanQing 的 m_mv 8 角形 + l2w 分叉
// 为自创等价形，2026-10-26 对参考定案后废弃）。
// ---------------------------------------------------------------------------
void Branch::unionRange(dqGeom::Range3d& range) const
{
    if (!m_child)
        return;
    dqGeom::Range3d thisRange = dqGeom::Range3d::CreateNull();
    m_child->unionRange(thisRange);
    range.ExtendRange(m_localToWorld.MultiplyRange(thisRange));
}

// ---------------------------------------------------------------------------
// Branch — isPickable
// Ported from: itwinjs-core Graphic.ts Branch.isPickable (line 392-395)
// ---------------------------------------------------------------------------
bool Branch::isPickable() const
{
    return m_child ? m_child->isPickable() : false;
}

// ---------------------------------------------------------------------------
// Branch — collectStatistics
// Ported from: itwinjs-core Graphic.ts Branch.collectStatistics
// ---------------------------------------------------------------------------
void Branch::collectStatistics(RenderMemory::Statistics& stats) const
{
    if (m_child) m_child->collectStatistics(stats);
}

// ---------------------------------------------------------------------------
// AnimationTransformBranch — addCommands
// Ported from: itwinjs-core Graphic.ts AnimationTransformBranch.addCommands
// ---------------------------------------------------------------------------
void AnimationTransformBranch::addCommands(RenderCommands& commands)
{
    // Ported from: itwinjs-core Graphic.ts AnimationTransformBranch.addCommands
    // Animation transform node ID is stored for the animation system.
    // When the animation system is fully implemented, this will set
    // target.currentAnimationTransformNodeId = m_nodeId before adding
    // commands and reset it after.
    // For now, the node ID is preserved for future animation support.
    (void)m_nodeId;
    if (m_graphic) m_graphic->addCommands(commands);
}

void AnimationTransformBranch::addHiliteCommands(RenderCommands& commands, RenderPass pass)
{
    if (m_graphic) m_graphic->addHiliteCommands(commands, pass);
}

// ---------------------------------------------------------------------------
// WorldDecorations
// Ported from: itwinjs-core Graphic.ts WorldDecorations (line 484-500)
// ---------------------------------------------------------------------------
WorldDecorations::WorldDecorations()
{
    // Set identity transform.
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    setTransform(identity, identity);

    // Ported from: itwinjs-core Target.getWorldDecorations (Target.ts:236-243):
    //   "Don't allow flags like monochrome etc to affect world decorations.
    //    Allow lighting in 3d only."（DanQing 恒 3d——无 2d 视口 → lighting=true）
    ViewFlags flags;
    flags.renderMode = RenderMode::SmoothShade;
    flags.clipVolume = false;
    flags.whiteOnWhiteReversal = false;
    flags.lighting = true;   // !this.is2d（Target.ts:241）
    flags.shadows = false;
    setViewFlags(flags);
    // 参考 WorldDecorations ctor（Graphic.ts:488-491）另置 branch.symbologyOverrides
    // = 空 Overrides + ignoreSubCategory=true——DanQing 无 branch 级 FeatureSymbology
    // 管线，网格/ACS 取放点均无 feature，此处仅注释登记。
}

void WorldDecorations::init(std::vector<std::unique_ptr<Graphic>> decorations)
{
    auto arr = std::make_unique<GraphicsArray>();
    for (auto& dec : decorations) {
        arr->add(std::move(dec));
    }
    setChild(std::move(arr));
}

END_DQ_RENDER_NAMESPACE
