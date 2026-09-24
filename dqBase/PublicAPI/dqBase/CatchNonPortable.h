// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/CatchNonPortable.h
// DanQing dqBase — 移植性检查
//
// 参考实现声明 ~120 个 Win32 函数的毒丸重载（strlwr/strcmpi/swprintf/CreateFileW/
// PathFileExistsW/InitializeCriticalSection/InterlockedIncrement/VirtualAlloc/CoInitialize 等）
// 加上 #define __super __super_is_not_portable_use_T_Super。
//
// DanQing 是跨平台 C++ 项目，不使用 Win32 API。此头文件保留作文档/提醒用途。
// 在 Windows 构建中，可激活毒丸机制阻止非移植函数调用。
#pragma once

#ifndef NDEBUG
    #if defined(_MSC_VER)
        // Win32 毒丸：声明非移植函数为已删除，强制使用可移植替代品。
        // 完整的毒丸列表需要 ~120 个函数重载（参见参考 CatchNonPortable.h:17-155）。
        // 当前仅作提醒，因为 DanQing 不使用 Win32 API。
    #endif
#endif
