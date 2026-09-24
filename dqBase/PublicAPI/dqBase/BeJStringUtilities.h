// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeJStringUtilities.h
// DanQing dqBase — JNI 字符串工具
//
// 1:1 对齐 imodel-native BeJStringUtilities。
// Android 平台专用。非 Android 平台为空桩。
#pragma once

#include "Export.h"
#include "DqTypes.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeJStringUtilities — JNI 字符串转换
// Ported from: imodel-native BeJStringUtilities.h
// ---------------------------------------------------------------------------
struct BeJStringUtilities {
#if defined(__ANDROID__)
    // Android JNI 字符串转换需要 JNIEnv*
    // 完整实现需要 <jni.h>
    static DqString InitUtf8StringFromJString(void* jniEnv, void* jstring);
    static void* JStringFromUtf8String(void* jniEnv, const DqString& str);
#else
    // 非 Android 平台：空桩
    static DqString InitUtf8StringFromJString(void*, void*) { return {}; }
    static void* JStringFromUtf8String(void*, const DqString&) { return nullptr; }
#endif
};

END_DQ_BASE_NAMESPACE
