// Ported from: FreeCAD src/Gui/Window.h
// Minimal stub for DisplayTestApp UI shell
#pragma once

#include <FCGlobal.h>
#include <Base/Parameter.h>

namespace Gui
{

/**
 * Adapter class to the parameter of FreeCAD for all windows.
 * Ported from: FreeCAD src/Gui/Window.h
 */
class GuiExport WindowParameter
{
public:
    WindowParameter(const char* name);
    virtual ~WindowParameter();

    static ParameterGrp::handle getDefaultParameter();
    ParameterGrp::handle getWindowParameter();

protected:
    bool setGroupName(const char* name);

private:
    ParameterGrp::handle _handle;
};

}  // namespace Gui
