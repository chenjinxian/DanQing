// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Platform abstraction
// Ported from: filament backend/include/backend/Platform.h
//
// Abstract factory for creating the backend driver.  Each platform (macOS,
// Linux, Windows) provides a concrete implementation.
#pragma once

#include <cstdint>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

class Driver;

// ---------------------------------------------------------------------------
// DriverConfig — hints for driver creation
// ---------------------------------------------------------------------------
struct DriverConfig {
    uint32_t handleArenaSize = 4 * 1024 * 1024;  // 4 MB default
    bool disableParallelShaderCompile = false;
};

// ---------------------------------------------------------------------------
// Platform — abstract factory for creating the RHI driver
// ---------------------------------------------------------------------------
class Platform {
public:
    virtual ~Platform() = default;

    /// Create the backend driver.  The sharedContext parameter allows sharing
    /// GL resources with an existing context (e.g., for parallel shader compilation).
    virtual Driver* createDriver(void* sharedContext,
                                 DriverConfig const& config) = 0;

    /// Pump platform events (e.g., window messages).  Returns true if events
    /// were processed.
    virtual bool pumpEvents() noexcept { return false; }
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
