// Ported from: FreeCAD src/Gui/CommandWindow.cpp:504-526 (CreateWindowStdCommands decl)
// Header for the Window-domain command registration helper.
#pragma once

namespace Gui {

class CommandManager;

/// Register Window-domain commands (Tile/Cascade/Activate Next+Prev/WindowsMenu)
/// with the given manager. Mirrors FreeCAD CreateWindowStdCommands() exactly, except
/// StdCmdCloseActiveWindow and StdCmdCloseAllWindows live in CommandDoc.cpp alongside
/// the File-domain close handling (already ported by region 1+2).
/// Std_Windows（no-op 存根）2026-10-07 删除侧已删——DTA 无"选择窗口"对话框。
/// Ported from: FreeCAD src/Gui/CommandWindow.cpp:507-524 (CreateWindowStdCommands)
void createWindowCommands(CommandManager& mgr);

} // namespace Gui
