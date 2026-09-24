// SPDX-License-Identifier: Apache-2.0
// Authored: module bootstrap, no direct reference equivalent
// DanQing dqBase — 全局初始化实现（零 Qt 依赖）
#include "dqBase/DqBase.h"
#include "dqBase/Logging.h"

#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

namespace {
bool g_initialized = false;
} // namespace

void DqBaseLib::Initialize() noexcept {
    if (g_initialized)
        return;
    g_initialized = true;
    // 运行时默认日志级别为 Info — 对齐原 DqBase 初始化行为。
    // Ported from: imodel-native Logging.h:45 — ConsoleLogger default severity.
    NativeLogging::ConsoleLogger::GetLogger().m_defaultSeverity = NativeLogging::LOG_INFO;
}

void DqBaseLib::Shutdown() noexcept {
    g_initialized = false;
}

// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/ScopedArray.h:16
void UnalignedMemcpy(void* dest, const void* source, size_t num) {
    std::memcpy(dest, source, num);
}

END_DQ_BASE_NAMESPACE
