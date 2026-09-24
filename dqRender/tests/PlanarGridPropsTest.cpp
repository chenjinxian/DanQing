// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/render/RenderSystem.ts:68-96
//              (PlanarGridTransparency / PlanarGridProps defaults)
// DanQing dqRender - PlanarGridProps unit tests
#include "dqRender/PlanarGridProps.h"

#include <gtest/gtest.h>

using namespace dqRender;

// Ported from: PlanarGridTransparency defaults (RenderSystem.ts:73-77).
TEST(PlanarGridPropsTest, TransparencyDefaults)
{
    PlanarGridTransparency t;
    EXPECT_FLOAT_EQ(t.planeTransparency, 0.9f);
    EXPECT_FLOAT_EQ(t.lineTransparency, 0.75f);
    EXPECT_FLOAT_EQ(t.refTransparency, 0.5f);
}

// Ported from: PlanarGridProps default construction (RenderSystem.ts:83-96).
TEST(PlanarGridPropsTest, PropsDefault)
{
    PlanarGridProps p;
    EXPECT_EQ(p.gridsPerRef, 0.0);
    EXPECT_FALSE(p.transparency.has_value());
    // rMatrix defaults to identity.
    EXPECT_NEAR(p.rMatrix.RowZ().z, 1.0, 1e-12);
}
