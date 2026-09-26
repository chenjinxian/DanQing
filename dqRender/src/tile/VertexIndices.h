// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — 24-bit vertex-index stream for imdl edge tables
// Ported from: itwinjs-core core/frontend/src/common/internal/render/VertexIndices.ts
//              (ctor :23-26 / length :28-29 / encodeIndex :40-45 /
//              setNthIndex :47-49 / decodeIndex :51-55)
//
// Holds an array of indices into a VertexTable. Each index is a 24-bit unsigned
// integer. The order of the indices specifies the order in which vertices are
// drawn.
//
// 未移植成员（U11(3) 消费链暂无调用者，登记后续）：
//   - fromArray (:31-38)、decodeIndices (:57-63)、[Symbol.iterator] (:65-72)。
#pragma once

#include "dqRender/tile/ImdlDocument.h"  // ImdlByteView（C++ 所有权适配的输入形态）

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

class VertexIndices {
public:
    // TS 参考字段 `public readonly data: Uint8Array`（本类型在 C++ 侧持有所有权
    // ——参考 ctor 契约 "This object takes ownership of the array"，:22-23）。
    // 断言 `0 === data.length % 3`（:25）在 C++ 生产的等价防御：仅由
    // indexedEdgeParamsFromCompactEdges（CompactEdges.cpp 的 numTotalEdges*6*3
    // 分配）与 findBufferView 校验过的区间构造，畸形长度在构造点被丢弃。
    std::vector<uint8_t> data;

    VertexIndices() = default;

    /// Directly construct from bytes in which each index occupies 3 contiguous
    /// bytes (VertexIndices.ts:19-26 — takes ownership).
    explicit VertexIndices(std::vector<uint8_t> bytes)
        : data(std::move(bytes))
    {
    }

    // C++ 所有权适配（§3.4）：参考侧 ParseImdlDocument.ts:720
    // `new VertexIndices(indices)` 直接包住二进制段上的 Uint8Array 视图（零拷贝
    // ——GC 持有）；C++ 的输入是 ImdlByteView（指入 ImdlDocument::binary），本
    // 构造做一次保形拷贝以取得所有权。EQUIVALENCE 登记：发散=索引流的一次
    // memcpy（参考零拷贝）；数值无发散；验证法=CompactEdges 数值锁
    // （展开输出的 edge 表字节不受输入构造方式影响）。
    explicit VertexIndices(ImdlByteView view)
        : data(view.data, view.data + view.byteLength)
    {
    }

    /// Get the number of 24-bit indices. (VertexIndices.ts:28-29)
    size_t length() const { return data.size() / 3; }

    /// Convert a 24-bit unsigned integer value into bytes.
    /// (VertexIndices.ts:40-45 encodeIndex)
    static void encodeIndex(uint32_t index, std::vector<uint8_t>& bytes, size_t byteIndex)
    {
        bytes[byteIndex + 0] = static_cast<uint8_t>(index & 0x000000ffu);
        bytes[byteIndex + 1] = static_cast<uint8_t>((index & 0x0000ff00u) >> 8);
        bytes[byteIndex + 2] = static_cast<uint8_t>((index & 0x00ff0000u) >> 16);
    }

    /// (VertexIndices.ts:47-49 setNthIndex)
    void setNthIndex(size_t n, uint32_t value)
    {
        encodeIndex(value, data, n * 3);
    }

    /// (VertexIndices.ts:51-55 decodeIndex)
    uint32_t decodeIndex(size_t index) const
    {
        size_t const byteIndex = index * 3;
        return static_cast<uint32_t>(data[byteIndex])
            | (static_cast<uint32_t>(data[byteIndex + 1]) << 8)
            | (static_cast<uint32_t>(data[byteIndex + 2]) << 16);
    }
};

END_DQ_RENDER_NAMESPACE
