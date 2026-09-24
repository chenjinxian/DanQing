// Ported from: FreeCAD src/Gui/CommandWindow.cpp:504-526 (CreateWindowStdCommands decl)
// Header for the Window-domain command registration helper.
#pragma once

namespace Gui {

class CommandManager;

/// Register Window-domain commands (Tile/Cascade/Activate Next+Prev/Windows/WindowsMenu)
/// with the given manager. Mirrors FreeCAD CreateWindowStdCommands() exactly, except
/// StdCmdCloseActiveWindow and StdCmdCloseAllWindows live in CommandDoc.cpp alongside
/// the File-domain close handling (already ported by region 1+2).
/// Ported from: FreeCAD src/Gui/CommandWindow.cpp:507-524 (CreateWindowStdCommands)
void createWindowCommands(CommandManager& mgr);

} // namespace Gui
