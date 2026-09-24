// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/ScopedArray.h
// DanQing dqBase — 栈/堆混合数组 + 内存对齐数组
//
// 1:1 对齐 imodel-native ScopedArray / IndexedScopedArray / AlignedArray。
// ScopedArray 分配原始字节，不调用构造/析构函数（参考明确契约）。
// AlignedArray 在不需要对齐的平台上是 no-op；需要对齐时用 GetAlignedData 惰性拷贝。
#pragma once

#include "Export.h"
#include "BeAssert.h"
#include "NonCopyable.h"

#include <cstdint>
#include <cstring>

// Reference pattern: anonymous struct inside anonymous union for alignment.
// Suppress GNU extension warnings for this well-known imodel-native pattern.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// UnalignedMemcpy — 非对齐内存拷贝
// Ported from: imodel-native ScopedArray.h:16
// ---------------------------------------------------------------------------
DQ_BASE_EXPORT void UnalignedMemcpy(void* dest, const void* source, size_t num);

/*=================================================================================**//**
* ScopedArray — 栈/堆混合数组（不调用构造/析构函数）。
*
* THRESHOLD 指定栈缓冲区的字节数。若所需内存 > THRESHOLD，则从堆分配。
* 分配和释放不调用任何 T 的构造函数或析构函数（原始字节操作）。
*
* THRESHOLD 应设为 sizeof(T) 的倍数减一。
*
* @bsiclass
+===============+===============+===============+===============+===============+======*/
template<class T, size_t THRESHOLD = 511>
struct ScopedArray
    {
private:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201) // 匿名 union/struct 是 imodel-native 逐字移植（TD-7），MSVC /W4 非标准扩展提示
#endif
    union
        {
        T m_unused; // 强制 m_mem 按 T 对齐

        struct
            {
            unsigned char   m_mem[THRESHOLD];   // 栈缓冲区
            bool            m_wasMalloced;       // 是否从堆分配
            };
        };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    T* m_data;

#ifndef NDEBUG
    size_t const m_numItems;
#endif

    ScopedArray (ScopedArray const&);
    ScopedArray const& operator= (ScopedArray const&);
    void UnconditionalAllocate (size_t numItems)
        {
        size_t requiredSize = numItems * sizeof (T);

        if (requiredSize <= THRESHOLD)
            {
            m_data          = reinterpret_cast<T*>(m_mem);
            m_wasMalloced   = false;
            return;
            }

        m_data          = reinterpret_cast<T*>(new unsigned char[requiredSize]);
        m_wasMalloced   = true;
        }

public:
    //! 创建指定大小的 ScopedArray。
    //! @param[in] numItems  元素数量。
    ScopedArray (size_t numItems)
#ifndef NDEBUG
    : m_numItems (numItems)
#endif
        {
        UnconditionalAllocate (numItems);
        }

    //! 分配 numItems 个元素，并从 data 拷贝内容。
    ScopedArray (size_t numItems, T const *data)
#ifndef NDEBUG
    : m_numItems (numItems)
#endif
        {
        UnconditionalAllocate (numItems);
        std::memcpy (GetData (), data, numItems * sizeof (T));
        }

    ~ScopedArray ()
        {
        if (!m_wasMalloced)
            return;

        delete[] reinterpret_cast<unsigned char*>(m_data);
        }

    //! 返回数组内存指针。
    T* GetData ()
        {
        return m_data;
        }

    //! 返回数组的 const 指针。
    T const* GetDataCP () const
        {
        return m_data;
        }
    }; // ScopedArray

// ---------------------------------------------------------------------------
// IndexedScopedArray — 带 operator[] 的 ScopedArray
// Ported from: imodel-native ScopedArray.h:129-135
// ---------------------------------------------------------------------------
template<typename T, size_t THRESHOLD = 511>
struct IndexedScopedArray : ScopedArray<T, THRESHOLD>
    {
    IndexedScopedArray (size_t numItems) : ScopedArray<T, THRESHOLD>(numItems) {}

    T& operator[](size_t i) {return (this->GetData())[i];}
    T const& operator[](size_t i) const {return (this->GetDataCP())[i];}
    };

// =======================================================================================
//! AlignedArray — 确保数据块正确对齐。
//!
//! 若 CPU 架构不要求对齐，则此类从不拷贝数据（GetAlignedData 直接返回原指针）。
//! T 用于强制对齐到合适的边界，不决定数据大小。
//!
//! @bsiclass
// =======================================================================================
template <class T, size_t THRESHOLD = 512, int ALIGNMENT = 0x3>
struct AlignedArray : DqNonCopyable
    {
#if !defined (TARGET_PROCESSOR_ARCHITECTURE_MEMORY_ALIGNMENT_REQUIRED)
    AlignedArray () {;}

    //! 若数据已对齐则直接返回原指针；否则返回原指针（不要求对齐）。
    //! @return pData
    //! @param pData 原始数据数组
    //! @param requiredSize 数据块大小
    T const* GetAlignedData (T const* pData, size_t /*requiredSize*/) {return pData;}

    //! 丢弃 GetAlignedData 分配的拷贝（no-op，因为从不拷贝）。
    void Clear() {;}
#else
    private:
    union
        {
        T               m_unused;           //!< 强制按 T 对齐
        unsigned char   m_mem[THRESHOLD];   //!< 内联缓冲区
        };

    T const* m_data;

    public:
    AlignedArray () :
        m_data (nullptr)
        {
        }

    ~AlignedArray ()
        {
        Clear();
        }

    //! 丢弃 GetAlignedData 分配的拷贝。
    void Clear()
        {
        if (reinterpret_cast<unsigned char const*>(m_data) != m_mem)
            delete[] reinterpret_cast<unsigned char*>(const_cast<T*>(m_data));
        m_data = nullptr;
        }

    //! 确保指定数据正确对齐。若未对齐则拷贝到对齐缓冲区。
    //! 缓冲区由本对象拥有，通过 Clear 或析构释放。
    //! @return 若 pData 已对齐则返回 pData，否则返回其内容的对齐拷贝。
    //! @param pData 原始数据数组
    //! @param requiredSize 数据块大小
    T const* GetAlignedData (T const* pData, size_t requiredSize)
        {
        // 已对齐？
        if (0 == (reinterpret_cast<uintptr_t>(pData) & ALIGNMENT))
            return pData;

        // 未对齐，需要拷贝
        if (nullptr != m_data)
            {
            DqAssert (false && "Call Clear before calling GetAlignedData a second time");
            return nullptr;
            }
        if (requiredSize <= sizeof (m_mem))
            m_data = reinterpret_cast<T*>(m_mem);
        else
            m_data = reinterpret_cast<T*>(new unsigned char[requiredSize]);

        UnalignedMemcpy (const_cast<T*>(m_data), pData, requiredSize);

        return m_data;
        }
#endif
    }; // AlignedArray

END_DQ_BASE_NAMESPACE

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
