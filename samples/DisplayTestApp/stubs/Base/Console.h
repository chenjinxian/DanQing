// Stub: FreeCAD Base::Console
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/Base/Console.h
#pragma once

#include <FCGlobal.h>

namespace Base {

class BaseExport ConsoleSingleton
{
public:
    static ConsoleSingleton& instance() {
        static ConsoleSingleton inst;
        return inst;
    }
    template<typename... Args> void log(const char*, Args&&...) {}
    template<typename... Args> void warning(const char*, Args&&...) {}
};

inline ConsoleSingleton& Console() { return ConsoleSingleton::instance(); }

} // namespace Base

#define FC_WARN(msg) Base::Console().warning("%s", msg)
