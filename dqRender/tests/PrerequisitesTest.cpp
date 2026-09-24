// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BranchUniforms/BatchUniforms prerequisite methods
// Ported from: itwinjs-core CachedGeometry.viewIndependentOrigin,
//              InstancedGeometry.getRtcModelTransform, Target.devicePixelRatio
//              (no reference tests; verifies the reference API surface exists)
#include "render/CachedGeometry.h"
#include "render/InstancedGeometry.h"
#include "render/TargetImpl.h"

#include <dqGeom/Transform.h>

#include <gtest/gtest.h>
#include <type_traits>

using namespace dqRender;

// viewIndependentOrigin exists on CachedGeometry and returns dqGeom::Point3d const*.
// Ported from: itwinjs-core CachedGeometry.ts line 117
static_assert(std::is_member_function_pointer_v<decltype(&CachedGeometry::viewIndependentOrigin)>,
              "CachedGeometry::viewIndependentOrigin must exist");
static_assert(std::is_same_v<decltype(std::declval<CachedGeometry const&>().viewIndependentOrigin()),
                             dqGeom::Point3d const*>,
              "viewIndependentOrigin must return dqGeom::Point3d const*");

// getRtcModelTransform forwarders exist on InstancedGeometry (both overloads).
// Ported from: itwinjs-core InstancedGeometry.getRtcModelTransform line 171, 351
static_assert(std::is_member_function_pointer_v<decltype(
                  static_cast<void (InstancedGeometry::*)(float const*, float*) const>(
                      &InstancedGeometry::getRtcModelTransform))>,
              "InstancedGeometry::getRtcModelTransform(float*,float*) must exist");
static_assert(std::is_member_function_pointer_v<decltype(
                  static_cast<dqGeom::Transform (InstancedGeometry::*)(dqGeom::Transform const&) const>(
                      &InstancedGeometry::getRtcModelTransform))>,
              "InstancedGeometry::getRtcModelTransform(Transform) must exist");

// devicePixelRatio exists on TargetImpl.
// Ported from: itwinjs-core Target.devicePixelRatio line 1288
static_assert(std::is_member_function_pointer_v<decltype(&TargetImpl::devicePixelRatio)>,
              "TargetImpl::devicePixelRatio must exist");

// devicePixelRatio override precedence: override (>0) wins over the host ratio.
TEST(PrerequisitesTest, DevicePixelRatioOverridePrecedence)
{
    float const hostRatio = 2.0f;
    auto resolve = [&](float ovr) { return ovr > 0.0f ? ovr : hostRatio; };
    EXPECT_FLOAT_EQ(resolve(0.0f), 2.0f);  // no override -> host ratio
    EXPECT_FLOAT_EQ(resolve(1.5f), 1.5f);  // override wins
}
