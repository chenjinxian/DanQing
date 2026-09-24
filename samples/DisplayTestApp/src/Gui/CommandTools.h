// Ported from: FreeCAD src/Gui/ (various Command*.cpp files)
// Declares createToolsCommands() — registers Tools-domain commands
// not already in CommandDoc/CommandView/CommandStd.
#pragma once

namespace Gui {

class CommandManager;

/// Register all Tools-domain commands with the given manager.
/// Covers module-level commands (AddonMgr, Measure, MassProperties, etc.)
/// and dependency graph commands from FreeCAD CommandDoc.cpp.
/// Ported from: FreeCAD src/Gui/CommandDoc.cpp + Mod/Measure/Gui/Command.cpp
void createToolsCommands(CommandManager& mgr);

} // namespace Gui
