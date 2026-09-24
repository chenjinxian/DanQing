// SPDX-License-Identifier: Apache-2.0
// Authored: no reference test exists in itwinjs-core for UniformBuffer C++ port
// DanQing dqRender — UniformBuffer tests
//
// Tests for UBO allocation and CPU-side data management.
// Full GPU upload tests require an OpenGL context (covered by integration tests).

#include "dqRender/rhi/UniformBuffer.h"

#include <gtest/gtest.h>

using namespace dqRender::rhi;

// ============================================================================
// UniformBuffer tests (CPU-side only)
// ============================================================================

// Authored: no reference test exists in itwinjs-core for UniformBuffer C++ port
TEST(UniformBufferTest, DefaultState)
{
    UniformBuffer ub;
    EXPECT_FALSE(ub.isAllocated());
    EXPECT_EQ(ub.getSize(), 0u);
    EXPECT_EQ(ub.getData(), nullptr);
}

// Authored: no reference test exists in itwinjs-core for UniformBuffer C++ port
TEST(UniformBufferTest, SetBeforeAllocate)
{
    UniformBuffer ub;
    float val = 1.0f;
    // Should be a no-op (no crash, no allocation).
    ub.set(&val, sizeof(float), 0);
    EXPECT_FALSE(ub.isAllocated());
    EXPECT_EQ(ub.getSize(), 0u);
}
