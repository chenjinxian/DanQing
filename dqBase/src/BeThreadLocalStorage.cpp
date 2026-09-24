// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThreadLocalStorage.h
//              and iModelCore/Bentley/src/BeThreadLocalStorage.cpp
// DanQing dqBase — 线程本地存储实现
#include "dqBase/BeThreadLocalStorage.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <pthread.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

// Ported from: imodel-native BeThreadLocalStorage.cpp — Create
void* BeThreadLocalStorage::Create(Destructor /*dtor*/) {
#if defined(_WIN32)
    // DWORD(32位) -> void*(64位)：经 intptr_t 中转，与 Delete 的反向转换对称（C4312）
    return reinterpret_cast<void*>(static_cast<intptr_t>(TlsAlloc()));
#else
    pthread_key_t* key = new pthread_key_t;
    pthread_key_create(key, nullptr);
    return static_cast<void*>(key);
#endif
}

// Ported from: imodel-native BeThreadLocalStorage.cpp — Delete
void BeThreadLocalStorage::Delete(void* key) {
    if (!key) return;
#if defined(_WIN32)
    TlsFree(static_cast<DWORD>(reinterpret_cast<intptr_t>(key)));
#else
    pthread_key_t* pk = static_cast<pthread_key_t*>(key);
    pthread_key_delete(*pk);
    delete pk;
#endif
}

// Ported from: imodel-native BeThreadLocalStorage.cpp — SetValue
void BeThreadLocalStorage::SetValue(void* key, void* val) {
#if defined(_WIN32)
    TlsSetValue(static_cast<DWORD>(reinterpret_cast<intptr_t>(key)), val);
#else
    pthread_key_t* pk = static_cast<pthread_key_t*>(key);
    pthread_setspecific(*pk, val);
#endif
}

// Ported from: imodel-native BeThreadLocalStorage.cpp — GetValue
void* BeThreadLocalStorage::GetValue(void* key) {
#if defined(_WIN32)
    return TlsGetValue(static_cast<DWORD>(reinterpret_cast<intptr_t>(key)));
#else
    pthread_key_t* pk = static_cast<pthread_key_t*>(key);
    return pthread_getspecific(*pk);
#endif
}

END_DQ_BASE_NAMESPACE
