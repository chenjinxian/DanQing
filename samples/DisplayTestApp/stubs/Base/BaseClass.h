// Stub: Base::BaseClass for FreeCAD UI shell
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/Base/BaseClass.h
#pragma once

#include <FCGlobal.h>
#include <Base/Type.h>

namespace Base {

class BaseExport BaseClass
{
public:
    virtual ~BaseClass() = default;
    virtual Base::Type getTypeId() const { return getClassTypeId(); }
    static Base::Type getClassTypeId() { return Base::Type::badType(); }
private:
    virtual void _anchor_vtable() const {}
};

} // namespace Base
