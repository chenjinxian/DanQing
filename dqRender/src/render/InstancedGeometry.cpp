// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instanced geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/InstancedGeometry.ts
#include "InstancedGeometry.h"
#include "AttributeMap.h"
#include "PlanarGridGraphic.h"
#include "SurfaceGeometry.h"
#include "IndexedEdgeGeometry.h"
#include "RealityMeshGeometry.h"

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// InstancedGeometry
// (Ported from: itwinjs-core InstancedGeometry.ts, line 345-470)
// ===========================================================================

InstancedGeometry::InstancedGeometry(CachedGeometry* repr, InstanceBuffers* buffers)
    : m_repr(repr)
    , m_buffers(buffers)
{
}

InstancedGeometry::~InstancedGeometry()
{
    delete m_buffers;
}

// --- Forwarding accessors (Ported from: itwinjs-core line 357-362) ---

// Forwarding accessors — forward to repr geometry.
// Ported from: itwinjs-core InstancedGeometry.ts line 357-362
CachedGeometry* InstancedGeometry::asLUT() { return m_repr ? m_repr->asLUT() : nullptr; }
SurfaceGeometry* InstancedGeometry::asSurface() { return m_repr ? m_repr->asSurface() : nullptr; }
MeshGeometry* InstancedGeometry::asMesh() { return m_repr ? m_repr->asMesh() : nullptr; }
EdgeGeometry* InstancedGeometry::asEdge() { return m_repr ? m_repr->asEdge() : nullptr; }
IndexedEdgeGeometry* InstancedGeometry::asIndexedEdge() { return m_repr ? m_repr->asIndexedEdge() : nullptr; }
RealityMeshGeometry* InstancedGeometry::asRealityMesh() { return m_repr ? m_repr->asRealityMesh() : nullptr; }
SilhouetteEdgeGeometry* InstancedGeometry::asSilhouette() { return m_repr ? m_repr->asSilhouette() : nullptr; }
PointCloudGeometry* InstancedGeometry::asPointCloud() { return m_repr ? m_repr->asPointCloud() : nullptr; }
PlanarGridGraphic* InstancedGeometry::asPlanarGrid() { return m_repr ? m_repr->asPlanarGrid() : nullptr; }

// --- Forwarded render properties (Ported from: itwinjs-core line 365-376) ---

TechniqueId InstancedGeometry::getTechniqueId() const { return m_repr ? m_repr->getTechniqueId() : TechniqueId::Surface; }
Pass InstancedGeometry::getPass(TargetImpl const& target) const { return m_repr ? m_repr->getPass(target) : Pass::Opaque; }
RenderOrder InstancedGeometry::getRenderOrder() const { return m_repr ? m_repr->getRenderOrder() : RenderOrder::None; }
bool InstancedGeometry::isLitSurface() const { return m_repr ? m_repr->isLitSurface() : false; }
bool InstancedGeometry::hasBakedLighting() const { return m_repr ? m_repr->hasBakedLighting() : false; }
bool InstancedGeometry::hasAnimation() const { return m_repr ? m_repr->hasAnimation() : false; }
bool InstancedGeometry::usesQuantizedPositions() const { return m_repr ? m_repr->usesQuantizedPositions() : true; }
float const* InstancedGeometry::getQOrigin() const { return m_repr ? m_repr->getQOrigin() : nullptr; }
float const* InstancedGeometry::getQScale() const { return m_repr ? m_repr->getQScale() : nullptr; }
RenderMaterialInternal const* InstancedGeometry::getMaterialInfo() const { return m_repr ? m_repr->getMaterialInfo() : nullptr; }
PolylineBuffers const* InstancedGeometry::getPolylineBuffers() const { return m_repr ? m_repr->getPolylineBuffers() : nullptr; }
bool InstancedGeometry::supportsThematicDisplay() const { return m_repr ? m_repr->supportsThematicDisplay() : false; }
rhi::TextureHandle InstancedGeometry::getSurfaceTexture() const { return m_repr ? m_repr->getSurfaceTexture() : rhi::TextureHandle{}; }
rhi::TextureHandle InstancedGeometry::getNormalMapTexture() const { return m_repr ? m_repr->getNormalMapTexture() : rhi::TextureHandle{}; }
float InstancedGeometry::getNormalMapScale() const { return m_repr ? m_repr->getNormalMapScale() : 1.0f; }

// --- Instance-specific queries ---
bool InstancedGeometry::hasFeatures() const {
    // Ported from: itwinjs-core line 374
    return m_buffers && m_buffers->hasFeatures();
}

// --- Draw (Ported from: itwinjs-core line 453-455) ---
// Binds the repr geometry's VAO, appends the instance attributes
// (InstancedGeometry.ts create :385-409 的 BuffersContainer 组装——
// DanQing RHI 无 container 概念，等价地就地绑定进 repr VAO，draw 后
// 复原 divisor + disable，避免共享 VAO 残留实例缓冲引用），然后按
// repr 形态发 instanced draw（drawArraysInstanced / drawElementsInstanced）。
void InstancedGeometry::draw(rhi::Driver& driver)
{
    // Ported from: itwinjs-core InstancedGeometry.draw()
    if (!m_repr || !m_buffers || !m_buffers->isValid()) return;

    uint32_t const instanceCount = m_buffers->getNumInstances();
    if (instanceCount == 0) return;

    // Step 1: Bind the repr geometry's VAO (without drawing).
    // Ported from: LUTGeometry.drawInstanced 的 bufs.bind()（container 内已
    // appendLinkages(repr.lutBuffers.linkages)——repr 的 attribute 布局）。
    m_repr->bindPrimitive(driver);

    // Step 2: Append instance attributes with divisor=1.
    // Locations come from the per-technique instanced AttributeMap
    // (AttributeMap.ts:36-51——a_pos@0 之后追加实例组)。
    TechniqueId const techId = getTechniqueId();
    uint32_t boundLocs[8] = {};
    uint32_t boundCount = 0;
    auto const bindAttrib = [&](rhi::BufferObjectHandle bo, char const* name,
                                uint32_t components, uint32_t stride, uint32_t offset,
                                rhi::ElementType type, bool normalized) {
        if (!bo) return;
        auto const* details = AttributeMap::findAttribute(name, techId, /*instanced*/ true);
        if (!details || details->location == 0)  // 0 = a_pos 位（实例组自 1 起）
            return;
        uint32_t const loc = details->location;
        driver.bindInstanceBuffer(bo, loc, components, stride, offset, type, normalized);
        driver.setVertexAttribDivisor(loc, 1);
        boundLocs[boundCount++] = loc;
    };

    // 每实例变换（3×vec4 行，stride 48——InstanceBuffers.createTransformBufferParameters）。
    auto const tparams = InstanceBuffers::getTransformBufferParams(techId);
    rhi::BufferObjectHandle const transforms = m_buffers->getTransforms();
    for (int i = 0; i < 3; ++i) {
        if (tparams.locations[i] == 0) continue;
        driver.bindInstanceBuffer(transforms, tparams.locations[i], 4,
                                  tparams.stride, tparams.offsets[i]);
        driver.setVertexAttribDivisor(tparams.locations[i], 1);
        boundLocs[boundCount++] = tparams.locations[i];
    }

    // 每实例 symbology 覆盖（a_instanceOverrides@+0 / a_instanceRgba@+4，
    // stride 8——InstancedGeometry.ts :392-401；UBYTE 非归一，着色器除 255）。
    rhi::BufferObjectHandle const symbology = m_buffers->getSymbology();
    bindAttrib(symbology, "a_instanceOverrides", 4, 8, 0,
               rhi::ElementType::UBYTE4, /*normalized*/ false);
    bindAttrib(symbology, "a_instanceRgba", 4, 8, 4,
               rhi::ElementType::UBYTE4, /*normalized*/ false);

    // 每实例 feature id（3×UBYTE，stride 0 紧凑——:402-406）。
    rhi::BufferObjectHandle const featureIds = m_buffers->getFeatureIds();
    bindAttrib(featureIds, "a_featureId", 3, 0, 0,
               rhi::ElementType::UBYTE3, /*normalized*/ false);

    // Step 3: Issue the instanced draw call in the repr's flavor.
    // Ported from: itwinjs-core LUTGeometry._draw / IndexedGeometry._draw
    // （SurfaceGeometry.ts:150-162 drawArrays；IndexedGeometry drawElements）。
    uint32_t const numIndices = m_repr->getDrawIndexCount();
    if (numIndices > 0) {
        if (m_repr->usesIndexBuffer())
            driver.draw2(0, numIndices, instanceCount);
        else
            driver.drawArrays(0, numIndices, instanceCount);
    }

    // Step 4: Restore the repr VAO to its non-instanced state.
    for (uint32_t i = 0; i < boundCount; ++i) {
        driver.setVertexAttribDivisor(boundLocs[i], 0);
        driver.disableVertexAttribArray(boundLocs[i]);
    }
}

// --- Range (Ported from: itwinjs-core line 457-459) ---
InstanceBuffers::Range3d InstancedGeometry::getRange() const {
    if (m_buffers) return m_buffers->getRange();
    return {};
}

// --- Statistics (Ported from: itwinjs-core line 461-465) ---
void InstancedGeometry::collectStatistics(RenderMemory::Statistics& stats) const
{
    if (m_repr) m_repr->collectStatistics(stats);
    if (m_buffers) m_buffers->collectStatistics(stats);
}

uint32_t InstancedGeometry::getInstanceCount() const noexcept {
    return m_buffers ? m_buffers->getNumInstances() : 0;
}

END_DQ_RENDER_NAMESPACE
