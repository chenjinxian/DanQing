// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Vertex table (LUT) builder helper
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Vertex.ts
//              addPositionFromLUT() (line 215-255)
//              addPosition() (line 258-280)
//
// Provides addVertexTable() which wires the full VertexLUT infrastructure
// into a ProgramBuilder: LUT globals, coordinate computation, position
// decode functions, texture/uniform bindings, and pre-read initializers.
//
// §3.4 deviation: itwinjs uses AttributeMap for attribute→location mapping;
// DanQing uses explicit layout(location=N) in the attribute declarations.
#pragma once

#include "CommonShaders.h"
#include "ShaderBindings.h"
#include "ShaderBuilder.h"
#include "shader/DecodeShaders.h"
#include "shader/LookupTableShaders.h"
#include "shader/VertexTableShaders.h"

#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// addVertexTable — wire the VertexLUT infrastructure into a ProgramBuilder
// Ported from: itwinjs-core Vertex.ts addPositionFromLUT() (line 215-255)
//              + addPosition() (line 258-280)
//
// Parameters:
//   builder   — the ProgramBuilder to configure
//   quantized — true for 16-bit quantized positions, false for 32-bit floats
//   attrName  — name of the 24-bit LUT-key attribute. Defaults to "a_qPosition"
//               (DanQing's existing Surface quantized path). Polyline passes
//               "a_pos" to match itwinjs AttributeMap.ts:69-74 verbatim.
//
// Adds:
//   Vertex globals: g_vertexLUTIndex, g_vertexBaseCoords, g_vertLutData0-5,
//                   g_vert_stepX, g_vert_center, g_featureAndMaterialIndex
//   Vertex functions: decodeUInt24, decodeUInt16, unquantizePosition,
//                     computeLUTCoords, compute_vert_coords,
//                     computeVertexPosition (from LUT)
//   Vertex uniforms: u_vertLUT (sampler2D), u_vertParams (vec4),
//                    u_qOrigin (vec3), u_qScale (vec3)
//   Vertex initializers: LUT step/center init, vertex data pre-read
//   AdjustRawPosition: calls computeVertexPosition (LUT decode)
// ---------------------------------------------------------------------------
inline void addVertexTable(ProgramBuilder& builder, bool quantized,
                           char const* attrName = "a_qPosition")
{
    auto& vert = builder.getVertexBuilder();

    // --- Globals ---
    // Ported from: itwinjs-core Vertex.ts addPositionFromLUT() line 217-220
    vert.addGlobal("g_vertexLUTIndex", VariableType::Float);
    vert.addGlobal("g_vertexBaseCoords", VariableType::Vec2);
    vert.addGlobal("g_vertLutData0", VariableType::Vec4);
    vert.addGlobal("g_vertLutData1", VariableType::Vec4);
    vert.addGlobal("g_vertLutData2", VariableType::Vec4);
    vert.addGlobal("g_vertLutData3", VariableType::Vec4);
    if (!quantized) {
        vert.addGlobal("g_vertLutData4", VariableType::Vec4);
        vert.addGlobal("g_vertLutData5", VariableType::Vec4);
    }
    vert.addGlobal("g_vert_stepX", VariableType::Float);
    vert.addGlobal("g_vert_center", VariableType::Vec2);
    vert.addGlobal("g_featureAndMaterialIndex", VariableType::Vec4);

    // --- Decode helper functions ---
    // Ported from: itwinjs-core Decode.ts
    vert.addFunction(std::string(kDecodeUint24));
    vert.addFunction(std::string(kDecodeUint16));

    // --- LUT coordinate computation ---
    // Ported from: itwinjs-core LookupTable.ts
    vert.addFunction(std::string(kLookupTableFunctions));
    vert.addFunction(std::string(kComputeVertCoords));

    // --- LUT step/center initializer ---
    // Ported from: itwinjs-core LookupTable.ts initializerTemplate
    vert.addInitializer(std::string(kVertLutInitTemplate));

    // --- Position decode function ---
    // Ported from: itwinjs-core Vertex.ts computeVertexPositionFromLUT /
    //              computeUnquantizedPosition
    vert.addFunction(std::string(kUnquantizePosition));
    if (quantized) {
        vert.addFunction(std::string(kComputeVertexPositionFromLUT));
    } else {
        vert.addFunction(std::string(kComputeUnquantizedPositionFromLUT));
    }

    // --- Uniforms ---
    // u_vertLUT sampler (texture unit binding wired at draw time).
    // Ported from: itwinjs-core Vertex.ts line 229-233
    vert.addUniform("u_vertLUT", VariableType::Sampler2D, nullptr);

    // u_vertParams: (width, height, numRgbaPerVertex, numVertices).
    // Ported from: itwinjs-core Vertex.ts line 235-246
    vert.addUniform("u_vertParams", VariableType::Vec4, nullptr);

    // u_qOrigin / u_qScale: quantization parameters.
    // Ported from: itwinjs-core Vertex.ts addPosition() line 261-269
    vert.addUniform("u_qOrigin", VariableType::Vec3, nullptr);
    vert.addUniform("u_qScale", VariableType::Vec3, nullptr);

    // --- Vertex index attribute ---
    // The vertex index is a 24-bit value (3 bytes) used to look up the
    // vertex's first texel in the LUT texture.
    // Ported from: itwinjs-core Vertex.ts (attribute map setup).
    // `attrName` is "a_qPosition" by default (Surface quantized path) or
    // "a_pos" (Polyline, matching itwinjs AttributeMap.ts:69-74 verbatim).
    vert.addVariable({attrName, VariableType::Vec3, VariableScope::Attribute, 0});

    // --- AdjustRawPosition: LUT decode ---
    // In the function-call convention, AdjustRawPosition returns the decoded
    // position.  The LUT decode happens in the initializer (pre-read), and
    // computeVertexPosition reads from the pre-read g_vertLutData globals.
    // AdjustRawPosition calls computeVertexPosition with the vertex index.
    // Ported from: itwinjs-core ShaderBuilder.ts line 725 + Vertex.ts
    vert.setVertexComponent(VertexShaderComponent::AdjustRawPosition,
        std::string("    return computeVertexPosition(vec3(") + attrName + "));\n");

    // --- Vertex index initializer ---
    // Must run BEFORE the pre-read (reads the LUT-key attribute to compute
    // base coords). Ported from: itwinjs-core Vertex.ts initializeVertLUTCoords
    // (line 20-23). The reference uses `qpos` (the result of the default
    // computeQuantizedPosition() → `return a_pos;`); DanQing's function-call
    // convention has no `qpos` local, so we reference the attribute directly.
    vert.addInitializer(std::string("  g_vertexLUTIndex = decodeUInt24(") +
                        attrName + ");\n" +
                        "  g_vertexBaseCoords = compute_vert_coords(g_vertexLUTIndex);");

    // --- Pre-read vertex data initializer ---
    // Reads RGBA texels from the LUT texture into g_vertLutData0-5.
    // Must run AFTER the LUT step/center and vertex index initializers.
    // Ported from: itwinjs-core Vertex.ts line 200-212
    if (quantized) {
        vert.addInitializer(std::string(kPreReadVertexDataQuantized));
    } else {
        vert.addInitializer(std::string(kPreReadVertexDataUnquantized));
    }
}

END_DQ_RENDER_NAMESPACE
