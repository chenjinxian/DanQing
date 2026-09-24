// SPDX-License-Identifier: Apache-2.0
// Ported from: filament OpenGLDriver vertex-attribute wiring + itwinjs WebGL byte attrs
//              (UBYTE3/UBYTE for Polyline corner buffer — itwinjs CachedGeometry.ts PolylineBuffers)
//
// Unit tests for the pure ElementType → (GL type, GL size, byte size) helpers
// extracted from OpenGLDriver.cpp's two `switch (ElementType)` blocks.
#include "rhi/opengl/OpenGLAttribute.h"

#include <gtest/gtest.h>

#include "dqRender/rhi/DriverEnums.h"   // rhi::ElementType
// GL enum values — same header OpenGLDriver.cpp uses.
#include "rhi/opengl/Gl.h"

using dqRender::rhi::ElementType;

// ---------------------------------------------------------------------------
// New behavior: UBYTE / UBYTE3 must wire to GL_UNSIGNED_BYTE (Polyline attrs).
// ---------------------------------------------------------------------------

// Ported from: filament OpenGLDriver vertex-attribute wiring
//              (UBYTE for itwinjs Polyline a_param —CachedGeometry.ts PolylineBuffers)
TEST(OpenGLAttribute, UbyteIsOneByteUnsigned) {
    int glType = 0, glSize = 0;
    dqRender::gl::elementFormat(ElementType::UBYTE, glType, glSize);
    EXPECT_EQ(GL_UNSIGNED_BYTE, glType);
    EXPECT_EQ(1, glSize);
    EXPECT_EQ(1, dqRender::gl::elementSize(ElementType::UBYTE));
}

// Ported from: filament OpenGLDriver vertex-attribute wiring
//              (UBYTE3 for itwinjs Polyline a_pos/a_prevIndex/a_nextIndex)
TEST(OpenGLAttribute, Ubyte3IsThreeBytesUnsigned) {
    int glType = 0, glSize = 0;
    dqRender::gl::elementFormat(ElementType::UBYTE3, glType, glSize);
    EXPECT_EQ(GL_UNSIGNED_BYTE, glType);
    EXPECT_EQ(3, glSize);
    EXPECT_EQ(3, dqRender::gl::elementSize(ElementType::UBYTE3));
}

// Ported from: filament OpenGLDriver vertex-attribute wiring
//              (UBYTE2 added symmetrically with UBYTE/UBYTE3 — same GL_UNSIGNED_BYTE family)
TEST(OpenGLAttribute, Ubyte2IsTwoBytesUnsigned) {
    int glType = 0, glSize = 0;
    dqRender::gl::elementFormat(ElementType::UBYTE2, glType, glSize);
    EXPECT_EQ(GL_UNSIGNED_BYTE, glType);
    EXPECT_EQ(2, glSize);
    EXPECT_EQ(2, dqRender::gl::elementSize(ElementType::UBYTE2));
}

// ---------------------------------------------------------------------------
// Regression guard: helpers must reproduce the CURRENT switches EXACTLY for
// every already-handled type (OpenGLDriver.cpp:381-393 and :408-419) and keep
// the current `default` mapping for everything else (e.g. UINT/BYTE/SHORT that
// fall through). This task only ADDS UBYTE/UBYTE2/UBYTE3 — it does NOT "fix"
// any other type.
// ---------------------------------------------------------------------------
// Ported from: filament OpenGLDriver vertex-attribute wiring
//              (regression guard: transcribed verbatim from OpenGLDriver.cpp switches)
TEST(OpenGLAttribute, ExistingTypesUnchanged) {
    auto check = [](ElementType t, int expType, int expSize, int expBytes) {
        int glType = 0, glSize = 0;
        dqRender::gl::elementFormat(t, glType, glSize);
        EXPECT_EQ(expType, glType);
        EXPECT_EQ(expSize, glSize);
        EXPECT_EQ(expBytes, dqRender::gl::elementSize(t));
    };

    // --- Explicitly handled in both current switches ---
    check(ElementType::FLOAT,  GL_FLOAT,         1,  4);
    check(ElementType::FLOAT2, GL_FLOAT,         2,  8);
    check(ElementType::FLOAT3, GL_FLOAT,         3, 12);
    check(ElementType::FLOAT4, GL_FLOAT,         4, 16);
    check(ElementType::HALF2,  GL_HALF_FLOAT,    2,  4);
    check(ElementType::HALF4,  GL_HALF_FLOAT,    4,  8);
    check(ElementType::UBYTE4, GL_UNSIGNED_BYTE, 4,  4);
    check(ElementType::SHORT2, GL_SHORT,         2,  4);
    check(ElementType::SHORT4, GL_SHORT,         4,  8);

    // --- UINT: explicitly handled in the SIZE switch (→4) but falls to `default`
    //     in the TYPE switch (→(GL_FLOAT,4)). Capture CURRENT behavior verbatim:
    //     NOT changed by this task. ---
    check(ElementType::UINT,   GL_FLOAT,         4,  4);

    // --- Everything else falls to `default` in BOTH current switches
    //     (size→16, format→(GL_FLOAT,4)). Capture CURRENT behavior verbatim for
    //     representative default-falling types; NOT changed by this task. ---
    check(ElementType::BYTE,   GL_FLOAT,         4, 16);
    check(ElementType::SHORT,  GL_FLOAT,         4, 16);
    check(ElementType::USHORT, GL_FLOAT,         4, 16);
    check(ElementType::INT,    GL_FLOAT,         4, 16);
    check(ElementType::HALF,   GL_FLOAT,         4, 16);
    check(ElementType::HALF3,  GL_FLOAT,         4, 16);
}
