// SPDX-License-Identifier: Apache-2.0
// Ported from: filament filament/src/driver/HandleBase.h, RasterState.h, StencilState.h
// DanQing dqRender — RHI integration tests
// Tests for the RHI layer and rendering skeleton.
#include <gtest/gtest.h>

#include "dqRender/rhi/Handle.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/BufferDescriptor.h"
#include "dqRender/rhi/Program.h"
#include "dqRender/rhi/PipelineState.h"

using namespace dqRender::rhi;

// ---------------------------------------------------------------------------
// Handle tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/HandleBase.h
//              TEST(HandleTest, DefaultIsNull)
TEST(HandleTest, DefaultIsNull)
{
    Handle<HwTexture> h;
    EXPECT_FALSE(h);
    EXPECT_EQ(h.getId(), HandleBase::nullid);
}

// Ported from: filament filament/src/driver/HandleBase.h
//              TEST(HandleTest, BoolConversion)
TEST(HandleTest, BoolConversion)
{
    Handle<HwTexture> h;
    EXPECT_FALSE(static_cast<bool>(h));
}

// ---------------------------------------------------------------------------
// RasterState tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/RasterState.h
//              TEST(RasterStateTest, DefaultValues)
TEST(RasterStateTest, DefaultValues)
{
    RasterState rs;
    EXPECT_EQ(rs.culling, CullingMode::BACK);
    EXPECT_EQ(rs.depthFunc, DepthFunc::LEQUAL);
    EXPECT_TRUE(rs.depthWrite);
    EXPECT_TRUE(rs.colorWrite);
    EXPECT_EQ(rs.blendEquationRGB, BlendEquation::ADD);
    EXPECT_EQ(rs.blendFunctionSrcRGB, BlendFunction::ONE);
    EXPECT_EQ(rs.blendFunctionDstRGB, BlendFunction::ZERO);
    EXPECT_FALSE(rs.alphaToCoverage);
    EXPECT_FALSE(rs.inverseFrontFaces);
}

// Ported from: filament filament/src/driver/RasterState.h
//              TEST(RasterStateTest, SizeIs4Bytes)
TEST(RasterStateTest, SizeIs4Bytes)
{
    EXPECT_EQ(sizeof(RasterState), 4u);
}

// ---------------------------------------------------------------------------
// StencilState tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/StencilState.h
//              TEST(StencilStateTest, DefaultValues)
TEST(StencilStateTest, DefaultValues)
{
    StencilState ss;
    EXPECT_EQ(ss.front.function, StencilFunction::ALWAYS);
    EXPECT_EQ(ss.front.stencilFail, StencilOperation::KEEP);
    EXPECT_EQ(ss.readMask, 0xff);
    EXPECT_EQ(ss.writeMask, 0xff);
    EXPECT_EQ(ss.ref, 0);
}

// Ported from: filament filament/src/driver/StencilState.h
//              TEST(StencilStateTest, SizeIsReasonable)
TEST(StencilStateTest, SizeIsReasonable)
{
    // Size depends on compiler bit-field packing (8-12 bytes typical).
    EXPECT_GE(sizeof(StencilState), 8u);
    EXPECT_LE(sizeof(StencilState), 16u);
}

// ---------------------------------------------------------------------------
// BufferDescriptor tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/BufferDescriptor.h
//              TEST(BufferDescriptorTest, MoveConstruct)
TEST(BufferDescriptorTest, MoveConstruct)
{
    float data[] = {1.0f, 2.0f, 3.0f};
    BufferDescriptor desc(data, sizeof(data));
    EXPECT_EQ(desc.buffer(), data);
    EXPECT_EQ(desc.size(), sizeof(data));

    BufferDescriptor desc2(std::move(desc));
    EXPECT_EQ(desc2.buffer(), data);
    EXPECT_EQ(desc2.size(), sizeof(data));
    EXPECT_EQ(desc.buffer(), nullptr);
    EXPECT_EQ(desc.size(), 0u);
}

// Ported from: filament filament/src/driver/BufferDescriptor.h
//              TEST(BufferDescriptorTest, CallbackOnDestroy)
TEST(BufferDescriptorTest, CallbackOnDestroy)
{
    bool called = false;
    {
        float data[] = {1.0f};
        BufferDescriptor desc(data, sizeof(data),
                              [](void*, size_t, void* user) {
                                  *static_cast<bool*>(user) = true;
                              },
                              &called);
    }
    EXPECT_TRUE(called);
}

// ---------------------------------------------------------------------------
// Program builder tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/Program.h
//              TEST(ProgramTest, FluentApi)
TEST(ProgramTest, FluentApi)
{
    Program prog;
    std::string vertSrc = "#version 410\nvoid main() {}";
    std::string fragSrc = "#version 410\nvoid main() {}";

    prog.shader(ShaderStage::VERTEX, vertSrc)
        .shader(ShaderStage::FRAGMENT, fragSrc)
        .shaderLanguage(ShaderLanguage::ESSL3)
        .name("test_program");

    EXPECT_EQ(prog.getName(), "test_program");
    EXPECT_EQ(prog.getShaderLanguage(), ShaderLanguage::ESSL3);
    EXPECT_EQ(prog.getShaderSource(ShaderStage::VERTEX).size(), vertSrc.size());
    EXPECT_EQ(prog.getShaderSource(ShaderStage::FRAGMENT).size(), fragSrc.size());
}

// ---------------------------------------------------------------------------
// PipelineState tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/PipelineState.h
//              TEST(PipelineStateTest, DefaultValues)
TEST(PipelineStateTest, DefaultValues)
{
    PipelineState ps;
    EXPECT_EQ(ps.primitiveType, PrimitiveType::TRIANGLES);
    EXPECT_EQ(ps.rasterState.culling, CullingMode::BACK);
}

// ---------------------------------------------------------------------------
// SamplerParams tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/DriverEnums.h
//              TEST(SamplerParamsTest, SizeIsReasonable)
TEST(SamplerParamsTest, SizeIsReasonable)
{
    // Size depends on compiler bit-field packing (3-4 bytes typical).
    EXPECT_GE(sizeof(SamplerParams), 3u);
    EXPECT_LE(sizeof(SamplerParams), 8u);
}

// ---------------------------------------------------------------------------
// RenderPassParams tests
// ---------------------------------------------------------------------------
// Ported from: filament filament/src/driver/DriverEnums.h
//              TEST(RenderPassParamsTest, DefaultValues)
TEST(RenderPassParamsTest, DefaultValues)
{
    RenderPassParams params;
    EXPECT_EQ(params.clearDepth, 1.0);
    EXPECT_EQ(params.clearStencil, 0u);
}
