// Stub: Base::UnitsApi for FreeCAD UI shell
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/Base/UnitsApi.h
#pragma once

#include <FCGlobal.h>
#include <string>
#include <vector>

namespace Base {

class BaseExport UnitsApi
{
public:
    static std::vector<std::string> getDescriptions() {
        return {
            "Standard (mm/kg/s)", "MKS (m/kg/s)", "Imperial (in/lb)",
            "Imperial Decimal", "Centimeters", "Imperial Building (ft-in)",
            "Mm/Min", "Imperial Civil (ft)", "FEM (mm/N/s)",
            "Meter", "Meter/kg/s"
        };
    }
};

} // namespace Base
