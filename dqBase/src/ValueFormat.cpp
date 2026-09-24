// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/ValueFormat.h
// DanQing dqBase — 数值格式化实现
#include "dqBase/ValueFormat.h"

#include <cmath>
#include <cstdio>
#include <cstring>

BEGIN_DQ_BASE_NAMESPACE

RefPtr<DoubleFormatter> DoubleFormatter::Create(PrecisionFormat format) {
    RefPtr<DoubleFormatter> f(new DoubleFormatter());
    f->m_precision = format;
    return f;
}

DqString DoubleFormatter::ToString(double value) const {
    char buf[128];
    int precision = 2;
    switch (m_precision) {
    case PrecisionFormat::DecimalWhole: precision = 0; break;
    case PrecisionFormat::Decimal1Place: precision = 1; break;
    case PrecisionFormat::Decimal2Places: precision = 2; break;
    case PrecisionFormat::Decimal3Places: precision = 3; break;
    case PrecisionFormat::Decimal4Places: precision = 4; break;
    case PrecisionFormat::Decimal5Places: precision = 5; break;
    case PrecisionFormat::Decimal6Places: precision = 6; break;
    case PrecisionFormat::Decimal7Places: precision = 7; break;
    case PrecisionFormat::Decimal8Places: precision = 8; break;
    default: precision = 2; break;
    }
    snprintf(buf, sizeof(buf), "%.*f", precision, value);
    return DqString(buf);
}

RefPtr<DoubleFormatter> DoubleFormatter::Clone() const {
    RefPtr<DoubleFormatter> f(new DoubleFormatter());
    f->m_precision = m_precision;
    f->m_decimalSeparator = m_decimalSeparator;
    f->m_thousandsSeparator = m_thousandsSeparator;
    f->m_insertThousands = m_insertThousands;
    return f;
}

END_DQ_BASE_NAMESPACE
