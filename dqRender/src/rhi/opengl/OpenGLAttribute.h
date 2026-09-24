// SPDX-License-Identifier: Apache-2.0
// Ported from: filament backend/src/opengl/OpenGLDriver.cpp
//              (vertex-attribute ElementType → GL size/format wiring, extracted
//               for unit testability — see OpenGLDriver.cpp bindRenderPrimitive)
//
// Pure helpers that mirror the two `switch (ElementType)` blocks in
// OpenGLDriver.cpp's bindRenderPrimitive:
//   - elementSize():   bytes consumed by one attribute of this type
//                      (mirrors the stride-computation switch).
//   - elementFormat(): (glType, glSize) passed to glVertexAttribPointer
//                      (mirrors the attribute-format switch).
//
// These functions are deliberately GL-context-free so they can be exercised
// in a plain unit test without a driver/context. The GL constants they return
// are the same ones OpenGLDriver.cpp passes to glVertexAttribPointer.
#pragma once

#include "dqRender/rhi/DriverEnums.h"

namespace dqRender::gl {

// Bytes consumed by one attribute of this type (mirrors the size switch in
// OpenGLDriver.cpp bindRenderPrimitive).
int elementSize(rhi::ElementType type) noexcept;

// (glType, glSize) passed to glVertexAttribPointer (mirrors the type switch in
// OpenGLDriver.cpp bindRenderPrimitive). outGlType is a GL enum value
// (e.g. GL_UNSIGNED_BYTE, GL_FLOAT); outGlSize is the component count.
void elementFormat(rhi::ElementType type, int& outGlType, int& outGlSize) noexcept;

}  // namespace dqRender::gl
