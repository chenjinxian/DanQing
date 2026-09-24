// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Descriptor set manager for UBO binding
// Ported from: filament DescriptorSet + itwinjs-core uniform binding
//
// Manages descriptor set layouts and sets for uniform buffer binding.
// Provides a simple interface for binding UBOs to shader programs.
#pragma once

#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/DriverEnums.h"
#include "dqRender/rhi/Handle.h"
#include "dqRender/rhi/UniformBuffer.h"

#include <cstdint>
#include <unordered_map>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// DescriptorSetManager — manages UBO descriptor sets
// ---------------------------------------------------------------------------
class DescriptorSetManager {
public:
    DescriptorSetManager() = default;
    ~DescriptorSetManager() = default;

    DescriptorSetManager(DescriptorSetManager const&) = delete;
    DescriptorSetManager& operator=(DescriptorSetManager const&) = delete;

    /// Initialize the manager (creates a default UBO descriptor set layout).
    void initialize(rhi::Driver& driver);

    /// Release all GPU resources.
    void destroy(rhi::Driver& driver);

    /// Get or create a descriptor set for the given program.
    /// The descriptor set binds the UBO at binding 0.
    rhi::DescriptorSetHandle getOrCreateDescriptorSet(
        rhi::Driver& driver,
        rhi::ProgramHandle program,
        rhi::BufferObjectHandle uboHandle);

    /// Bind a descriptor set for the current draw call.
    void bindDescriptorSet(rhi::Driver& driver,
                           rhi::DescriptorSetHandle dsh,
                           rhi::BufferObjectHandle uboHandle);

    /// Check if initialized.
    bool isInitialized() const noexcept { return m_initialized; }

private:
    rhi::DescriptorSetLayoutHandle m_layoutHandle;
    bool m_initialized = false;
};

END_DQ_RENDER_NAMESPACE
