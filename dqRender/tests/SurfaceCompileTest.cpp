// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface shader GL-compile verification.
// Authored: no reference test exists in itwinjs-core for offline GLSL compile
//           verification (itwinjs compiles live in a browser). This test
//           composes the modular Surface helpers (createCommon / addColor /
//           addSurfaceFlags / addNormal / addTexture / addMaterial / addLighting
//           / addFragData) into a ProgramBuilder, generates vert+frag source,
//           and asserts the result actually compiles + links via the GL-compile
//           harness (GlslCompileHarness.h).
//
// Regression net for CLAUDE.md §5/§6: the 13 Surface*ShaderTests verify GLSL
// *substrings*; this test verifies the assembled GLSL is *compilable* — the
// gap that hid the undefined-function bug (buildFragmentMain emits calls to
// applyMaterialOverrides/applyLighting/assignFragData that nothing defined).

#include "GlslCompileHarness.h"

#include "render/LightingShaders.h"
#include "render/ShaderBuilder.h"
#include "render/SurfaceCommon.h"
#include "render/SurfaceFlags.h"
#include "render/SurfaceMaterial.h"
#include "render/SurfaceNormal.h"
#include "render/SurfaceTexture.h"
#include "render/SurfaceVariantCompiler.h"
#include "render/PolylineVariantCompiler.h"
#include "render/TechniqueImpl.h"
#include "render/shader/EdgeShaderBuilder.h"
#include "render/shader/PolylineShaderBuilder.h"
#include "render/SolarShadowShaders.h"
#include "render/AtmosphereShaderHelpers.h"
#include "render/ThematicDisplayShaders.h"
#include "rhi/opengl/OpenGLProgram.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

using namespace dqRender;

// Compose the full modular Surface program (non-quantized, non-instanced)
// and verify it GL-compiles + links. Uses the real createCommon / addColor /
// addFragData from SurfaceCommon.h.
// Ported from: itwinjs-core Surface.ts createSurfaceBuilder() (line 725-850)
TEST(SurfaceCompileTest, ModularSurfaceProgramCompilesAndLinks)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");

    // Use the function-call vertex-main convention (itwinjs VertexShaderBuilder.
    // buildSource) — the modular addXxx vertex slots are function bodies.
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    // Foundation: position pipeline (u_mvp, u_mv, addFrustum, v_eyeSpace,
    // AdjustRawPosition, ComputePosition).
    createCommon(pb, /*instanced*/false, /*quantized*/false);

    // Vertex color (a_color → v_color → ComputeBaseColor).
    addColor(pb);

    // Surface flags (u_surfaceFlags[12], v_surfaceFlags, computeSurfaceFlags).
    addSurfaceFlags(pb, /*withFeatureOverrides*/false, /*withFeatureColor*/false);

    // Surface normals (u_normalMatrix, a_normal, v_n, FinalizeNormal).
    addNormal(pb);

    // Surface texture (a_texCoord, v_texCoord, s_texture, ComputeBaseColor).
    addTexture(pb);

    // Material system (u_materialColor, u_materialParams, mat_*, ComputeMaterial).
    // Non-quantized: no material atlas (readMaterialAtlas depends on VertexLUT
    // globals only present on the quantized path). quantized=false gates it off.
    addMaterial(pb, /*instanced*/true, /*quantized*/false);

    // Lighting (addFrustum, computeDirectionalLighting, ApplyLighting).
    addLighting(pb);

    // Fragment output (fragColor + assignFragData).
    addFragData(pb);

    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// Compose a quantized Surface program with full VertexLUT path and verify
// it GL-compiles + links. This exercises addVertexTable (LUT globals,
// coordinate computation, position decode from texture, pre-read).
// Ported from: itwinjs-core Surface.ts createSurfaceBuilder() with
//              positionType=Quantized
TEST(SurfaceCompileTest, QuantizedVertexLUTProgramCompilesAndLinks)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    // Foundation with full LUT quantized path (addVertexTable).
    createCommon(pb, /*instanced*/false, /*quantized*/true);

    addColor(pb);
    addSurfaceFlags(pb, false, false);
    addNormal(pb);
    addTexture(pb);

    // Non-instanced + quantized material: readMaterialAtlas depends on LUT globals
    // (g_featureAndMaterialIndex, computeLUTCoords, g_vert_center, u_vertLUT,
    // u_vertParams) which are provided by addVertexTable via createCommon.
    addMaterial(pb, /*instanced*/false, /*quantized*/true);

    addLighting(pb);
    addFragData(pb);

    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// Edge shader compile tests
// Ported from: itwinjs-core Edge.ts createEdgeBuilder()
// ---------------------------------------------------------------------------

// SegmentEdge: visible edge with modelToWindowCoordinates + perpendicular offset
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(EdgeCompileTest, SegmentEdgeCompilesAndLinks)
{
    auto builder = createEdgeProgramBuilder(EdgeBuilderType::SegmentEdge,
                                            FeatureMode::None, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// Silhouette edge: adds normal matrix + octDecodeNormal + silhouette discard
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(EdgeCompileTest, SilhouetteEdgeCompilesAndLinks)
{
    auto builder = createEdgeProgramBuilder(EdgeBuilderType::Silhouette,
                                            FeatureMode::None, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// Edge with feature overrides
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(EdgeCompileTest, EdgeWithFeatureOverridesCompilesAndLinks)
{
    auto builder = createEdgeProgramBuilder(EdgeBuilderType::SegmentEdge,
                                            FeatureMode::Overrides, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// Polyline shader compile tests
// Ported from: itwinjs-core Polyline.ts createPolylineBuilder()
// ---------------------------------------------------------------------------

// Polyline: full joint/miter system with line code pipeline
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(PolylineCompileTest, PolylineCompilesAndLinks)
{
    auto builder = createPolylineProgramBuilder(FeatureMode::None, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// Polyline with feature overrides
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(PolylineCompileTest, PolylineWithFeatureOverridesCompilesAndLinks)
{
    auto builder = createPolylineProgramBuilder(FeatureMode::Overrides, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// PolylineVariantCompiler — live Polyline technique variant
// Drives the real PolylineVariantCompiler::buildProgram (the path Task 5 will
// route PolylineGeometry through), not the bare createPolylineProgramBuilder
// helper above. This catches the function-call / attribute-map / uniform-binding
// bugs that would crash the app when the ACS triad lands on the Polyline
// technique.
// Ported from: itwinjs-core webgl/Technique.ts:445-476 (Polyline technique
//              registration) + Polyline.ts createPolylineBuilder() (line 413-429)
// Authored: no reference test exists in itwinjs-core for offline GLSL compile
//           verification (itwinjs compiles live in a browser).
TEST(PolylineCompileTest, PolylineVariantCompilerAcsVariantCompilesAndLinks)
{
    PolylineVariantCompiler compiler;
    TechniqueFlags flags;                                // ACS variant: default
    flags.positionType = PositionType::Unquantized;      // ACS = unquantized LUT
    flags.featureMode = FeatureMode::None;               // no feature overrides

    ShaderProgram prog;
    compiler.buildProgram(prog, flags);

    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// PolylineVariantCompiler must bind the 4-attribute map (a_pos/a_prevIndex/
// a_nextIndex/a_param at locations 0/1/2/3) matching itwinjs AttributeMap, via
// glBindAttribLocation BEFORE link (the setAttributeMap → OpenGLProgram::compile
// path). Mirrors SurfaceAttribLocationsBoundExplicitly for the Surface map.
// Ported from: itwinjs-core AttributeMap.ts:69-74 (polyline attribute map) +
//               CachedGeometry.ts:1141-1146 (PolylineBuffers BufferParameters).
// Authored: no itwinjs offline test for explicit GL attribute binding.
TEST(PolylineCompileTest, PolylineVariantCompilerAttributeLocationsBoundExplicitly)
{
    PolylineVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    test::GlslCompileResult probe = test::compileAndLinkProgram(vert, frag);
    if (!probe.contextAvailable) GTEST_SKIP() << "no GL context";
    ASSERT_TRUE(probe.linked) << probe.linkLog;

    // Compile through the real rhi::OpenGLProgram path WITH explicit attribute
    // locations (mirrors PolylineVariantCompiler's setAttributeMap → Program path).
    std::vector<std::pair<std::string, uint8_t>> attrLocs = {
        {"a_pos", 0}, {"a_prevIndex", 1}, {"a_nextIndex", 2}, {"a_param", 3},
    };
    rhi::OpenGLProgram rhiProg;
    ASSERT_TRUE(rhiProg.compile(vert.c_str(), frag.c_str(), "Polyline-ACS-attribs", attrLocs))
        << "compile+link failed with explicit attribute locations";

    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_pos"), 0);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_prevIndex"), 1);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_nextIndex"), 2);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_param"), 3);
}

// PointCloud: gl_PointSize + circular point shape
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(PointCloudCompileTest, PointCloudCompilesAndLinks)
{
    auto builder = createPointCloudProgramBuilder(FeatureMode::None, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// SolarShadow compile test
// Ported from: itwinjs-core SolarShadowMapping.ts addSolarShadowMapping()
// ---------------------------------------------------------------------------

// Surface with solar shadow mapping
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(SolarShadowCompileTest, SurfaceWithShadowCompilesAndLinks)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    createCommon(pb, false, false);
    addColor(pb);
    addSurfaceFlags(pb, false, false);
    addNormal(pb);
    addTexture(pb);
    addMaterial(pb, /*instanced*/true, /*quantized*/false);
    addLighting(pb);
    addFragData(pb);
    addSolarShadowMap(pb);

    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// Atmospheric Scattering compile test
// Ported from: itwinjs-core Atmosphere.ts addAtmosphericScatteringEffect()
// ---------------------------------------------------------------------------

// Surface with atmospheric scattering
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(AtmosphereCompileTest, SurfaceWithAtmosphereCompilesAndLinks)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    createCommon(pb, false, false);
    addColor(pb);
    addSurfaceFlags(pb, false, false);
    addNormal(pb);
    addTexture(pb);
    addMaterial(pb, /*instanced*/true, /*quantized*/false);
    addLighting(pb);
    addFragData(pb);
    addAtmosphericScatteringEffect(pb, true);

    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// Thematic Display compile test
// Ported from: itwinjs-core Thematic.ts addThematicDisplay()
// ---------------------------------------------------------------------------

// Surface with thematic display
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(ThematicCompileTest, SurfaceWithThematicCompilesAndLinks)
{
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    createCommon(pb, false, false);
    addColor(pb);
    addSurfaceFlags(pb, false, false);
    addNormal(pb);
    addTexture(pb);
    addMaterial(pb, /*instanced*/true, /*quantized*/false);
    addLighting(pb);
    addFragData(pb);
    addThematicDisplay(pb);

    std::string const vert = pb.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = pb.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// ---------------------------------------------------------------------------
// SurfaceVariantCompiler — live Surface-Opaque program
// Drives the real SurfaceVariantCompiler::buildProgram (the variant glTF imports
// land on), not the helper-composed approximation above.
// ---------------------------------------------------------------------------

// The LIVE Surface-Opaque program (the variant glTF imports land on) must
// GL-compile + link. Drives the real SurfaceVariantCompiler::buildProgram —
// not the helper-composed approximation — so it catches the function-call /
// duplicate-definition bugs that crash the app.
// Authored: no reference test exists in itwinjs-core for offline GLSL compile
//           verification (itwinjs compiles live in a browser).
TEST(SurfaceCompileTest, SurfaceVariantCompilerSurfaceOpaqueCompilesAndLinks)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;                              // opaque, no features, no clip
    flags.positionType = PositionType::Unquantized;    // glTF polyface = non-quantized

    ShaderProgram prog;
    compiler.buildProgram(prog, flags);

    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// FUNDAMENTAL TEST: can the GL context actually RENDER (draw + readback)?
// Creates an FBO, draws a fullscreen red triangle, reads back the pixel.
// If this fails, GL rendering is broken on this machine — ALL app-level
// debugging is futile.
// Authored: diagnostic — isolates GL render capability from app complexity.
TEST(SurfaceCompileTest, HeadlessCanRenderAtAll)
{
    // Establish context
    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410\nvoid main(){}",
        "#version 410\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no GL context";

    // Create FBO + renderbuffer
    GLuint fbo; glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLuint rbo; glGenRenderbuffers(1, &rbo); glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    // Trivial program
    auto mk = [](GLenum t, const char* src) {
        GLuint s = glCreateShader(t); glShaderSource(s, 1, &src, nullptr); glCompileShader(s); return s;
    };
    GLuint p = glCreateProgram();
    glAttachShader(p, mk(GL_VERTEX_SHADER,
        "#version 410\nin vec2 a_pos; void main(){ gl_Position = vec4(a_pos,0,1); }"));
    glAttachShader(p, mk(GL_FRAGMENT_SHADER,
        "#version 410\nout vec4 fragColor; void main(){ fragColor = vec4(1,0,0,1); }"));
    glLinkProgram(p);
    GLint linked = GL_FALSE; glGetProgramiv(p, GL_LINK_STATUS, &linked);
    ASSERT_TRUE(linked) << "trivial program failed to link";
    glUseProgram(p);

    // Fullscreen triangle
    GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    GLuint vbo; glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float verts[] = { -1,-1, 3,-1, -1,3 };
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GLint loc = glGetAttribLocation(p, "a_pos");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Draw + readback
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, 256, 256);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFlush();

    float px[4] = {0,0,0,0};
    glReadPixels(128, 128, 1, 1, GL_RGBA, GL_FLOAT, px);
    std::cout << "  HEADLESS RENDER: center pixel = (" << px[0] << "," << px[1] << "," << px[2] << ")\n";

    EXPECT_GT(px[0], 0.5f) << "GL CANNOT render — expected red, got something else";

    glDeleteProgram(p); glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao);
    glDeleteRenderbuffers(1, &rbo); glDeleteFramebuffers(1, &fbo);
}

// Can the Surface VERTEX shader produce valid positions for cube geometry?
// Uses the real SurfaceVariantCompiler vertex source + a trivial red fragment.
// If this renders RED → vertex shader works → app pipeline is the bug.
// If WHITE → vertex shader produces degenerate positions → shader bug.
TEST(SurfaceCompileTest, HeadlessCubeViaSurfaceVertexShader)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    // BISECT: Surface vert + fragment that outputs v_color directly (skip chain)
    std::string const frag = prog.getFragSource();  // FULL Surface fragment shader

    test::GlslCompileResult probe = test::compileAndLinkProgram(
        "#version 410 core\nvoid main(){}", "#version 410 core\nvoid main(){}");
    if (!probe.contextAvailable) GTEST_SKIP() << "no GL context";

    // FBO
    GLuint fbo; glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLuint rbo; glGenRenderbuffers(1, &rbo); glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    // Compile+link Surface vert + trivial red frag, with attrib locations
    auto mk = [](GLenum t, std::string const& src) {
        GLuint s = glCreateShader(t);
        char const* p = src.c_str(); GLint l = static_cast<GLint>(src.size());
        glShaderSource(s, 1, &p, &l); glCompileShader(s); return s;
    };
    GLuint program = glCreateProgram();
    glAttachShader(program, mk(GL_VERTEX_SHADER, vert));
    glAttachShader(program, mk(GL_FRAGMENT_SHADER, frag));
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_normal");
    glBindAttribLocation(program, 2, "a_color");
    glBindAttribLocation(program, 3, "a_texCoord");
    glLinkProgram(program);
    GLint linked = GL_FALSE; glGetProgramiv(program, GL_LINK_STATUS, &linked);
    ASSERT_TRUE(linked) << "Surface vert + trivial frag failed to link";
    glUseProgram(program);

    // Cube VAO+VBO+IBO (8 verts, 12 tris, PolyfaceGraphic layout: pos3+normal3+color4+uv2+featId1 = 52 bytes)
    struct V { float p[3], n[3], c[4], uv[2]; uint32_t fid; };
    V verts[8];
    float const P[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    for (int i = 0; i < 8; ++i) {
        verts[i].p[0]=P[i][0]; verts[i].p[1]=P[i][1]; verts[i].p[2]=P[i][2];
        verts[i].n[0]=0; verts[i].n[1]=0; verts[i].n[2]=1;
        verts[i].c[0]=0.8f; verts[i].c[1]=0.2f; verts[i].c[2]=0.2f; verts[i].c[3]=1.0f;
        verts[i].uv[0]=0; verts[i].uv[1]=0; verts[i].fid=0;
    }
    uint32_t idx[36] = {1,6,5, 1,2,6, 0,7,3, 0,4,7, 2,7,6, 2,3,7,
                        0,5,4, 0,1,5, 4,5,6, 4,6,7, 0,2,1, 0,3,2};
    GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    GLuint vbo; glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GLuint ibo; glGenBuffers(1, &ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(24));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(40));

    // Set key uniforms: u_mvp = scale(0.5) so cube at NDC ±0.5 (visible)
    auto setMat4 = [&](const char* name, float const* m) {
        GLint l = glGetUniformLocation(program, name);
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m);
    };
    float mvp[16] = {0}; mvp[0]=0.5f; mvp[5]=0.5f; mvp[10]=0.5f; mvp[15]=1.0f;
    float ident[16] = {0}; ident[0]=1; ident[5]=1; ident[10]=1; ident[15]=1;
    setMat4("u_mvp", mvp);
    setMat4("u_mv", ident);
    GLint l;
    if ((l=glGetUniformLocation(program,"u_renderOrder"))>=0) glUniform1f(l, 0.0f);
    if ((l=glGetUniformLocation(program,"u_frustum"))>=0) glUniform3f(l, 1.0f, 100.0f, 2.0f);
    // Fragment uniforms: disable texture/lighting/monochrome → should output v_color (red)
    if ((l=glGetUniformLocation(program,"u_renderPass"))>=0) glUniform1f(l, 0.0f);
    if ((l=glGetUniformLocation(program,"u_surfaceFlags[0]"))>=0) { GLint z[12]={}; glUniform1iv(l,12,z); }
    if ((l=glGetUniformLocation(program,"u_shaderFlags[0]"))>=0) { GLuint z[5]={}; glUniform1uiv(l,5,z); }
    if ((l=glGetUniformLocation(program,"u_applyGlyphTex"))>=0) glUniform1i(l, 0);
    if ((l=glGetUniformLocation(program,"u_reverseWhiteOnWhite"))>=0) glUniform1i(l, 0);
    // Set u_materialColor to the cube's baseColorFactor — without this, applyMaterialColor
    // mixes v_color with u_materialColor=(0,0,0,0) → mix(red, black, 1) = BLACK.
    if ((l=glGetUniformLocation(program,"u_materialColor"))>=0) glUniform4f(l, 0.8f, 0.2f, 0.2f, 1.0f);

    // Draw
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glViewport(0, 0, 256, 256);
    glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    glFlush();

    float px[4] = {0,0,0,0};
    glReadPixels(128, 128, 1, 1, GL_RGBA, GL_FLOAT, px);
    std::cout << "  SURFACE VERT CUBE: center pixel = (" << px[0] << "," << px[1] << "," << px[2] << ")\n";

    EXPECT_GT(px[0], 0.5f) << "Surface vertex shader produced NO visible geometry — shader bug";

    glDeleteProgram(program); glDeleteBuffers(1, &vbo); glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao); glDeleteRenderbuffers(1, &rbo); glDeleteFramebuffers(1, &fbo);
}

// Reproduce the live-app "Compiled vertex/fragment shader was corrupt" link
// failure through the REAL rhi::OpenGLProgram::compile path (what the app
// uses), NOT the harness's re-create-and-link approach. If this FAILS while the
// harness PASSES above, the bug is in OpenGLProgram::compile's
// attach-same-shaders-then-link sequence (or a context-current issue).
// Authored: diagnostic repro for the macOS Apple-GL "corrupt shader" link error.
TEST(SurfaceCompileTest, SurfaceVariantCompilerCompilesViaRealRhiProgram)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    // Establish a headless GL context (side effect: leaves it current for the
    // real-OpenGLProgram compile below). Also confirms the harness links it.
    test::GlslCompileResult probe = test::compileAndLinkProgram(vert, frag);
    if (!probe.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available";
    }
    ASSERT_TRUE(probe.linked) << "harness failed to link; source is broken:\n" << probe.fragLog;

    // Now compile through the REAL rhi::OpenGLProgram path with the context current.
    rhi::OpenGLProgram rhiProg;
    bool const ok = rhiProg.compile(vert.c_str(), frag.c_str(), "Surface-Opaque-RHI-repro");
    EXPECT_TRUE(ok) << "rhi::OpenGLProgram::compile FAILED — reproduces the live 'corrupt "
                       "shader' link failure; the harness linked the same source fine";
}

// Regression: the Surface program's attributes must bind to PolyfaceGraphic's VAO
// layout (0=pos,1=normal,2=color,3=texCoord,4=featureId). SurfaceVariantCompiler
// sets these via setAttributeMap; OpenGLProgram::compile binds them with
// glBindAttribLocation BEFORE link. Before the fix, GL auto-assigned locations and
// a_color landed on 3 (texCoord data) → invisible cube.
// Authored: no itwinjs offline test for explicit GL attribute binding.
TEST(SurfaceCompileTest, SurfaceAttribLocationsBoundExplicitly)
{
    SurfaceVariantCompiler compiler;
    TechniqueFlags flags;
    flags.positionType = PositionType::Unquantized;
    ShaderProgram prog;
    compiler.buildProgram(prog, flags);
    std::string const vert = prog.getVertSource();
    std::string const frag = prog.getFragSource();

    test::GlslCompileResult probe = test::compileAndLinkProgram(vert, frag);
    if (!probe.contextAvailable) GTEST_SKIP() << "no GL context";
    ASSERT_TRUE(probe.linked) << probe.linkLog;

    // Compile through the real rhi::OpenGLProgram path WITH explicit attribute
    // locations (mirrors SurfaceVariantCompiler's setAttributeMap → Program path).
    std::vector<std::pair<std::string, uint8_t>> attrLocs = {
        {"a_position", 0}, {"a_normal", 1}, {"a_color", 2},
        {"a_texCoord", 3}, {"a_featureId", 4},
    };
    rhi::OpenGLProgram rhiProg;
    ASSERT_TRUE(rhiProg.compile(vert.c_str(), frag.c_str(), "Surface-Opaque-attribs", attrLocs))
        << "compile+link failed with explicit attribute locations";

    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_position"), 0);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_normal"), 1);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_color"), 2);
    EXPECT_EQ(glGetAttribLocation(rhiProg.getProgram(), "a_texCoord"), 3);
}

// ---------------------------------------------------------------------------
// Indexed Edge compile test
// Ported from: itwinjs-core Edge.ts createEdgeBuilder(IndexedEdge)
// ---------------------------------------------------------------------------

// IndexedEdge: LUT-based edge rendering with silhouette detection
// Authored: no reference test exists in itwinjs-core for offline GLSL compile verification
TEST(EdgeCompileTest, IndexedEdgeCompilesAndLinks)
{
    auto builder = createEdgeProgramBuilder(EdgeBuilderType::IndexedEdge,
                                            FeatureMode::None, PositionType::Unquantized);

    std::string const vert = builder.getVertexBuilder().buildSourceWithComponents();
    std::string const frag = builder.getFragmentBuilder().buildSourceWithComponents();

    test::GlslCompileResult r = test::compileAndLinkProgram(vert, frag);
    if (!r.contextAvailable) {
        GTEST_SKIP() << "No offscreen GL context available; cannot verify GLSL compilation";
    }

    EXPECT_TRUE(r.vertCompiled)
        << "vertex shader compile failed:\n" << r.vertLog
        << "\n--- vertex source ---\n" << vert;
    EXPECT_TRUE(r.fragCompiled)
        << "fragment shader compile failed:\n" << r.fragLog
        << "\n--- fragment source ---\n" << frag;
    EXPECT_TRUE(r.linked)
        << "program link failed:\n" << r.linkLog;
}

// buildFragmentMain function-call convention: each set slot is wrapped as a
// named GLSL function and called once in main(), baseColor declared once.
// Ported from: itwinjs-core ShaderBuilder.ts FragmentShaderBuilder.buildSource (971-1147)
TEST(SurfaceCompileTest, FragmentFunctionCallMainWrapsSlotsAsFunctions)
{
    ShaderBuilder frag;
    frag.setStage(ShaderStage::Fragment);
    frag.setFunctionCallFragMain(true);
    frag.setFragmentComponent(FragmentShaderComponent::ComputeBaseColor, "  return v_color;\n");
    frag.setFragmentComponent(FragmentShaderComponent::ApplyMonochrome, "  return baseColor;\n");
    frag.setFragmentComponent(FragmentShaderComponent::AssignFragData, "  fragColor = baseColor;\n");
    frag.addCode("out vec4 fragColor;\n");

    auto count = [](std::string const& haystack, std::string const& needle) -> long {
        long n = 0; size_t pos = 0;
        while ((pos = haystack.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
        return n;
    };

    std::string const src = frag.buildSourceWithComponents();
    std::string const main = src.substr(src.find("void main()"));

    // Each set slot is wrapped as a named GLSL function (function-call convention).
    EXPECT_NE(src.find("vec4 computeBaseColor()"), std::string::npos);
    EXPECT_NE(src.find("vec4 applyMonochrome(vec4 baseColor)"), std::string::npos);
    EXPECT_NE(src.find("void assignFragData(vec4 baseColor)"), std::string::npos);

    // main() declares baseColor exactly once and calls each slot exactly once
    // (the inline-convention bug redeclared/double-invoked — count guards it).
    EXPECT_EQ(count(main, "vec4 baseColor = computeBaseColor();"), 1);
    EXPECT_EQ(count(main, "baseColor = applyMonochrome(baseColor);"), 1);
    EXPECT_EQ(count(main, "assignFragData(baseColor);"), 1);
}
