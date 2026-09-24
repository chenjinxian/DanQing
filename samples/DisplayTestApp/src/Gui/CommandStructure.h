// Ported from: FreeCAD src/Gui/CommandStructure.cpp (header for registration)
// Declares createStructureCommands() — registers all Structure domain commands.
#pragma once

namespace Gui {

class CommandManager;

/// Register all Structure domain commands (Part, Group, VarSet) with the given manager.
/// Ported from: FreeCAD src/Gui/CommandStructure.cpp:254-266 (CreateStructureCommands)
void createStructureCommands(CommandManager& mgr);

} // namespace Gui
