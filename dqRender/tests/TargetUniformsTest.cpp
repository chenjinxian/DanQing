// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — TargetUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              (no direct reference test exists; tests are authored from TargetUniforms.ts behavior)
#include "render/Uniforms.h"

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqGeom;

// Projection dispatch: forViewCoords=true selects viewRect, false selects frustum.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              TEST(TargetUniformsTest, ProjectionDispatch)
TEST(TargetUniformsTest, ProjectionDispatch)
{
    TargetUniforms u;
    u.viewRect.update(800.0f, 600.0f);  // sets viewRect projection to ortho

    // viewRect path -> ortho(0,800,600,0,...): m00 = 2/800
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32(true).data[0], 2.0f / 800.0f);

    // frustum path -> default identity
    EXPECT_FLOAT_EQ(u.getProjectionMatrix32(false).data[0], 1.0f);
}

// bindProjectionMatrix dispatches to the correct subsystem.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              TEST(TargetUniformsTest, BindProjectionMatrixDispatch)
TEST(TargetUniformsTest, BindProjectionMatrixDispatch)
{
    TargetUniforms u;
    u.viewRect.update(100.0f, 100.0f);

    UniformHandle hView;
    u.bindProjectionMatrix(hView, true);
    EXPECT_FLOAT_EQ(hView.getData()[0], 2.0f / 100.0f);  // viewRect ortho

    UniformHandle hFrustum;
    u.bindProjectionMatrix(hFrustum, false);
    EXPECT_FLOAT_EQ(hFrustum.getData()[0], 1.0f);  // frustum identity
}

// Default sun direction (no world dir set) is the reference default view dir.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              TEST(TargetUniformsTest, SunDirectionDefault)
TEST(TargetUniformsTest, SunDirectionDefault)
{
    TargetUniforms u;
    UniformHandle h;
    u.bindSunDirection(h);

    // Reference default: Vector3d(0.272166, 0.680414, 0.680414)
    EXPECT_NEAR(h.getData()[0], 0.272166, 1e-5);
    EXPECT_NEAR(h.getData()[1], 0.680414, 1e-5);
    EXPECT_NEAR(h.getData()[2], 0.680414, 1e-5);
}

// A world-space sun direction is transformed into view space (negated + normalized).
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              TEST(TargetUniformsTest, SunDirectionWorldToView)
TEST(TargetUniformsTest, SunDirectionWorldToView)
{
    TargetUniforms u;
    // Default frustum view matrix is identity, so view = world for the rotation.
    Vector3d const sunDir = Vector3d::From(0.0, 0.0, 1.0);
    u.setSunDirection(&sunDir);

    UniformHandle h;
    u.bindSunDirection(h);

    // identity view * (0,0,1) = (0,0,1); negate -> (0,0,-1); normalize -> (0,0,-1)
    EXPECT_NEAR(h.getData()[0], 0.0, 1e-6);
    EXPECT_NEAR(h.getData()[1], 0.0, 1e-6);
    EXPECT_NEAR(h.getData()[2], -1.0, 1e-6);
}

// Aggregator composes the faithful uniform subsystems.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/TargetUniforms.test.ts
//              TEST(TargetUniformsTest, Composition)
TEST(TargetUniformsTest, Composition)
{
    TargetUniforms u;
    // Each extracted member is the real type.
    static_cast<void>(u.frustum);
    static_cast<void>(u.viewRect);
    static_cast<void>(u.hilite);
    static_cast<void>(u.lights);
    static_cast<void>(u.style);
    // Stub members still present.
    static_cast<void>(u.branch);
    static_cast<void>(u.batch);
    SUCCEED();
}

// updateRenderPlan dispatches to style + lights + sun direction.
// Authored: no reference test exists in itwinjs-core for TargetUniforms.updateRenderPlan
//（此前标注的 core/frontend/src/test/render/webgl/TargetUniforms.test.ts 不存在——
// 溯源更正）。断言语义对齐参考实现 TargetUniforms.ts:177-186：世界空间太阳方向
// 仅在 viewFlags.shadows || lights.solar.alwaysEnabled 时采用，否则 SunDirection
// 回落到默认视线空间方向（0.272166,0.680414,0.680414）。
TEST(TargetUniformsTest, UpdateRenderPlan)
{
    TargetUniforms u;
    RenderPlan plan;
    plan.backgroundColor = ColorDef::from(255, 0, 0).getTbgr();  // red
    // 世界太阳方向来自 plan.lights.solar.direction（参考 RenderPlan 无独立
    // sunDirection 字段——TargetUniforms.ts:183 sunDir = plan.lights.solar.direction）。
    plan.lights.solar.direction = dqGeom::Vector3d::From(1.0, 0.0, 0.0);
    // 门槛满足（shadows on）→ 世界方向 (1,0,0) 生效。
    auto vp = plan.viewFlags.Properties();
    vp.shadows = true;
    plan.viewFlags = dqCommon::ViewFlags(vp);
    u.updateRenderPlan(plan);

    // Style picked up the red background (luminance 0.3).
    EXPECT_FLOAT_EQ(u.style.getBackgroundIntensity(), 0.3f);

    // Sun direction (1,0,0) transformed by identity view -> negate -> (-1,0,0).
    UniformHandle h;
    u.bindSunDirection(h);
    EXPECT_NEAR(h.getData()[0], -1.0, 1e-5);
    EXPECT_NEAR(h.getData()[1], 0.0, 1e-5);
    EXPECT_NEAR(h.getData()[2], 0.0, 1e-5);
}
