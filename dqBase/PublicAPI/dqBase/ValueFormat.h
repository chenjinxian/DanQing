// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/ValueFormat.h
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/ValueFormat.r.h
// DanQing dqBase — 数值格式化
//
// 1:1 对齐 imodel-native DoubleFormatter / PrecisionType / PrecisionFormat。
#pragma once

#include "Export.h"
#include "RefCounted.h"
#include "DqTypes.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 精度类型枚举
// Ported from: imodel-native ValueFormat.r.h
// ---------------------------------------------------------------------------
enum class PrecisionType : int {
    Decimal     = 0,
    Fractional  = 1,
    Scientific  = 2,
};

enum class PrecisionFormat : int {
    DecimalWhole         = 100,
    Decimal1Place        = 101,
    Decimal2Places       = 102,
    Decimal3Places       = 103,
    Decimal4Places       = 104,
    Decimal5Places       = 105,
    Decimal6Places       = 106,
    Decimal7Places       = 107,
    Decimal8Places       = 108,
    FractionalWhole      = 200,  // Ported from: imodel-native ValueFormat.r.h:39
    FractionalHalf       = 201,
    FractionalQuarter    = 202,
    FractionalEighth     = 203,  // Ported from: imodel-native ValueFormat.r.h:43 (原 Fractional8th)
    Fractional16th       = 204,
    Fractional32nd       = 205,
    Fractional64th       = 206,
    Fractional128th      = 207,
    Fractional256th      = 208,
    ScientificWhole      = 300,  // Ported from: imodel-native ValueFormat.r.h:48
    Scientific1Place     = 301,
    Scientific2Places    = 302,
    Scientific3Places    = 303,
    Scientific4Places    = 304,
    Scientific5Places    = 305,
    Scientific6Places    = 306,
    Scientific7Places    = 307,
    Scientific8Places    = 308,
};

// ---------------------------------------------------------------------------
// DoubleFormatter — 数值格式化器
// Ported from: imodel-native ValueFormat.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT DoubleFormatter : public RefCounted<DoubleFormatter> {
public:
    static RefPtr<DoubleFormatter> Create(PrecisionFormat format = PrecisionFormat::Decimal2Places);

    /// 格式化数值为字符串
    DqString ToString(double value) const;

    /// 获取/设置精度
    PrecisionFormat GetPrecision() const { return m_precision; }
    void SetPrecision(PrecisionFormat precision) { m_precision = precision; }

    /// 获取/设置小数分隔符
    char GetDecimalSeparator() const { return m_decimalSeparator; }
    void SetDecimalSeparator(char sep) { m_decimalSeparator = sep; }

    /// 获取/设置千位分隔符
    char GetThousandsSeparator() const { return m_thousandsSeparator; }
    void SetThousandsSeparator(char sep) { m_thousandsSeparator = sep; }

    /// 是否插入千位分隔符
    bool GetInsertThousandsSeparator() const { return m_insertThousands; }
    void SetInsertThousandsSeparator(bool v) { m_insertThousands = v; }

    /// 克隆
    RefPtr<DoubleFormatter> Clone() const;

private:
    DoubleFormatter() = default;
    PrecisionFormat m_precision = PrecisionFormat::Decimal2Places;
    char m_decimalSeparator = '.';
    char m_thousandsSeparator = ',';
    bool m_insertThousands = false;
};

using DoubleFormatterPtr = RefPtr<DoubleFormatter>;

END_DQ_BASE_NAMESPACE
