// Ported from: FreeCAD src/Gui/CommandMacro.cpp (header for registration)
// Declares createMacroCommands() — registers all Macro domain commands.
#pragma once

namespace Gui {

class CommandManager;

/// Register all Macro domain commands with the given manager.
/// Ported from: FreeCAD src/Gui/CommandMacro.cpp:206-218 (CreateMacroCommands)
void createMacroCommands(CommandManager& mgr);

} // namespace Gui
