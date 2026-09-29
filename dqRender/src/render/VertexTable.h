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
//   Vertex slots:     ComputeQuantizedPosition 缺省 `return <attrName>;`
//                     （ShaderBuilder.ts:757——变体可覆盖，Edge.ts:263）
//   Vertex initializers: LUT step/center init, qpos→LUT 坐标初始化, vertex
//                     data pre-read
//   main 的 rawPosition 主行（computeVertexPosition(qpos)，ShaderBuilder.ts
//   :770）由 function-call main 装配（ShaderBuilder.cpp hasQPos 分支）——
//   本函数不占用 AdjustRawPosition 槽（该槽归动画位移等复合加算，
//   ShaderBuilder.ts:771-774）。
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

    // --- ComputeQuantizedPosition 缺省 ---
    // Ported from: itwinjs-core ShaderBuilder.ts:757——
    // `const computeQPos = this.get(ComputeQuantizedPosition) ?? "return a_pos;"`
    // （本路径的 "a_pos" = attrName——Surface 为 a_qPosition）。function-call
    // main 的首行是 `vec3 qpos = computeQuantizedPosition();`（:759——
    // ShaderBuilder.cpp buildVertexMain）；indexed 边等变体随后覆盖本槽
    // （Edge.ts:263——qpos 变为边表解码出的顶点表索引）。
    vert.setVertexComponent(VertexShaderComponent::ComputeQuantizedPosition,
        std::string("return ") + attrName + ";");

    // --- rawPosition 来源（ShaderBuilder.ts:770）---
    // 参考 main 是 `vec4 rawPosition = computeVertexPosition(qpos);`（主行，
    // 非槽位）+ `rawPosition = adjustRawPosition(rawPosition)` 复合（:771-774，
    // 动画位移加算）。DanQing 的 function-call main 同形（ShaderBuilder.cpp
    // hasQPos 分支）；本函数**不再占用 AdjustRawPosition 槽**（历史上占槽会
    // 被 addAnimation 的位移体替换 → LUT 解码整段丢失——M-I(4) 修复）。
    // 注意：computeVertexPosition 的 LUT 变体消费预读全局、忽略实参
    //（Vertex.ts:36-41 computeVertexPositionFromLUT）。

    // --- Vertex index initializer ---
    // Must run BEFORE the pre-read (reads qpos to compute base coords).
    // Ported from: itwinjs-core Vertex.ts initializeVertLUTCoords
    // (line 20-23)——`g_vertexLUTIndex = decodeUInt24(qpos)`：qpos =
    // computeQuantizedPosition() 的结果（主行首行）。对 surface/polyline
    // qpos==属性原值（语义不变）；对 indexed 边 qpos=边表解码的顶点表索引
    //（历史上此处硬编码 decodeUInt24(a_pos) 把**边表索引**当顶点索引——
    // M-I(4) 修复）。
    vert.addInitializer(std::string("  g_vertexLUTIndex = decodeUInt24(qpos);\n") +
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
