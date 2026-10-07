// Ported from: FreeCAD src/Gui/CommandStd.cpp CreateStdCommands()
// Master registry declaration — ties all domain command files together.
#pragma once

namespace Gui {

class CommandManager;

/// Master registry: registers ALL FreeCAD standard commands across all domains.
/// Calls domain-specific helpers: createFileEditCommands, createViewCommands,
/// createWindowCommands.
/// （createStructureCommands/createToolsCommands/createMacroCommands M-L(2) 删；
///   createStdDomainCommands 2026-10-07 删除侧删——DTA 无 About 对话框。）
/// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
void createStdCommands(CommandManager& mgr);

} // namespace Gui
