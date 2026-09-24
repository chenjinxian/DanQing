// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — DescriptorSetManager implementation
// Ported from: filament DescriptorSet + itwinjs-core uniform binding
#include "DescriptorSetManager.h"

BEGIN_DQ_RENDER_NAMESPACE

void DescriptorSetManager::initialize(rhi::Driver& driver)
{
    if (m_initialized)
        return;

    // Descriptor sets are a Vulkan/Metal concept.
    // In OpenGL, UBOs are bound directly without descriptor sets.
    // Mark as initialized but with no layout handle — all operations become no-ops.
    (void)driver;
    m_initialized = true;
}

void DescriptorSetManager::destroy(rhi::Driver& driver)
{
    if (m_layoutHandle) {
        driver.destroyDescriptorSetLayout(m_layoutHandle);
        m_layoutHandle = rhi::DescriptorSetLayoutHandle{};
    }
    m_initialized = false;
}

rhi::DescriptorSetHandle DescriptorSetManager::getOrCreateDescriptorSet(
    rhi::Driver& driver,
    rhi::ProgramHandle /*program*/,
    rhi::BufferObjectHandle uboHandle)
{
    if (!m_initialized)
        return rhi::DescriptorSetHandle{};

    // Create a new descriptor set.
    auto dsh = driver.createDescriptorSet(m_layoutHandle);
    if (!dsh)
        return rhi::DescriptorSetHandle{};

    // Bind the UBO to binding 0.
    driver.updateDescriptorSetBuffer(dsh, 0, uboHandle, 0, 0);

    return dsh;
}

void DescriptorSetManager::bindDescriptorSet(rhi::Driver& driver,
                                              rhi::DescriptorSetHandle dsh,
                                              rhi::BufferObjectHandle /*uboHandle*/)
{
    if (!dsh)
        return;

    // Bind the descriptor set at set 0.
    driver.bindDescriptorSet(dsh, 0, nullptr, 0);
}

END_DQ_RENDER_NAMESPACE
