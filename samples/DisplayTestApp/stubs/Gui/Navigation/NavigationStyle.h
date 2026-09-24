// Ported from: FreeCAD src/Gui/Navigation/NavigationStyle.h:449-462
//                FreeCAD src/Gui/Navigation/NavigationStyle.cpp:2500-2536
//                FreeCAD src/Gui/CMakeLists.txt SET(Navigation_CPP_SRCS ...)
// Stub: minimal for DTA FirstStart — only the API surface consumed by
// StartGui::GeneralSettingsWidget is provided. The real FreeCAD
// UserNavigationStyle::getUserFriendlyNames() walks Base::Type registrations
// of every NavigationStyle subclass compiled into the Gui lib (see
// FreeCAD src/Gui/CMakeLists.txt SET(Navigation_CPP_SRCS ...)) and calls the
// virtual userFriendlyName() on each. This stub returns the same language-
// neutral names (the values the base implementation returns when no
// translation is installed) keyed by the type name string.
//
// Deviations from reference (§0 backend-deferred):
//   - Returns std::map<std::string /*type name*/, std::string /*friendly*/>
//     instead of std::map<Base::Type, std::string>, because the DTA shell
//     does not ship a Base::Type registry. Type-name strings are used as
//     keys so the combo can still store the same data FreeCAD stores in the
//     "NavigationStyle" preference.
//   - defaultStyleName() stands in for
//     Gui::CADNavigationStyle::getClassTypeId().getName() (the FreeCAD
//     default for the "NavigationStyle" preference).
#pragma once

#include <FCGlobal.h>
#include <map>
#include <string>

namespace Gui
{

// Ported from: FreeCAD src/Gui/Navigation/NavigationStyle.h:452-462
class GuiExport UserNavigationStyle
{
public:
    UserNavigationStyle() = default;
    virtual ~UserNavigationStyle() = default;
    virtual std::string userFriendlyName() const { return {}; }

    // Ported from: FreeCAD src/Gui/Navigation/NavigationStyle.cpp:2519-2536
    // Stub: FreeCAD walks Base::Type::getAllDerivedFrom(UserNavigationStyle) and
    // calls inst->userFriendlyName(); here we return the fixed table shipped by
    // the 12 NavigationStyle subclasses in FreeCAD src/Gui/Navigation/*.cpp
    // (the base UserNavigationStyle::userFriendlyName strips the
    // "::Gui::" prefix and "NavigationStyle" suffix from the type name).
    static std::map<std::string, std::string> getUserFriendlyNames()
    {
        return {
            {"Gui::BlenderNavigationStyle",     "Blender"},
            {"Gui::CADNavigationStyle",         "CAD"},
            {"Gui::GestureNavigationStyle",     "Gesture"},
            {"Gui::InventorNavigationStyle",    "OpenInventor"},
            {"Gui::MayaGestureNavigationStyle", "MayaGesture"},
            {"Gui::OpenCascadeNavigationStyle", "OpenCascade"},
            {"Gui::OpenSCADNavigationStyle",    "OpenSCAD"},
            {"Gui::RevitNavigationStyle",       "Revit"},
            {"Gui::SiemensNXNavigationStyle",   "Siemens NX"},
            {"Gui::SolidWorksNavigationStyle",  "SolidWorks"},
            {"Gui::TinkerCADNavigationStyle",   "TinkerCAD"},
            {"Gui::TouchpadNavigationStyle",    "Touchpad"},
        };
    }

    // Ported from: FreeCAD src/Mod/Start/Gui/GeneralSettingsWidget.cpp:247
    // Stub: stands in for Gui::CADNavigationStyle::getClassTypeId().getName()
    // — the FreeCAD default for the "NavigationStyle" preference.
    static constexpr const char* defaultStyleName() { return "Gui::CADNavigationStyle"; }
};

}  // namespace Gui
