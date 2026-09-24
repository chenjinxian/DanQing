// Ported from: FreeCAD src/Gui/Window.cpp
// Minimal stub for DisplayTestApp UI shell

#include <App/Application.h>
#include "Window.h"

using namespace Gui;

WindowParameter::WindowParameter(const char* name)
{
    if (name && strcmp(name, "") != 0) {
        _handle = getDefaultParameter()->GetGroup(name);
    }
}

WindowParameter::~WindowParameter() = default;

ParameterGrp::handle WindowParameter::getWindowParameter()
{
    return _handle;
}

bool WindowParameter::setGroupName(const char* name)
{
    if (_handle.isValid()) {
        return false;
    }
    if (name) {
        _handle = App::GetApplication().GetParameterGroupByPath(name);
    }
    return true;
}

ParameterGrp::handle WindowParameter::getDefaultParameter()
{
    return App::GetApplication().GetUserParameter().GetGroup("BaseApp")->GetGroup("Preferences");
}
