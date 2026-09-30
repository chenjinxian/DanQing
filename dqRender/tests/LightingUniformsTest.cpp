// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — LightingUniforms tests
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              (no direct reference test exists; tests are authored from LightingUniforms.ts behavior)
#include "render/LightingUniforms.h"
#include "render/Uniforms.h"

#include <dqCommon/Frustum.h>
#include <dqCommon/Npc.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Vector3d.h>

#include <cmath>
#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;
using namespace dqGeom;

// Default-constructed uniforms are uninitialized (all-zero data) until update().
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, DefaultUninitialized)
TEST(LightingUniformsTest, DefaultUninitialized)
{
    LightingUniforms u;
    for (int i = 0; i < 16; ++i)
        EXPECT_FLOAT_EQ(u.getData()[i], 0.0f);
}

// update() packs the 16-float lighting array per the reference layout.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, UpdatePacksLayout)
TEST(LightingUniformsTest, UpdatePacksLayout)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 2.0;
    s.ambient.color = RgbColor(255, 0, 0);       // red
    s.ambient.intensity = 0.5;
    s.hemisphere.lowerColor = RgbColor(0, 255, 0);  // green
    s.hemisphere.upperColor = RgbColor(0, 0, 255);  // blue
    s.hemisphere.intensity = 0.75;
    s.portraitIntensity = 0.4;
    s.specularIntensity = 1.5;
    s.numCels = 3;
    s.fresnel.intensity = 0.6;
    s.fresnel.invert = false;

    u.update(s);

    auto const& d = u.getData();
    EXPECT_FLOAT_EQ(d[0], 2.0f);                    // solar intensity
    EXPECT_FLOAT_EQ(d[1], 1.0f); EXPECT_FLOAT_EQ(d[2], 0.0f); EXPECT_FLOAT_EQ(d[3], 0.0f);  // ambient red
    EXPECT_FLOAT_EQ(d[4], 0.5f);                    // ambient intensity
    EXPECT_FLOAT_EQ(d[5], 0.0f); EXPECT_FLOAT_EQ(d[6], 1.0f); EXPECT_FLOAT_EQ(d[7], 0.0f);  // lower green
    EXPECT_FLOAT_EQ(d[8], 0.0f); EXPECT_FLOAT_EQ(d[9], 0.0f); EXPECT_FLOAT_EQ(d[10], 1.0f); // upper blue
    EXPECT_FLOAT_EQ(d[11], 0.75f);                  // hemisphere intensity
    EXPECT_FLOAT_EQ(d[12], 0.4f);                   // portrait
    EXPECT_FLOAT_EQ(d[13], 1.5f);                   // specular
    EXPECT_FLOAT_EQ(d[14], 3.0f);                   // num cels
    EXPECT_FLOAT_EQ(d[15], 0.6f);                   // fresnel (positive)
}

// Fresnel invert negates the stored intensity.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, FresnelInvertedNegates)
TEST(LightingUniformsTest, FresnelInvertedNegates)
{
    LightingUniforms u;
    LightSettings s;
    s.fresnel.intensity = 0.6;
    s.fresnel.invert = true;
    u.update(s);
    EXPECT_FLOAT_EQ(u.getData()[15], -0.6f);
}

// bind() uploads 16 floats.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, BindUploadsArray)
TEST(LightingUniformsTest, BindUploadsArray)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 1.0;
    u.update(s);

    UniformHandle h;
    u.bind(h);
    EXPECT_EQ(h.getType(), UniformHandle::UniformType::FloatArray);
    EXPECT_FLOAT_EQ(h.getData()[0], 1.0f);
}

// Updating with equal settings is a no-op.
// Ported from: itwinjs-core core/frontend/src/test/render/webgl/LightingUniforms.test.ts
//              TEST(LightingUniformsTest, UpdateEqualSettingsNoChange)
TEST(LightingUniformsTest, UpdateEqualSettingsNoChange)
{
    LightingUniforms u;
    LightSettings s;
    s.solar.intensity = 5.0;
    u.update(s);
    u.update(s);  // equal -> no recompute
    EXPECT_FLOAT_EQ(u.getData()[0], 5.0f);
}

// ---------------------------------------------------------------------------
// SunDirection（TargetUniforms 太阳方向——M-M(1) frustum 重同步腿）。
// Authored: no reference test exists in itwinjs-core for SunDirection
// (browser-only); the math is TargetUniforms.ts:99-113 — a world direction is
// transformed into view space and negated; no world direction falls back to the
// constant view-space default (0.272166, 0.680414, 0.680414). The reference
// re-runs the transform whenever the frustum desyncs (sync(uniforms.frustum,
// this)); the second test pins that leg with a rotated view.
// ---------------------------------------------------------------------------

static Frustum makeSunBoxFrustum()
{
    // Axis-aligned box: near z=-5, far z=+5 — view rows = world X/Y/Z.
    Frustum f;
    f.getCorner(Npc::LeftBottomRear) = Point3d::From(-10, -10, 5);
    f.getCorner(Npc::RightBottomRear) = Point3d::From(10, -10, 5);
    f.getCorner(Npc::LeftTopRear) = Point3d::From(-10, 10, 5);
    f.getCorner(Npc::RightTopRear) = Point3d::From(10, 10, 5);
    f.getCorner(Npc::LeftBottomFront) = Point3d::From(-10, -10, -5);
    f.getCorner(Npc::RightBottomFront) = Point3d::From(10, -10, -5);
    f.getCorner(Npc::LeftTopFront) = Point3d::From(-10, 10, -5);
    f.getCorner(Npc::RightTopFront) = Point3d::From(10, 10, -5);
    return f;
}

// No world direction -> constant view-space default, regardless of frustum.
TEST(SunDirection, NoWorldDirUsesViewSpaceDefault)
{
    SunDirection sd;
    FrustumUniforms fr;
    fr.changeFrustum(makeSunBoxFrustum(), 0.0, true);

    UniformHandle h;
    sd.bind(h, fr);
    float const* d = sd.getSunDirView();
    EXPECT_NEAR(d[0], 0.272166f, 1e-5f);
    EXPECT_NEAR(d[1], 0.680414f, 1e-5f);
    EXPECT_NEAR(d[2], 0.680414f, 1e-5f);
}

// World direction (shadows-on path): view-space result follows the view matrix
// AND is recomputed when the frustum changes without a new update() call —
// the M-M(1) fix (previously a stale direction survived camera motion).
TEST(SunDirection, RecomputesOnFrustumChange)
{
    SunDirection sd;
    FrustumUniforms fr;

    // Identity view: world +X sun -> view-space -(1,0,0).
    Vector3d worldSun = Vector3d::From(1.0, 0.0, 0.0);
    sd.update(&worldSun);
    fr.changeFrustum(makeSunBoxFrustum(), 0.0, true);
    UniformHandle h;
    sd.bind(h, fr);
    EXPECT_NEAR(sd.getSunDirView()[0], -1.0f, 1e-5f);
    EXPECT_NEAR(sd.getSunDirView()[1], 0.0f, 1e-5f);
    EXPECT_NEAR(sd.getSunDirView()[2], 0.0f, 1e-5f);

    // Rotate the view 90° about world Z ((x,y,z) -> (-y,x,z) per corner):
    // viewX = world Y, viewY = world -X. World +X sun now maps to view -Y,
    // then negates to view +Y — WITHOUT another update() call.
    Frustum rot = makeSunBoxFrustum();
    auto rotCorner = [](Point3d c) { return Point3d::From(-c.y, c.x, c.z); };
    rot.getCorner(Npc::LeftBottomRear) = rotCorner(rot.getCorner(Npc::LeftBottomRear));
    rot.getCorner(Npc::RightBottomRear) = rotCorner(rot.getCorner(Npc::RightBottomRear));
    rot.getCorner(Npc::LeftTopRear) = rotCorner(rot.getCorner(Npc::LeftTopRear));
    rot.getCorner(Npc::RightTopRear) = rotCorner(rot.getCorner(Npc::RightTopRear));
    rot.getCorner(Npc::LeftBottomFront) = rotCorner(rot.getCorner(Npc::LeftBottomFront));
    rot.getCorner(Npc::RightBottomFront) = rotCorner(rot.getCorner(Npc::RightBottomFront));
    rot.getCorner(Npc::LeftTopFront) = rotCorner(rot.getCorner(Npc::LeftTopFront));
    rot.getCorner(Npc::RightTopFront) = rotCorner(rot.getCorner(Npc::RightTopFront));
    fr.changeFrustum(rot, 0.0, true);
    sd.bind(h, fr);
    float const* d1 = sd.getSunDirView();
    EXPECT_NEAR(d1[0], 0.0f, 1e-5f);
    EXPECT_NEAR(d1[1], 1.0f, 1e-5f);
    EXPECT_NEAR(d1[2], 0.0f, 1e-5f);
}
