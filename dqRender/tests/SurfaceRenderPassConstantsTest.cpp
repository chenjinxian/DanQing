// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Surface render-pass constant fidelity test.
// Authored: no reference test exists for this contract. Regression guard for the
// ACS-triad-fill-opacity bug: SurfaceCommon.h's addFragData re-implemented the
// kRenderPass_* constants with DEVIANANT values (OpaqueLinear=0, OpaquePlanar=1,
// OpaqueGeneral=2) instead of the authoritative RenderPass enum values
// (2/3/5) that RenderPassShaders.h addRenderPass() uses.
//
// Why a value test (not a compile test): the GLSL-compile harness verifies the
// assembled shader COMPILES, but a wrong constant VALUE compiles fine — only a
// value-level assertion catches it. The deviant constants made the assignFragData
// check `u_renderPass in [OpaqueLinear..OpaqueGeneral]` TRUE for the default
// u_renderPass=0 (never uploaded), forcing `baseColor.a = 1.0` for EVERY surface
// fragment — so the triad's translucent overlay fill (alpha 0.216) rendered fully
// opaque (thick arrows, solid disc) instead of premultiplied/faint. With the
// faithful constants, 0 falls outside [2..5] → the else branch premultiplies.
#include "gl/RenderFlags.h"      // RenderPass enum (authoritative values)
#include "render/ShaderBuilder.h"
#include "render/SurfaceCommon.h"  // addFragData

#include <gtest/gtest.h>

#include <string>

using namespace dqRender;

namespace {
// Compose the Surface fragment foundation + addFragData (which registers
// u_renderPass + the kRenderPass_* constants + the assignFragData slot), and
// return the generated fragment source. Mirrors SurfaceCompileTest's setup so
// buildSourceWithComponents emits a well-formed source. No GL context needed —
// this inspects generated GLSL text, not a compiled program.
std::string buildSurfaceFragSource() {
    ProgramBuilder pb;
    pb.getVertexBuilder().setVersion("410 core");
    pb.getFragmentBuilder().setVersion("410 core");
    pb.enableFunctionCallVertexMain();
    pb.enableFunctionCallFragmentMain();

    createCommon(pb, /*instanced*/ false, /*quantized*/ false);
    addColor(pb);
    addFragData(pb);

    return pb.getFragmentBuilder().buildSourceWithComponents();
}
}  // namespace

// The Surface fragment must define kRenderPass_OpaqueGeneral / _OpaqueLinear
// with the faithful RenderPass enum values (5 / 2), matching RenderPassShaders.h
// addRenderPass(). Deviant values (e.g. OpaqueGeneral=2, OpaqueLinear=0) make the
// default u_renderPass=0 fall inside the opaque range → alpha forced to 1 →
// translucent overlay fills (ACS triad) render opaque.
TEST(SurfaceRenderPassConstantsTest, FragDataUsesFaithfulRenderPassConstants)
{
    std::string const src = buildSurfaceFragSource();

    std::string const general = "kRenderPass_OpaqueGeneral = " +
        std::to_string(static_cast<int>(RenderPass::OpaqueGeneral));
    std::string const linear = "kRenderPass_OpaqueLinear = " +
        std::to_string(static_cast<int>(RenderPass::OpaqueLinear));

    EXPECT_NE(src.find(general), std::string::npos)
        << "Surface fragment must use the faithful kRenderPass_OpaqueGeneral ("
        << static_cast<int>(RenderPass::OpaqueGeneral)
        << ", from the RenderPass enum / addRenderPass). A deviant value makes "
        << "translucent overlay fills render opaque (ACS triad bug).\n--- src ---\n"
        << src;
    EXPECT_NE(src.find(linear), std::string::npos)
        << "Surface fragment must use the faithful kRenderPass_OpaqueLinear ("
        << static_cast<int>(RenderPass::OpaqueLinear) << ").\n--- src ---\n" << src;
}
