// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDebugUtilities.h
// DanQing dqBase — 调试工具
//
// 1:1 对齐 imodel-native BeDebugUtilities。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstddef>
#include <vector>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeDebugUtilities — 调试工具
// Ported from: imodel-native BeDebugUtilities.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeDebugUtilities {
    //! Information about specific frame in stack
    // Ported from: imodel-native BeDebugUtilities.h StackFrameInfo
    struct StackFrameInfo {
        DqString functionName;
        DqString fileName;
        uint64_t fileLine = 0;
        bool IsValid() const noexcept { return !functionName.empty() && !fileName.empty(); }
    };

    /// Get stack trace formatted into newlines. Function is not included in frames.
    /// @param maxFrames maximum number of frames (depth) to return
    /// Ported from: imodel-native BeDebugUtilities.h GetStackTraceDescription
    static DqString GetStackTraceDescription(size_t maxFrames);

    /// Get stack frame information at specific index. 0 is current frame.
    /// Ported from: imodel-native BeDebugUtilities.h GetStackFrameInfoAt
    static StackFrameInfo GetStackFrameInfoAt(size_t frameIndex);

    /// Get stack frames information at specific index and up. 0 is current frame.
    /// Ported from: imodel-native BeDebugUtilities.h GetStackFrameInfosAt
    static std::vector<StackFrameInfo> GetStackFrameInfosAt(size_t frameIndex, size_t frameCount);

    /// Get process memory use in bytes. Returns 0 if not implemented.
    /// Ported from: imodel-native BeDebugUtilities.h GetMemoryUsed
    static size_t GetMemoryUsed();
};

END_DQ_BASE_NAMESPACE
