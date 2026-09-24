// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Float RGBA color representation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/FloatRGBA.ts
//
// FloatRgb / FloatRgba: float color values (0-1) derived from a ColorDef's
// packed 0xTTBBGGRR tbgr value. Components are kept in sync with the tbgr so
// that change detection can compare a single integer.
#pragma once

#include "UniformHandle.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/RgbColor.h>

#include <array>
#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

using dqCommon::ColorComponents;
using dqCommon::ColorDef;
using dqCommon::ColorDefProps;

// Ported from: itwinjs-core FloatRGBA.ts clamp()/scale()
inline float FloatColorClamp(float norm) noexcept  // NOLINT(readability-identifier-naming) TS is module-level clamp(); FloatColor prefix is a DanQing F4 invention (case-fix deferred)
{
    return norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
}

inline uint32_t FloatColorScale(float norm) noexcept  // NOLINT(readability-identifier-naming) TS is module-level scale(); FloatColor prefix is a DanQing F4 invention (case-fix deferred)
{
    return static_cast<uint32_t>(norm * 255.0f + 0.5f);
}

// ---------------------------------------------------------------------------
// FloatRgb — float RGB color (0-1 range)
// Ported from: itwinjs-core FloatRgb
//
// maskTbgr strips the transparency byte: only the RGB packed value is tracked.
// ---------------------------------------------------------------------------
struct FloatRgb {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    FloatRgb() = default;
    FloatRgb(float inR, float inG, float inB) { set(inR, inG, inB); }

    // Ported from: itwinjs-core FloatColor.red/green/blue
    float red() const noexcept { return r; }
    float green() const noexcept { return g; }
    float blue() const noexcept { return b; }

    // Ported from: itwinjs-core FloatColor.tbgr
    uint32_t getTbgr() const noexcept { return m_tbgr; }

    // Ported from: itwinjs-core FloatColor.isWhite
    bool isWhite() const noexcept { return 1.0f == r && 1.0f == g && 1.0f == b; }

    // Ported from: itwinjs-core FloatColor.setTbgr()
    void setTbgr(uint32_t tbgr) noexcept
    {
        tbgr &= 0x00ffffffu;  // maskTbgr
        if (tbgr == m_tbgr)
            return;
        ColorComponents c = ColorDef::getColors(tbgr);
        r = c.r / 255.0f;
        g = c.g / 255.0f;
        b = c.b / 255.0f;
        m_tbgr = tbgr;
    }

    // Ported from: itwinjs-core FloatColor.setColorDef()
    void setColorDef(ColorDef const& def) noexcept { setTbgr(def.getTbgr()); }

    // Ported from: itwinjs-core FloatColor.setRgbColor()
    void setRgbColor(dqCommon::RgbColor const& rgb) noexcept
    {
        setTbgr(static_cast<uint32_t>(rgb.r | (rgb.g << 8) | (rgb.b << 16)));
    }

    // Ported from: itwinjs-core FloatRgb.set() -> setRgbaComponents(r,g,b,1)
    void set(float inR, float inG, float inB) noexcept
    {
        inR = FloatColorClamp(inR);
        inG = FloatColorClamp(inG);
        inB = FloatColorClamp(inB);
        uint32_t tbgr = FloatColorScale(inR) | (FloatColorScale(inG) << 8) | (FloatColorScale(inB) << 16);
        m_tbgr = tbgr & 0x00ffffffu;
        r = inR;
        g = inG;
        b = inB;
    }

    // Ported from: itwinjs-core FloatRgb.bind()
    void bind(UniformHandle& uniform) const noexcept { uniform.setUniform3fv(&r); }

    // Ported from: itwinjs-core FloatRgb.fromColorDef()
    static FloatRgb fromColorDef(ColorDef const& def) noexcept { return fromTbgr(def.getTbgr()); }

    // Ported from: itwinjs-core FloatRgb.fromRgbColor()
    static FloatRgb fromRgbColor(dqCommon::RgbColor const& rgb) noexcept
    {
        return from(static_cast<float>(rgb.r) / 255.0f,
                    static_cast<float>(rgb.g) / 255.0f,
                    static_cast<float>(rgb.b) / 255.0f);
    }

    // Ported from: itwinjs-core FloatRgb.from()
    static FloatRgb from(float inR, float inG, float inB) noexcept
    {
        FloatRgb rgb;
        rgb.set(inR, inG, inB);
        return rgb;
    }

    // Ported from: itwinjs-core FloatRgb.fromTbgr()
    static FloatRgb fromTbgr(uint32_t tbgr) noexcept
    {
        FloatRgb rgb;
        rgb.setTbgr(tbgr);
        return rgb;
    }

    // --- Backward-compatible factories (kept for existing callers) ---
    static FloatRgb fromBytes(uint8_t inR, uint8_t inG, uint8_t inB) noexcept
    {
        return from(inR / 255.0f, inG / 255.0f, inB / 255.0f);
    }

    static FloatRgb fromHex(uint32_t hex) noexcept
    {
        return fromBytes((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
    }

    float const* data() const noexcept { return &r; }

private:
    uint32_t m_tbgr = 0;
};

// ---------------------------------------------------------------------------
// FloatRgba — float RGBA color (0-1 range)
// Ported from: itwinjs-core FloatRgba
//
// maskTbgr is the identity: the full 0xTTBBGGRR value (including transparency)
// is tracked. Alpha is computed as 1 - transparency/255.
// ---------------------------------------------------------------------------
struct FloatRgba {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    FloatRgba() = default;
    FloatRgba(float inR, float inG, float inB, float inA = 1.0f) { set(inR, inG, inB, inA); }

    // Ported from: itwinjs-core FloatColor.red/green/blue
    float red() const noexcept { return r; }
    float green() const noexcept { return g; }
    float blue() const noexcept { return b; }

    // Ported from: itwinjs-core FloatRgba.alpha
    float alpha() const noexcept { return a; }
    void setAlpha(float alpha) noexcept { a = alpha; }

    // Ported from: itwinjs-core FloatRgba.hasTranslucency
    bool hasTranslucency() const noexcept { return 1.0f != a; }

    // Ported from: itwinjs-core FloatColor.tbgr
    uint32_t getTbgr() const noexcept { return m_tbgr; }

    // Ported from: itwinjs-core FloatColor.isWhite
    bool isWhite() const noexcept { return 1.0f == r && 1.0f == g && 1.0f == b; }

    // Ported from: itwinjs-core FloatColor.setTbgr() (maskTbgr = identity)
    void setTbgr(uint32_t tbgr) noexcept
    {
        if (tbgr == m_tbgr)
            return;
        ColorComponents c = ColorDef::getColors(tbgr);
        r = c.r / 255.0f;
        g = c.g / 255.0f;
        b = c.b / 255.0f;
        a = 1.0f - c.t / 255.0f;
        m_tbgr = tbgr;
    }

    // Ported from: itwinjs-core FloatColor.setColorDef()
    void setColorDef(ColorDef const& def) noexcept { setTbgr(def.getTbgr()); }

    // Ported from: itwinjs-core FloatRgba.set() -> setRgbaComponents()
    void set(float inR, float inG, float inB, float inA) noexcept
    {
        inR = FloatColorClamp(inR);
        inG = FloatColorClamp(inG);
        inB = FloatColorClamp(inB);
        inA = FloatColorClamp(inA);
        uint32_t tbgr = FloatColorScale(inR)
                      | (FloatColorScale(inG) << 8)
                      | (FloatColorScale(inB) << 16)
                      | (FloatColorScale(1.0f - inA) << 24);
        m_tbgr = tbgr;  // maskTbgr = identity
        r = inR;
        g = inG;
        b = inB;
        a = inA;
    }

    // Ported from: itwinjs-core FloatRgba.bind()
    void bind(UniformHandle& uniform) const noexcept { uniform.setUniform4fv(&r); }

    // Ported from: itwinjs-core FloatRgba.fromColorDef()
    static FloatRgba fromColorDef(ColorDef const& def) noexcept { return fromTbgr(def.getTbgr()); }

    // Ported from: itwinjs-core FloatRgba.fromTbgr()
    static FloatRgba fromTbgr(uint32_t tbgr) noexcept
    {
        FloatRgba rgba;
        rgba.setTbgr(tbgr);
        return rgba;
    }

    // Ported from: itwinjs-core FloatRgba.from()
    static FloatRgba from(float inR, float inG, float inB, float inA) noexcept
    {
        FloatRgba rgba;
        rgba.set(inR, inG, inB, inA);
        return rgba;
    }

    // Ported from: itwinjs-core FloatRgba.clone()
    FloatRgba clone() const noexcept { return FloatRgba(r, g, b, a); }
    FloatRgba cloneTo(FloatRgba& out) const noexcept
    {
        out.set(r, g, b, a);
        return out;
    }

    // --- Backward-compatible factories (kept for existing callers) ---
    static FloatRgba fromBytes(uint8_t inR, uint8_t inG, uint8_t inB, uint8_t inA = 255) noexcept
    {
        return from(inR / 255.0f, inG / 255.0f, inB / 255.0f, inA / 255.0f);
    }

    static FloatRgba fromHex(uint32_t hex, uint8_t alpha = 255) noexcept
    {
        return fromBytes((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, alpha);
    }

    /// Convert RGB components back to a packed 0xRRGGBB hex value.
    uint32_t toHex() const noexcept
    {
        auto q = [](float v) -> uint8_t {
            return static_cast<uint8_t>(v < 0.0f ? 0 : (v > 1.0f ? 255 : v * 255.0f));
        };
        return (q(r) << 16) | (q(g) << 8) | q(b);
    }

    float const* data() const noexcept { return &r; }
    std::array<float, 4> toArray() const { return {{r, g, b, a}}; }

private:
    uint32_t m_tbgr = 0;
};

END_DQ_RENDER_NAMESPACE
