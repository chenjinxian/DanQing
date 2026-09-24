// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Instance buffers implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/InstancedGeometry.ts
#include "InstanceBuffers.h"
#include "dqRender/RenderMemory.h"
#include "TechniqueImpl.h"

#include <cmath>
#include <cstring>

BEGIN_DQ_RENDER_NAMESPACE

// ===========================================================================
// InstanceData — RTC transform management
// (Ported from: itwinjs-core InstancedGeometry.ts InstanceData, line 31-63)
// ===========================================================================

InstanceData::InstanceData(uint32_t numInstances,
                            float rtcCenterX, float rtcCenterY, float rtcCenterZ)
    : m_numInstances(numInstances)
{
    m_rtcCenter[0] = rtcCenterX;
    m_rtcCenter[1] = rtcCenterY;
    m_rtcCenter[2] = rtcCenterZ;

    // Transform-space RTC-only translation = createTranslation(rtcCenter).
    // Ported from: itwinjs-core InstanceData constructor (line 42)
    m_rtcOnlyTransform = dqGeom::Transform::CreateTranslation(rtcCenterX, rtcCenterY, rtcCenterZ);
    // Pre-warm the cache with rtcOnly (== identity-model result), so the first
    // identity-model call returns rtcOnly without recomputing.
    // Ported from: itwinjs-core InstanceData constructor (line 43)
    m_rtcModelTransformT = m_rtcOnlyTransform;

    // Initialize rtcOnlyTransform as translation by rtcCenter
    // Ported from: itwinjs-core InstanceData constructor
    std::memset(m_rtcModelTransform, 0, sizeof(m_rtcModelTransform));
    m_rtcModelTransform[0] = 1.0f;
    m_rtcModelTransform[5] = 1.0f;
    m_rtcModelTransform[10] = 1.0f;
    m_rtcModelTransform[12] = rtcCenterX;
    m_rtcModelTransform[13] = rtcCenterY;
    m_rtcModelTransform[14] = rtcCenterZ;
    m_rtcModelTransform[15] = 1.0f;

    std::memcpy(m_lastModelMatrix, m_rtcModelTransform, sizeof(m_lastModelMatrix));
}

void InstanceData::getRtcModelTransform(float const* modelMatrix16, float* out16) const
{
    // Ported from: itwinjs-core InstanceData.getRtcModelTransform
    // Check if model matrix changed; if so, recompute rtcModelTransform.
    bool changed = false;
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(modelMatrix16[i] - m_lastModelMatrix[i]) > 1e-10f) {
            changed = true;
            break;
        }
    }

    if (changed) {
        std::memcpy(m_lastModelMatrix, modelMatrix16, sizeof(m_lastModelMatrix));
        // Compute modelMatrix * rtcOnlyTransform
        // rtcOnlyTransform is a translation matrix, so the result is:
        // modelMatrix with translation adjusted by rtcCenter
        float const* m = modelMatrix16;
        float const* c = m_rtcCenter;
        m_rtcModelTransform[0]  = m[0];  m_rtcModelTransform[1]  = m[1];  m_rtcModelTransform[2]  = m[2];  m_rtcModelTransform[3]  = m[3];
        m_rtcModelTransform[4]  = m[4];  m_rtcModelTransform[5]  = m[5];  m_rtcModelTransform[6]  = m[6];  m_rtcModelTransform[7]  = m[7];
        m_rtcModelTransform[8]  = m[8];  m_rtcModelTransform[9]  = m[9];  m_rtcModelTransform[10] = m[10]; m_rtcModelTransform[11] = m[11];
        m_rtcModelTransform[12] = m[12] + m[0]*c[0] + m[4]*c[1] + m[8]*c[2];
        m_rtcModelTransform[13] = m[13] + m[1]*c[0] + m[5]*c[1] + m[9]*c[2];
        m_rtcModelTransform[14] = m[14] + m[2]*c[0] + m[6]*c[1] + m[10]*c[2];
        m_rtcModelTransform[15] = m[15];
    }

    std::memcpy(out16, m_rtcModelTransform, 16 * sizeof(float));
}

dqGeom::Transform InstanceData::getRtcModelTransform(dqGeom::Transform const& modelMatrix) const
{
    // Ported from: itwinjs-core InstanceData.getRtcModelTransform (line 46-53)
    // Recompute only when modelMatrix changes (isAlmostEqual change detection),
    // then cache: rtcModel = modelMatrix * rtcOnlyTransform.
    if (!m_lastModelMatrixT.IsAlmostEqual(modelMatrix)) {
        m_lastModelMatrixT = modelMatrix;
        m_rtcModelTransformT = modelMatrix.MultiplyTransform(m_rtcOnlyTransform);
    }
    return m_rtcModelTransformT;
}

void InstanceData::getRtcOnlyTransform(float* out16) const
{
    // Ported from: itwinjs-core InstanceData.getRtcOnlyTransform
    // Returns the translation matrix by rtcCenter.
    std::memset(out16, 0, 16 * sizeof(float));
    out16[0] = 1.0f;
    out16[5] = 1.0f;
    out16[10] = 1.0f;
    out16[12] = m_rtcCenter[0];
    out16[13] = m_rtcCenter[1];
    out16[14] = m_rtcCenter[2];
    out16[15] = 1.0f;
}

float const* InstanceData::getPatternFeatureId() const
{
    // Ported from: itwinjs-core InstanceData._noFeatureId
    static const float kNoFeatureId[3] = {0.0f, 0.0f, 0.0f};
    return kNoFeatureId;
}

// ===========================================================================
// InstanceBuffers
// (Ported from: itwinjs-core InstancedGeometry.ts InstanceBuffers, line 156-267)
// ===========================================================================

InstanceBuffers::InstanceBuffers(uint32_t count,
                                  rhi::BufferObjectHandle transforms,
                                  float rtcCenterX, float rtcCenterY, float rtcCenterZ,
                                  rhi::BufferObjectHandle featureIds,
                                  rhi::BufferObjectHandle symbology)
    : InstanceData(count, rtcCenterX, rtcCenterY, rtcCenterZ)
    , m_transforms(transforms)
    , m_featureIds(featureIds)
    , m_symbology(symbology)
{
}

void InstanceBuffers::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core InstanceBuffers.collectStatistics
    // Approximate buffer sizes based on instance count.
    uint64_t transformBytes = static_cast<uint64_t>(getNumInstances()) * 12 * sizeof(float);
    uint64_t featureBytes = m_featureIds ? static_cast<uint64_t>(getNumInstances()) * 3 : 0;
    uint64_t symBytes = m_symbology ? static_cast<uint64_t>(getNumInstances()) * 8 : 0;
    stats.addInstances( transformBytes + featureBytes + symBytes);
}

InstanceBuffers::TransformBufferParams InstanceBuffers::getTransformBufferParams(TechniqueId /*techId*/)
{
    // Ported from: itwinjs-core InstanceBuffers.createTransformBufferParameters
    // 3 rows per instance; 4 floats per row; 4 bytes per float.
    TransformBufferParams params;
    constexpr uint32_t floatsPerRow = 4;
    constexpr uint32_t bytesPerVertex = floatsPerRow * sizeof(float);
    constexpr uint32_t stride = 3 * bytesPerVertex;

    params.stride = stride;
    for (uint32_t row = 0; row < 3; ++row) {
        // Attribute names: a_instanceMatrixRow0, a_instanceMatrixRow1, a_instanceMatrixRow2
        // Locations are determined by the shader program; use default layout indices.
        params.locations[row] = row;  // placeholder; actual binding done at draw time
        params.offsets[row] = row * bytesPerVertex;
    }

    return params;
}

InstanceBuffers::Range3d InstanceBuffers::computeRange(
    float const* reprMin, float const* reprMax,
    float const* transforms, uint32_t numFloats,
    float const* rtcCenter)
{
    // Ported from: itwinjs-core InstanceBuffers.computeRange
    // For each instance, transform the 8 corners of the representation range
    // by the instance's 3×4 transform matrix, and accumulate into the output range.
    Range3d range;
    range.minX = range.minY = range.minZ = 1e30f;
    range.maxX = range.maxY = range.maxZ = -1e30f;

    constexpr uint32_t floatsPerTransform = 3 * 4;  // 3 rows × 4 floats

    // 8 corners of the representation range
    float corners[8][3] = {
        {reprMin[0], reprMin[1], reprMin[2]},
        {reprMin[0], reprMin[1], reprMax[2]},
        {reprMin[0], reprMax[1], reprMin[2]},
        {reprMin[0], reprMax[1], reprMax[2]},
        {reprMax[0], reprMin[1], reprMin[2]},
        {reprMax[0], reprMin[1], reprMax[2]},
        {reprMax[0], reprMax[1], reprMin[2]},
        {reprMax[0], reprMax[1], reprMax[2]},
    };

    for (uint32_t i = 0; i + floatsPerTransform <= numFloats; i += floatsPerTransform) {
        float const* t = transforms + i;
        for (auto const& corner : corners) {
            float x = corner[0], y = corner[1], z = corner[2];
            float tx = t[3]  + t[0]*x + t[1]*y + t[2]*z;
            float ty = t[7]  + t[4]*x + t[5]*y + t[6]*z;
            float tz = t[11] + t[8]*x + t[9]*y + t[10]*z;

            if (tx < range.minX) range.minX = tx;
            if (ty < range.minY) range.minY = ty;
            if (tz < range.minZ) range.minZ = tz;
            if (tx > range.maxX) range.maxX = tx;
            if (ty > range.maxY) range.maxY = ty;
            if (tz > range.maxZ) range.maxZ = tz;
        }
    }

    // add RTC center
    range.minX += rtcCenter[0]; range.minY += rtcCenter[1]; range.minZ += rtcCenter[2];
    range.maxX += rtcCenter[0]; range.maxY += rtcCenter[1]; range.maxZ += rtcCenter[2];

    return range;
}

InstanceBuffers* InstanceBuffers::create(
    rhi::Driver& driver, uint32_t count,
    float const* transforms, float const* transformCenter,
    float const* featureIds, float const* symbology)
{
    // Ported from: itwinjs-core InstanceBuffersData.create
    if (count == 0 || !transforms) return nullptr;

    // Create transform buffer: 3 vec4 rows per instance (12 floats = 48 bytes)
    uint32_t transformSize = count * 12 * sizeof(float);
    auto transformBuf = driver.createBufferObject(
        transformSize, rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
    if (!transformBuf) return nullptr;

    rhi::BufferDescriptor transformData(transforms, transformSize);
    driver.updateBufferObject(transformBuf, std::move(transformData), 0);

    rhi::BufferObjectHandle featureIdBuf;
    if (featureIds) {
        uint32_t featureIdSize = count * 3 * sizeof(float);
        featureIdBuf = driver.createBufferObject(
            featureIdSize, rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
        if (featureIdBuf) {
            rhi::BufferDescriptor featureData(featureIds, featureIdSize);
            driver.updateBufferObject(featureIdBuf, std::move(featureData), 0);
        }
    }

    rhi::BufferObjectHandle symbologyBuf;
    if (symbology) {
        uint32_t symSize = count * 8 * sizeof(uint8_t);
        symbologyBuf = driver.createBufferObject(
            symSize, rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
        if (symbologyBuf) {
            rhi::BufferDescriptor symData(symbology, symSize);
            driver.updateBufferObject(symbologyBuf, std::move(symData), 0);
        }
    }

    float cx = transformCenter ? transformCenter[0] : 0.0f;
    float cy = transformCenter ? transformCenter[1] : 0.0f;
    float cz = transformCenter ? transformCenter[2] : 0.0f;

    return new InstanceBuffers(count, transformBuf, cx, cy, cz, featureIdBuf, symbologyBuf);
}

// ===========================================================================
// PatternBuffers
// (Ported from: itwinjs-core InstancedGeometry.ts PatternBuffers, line 270-342)
// ===========================================================================

PatternBuffers::PatternBuffers(uint32_t count,
                                rhi::BufferObjectHandle offsets,
                                float const* patternParams4,
                                float const* origin2,
                                float const* orgTransform16,
                                float const* localToModel16,
                                float const* symbolToLocal16,
                                uint32_t featureId,
                                bool hasFeatureId)
    : InstanceData(count, 0.0f, 0.0f, 0.0f)
    , m_offsets(offsets)
    , m_hasFeatureId(hasFeatureId)
{
    std::memcpy(m_patternParams, patternParams4, 4 * sizeof(float));

    if (orgTransform16) std::memcpy(m_transforms.orgTransform, orgTransform16, 16 * sizeof(float));
    if (localToModel16) std::memcpy(m_transforms.localToModel, localToModel16, 16 * sizeof(float));
    if (symbolToLocal16) std::memcpy(m_transforms.symbolToLocal, symbolToLocal16, 16 * sizeof(float));
    if (origin2) {
        m_transforms.origin[0] = origin2[0];
        m_transforms.origin[1] = origin2[1];
    }

    if (hasFeatureId) {
        m_featureId[0] = static_cast<float>((featureId & 0x0000FF) >> 0);
        m_featureId[1] = static_cast<float>((featureId & 0x00FF00) >> 8);
        m_featureId[2] = static_cast<float>((featureId & 0xFF0000) >> 16);
    }
}

float const* PatternBuffers::getPatternFeatureId() const
{
    if (m_hasFeatureId) return m_featureId;
    return InstanceData::getPatternFeatureId();
}

void PatternBuffers::collectStatistics(RenderMemory::Statistics& stats) const
{
    // Ported from: itwinjs-core PatternBuffers.collectStatistics
    stats.addInstances(
                    static_cast<uint64_t>(getNumInstances()) * 2 * sizeof(float));
}

PatternBuffers* PatternBuffers::create(
    rhi::Driver& driver,
    uint32_t count, float const* xyOffsets,
    float spacingX, float spacingY, float scale,
    float originX, float originY,
    float const* orgTransform16,
    float const* patternToModel16,
    float const* symbolTranslation16,
    uint32_t featureId)
{
    if (count == 0 || !xyOffsets) return nullptr;

    // Create offset buffer: 2 floats per instance (XY offset)
    uint32_t offsetSize = count * 2 * sizeof(float);
    auto offsetBuf = driver.createBufferObject(
        offsetSize, rhi::BufferObjectBinding::VERTEX, rhi::BufferUsage::STATIC);
    if (!offsetBuf) return nullptr;

    rhi::BufferDescriptor offsetData(xyOffsets, offsetSize);
    driver.updateBufferObject(offsetBuf, std::move(offsetData), 0);

    float patternParams[4] = {1.0f, spacingX, spacingY, scale};
    float origin[2] = {originX, originY};

    return new PatternBuffers(
        count, offsetBuf, patternParams, origin,
        orgTransform16, patternToModel16, symbolTranslation16,
        featureId, featureId != 0);
}

END_DQ_RENDER_NAMESPACE
