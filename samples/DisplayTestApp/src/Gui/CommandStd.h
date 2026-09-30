// Ported from: FreeCAD src/Gui/CommandStd.cpp (header for registration)
// Declares createStdDomainCommands() — registers the Std-domain remainder.
// M-L(2)：Help 链接命令族与 Tools/Macro 存根全删（分析报告 §3.1/§3.5），
// 本域仅存 Std_AboutQt（真功能，归 Tools）。
#pragma once

namespace Gui {

class CommandManager;

/// Register the surviving Std-domain commands with the given manager.
/// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
void createStdDomainCommands(CommandManager& mgr);

} // namespace Gui
