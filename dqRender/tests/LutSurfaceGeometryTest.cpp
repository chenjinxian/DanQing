// SPDX-License-Identifier: Apache-2.0
// Authored: no reference unit test exists in itwinjs-core for the C++-side
// geometry object itself（参考侧 SurfaceGeometry 由 System 创建后经 WebGL 渲染
// 测试覆盖）；行为锁在 Task 5 的像素回归。
// DanQing dqRender — SurfaceGeometry LUT 形态（量化 imdl 网格）接口语义测试。
//
// Ported from: itwinjs-core SurfaceGeometry.ts（构造的 BuffersContainer + a_pos
// 24-bit UBYTE3 索引 attribute :378-387；_draw 的 drawArrays :150-162；
// LUTGeometry.usesQuantizedPositions CachedGeometry.ts:98/207）——几何形态断言；
// NullDriver/空 handle 构造形态参照 PolylineGeometry 既有测试（PolylineRenderTest）。
#include <gtest/gtest.h>

#include "NullDriver.h"

#include "render/SurfaceGeometry.h"
#include "render/VertexLutTexture.h"

#include <utility>

using namespace dqRender;
using namespace dqRender::rhi;

// Authored: no reference unit test exists in itwinjs-core for the C++-side
//           geometry object itself（参考侧 SurfaceGeometry 由 System 创建后经
//           WebGL 渲染测试覆盖）；行为锁在 Task 5 的像素回归。
TEST(LutSurfaceGeometryTest, LutFormReportsQuantizedAndExposesLut)
{
    NullDriver driver;
    VertexLutTexture lut;  // 空 LUT（不 create）——本测试只锁形态/接口语义
    SurfaceGeometry geom(driver, std::move(lut), /*lutIndexBuffer*/ {},
                         /*numIndices*/ 9, SurfaceType::Opaque, /*isPlanar*/ true,
                         /*hasTextures*/ false);
    EXPECT_TRUE(geom.usesQuantizedPositions());
    EXPECT_EQ(geom.getTechniqueId(), TechniqueId::Surface);
    EXPECT_EQ(geom.getNumIndices(), 9u);
    EXPECT_EQ(geom.getLutIndexBuffer(), BufferObjectHandle{});
    // 量化原点/缩放经基类 LUT 转发（CachedGeometry.ts:208-209 qOrigin/qScale ← lut）。
    EXPECT_EQ(geom.getQOrigin(), geom.getLut().getQOrigin());
    EXPECT_EQ(geom.getQScale(), geom.getLut().getQScale());
}

// Authored: 同上——VBO 形态（既有构造器）零变化回归：usesQuantizedPositions
//           必须保持 false（MeshGeometry 注释的 PolyfaceGraphic/MeshRenderGeometry
//           非量化路径约定）。
TEST(LutSurfaceGeometryTest, VboFormIsNotQuantized)
{
    NullDriver driver;
    SurfaceGeometry geom(driver, rhi::IndexBufferHandle{}, /*numIndices*/ 36,
                         SurfaceType::Opaque, /*isPlanar*/ false, /*hasTextures*/ false);
    EXPECT_FALSE(geom.usesQuantizedPositions());
    EXPECT_EQ(geom.getTechniqueId(), TechniqueId::Surface);
    EXPECT_EQ(geom.getNumIndices(), 36u);
}

// Authored: 同上——量化路径几何持色（u_color 源；Color.ts:51-60 u_color ←
//           lutGeom.getColor(target).uniform，对齐 PolylineGeometry::getColor
//           Polyline.ts:129-131）。
TEST(LutSurfaceGeometryTest, LutFormHoldsUniformColor)
{
    NullDriver driver;
    VertexLutTexture lut;
    SurfaceGeometry geom(driver, std::move(lut), /*lutIndexBuffer*/ {},
                         /*numIndices*/ 3, SurfaceType::Opaque, /*isPlanar*/ false,
                         /*hasTextures*/ false);
    dqCommon::ColorDef const red = dqCommon::ColorDef::fromTbgr(0x000000ff);  // r=255
    geom.setColor(red);
    EXPECT_EQ(geom.getColor().getTbgr(), red.getTbgr());
}
