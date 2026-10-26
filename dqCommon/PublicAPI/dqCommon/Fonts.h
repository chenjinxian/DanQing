// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — font identifiers
// Ported from: itwinjs-core core/common/src/Fonts.ts（FontType/FontId 节选——
// FontMap/FontFamilyDescriptor 随消费面落地）
#pragma once

#include "DqCommon.h"

#include <cstdint>
#include <optional>
#include <string>

BEGIN_DQ_COMMON_NAMESPACE

// The encoding of a font family's faces in the iModel's embedded font files.
// Ported from: itwinjs-core FontType (Fonts.ts:14-32)
enum class FontType : int32_t {
    // OpenType format, derived from and compatible with the earlier TrueType
    // format (.ttf/.ttc/.otf/.otc).
    TrueType = 1,
    // RSC — a simple font format originating in MicroStation.
    Rsc = 2,
    // SHX — a simple font format originating in AutoCAD (.shx).
    Shx = 3,
};

// An unsigned integer uniquely identifying a font family in the context of an
// iModel. A font Id of zero represents an invalid/non-existent font.
// Ported from: itwinjs-core FontId (Fonts.ts:42)
using FontId = uint32_t;

// Uniquely describes one or more FontFaces sharing the same name and type
// comprising a single font family.
// Ported from: itwinjs-core FontFamilySelector (Fonts.ts:51-58) —
// FontFamilyDescriptor 的 type 可选形态（IModelDbFonts.findId 查询面）。
struct FontFamilySelector {
    // The encoding in which the font family's faces are stored.
    //（nullopt = 查询面"任意类型"语义——参考 `type?: FontType`。）
    std::optional<FontType> type;
    // The name of the family.
    std::string name;
};

END_DQ_COMMON_NAMESPACE
