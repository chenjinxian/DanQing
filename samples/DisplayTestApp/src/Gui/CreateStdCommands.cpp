// Ported from: FreeCAD src/Gui/CommandStd.cpp CreateStdCommands()
//              Master registry that ties all domain command files together.
//              Each domain helper registers its own commands; this file calls them all.
#include "Command.h"
#include "CommandDoc.h"        // createFileEditCommands
#include "CommandView.h"       // createViewCommands
#include "CommandStructure.h"  // createStructureCommands
#include "CommandStd.h"        // createStdDomainCommands (Tools/View/Macro items)
#include "CommandTools.h"      // createToolsCommands (module-level stubs + dependency graph)
#include "CommandMacro.h"      // createMacroCommands (DlgMacroRecord/Execute/etc.)
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
    createFileEditCommands(mgr);    // from CommandDoc  — File + Edit domains
    createViewCommands(mgr);        // from CommandView — View domain (cameras, draw style, etc.)
    createStructureCommands(mgr);   // from CommandStructure — Part, Group, VarSet, LinkActions
    createStdDomainCommands(mgr);   // from CommandStd — Tools/View/Macro/Help items from CommandStd.cpp
    createToolsCommands(mgr);       // from CommandTools — module-level stubs + dependency graph
    createMacroCommands(mgr);       // from CommandMacro — DlgMacroRecord/Execute/etc.
    createWindowCommands(mgr);      // from CommandWindow — Window domain (region 4 task 1)
}

} // namespace Gui
