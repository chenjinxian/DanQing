// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/PTypesU.h
// DanQing dqBase — 低级类型联合
//
// 1:1 对齐 imodel-native PTypesU.h。
// 在不要求对齐的平台上，非对齐类型是原生类型的别名。
// 在要求对齐的平台上（DATA_ALIGNMENT_FORCED），非对齐类型是字节数组 + memcpy 转换。
// Ptypes_u 联合提供类型双关（type punning）能力。
#pragma once

#include <cstdint>
#include <cstring>

namespace dqBase {

// ---------------------------------------------------------------------------
// 非对齐类型（对齐 imodel-native PTypesU.h:7-53）
// ---------------------------------------------------------------------------

#if defined (DATA_ALIGNMENT_FORCED)
struct UnalignedShort {
    char sh [sizeof (short)];
};

struct UnalignedLong {
    char lo [sizeof (long)];
};

struct UnalignedPointer {
    char ptr [sizeof (void *)];
};

struct UnalignedDouble {
    char db [sizeof (double)];
};

struct UnalignedInt64 {
    char db [sizeof (int64_t)];
};

#if defined (FULL_DOUBLE_ALIGNMENT)
struct LongAlignedDouble {
    long dl [2];
};

struct LongAlignedInt64 {
    long dl [2];
};
#else
typedef double LongAlignedDouble;
typedef int64_t LongAlignedInt64;
#endif

#else
// 不要求对齐：直接用原生类型
typedef short UnalignedShort;
typedef long  UnalignedLong;
typedef double UnalignedDouble, LongAlignedDouble;
typedef void *UnalignedPointer;
typedef int64_t UnalignedInt64, LongAlignedInt64;
#endif

// ---------------------------------------------------------------------------
// 类型联合（对齐 imodel-native PTypesU.h:55-88）
// ---------------------------------------------------------------------------

union Shorts {
    unsigned short ush;
    short          sh;
    UnalignedShort unalignedShort;
};

union Longs {
    uint32_t    uLg;
    long        lg;
    UnalignedLong unalignedLong;
};

union Pointers {
    void            *pVoid;
    UnalignedPointer uap;
};

struct StackDouble {
    long sd [2];
};

// StackInt64 与 StackDouble 共享布局（对齐 imodel-native: 同一 struct 的两个 typedef）
typedef StackDouble StackInt64;

union DoubleArg {
    StackDouble ld;
    double      d;
};

// ---------------------------------------------------------------------------
// Ptypes_u — 通用类型双关联合（对齐 imodel-native PTypesU.h:89-121）
// ---------------------------------------------------------------------------
union Ptypes_u {
    void            *pv;
    char            *pc;
    unsigned char   *puc;
    wchar_t         *pWideChar;
    uint16_t        *pUtf16Char;
    short           *ps;
    UnalignedShort  *pUnalignedShort;
    unsigned short  *pus;
    long            *pl;
    unsigned long   *pul;
    UnalignedLong   *pUnalignedLong;
    int             *pi;
    float           *pf;
    double          *pd;
    UnalignedDouble *pUnalignedDouble;
    LongAlignedDouble *pLongAlignedDouble;
    int64_t         *pi64;
    uint64_t        *pui64;
    UnalignedInt64  *pUnalignedInt64;
    LongAlignedInt64 *pLongAlignedInt64;
    void            **ppv;
    char            **ppc;
    short           **pps;
    unsigned short  **ppus;
    int             **ppi;
    long            **ppl;
    unsigned char   **ppuc;
    double          **ppd;
    uint64_t        **ppui64;
};

} // namespace dqBase
