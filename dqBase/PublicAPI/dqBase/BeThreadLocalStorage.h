// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeThreadLocalStorage.h
// DanQing dqBase — 线程本地存储
//
// 1:1 对齐 imodel-native BeThreadLocalStorage。
// 提供静态 Create/Delete/SetValue/GetValue by-key API 和实例方法。
#pragma once

#include "Export.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

/*=================================================================================**//**
* 线程本地存储
*
* 用法:
*   BeThreadLocalStorage* tls = BeThreadLocalStorage::Create();
*   tls->SetValueAsPointer(new MyObject);
*   MyObject* obj = static_cast<MyObject*>(tls->GetValueAsPointer());
*   delete tls;
*
* @bsiclass
+===============+===============+===============+===============+===============+======*/
struct BeThreadLocalStorage
{
private:
    void* m_key;

public:
    typedef void(*Destructor)(void*);

    /// 分配 OS TLS 槽
    /// Ported from: imodel-native BeThreadLocalStorage.h:54
    DQ_BASE_EXPORT static void* Create(Destructor dtor = nullptr);

    /// 释放 OS TLS 槽
    /// Ported from: imodel-native BeThreadLocalStorage.h:55
    DQ_BASE_EXPORT static void Delete(void* key);

    /// 通过 key 设置值
    /// Ported from: imodel-native BeThreadLocalStorage.h:56
    DQ_BASE_EXPORT static void SetValue(void* key, void* val);

    /// 通过 key 获取值
    /// Ported from: imodel-native BeThreadLocalStorage.h:57
    DQ_BASE_EXPORT static void* GetValue(void* key);

    /// 分配线程本地存储槽
    /// Ported from: imodel-native BeThreadLocalStorage.h:60
    BeThreadLocalStorage(Destructor destructor = nullptr) : m_key(Create(destructor)) {}

    /// 释放线程本地存储槽
    /// Ported from: imodel-native BeThreadLocalStorage.h:64
    ~BeThreadLocalStorage() { Delete(m_key); }

    /// 存储指针值
    /// Ported from: imodel-native BeThreadLocalStorage.h:68
    void SetValueAsPointer(void* val) { SetValue(m_key, val); }

    /// 存储整数值
    /// Ported from: imodel-native BeThreadLocalStorage.h:71
    void SetValueAsInteger(intptr_t v) { SetValueAsPointer(reinterpret_cast<void*>(v)); }

    /// 获取存储的指针值
    /// Ported from: imodel-native BeThreadLocalStorage.h:75
    void* GetValueAsPointer() { return GetValue(m_key); }

    /// 获取存储的整数值
    /// Ported from: imodel-native BeThreadLocalStorage.h:79
    intptr_t GetValueAsInteger() { return reinterpret_cast<intptr_t>(GetValueAsPointer()); }
};

END_DQ_BASE_NAMESPACE
