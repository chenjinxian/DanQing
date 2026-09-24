// Stub: Base::Type for FreeCAD UI shell
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/Base/Type.h
#pragma once

#include <FCGlobal.h>
#include <string>

namespace Base {

class BaseExport Type
{
public:
    Type() = default;
    static Type badType() { return {}; }
};

} // namespace Base

// TYPESYSTEM macros — only the ones used by the UI shell
#define TYPESYSTEM_HEADER_WITH_OVERRIDE() \
public: \
    static Base::Type getClassTypeId(); \
    Base::Type getTypeId() const override; \
private: \
    static Base::Type classTypeId

#define TYPESYSTEM_SOURCE_ABSTRACT(_class_, _parentclass_) \
    Base::Type _class_::getClassTypeId() { return _class_::classTypeId; } \
    Base::Type _class_::getTypeId() const { return _class_::classTypeId; } \
    Base::Type _class_::classTypeId = Base::Type::badType();
