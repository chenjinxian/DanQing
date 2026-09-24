// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeGetProcAddress.h
// DanQing dqBase — 动态库加载实现
#include "dqBase/BeGetProcAddress.h"

#if defined(_WIN32)
    #include <windows.h>
    // windows.h 定义 #define LoadLibrary LoadLibraryA，会把下方成员函数定义
    // BeGetProcAddress::LoadLibrary 宏展开改名为 LoadLibraryA（C2039）。#undef 恢复 1:1 成员名。
    #undef LoadLibrary
#else
    #include <dlfcn.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

void* BeGetProcAddress::LoadLibrary(const DqString& path) {
#if defined(_WIN32)
    return ::LoadLibraryA(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY);
#endif
}

void BeGetProcAddress::UnloadLibrary(void* handle) {
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* BeGetProcAddress::GetProcAddress(void* handle, const char* procName) {
#if defined(_WIN32)
    return ::GetProcAddress(static_cast<HMODULE>(handle), procName);
#else
    return dlsym(handle, procName);
#endif
}

void BeGetProcAddress::SetLibrarySearchPath(const DqString& /*path*/) {
    // 平台相关实现
}

END_DQ_BASE_NAMESPACE
