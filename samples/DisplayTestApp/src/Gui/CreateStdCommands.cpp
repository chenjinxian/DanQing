// Ported from: FreeCAD src/Gui/CommandStd.cpp CreateStdCommands()
//              Master registry that ties all domain command files together.
//              Each domain helper registers its own commands; this file calls them all.
#include "Command.h"
#include "CommandDoc.h"        // createFileEditCommands (File: New/Import/Quit)
#include "CommandView.h"       // createViewCommands (View: cameras/standard views/panels/status bar)
#include "CommandWindow.h"     // createWindowCommands (Window domain: Tile/Cascade/Activate/WindowsMenu)
namespace Gui {

// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
// C++ adaptation: dispatches to domain-specific registration helpers.
// Naming: the master function is createStdCommands(); the Std-domain helper
// is createStdDomainCommands() to avoid clash.
// Help commands live in CommandStd.cpp (matching FreeCAD) and are registered
// by createStdDomainCommands(); no separate createHelpCommands forwarder needed.
void createStdCommands(CommandManager& mgr)
{
    createFileEditCommands(mgr);    // from CommandDoc  — File domain (New/Import/Quit)
    createViewCommands(mgr);        // from CommandView — View domain (cameras, standard views, etc.)
    // M-L(2)：Structure/Tools/Macro 三域全删（注册 15、真功能 0——分析报告 §3.5
    // 按域量化表）。Std 域（Std_AboutQt）2026-10-07 删除侧再清：DTA 无 About
    // 对话框——CommandStd.cpp/.h 整删（Tools 菜单随之消失）。
    createWindowCommands(mgr);      // from CommandWindow — Window domain (region 4 task 1)
}

} // namespace Gui
