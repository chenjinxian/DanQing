// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instanced geometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/InstancedGeometry.ts
#include "InstancedGeometry.h"
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
Pass InstancedGeometry::getPass() const { return m_repr ? m_repr->getPass() : Pass::Opaque; }
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
void InstancedGeometry::draw(rhi::Driver& driver)
{
    // Ported from: itwinjs-core InstancedGeometry.draw()
    // Binds the repr geometry's VAO, adds instance transform attributes,
    // and issues an instanced draw call.
    if (!m_repr || !m_buffers || !m_buffers->isValid()) return;

    uint32_t instanceCount = m_buffers->getNumInstances();
    if (instanceCount == 0) return;

    // Step 1: Bind the repr geometry's VAO (without drawing).
    m_repr->bindPrimitive(driver);

    // Step 2: Bind instance transform buffer and set vertex attribute divisors.
    // The 3 transform rows are stored as 3 vec4 attributes per instance.
    // Ported from: itwinjs-core InstanceBuffers.getTransformBufferParams()
    auto params = InstanceBuffers::getTransformBufferParams(getTechniqueId());
    rhi::BufferObjectHandle transforms = m_buffers->getTransforms();
    if (transforms) {
        // Bind the transform buffer as 3 vec4 vertex attributes (one per matrix row).
        for (int i = 0; i < 3; ++i) {
            if (params.locations[i] > 0) {
                driver.bindInstanceBuffer(transforms, params.locations[i],
                                          4, params.stride, params.offsets[i]);
                driver.setVertexAttribDivisor(params.locations[i], 1);
            }
        }
    }

    // Step 3: Issue instanced draw call.
    uint32_t numIndices = m_repr->getDrawIndexCount();
    if (numIndices > 0) {
        driver.draw2(0, numIndices, instanceCount);
    }

    // Step 4: Reset vertex attribute divisors to 0 (per-vertex).
    if (transforms) {
        for (int i = 0; i < 3; ++i) {
            if (params.locations[i] > 0) {
                driver.setVertexAttribDivisor(params.locations[i], 0);
            }
        }
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
