// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — text field types (FieldRun dependencies)
// Ported from: itwinjs-core core/common/src/annotation/TextField.ts
#pragma once

#include "DqCommon.h"

#include <dqBase/DqId.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Kinds of property values usable as the basis of a FieldRun's display string
// ("quantity"/"coordinate"/"string"/"boolean"/"datetime"/"int-enum"/"string-enum"
// — currently only "string" and "datetime" formatting implemented, :22).
// Ported from: itwinjs-core FieldPropertyType (TextField.ts:25)
namespace FieldPropertyType {
    inline constexpr char const* Quantity = "quantity";
    inline constexpr char const* Coordinate = "coordinate";
    inline constexpr char const* String = "string";
    inline constexpr char const* Boolean = "boolean";
    inline constexpr char const* Datetime = "datetime";
    inline constexpr char const* IntEnum = "int-enum";
    inline constexpr char const* StringEnum = "string-enum";
}

// One step of a FieldPropertyPath: a property name or an array index.
// Ported from: itwinjs-core FieldPropertyPath.accessors element
//              (Array<string | number>, TextField.ts:47)
struct FieldAccessor {
    enum class Kind : int32_t { Name = 0, Index };
    Kind kind = Kind::Name;
    std::string name;   // valid iff kind == Name
    int64_t index = 0;  // valid iff kind == Index

    bool operator==(FieldAccessor const& other) const noexcept
    {
        return kind == other.kind && (kind == Kind::Name ? name == other.name
                                                         : index == other.index);
    }
};

// A chain of property accesses resolving to the primitive value that forms the
// basis of a FieldRun's displayed content.
// Ported from: itwinjs-core FieldPropertyPath (TextField.ts:43-48)
struct FieldPropertyPath {
    // The BIS property of the FieldPropertyHost serving as the root of the path.
    std::string propertyName;
    // Property names and/or array indices from propertyName to the ultimate
    // property.（空 = 参考的 undefined。）
    std::vector<FieldAccessor> accessors;

    // FieldRun.equals 只比较 propertyName 与 accessors（TextBlock.ts:534-547）。
    bool operator==(FieldPropertyPath const& other) const noexcept
    {
        if (propertyName != other.propertyName || accessors.size() != other.accessors.size())
            return false;
        for (size_t i = 0; i < accessors.size(); ++i)
            if (!(accessors[i] == other.accessors[i]))
                return false;
        return true;
    }
};

// The source of the property value against which a FieldPropertyPath is
// evaluated — always an Element.
// Ported from: itwinjs-core FieldPropertyHost (TextField.ts:58-65)
struct FieldPropertyHost {
    dqBase::DqId elementId;
    std::string schemaName;
    std::string className;

    // TextBlock.ts:526-529 FieldRun.equals 的 host 比较面。
    bool operator==(FieldPropertyHost const& other) const noexcept
    {
        return elementId == other.elementId && schemaName == other.schemaName
            && className == other.className;
    }
};

// How to modify the case of a display string: "as-is" | "upper" | "lower".
// Ported from: itwinjs-core FieldCase (TextField.ts:71)
namespace FieldCase {
    inline constexpr char const* AsIs = "as-is";
    inline constexpr char const* Upper = "upper";
    inline constexpr char const* Lower = "lower";
}

// Datetime formatting options（Intl.DateTimeFormatOptions 为 JS 标准库面，
// DanQing 无对应——按 FieldRun.equals 的字符串表示比较语义（TextBlock.ts
// :550-557 "compare the string representations"）以 JSON 序列化形态承载。
// TODO: 字段求值面（ElementDrivesTextAnnotation——➖9 编辑写入族结案）落地时复核。
// Ported from: itwinjs-core DateTimeFieldFormatOptions (TextField.ts:76-83)
struct DateTimeFieldFormatOptions {
    std::optional<std::string> locale;        // default "en-US"
    std::optional<std::string> formatOptionsJson;  // ← Intl.DateTimeFormatOptions 序列化

    bool operator==(DateTimeFieldFormatOptions const& other) const noexcept
    {
        return locale == other.locale && formatOptionsJson == other.formatOptionsJson;
    }
};

// Customizes how to format a raw property value into a FieldRun's display string.
// Ported from: itwinjs-core FieldFormatOptions (TextField.ts:89-98)。
// `case` 为 C++ 关键字——最小区分改名 caseForm（§3.4 碰撞例外）。
struct FieldFormatOptions {
    std::optional<std::string> prefix;   // not affected by caseForm
    std::optional<std::string> suffix;   // not affected by caseForm
    std::optional<std::string> caseForm;  // ← 参考名 `case`（FieldCase 值域）
    std::optional<DateTimeFieldFormatOptions> dateTime;

    // 参考以 JSON.stringify 比较两者（TextBlock.ts:550-557，"anticipate new
    // formatting options"）——C++ 形逐字段等值（新字段加入时同步 ==）。
    bool operator==(FieldFormatOptions const& other) const noexcept
    {
        bool const dc = dateTime.has_value() == other.dateTime.has_value()
            && (!dateTime.has_value() || *dateTime == *other.dateTime);
        return prefix == other.prefix && suffix == other.suffix
            && caseForm == other.caseForm && dc;
    }
};

END_DQ_COMMON_NAMESPACE
