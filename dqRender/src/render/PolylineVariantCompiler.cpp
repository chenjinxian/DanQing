// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Polyline variant shader compiler implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/glsl/Polyline.ts
//              createPolylineBuilder() (line 413-429) + webgl/Technique.ts
//              (line 445-476 — Polyline technique registration).
//
// Composes the Polyline shader via createPolylineProgramBuilder (the 1:1
// faithful composition: addShaderFlags → addCommon → polylineAddLineCode →
// addColor → addEdgeContrast → addWhiteOnWhiteReversal), sets the
// 4-attribute map verbatim from AttributeMap.ts:69-74, and transfers uniform
// bindings (addVertexTable/addLineWeight/addColor wired them on the builder's
// ShaderBuilders; addBindings moves them onto the live ShaderProgram).
#include "PolylineVariantCompiler.h"
#include "ShaderBuilder.h"
#include "shader/PolylineShaderBuilder.h"  // createPolylineProgramBuilder

#include <string>
#include <utility>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// PolylineVariantCompiler::buildProgram
// Ported from: itwinjs-core Polyline.ts createPolylineBuilder() (line 413-429)
// + Technique.ts:445-476 (Polyline technique registration).
//
// GLSL generation is delegated to createPolylineProgramBuilder; this compiler
// layer sets the explicit attribute map (matching AttributeMap.ts:69-74 and
// PolylineBuffers' BufferParameters layout in CachedGeometry.ts:1141-1146) and
// transfers uniform bindings.
// ---------------------------------------------------------------------------
void PolylineVariantCompiler::buildProgram(ShaderProgram& prog, TechniqueFlags const& flags)
{
    FeatureMode featureMode = flags.featureMode;
    PositionType posType = flags.positionType;

    auto builder = createPolylineProgramBuilder(featureMode, posType);

    std::string vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string frag = builder.getFragmentBuilder().buildSourceWithComponents();

    std::string description = std::string("Polyline-") + flags.buildDescription();
    prog.setSource(std::move(vert), std::move(frag), std::move(description));

    // Explicit attribute locations — bound via glBindAttribLocation BEFORE link
    // (Program::attributeLocation → ShaderProgram::compile → OpenGLProgram::compile).
    // Matches PolylineBuffers' VAO layout in CachedGeometry.ts:1141-1146:
    //   attrPos        location 0, 3 UnsignedByte (24-bit LUT key for a_pos)
    //   attrPrevIndex  location 1, 3 UnsignedByte (24-bit LUT key for prev)
    //   attrNextIndex  location 2, 3 UnsignedByte (24-bit LUT key for next)
    //   attrParam      location 3, 1 UnsignedByte (joint-type byte → float)
    // Ported from: itwinjs-core AttributeMap (explicit per-technique attribute
    // name→location mapping, AttributeMap.ts:69-74).
    prog.setAttributeMap({
        {"a_pos", 0},
        {"a_prevIndex", 1},
        {"a_nextIndex", 2},
        {"a_param", 3},
    });

    // --- Transfer uniform bindings ---
    // The modular helpers (addVertexTable, addLineWeight, addModelToWindowCoords,
    // ...) registered their bindings on the builder's ShaderBuilders. Transfer
    // them to prog so glsl-module addUniform callbacks become live binds.
    builder.getVertexBuilder().addBindings(prog);
    builder.getFragmentBuilder().addBindings(prog);
}

END_DQ_RENDER_NAMESPACE
