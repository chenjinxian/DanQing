// Ported from: FreeCAD src/Gui/CommandStd.cpp (header for registration)
// Declares createStdDomainCommands() — registers all Std-domain commands
// (Help, Tools, Macro items) not already in CommandDoc/CommandView.
#pragma once

namespace Gui {

class CommandManager;

/// Register all Std-domain commands with the given manager.
/// Covers Help, Tools, Macro items from FreeCAD CommandStd.cpp.
/// Note: Std_Workbench, Std_RecentFiles, Std_UserEditMode already in CommandView/CommandDoc.
/// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
void createStdDomainCommands(CommandManager& mgr);

} // namespace Gui
