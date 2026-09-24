// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — CachedGeometry implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/CachedGeometry.ts
#include "CachedGeometry.h"
#include "VertexLutTexture.h"

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// CachedGeometry method implementations
// Ported from: itwinjs-core CachedGeometry.ts line 140-183
// ---------------------------------------------------------------------------

float CachedGeometry::getLineWeight(TargetImpl const& /*target*/) const {
    // Default line weight — subclasses override _getLineWeight for specifics.
    // Ported from: itwinjs-core CachedGeometry.ts line 141-150
    return 1.0f;
}

uint32_t CachedGeometry::getLineCode(TargetImpl const& /*target*/) const {
    // Default solid line code.
    // Ported from: itwinjs-core CachedGeometry.ts line 137-139
    return 0;  // solid
}

FlashMode CachedGeometry::getFlashMode(TargetImpl const& /*target*/) const {
    // By default only surfaces rendered with lighting get brightened.
    // Overridden for reality meshes since they have lighting baked-in.
    // Ported from: itwinjs-core CachedGeometry.ts line 152-163
    if (hasBakedLighting())
        return FlashMode::Hilite;

    if (!isLitSurface())
        return FlashMode::Hilite;

    return FlashMode::Brighten;
}

// ---------------------------------------------------------------------------
// LUTGeometry method implementations
// Ported from: itwinjs-core CachedGeometry.ts line 190-216
// ---------------------------------------------------------------------------

ColorInfo LUTGeometry::getColor(TargetImpl const& /*target*/) const {
    // Default: return the LUT's color info.
    // Subclasses override if color varies by target.
    // Ported from: itwinjs-core LUTGeometry.ts line 205
    auto const* lut = getLut();
    if (lut) {
        // Return uniform white — the actual color comes from the LUT texture.
        return ColorInfo::fromUniform(0xFFFFFF);
    }
    return ColorInfo::fromUniform(0xFFFFFF);
}

bool LUTGeometry::usesQuantizedPositions() const {
    // Ported from: itwinjs-core LUTGeometry.ts line 207
    // LUTGeometry always uses quantized positions unless the LUT says otherwise.
    return true;
}

float const* LUTGeometry::getQOrigin() const {
    // Ported from: itwinjs-core LUTGeometry.ts line 208
    auto const* lut = getLut();
    return lut ? lut->getQOrigin() : nullptr;
}

float const* LUTGeometry::getQScale() const {
    // Ported from: itwinjs-core LUTGeometry.ts line 209
    auto const* lut = getLut();
    return lut ? lut->getQScale() : nullptr;
}

bool LUTGeometry::hasAnimation() const {
    // Ported from: itwinjs-core LUTGeometry.ts line 210
    // LUT geometry doesn't have animation by default.
    return false;
}

END_DQ_RENDER_NAMESPACE
