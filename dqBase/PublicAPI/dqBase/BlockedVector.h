// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Polyface.h BlockedVector
//              and iModelCore/GeomLibs/geom/src/polyface/BlockedVector.cpp
// DanQing dqBase — 分块向量（几何数据缓存用）
//
// 1:1 对齐 imodel-native BlockedVector<T>：继承 bvector<T>，增加 6 个元数据标量和 ~20 个辅助方法。
// 主要供 dqGeom 存储点/法线/UV 等分块几何数据。
#pragma once

#include "DqBase.h"
#include "DqTypes.h"
#include "bvector.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

/// BlockedVector<T> — 带阻塞和上下文信息的 bvector<T>。
///
/// 对齐 imodel-native Polyface.h BlockedVector。
/// 元数据对应 DGN 文件 MATRIX_HEADER_ELM：
///   NumPerStruct  — 单个结构中机器原语（int/double）的数量
///   StructsPerRow — 坐标网格中每行的结构数
///   Tag           — 应用标签
///   IndexFamily   — 索引数据解释方式
///   IndexedBy     — 引用本数组的索引数组标签
///   Active        — 是否激活（Polyface 声明所有可能的数组，未使用的标记为 inactive）
///
/// @bsiclass
template <typename T>
struct BlockedVector : bvector<T>
{
protected:
    uint32_t m_numPerStruct;
    uint32_t m_structsPerRow;
    uint32_t m_tag;
    uint32_t m_indexFamily;
    uint32_t m_indexedBy;
    bool m_active;

public:
    //! 构造函数（带可选标签值）。
    //! Ported from: imodel-native BlockedVector.cpp:134-151
    BlockedVector (uint32_t numPerStruct,
        uint32_t structsPerRow = 0, uint32_t tag = 0,
        uint32_t indexFamily = 0, uint32_t indexedBy = 0, bool active = false)
        {
        m_numPerStruct  = numPerStruct;
        m_structsPerRow = structsPerRow;
        m_tag           = tag;
        m_indexFamily   = indexFamily;
        m_indexedBy     = indexedBy;
        m_active        = active;
        }

    //! 默认构造函数（所有标签为零）。
    //! Ported from: imodel-native BlockedVector.cpp:157-166
    BlockedVector ()
        {
        m_numPerStruct  = 1;
        m_structsPerRow = 1;
        m_tag           = 0;
        m_indexFamily   = 0;
        m_indexedBy     = 0;
        m_active        = false;
        }

    //! 查询单个结构中的机器原语数量。
    //! Ported from: imodel-native BlockedVector.cpp:13
    uint32_t NumPerStruct () const { return m_numPerStruct; }

    //! 查询每行的结构数。
    //! Ported from: imodel-native BlockedVector.cpp:18
    uint32_t StructsPerRow () const { return m_structsPerRow; }

    //! 设置每行的结构数。
    //! Ported from: imodel-native BlockedVector.cpp:23
    void SetStructsPerRow (uint32_t n) { m_structsPerRow = n; }

    //! 返回上下文标签。
    //! Ported from: imodel-native BlockedVector.cpp:28
    uint32_t Tag () const { return m_tag; }

    //! 返回 IndexFamily。
    //! Ported from: imodel-native BlockedVector.cpp:33
    uint32_t IndexFamily () const { return m_indexFamily; }

    //! 返回 IndexedBy。
    //! Ported from: imodel-native BlockedVector.cpp:38
    uint32_t IndexedBy () const { return m_indexedBy; }

    //! 一次性设置所有阻塞和标签数据。
    //! Ported from: imodel-native BlockedVector.cpp:69-86
    void SetTags (uint32_t numPerStruct, uint32_t structsPerRow, uint32_t tag,
                  uint32_t indexFamily, uint32_t indexedBy, bool active)
        {
        m_numPerStruct  = numPerStruct;
        m_structsPerRow = structsPerRow;
        m_tag           = tag;
        m_indexFamily   = indexFamily;
        m_indexedBy     = indexedBy;
        m_active        = active;
        }

    //! 查询是否激活。
    //! Ported from: imodel-native Polyface.h:190
    bool Active () const { return m_active; }

    //! 标记为激活。
    //! Ported from: imodel-native Polyface.h:192
    void SetActive (bool active) { m_active = active; }

    //! 清空 bvector，然后从源 vector push_back。保留阻塞数据。
    //! Ported from: imodel-native BlockedVector.cpp:120-128
    void CopyVectorFrom (bvector<T>& source)
        {
        this->clear ();
        for (size_t i = 0, n = source.size (); i < n; i++)
            this->push_back (source[i]);
        }

    //! 清空本 vector。从 source 的 i0 开始追加最多 n 个值，并复制前 numWrap 个值到末尾。
    //! Ported from: imodel-native BlockedVector.cpp:172-199
    uint32_t ClearAndAppendBlock (BlockedVector<T>& source, uint32_t i0, uint32_t n, uint32_t numWrap)
        {
        uint32_t numOut = 0;
        this->clear ();
        size_t numSource = source.size ();
        if (numSource > 0 && source.Active ())
            {
            for (; numOut < n && i0 + numOut < numSource; numOut++)
                this->push_back (source[i0 + numOut]);
            SetActive (true);

            if (numOut > 0)
                {
                auto numSoFar = numOut;
                for (uint32_t i = 0; i < numWrap; i++)
                    {
                    auto iA = i % numSoFar;
                    T wrap = this->at (iA);
                    this->push_back (wrap);
                    numOut++;
                    }
                }
            }
        return numOut;
        }

    //! 清空本 vector。从 source 数组的 i0 开始追加最多 n 个值，并复制前 numWrap 个值到末尾。
    //! Ported from: imodel-native BlockedVector.cpp:213-239
    uint32_t ClearAndAppendBlock (T const* source, size_t numSource, uint32_t i0, uint32_t n, uint32_t numWrap)
        {
        uint32_t numOut = 0;
        this->clear ();
        if (source != nullptr && numSource > 0)
            {
            for (; numOut < n && i0 + numOut < numSource; numOut++)
                this->push_back (source[i0 + numOut]);
            SetActive (true);

            if (numOut > 0)
                {
                auto numSoFar = numOut;
                for (uint32_t i = 0; i < numWrap; i++)
                    {
                    auto iA = i % numSoFar;
                    T wrap = this->at (iA);
                    this->push_back (wrap);
                    numOut++;
                    }
                }
            }
        return numOut;
        }

    //! 清空本 vector，追加 source 的全部数据。
    //! Ported from: imodel-native BlockedVector.cpp:201-207
    void ClearAndAppend (bvector<T> const& source)
        {
        this->clear ();
        for (auto const& value : source)
            this->push_back (value);
        }

    //! 清空本 vector，从 oneBasedIndices 中按 1-based 索引从 source 追加数据。
    //! Ported from: imodel-native BlockedVector.cpp:352-403
    uint32_t ClearAndAppendByOneBasedIndices
        (
        bvector<int>& zeroBasedIndices,
        bvector<bool>* positive,
        bvector<T>& source,
        bvector<int>& oneBasedIndices,
        uint32_t i0,
        uint32_t numIndex,
        uint32_t numWrap
        )
        {
        uint32_t numOut;
        this->clear ();
        zeroBasedIndices.clear ();
        if (positive != nullptr)
            positive->clear ();
        size_t maxIndex = oneBasedIndices.size ();
        size_t numSource = source.size ();
        for (numOut = 0; numOut < numIndex && i0 + numOut < maxIndex; numOut++)
            {
            int k1 = oneBasedIndices[i0 + numOut];
            if (k1 == 0)
                break;
            uint32_t k0 = static_cast<uint32_t>(std::abs (k1)) - 1;
            if (k0 >= numSource)
                break;
            zeroBasedIndices.push_back (k0);
            if (positive != nullptr)
                positive->push_back (k1 > 0);
            this->push_back (source[k0]);
            }
        if (numOut > 0)
            {
            auto numSoFar = numOut;
            for (uint32_t i = 0; i < numWrap; i++)
                {
                auto iA = i % numSoFar;
                T wrap = this->at (iA);
                this->push_back (wrap);
                numOut++;
                uint32_t index = zeroBasedIndices[iA];
                zeroBasedIndices.push_back (index);
                if (positive != nullptr)
                    positive->push_back (positive->at (iA));
                }
            }
        return numOut;
        }

    //! 清空本 vector，从 raw 数组按 1-based 索引追加数据。
    //! Ported from: imodel-native BlockedVector.cpp:410-459
    uint32_t ClearAndAppendByOneBasedIndices
        (
        bvector<int>& zeroBasedIndices,
        bvector<bool>* positive,
        T const* source,
        size_t sourceCount,
        int const* oneBasedIndices,
        size_t oneBasedIndexCount,
        uint32_t i0,
        uint32_t numIndex,
        uint32_t numWrap
        )
        {
        uint32_t numOut;
        this->clear ();
        zeroBasedIndices.clear ();
        if (positive != nullptr)
            positive->clear ();
        if (oneBasedIndices == nullptr)
            return 0;
        for (numOut = 0; numOut < numIndex && i0 + numOut < oneBasedIndexCount; numOut++)
            {
            int k1 = oneBasedIndices[i0 + numOut];
            if (k1 == 0)
                break;
            uint32_t k0 = static_cast<uint32_t>(std::abs (k1)) - 1;
            if (k0 >= sourceCount)
                break;
            zeroBasedIndices.push_back (k0);
            if (positive != nullptr)
                positive->push_back (k1 > 0);
            this->push_back (source[k0]);
            }
        if (numOut > 0)
            {
            auto numSoFar = numOut;
            for (uint32_t i = 0; i < numWrap; i++)
                {
                auto iA = i % numSoFar;
                T wrap = this->at (iA);
                zeroBasedIndices.push_back (zeroBasedIndices[iA]);
                this->push_back (wrap);
                numOut++;
                if (positive)
                    positive->push_back (positive->at (iA));
                }
            }
        return numOut;
        }

    //! 从数组追加。SetActive 并返回新大小。
    //! Ported from: imodel-native BlockedVector.cpp:244-255
    size_t Append (T const* pData, size_t n)
        {
        if (n > 0 && pData != nullptr)
            {
            size_t oldSize = this->size ();
            this->resize (oldSize + n);
            std::copy (pData, pData + n, this->begin () + oldSize);
            SetActive (true);
            }
        return this->size ();
        }

    //! 从 BlockedVector 追加。SetActive 并返回新大小。
    //! Ported from: imodel-native BlockedVector.cpp:278-286
    size_t Append (BlockedVector<T> const& source)
        {
        size_t n = source.size ();
        for (size_t i = 0; i < n; i++)
            this->push_back (source[i]);
        SetActive (true);
        return this->size ();
        }

    //! 追加单个值。SetActive 并返回新大小。
    //! Ported from: imodel-native BlockedVector.cpp:257-263
    size_t Append (T const& data)
        {
        this->push_back (data);
        SetActive (true);
        return this->size ();
        }

    //! 追加单个值。SetActive 并返回新元素的索引。
    //! Ported from: imodel-native BlockedVector.cpp:265-272
    size_t AppendAndReturnIndex (T const& data)
        {
        size_t index = this->size ();
        this->push_back (data);
        SetActive (true);
        return index;
        }

    //! 返回完整行数（基于 StructsPerRow 和实际大小）。
    //! Ported from: imodel-native BlockedVector.cpp:340-346
    size_t NumCompleteRows ()
        {
        size_t numStruct = this->size ();
        size_t numPerRow = StructsPerRow ();
        return numPerRow > 1 ? numStruct / numPerRow : numStruct;
        }

    //! 返回平坦缓冲区指针。
    //! Ported from: imodel-native BlockedVector.cpp:92-101
    T* GetPtr ()
        {
        if (this->size () > 0)
            return &this->at (0);
        else
            return nullptr;
        }

    //! 返回平坦缓冲区 const 指针。
    //! Ported from: imodel-native BlockedVector.cpp:107-114
    T const* GetCP () const
        {
        if (this->size () > 0)
            return &this->at (0);
        else
            return nullptr;
        }

    //! 反转 iFirst < i < iLast 范围内的元素。
    //! Ported from: imodel-native BlockedVector.cpp:292-302
    void ReverseInRange (size_t iFirst, size_t iLast)
        {
        T temp;
        for (; iFirst < iLast; iFirst++, iLast--)
            {
            temp = this->at (iFirst);
            this->at (iFirst) = this->at (iLast);
            this->at (iLast) = temp;
            }
        }

    //! 从一个位置复制到另一个位置（忽略越界）。
    //! Ported from: imodel-native BlockedVector.cpp:308-314
    void CopyData (size_t iFrom, size_t iTo)
        {
        size_t count = this->size ();
        if (iFrom != iTo && iFrom < count && iTo < count)
            this->at (iTo) = this->at (iFrom);
        }

    //! 从 index0 开始复制 count 个值到开头，然后截断。
    //! Ported from: imodel-native BlockedVector.cpp:320-334
    void Trim (size_t index0, size_t count)
        {
        size_t n = this->size ();
        if (index0 >= n)
            return;
        if (index0 + count > n)
            count = n - index0;
        if (index0 > 0)
            {
            for (size_t i = 0; i < count; i++)
                this->at (i) = this->at (index0 + i);
            }
        this->resize (count);
        }

    //! 带检查的访问。
    //! Ported from: imodel-native BlockedVector.cpp:53-64
    bool TryGetAt (size_t index, T const& defaultValue, T& value) const
        {
        size_t n = this->size ();
        if (index < n)
            {
            value = this->at (index);
            return true;
            }
        value = defaultValue;
        return false;
        }
};

/// BlockedVectorInt — BlockedVector<int> 的特化，提供网格索引操作。
/// Ported from: imodel-native Polyface.h:282-348 and BlockedVector.cpp:466-862
struct BlockedVectorInt : BlockedVector<int>
{
    BlockedVectorInt () : BlockedVector<int> () {}

    BlockedVectorInt (uint32_t numPerStruct,
        uint32_t structsPerRow = 0, uint32_t tag = 0,
        uint32_t indexFamily = 0, uint32_t indexedBy = 0, bool active = false)
        : BlockedVector<int> (numPerStruct, structsPerRow, tag, indexFamily, indexedBy, active)
        {}

    //! 将阻塞形式展开为 0 终止的可变长度形式。
    //! Ported from: imodel-native BlockedVector.cpp:596-644
    void ConvertBlockedToZeroTerminated ()
        {
        if (m_structsPerRow > 1)
            {
            int terminator = 0;
            size_t oldSize = size ();
            size_t oldNumRows = oldSize / m_structsPerRow;
            oldSize = oldNumRows * m_structsPerRow;
            size_t newBlockedSize = oldSize + oldNumRows;
            size_t dest, source;
            size_t numCopiedFromCurrentRow;
            reserve (newBlockedSize);
            for (dest = oldSize; dest < newBlockedSize; dest++)
                push_back (0);
            for (dest = newBlockedSize, source = oldSize, numCopiedFromCurrentRow = 0;
                 source > 0;)
                {
                if (numCopiedFromCurrentRow == 0)
                    at (--dest) = terminator;
                at (--dest) = at (--source);
                numCopiedFromCurrentRow++;
                if (numCopiedFromCurrentRow == m_structsPerRow)
                    numCopiedFromCurrentRow = 0;
                }

            size_t newSize = 0;
            for (source = 0; source < newBlockedSize;)
                {
                int a = at (newSize++) = at (source++);
                if (a == terminator)
                    while (source < newBlockedSize && at (source) == terminator)
                        source++;
                }
            resize (newSize);
            m_structsPerRow = 0;
            }
        }

    //! 统计零的数量。
    //! Ported from: imodel-native BlockedVector.cpp:466-476
    size_t CountZeros ()
        {
        size_t numZero = 0;
        size_t count = size ();
        for (size_t i = 0; i < count; i++)
            if (at (i) == 0)
                numZero++;
        return numZero;
        }

    //! 添加 numRow 块，每块 numPerRow 个顺序值，每行后加终止符。
    //! Ported from: imodel-native BlockedVector.cpp:575-594
    void AddTerminatedSequentialBlocks (size_t numRow, size_t numPerRow,
        bool clearFirst = false, int firstValue = 1, int terminator = 0)
        {
        SetActive (true);
        if (clearFirst)
            clear ();
        int value = firstValue;
        for (uint32_t row = 0; row < numRow; row++)
            {
            for (uint32_t i = 0; i < numPerRow; i++)
                push_back (value++);
            push_back (terminator);
            }
        }

    //! 添加一行带环绕的顺序块。
    //! Ported from: imodel-native BlockedVector.cpp:542-571
    void AddSequentialBlock (int firstValue, size_t numValue, size_t numWrap,
        size_t numTrailingZero = 0, bool clearFirst = false)
        {
        if (Active ())
            {
            if (clearFirst)
                clear ();
            if (numValue > 0)
                {
                auto i0 = size ();
                for (size_t i = 0; i < numValue; i++)
                    push_back (firstValue + static_cast<int>(i));
                for (size_t i = 0; i < numWrap; i++)
                    {
                    auto iA = i % numValue;
                    int value = at (i0 + iA);
                    push_back (value);
                    }
                }
            if (numTrailingZero > 0)
                for (size_t i = 0; i < numTrailingZero; i++)
                    push_back (0);
            }
        }

    //! 添加一行带步进的块。
    //! Ported from: imodel-native BlockedVector.cpp:508-538
    void AddSteppedBlock (int value, int valueStep, size_t numValue, size_t numWrap,
        size_t numTrailingZero = 0, bool clearFirst = false)
        {
        if (Active ())
            {
            if (clearFirst)
                clear ();
            if (numValue > 0)
                {
                auto i0 = size ();
                for (size_t i = 0; i < numValue; i++)
                    push_back (value + static_cast<int>(i) * valueStep);
                for (size_t i = 0; i < numWrap; i++)
                    {
                    auto iA = i % numValue;
                    int newvalue = at (i0 + iA);
                    push_back (newvalue);
                    }
                }
            if (numTrailingZero > 0)
                for (size_t i = 0; i < numTrailingZero; i++)
                    push_back (0);
            }
        }

    //! 追加值并终止。
    //! Ported from: imodel-native BlockedVector.cpp:482-506
    bool AddAndTerminate (int const* pData, size_t n)
        {
        if (nullptr != pData)
            {
            if (m_structsPerRow <= 1)
                {
                for (size_t i = 0; i < n; i++)
                    push_back (pData[i]);
                push_back (0);
                }
            else if (n <= m_structsPerRow)
                {
                for (size_t i = 0; i < n; i++)
                    push_back (pData[i]);
                for (size_t i = n; i < m_structsPerRow; i++)
                    push_back (0);
                }
            return true;
            }
        return false;
        }

    //! 创建矩形网格索引。
    //! Ported from: imodel-native BlockedVector.cpp:648-696
    void AddTerminatedGridBlocks (size_t numRow, size_t numPerRow,
        size_t rowStep, size_t colStep, bool triangulated,
        bool clearFirst = false, int firstValue = 1, int terminator = 0)
        {
        if (clearFirst)
            clear ();
        SetActive (true);
        for (uint32_t row1 = 1; row1 < numRow; row1++)
            {
            uint32_t row0 = row1 - 1;
            for (uint32_t col1 = 1; col1 < numPerRow; col1++)
                {
                size_t col0 = col1 - 1;
                int i00 = static_cast<int>(firstValue + col0 * rowStep + row0 * colStep);
                int i01 = static_cast<int>(firstValue + col1 * rowStep + row0 * colStep);
                int i10 = static_cast<int>(firstValue + col0 * rowStep + row1 * colStep);
                int i11 = static_cast<int>(firstValue + col1 * rowStep + row1 * colStep);
                if (triangulated)
                    {
                    push_back (i00); push_back (i01); push_back (i10); push_back (terminator);
                    push_back (i01); push_back (i11); push_back (i10); push_back (terminator);
                    }
                else
                    {
                    push_back (i00); push_back (i01); push_back (i11); push_back (i10);
                    push_back (terminator);
                    }
                }
            }
        }

    //! 计算最小值和最大值。
    //! Ported from: imodel-native BlockedVector.cpp:702-718
    bool MinMax (int& minValue, int& maxValue) const
        {
        minValue = INT_MAX;
        maxValue = INT_MIN;
        if (size () == 0)
            return false;
        minValue = maxValue = at (0);
        for (size_t i = 0; i < size (); i++)
            {
            int value = at (i);
            if (value < minValue) minValue = value;
            if (value > maxValue) maxValue = value;
            }
        return true;
        }

    //! 检查范围内是否全部为负。
    //! Ported from: imodel-native BlockedVector.cpp:725-731
    bool AllNegativeInRange (size_t iFirst, size_t iLast)
        {
        for (size_t i = iFirst; i <= iLast; i++)
            if (at (i) >= 0)
                return false;
        return true;
        }

    //! 取反范围内的值。
    //! Ported from: imodel-native BlockedVector.cpp:737-741
    void NegateInRange (size_t iFirst, size_t iLast)
        {
        for (size_t i = iFirst; i <= iLast; i++)
            at (i) = -at (i);
        }

    //! 范围内取绝对值。
    //! Ported from: imodel-native BlockedVector.cpp:747-751
    void AbsInRange (size_t iFirst, size_t iLast)
        {
        for (size_t i = iFirst; i <= iLast; i++)
            at (i) = std::abs (at (i));
        }

    //! 全部取绝对值。
    //! Ported from: imodel-native BlockedVector.cpp:757-760
    void Abs ()
        {
        if (size () > 0)
            AbsInRange (0, size () - 1);
        }

    //! 范围内取负绝对值。
    //! Ported from: imodel-native BlockedVector.cpp:766-770
    void NegativeAbsInRange (size_t iFirst, size_t iLast)
        {
        for (size_t i = iFirst; i <= iLast; i++)
            at (i) = -std::abs (at (i));
        }

    //! 从前任循环移位符号。
    //! Ported from: imodel-native BlockedVector.cpp:777-787
    void ShiftSignsFromCyclicPredecessorsInRange (size_t kFirst, size_t kLast)
        {
        int sign0 = at (kLast) >= 0 ? 1 : -1;
        int sign1;
        for (size_t k = kFirst; k <= kLast; k++)
            {
            sign1 = at (k) >= 0 ? 1 : -1;
            at (k) = sign0 * std::abs (at (k));
            sign0 = sign1;
            }
        }

    //! 定界面。
    //! Ported from: imodel-native BlockedVector.cpp:792-830
    bool DelimitFace (int numPerFace, size_t iFirst, size_t& iLast, size_t& iNext)
        {
        size_t count = size ();
        if (iFirst >= count)
            {
            iNext = iFirst;
            iLast = iFirst - 1;
            return false;
            }
        if (numPerFace > 1)
            {
            iLast = iFirst;
            size_t limit = iNext;
            while (iLast < limit && at (iLast) != 0)
                iLast++;
            iLast--;
            iNext = iFirst + numPerFace;
            return iLast >= iFirst;
            }
        else
            {
            while (iFirst < count && at (iFirst) == 0)
                iFirst++;
            if (iFirst >= count)
                {
                iNext = iFirst;
                iLast = iFirst - 1;
                return false;
                }
            iLast = iFirst + 1;
            while (iLast < count && at (iLast) != 0)
                iLast++;
            iNext = iLast + 1;
            iLast--;
            return true;
            }
        }

    //! 从源数组追加，非零索引按 shift 偏移。
    //! Ported from: imodel-native BlockedVector.cpp:833-845
    void AppendShifted (BlockedVectorInt const& source, int shift)
        {
        size_t n = source.size ();
        for (size_t i = 0; i < n; i++)
            {
            int value = source[i];
            if (value > 0)
                value += shift;
            else if (value < 0)
                value -= shift;
            push_back (value);
            }
        }
};

END_DQ_BASE_NAMESPACE
