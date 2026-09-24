// Ported from: FreeCAD src/Gui/CommandMacro.cpp
//              Macro domain commands. Each command ctor metadata is ported verbatim
//              from FreeCAD source; activated() bodies are stubs until backend is wired.
#include "CommandMacro.h"
#include "Command.h"

namespace Gui {

// =====================================================================
// Std_DlgMacroRecord
// ============================================================================

DEF_STD_CMD_A(StdCmdDlgMacroRecord)

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:43-54
StdCmdDlgMacroRecord::StdCmdDlgMacroRecord()
    : Command("Std_DlgMacroRecord")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("Record &Macro");
    sToolTipText = QT_TR_NOOP("Opens a dialog to record a macro");
    sWhatsThis = "Std_DlgMacroRecord";
    sStatusTip = sToolTipText;
    sPixmap = "media-record";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:56-79
void StdCmdDlgMacroRecord::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (macro record dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:81-84
bool StdCmdDlgMacroRecord::isActive()
{
    return false;  // deferred — Step 3+ (macro record dialog backend)
}

// =====================================================================
// Std_DlgMacroExecute
// ============================================================================

DEF_STD_CMD_A(StdCmdDlgMacroExecute)

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:91-102
StdCmdDlgMacroExecute::StdCmdDlgMacroExecute()
    : Command("Std_DlgMacroExecute")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("Ma&cros");
    sToolTipText = QT_TR_NOOP("Opens a dialog to execute a recorded macro");
    sWhatsThis = "Std_DlgMacroExecute";
    sStatusTip = sToolTipText;
    sPixmap = "accessories-text-editor";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:104-109
void StdCmdDlgMacroExecute::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (macro execute dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:111-114
bool StdCmdDlgMacroExecute::isActive()
{
    return false;  // deferred — Step 3+ (macro execute dialog backend)
}

// =====================================================================
// Std_DlgMacroExecuteDirect
// ============================================================================

DEF_STD_CMD_A(StdCmdDlgMacroExecuteDirect)

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:121-132
StdCmdDlgMacroExecuteDirect::StdCmdDlgMacroExecuteDirect()
    : Command("Std_DlgMacroExecuteDirect")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("&Execute Macro");
    sToolTipText = QT_TR_NOOP("Executes the macro in the editor");
    sWhatsThis = "Std_DlgMacroExecuteDirect";
    sStatusTip = sToolTipText;
    sPixmap = "media-playback-start";
    sAccel = "Ctrl+F6";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:134-138
void StdCmdDlgMacroExecuteDirect::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (macro execute-direct tool backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:140-143
bool StdCmdDlgMacroExecuteDirect::isActive()
{
    return false;  // deferred — Step 3+ (macro execute-direct tool backend)
}

// =====================================================================
// Std_MacroAttachDebugger
// ============================================================================

DEF_STD_CMD_A(StdCmdMacroAttachDebugger)

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:147-157
StdCmdMacroAttachDebugger::StdCmdMacroAttachDebugger()
    : Command("Std_MacroAttachDebugger")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("&Attach to Remote Debugger");
    sToolTipText = QT_TR_NOOP("Attaches to a remotely running debugger");
    sWhatsThis = "Std_MacroAttachDebugger";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:159-167
void StdCmdMacroAttachDebugger::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (remote debugger dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:169-172
bool StdCmdMacroAttachDebugger::isActive()
{
    return false;  // deferred — Step 3+ (remote debugger dialog backend)
}

// =====================================================================
// Std_OpenMacrosFolder
// ============================================================================

DEF_STD_CMD_A(StdCmdMacrosFolder)

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:176-187
StdCmdMacrosFolder::StdCmdMacrosFolder()
    : Command("Std_OpenMacrosFolder")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("Open Macro Folder");
    sToolTipText = QT_TR_NOOP("Opens the macros folder in the system file manager");
    sWhatsThis = "Std_OpenMacrosFolder";
    sStatusTip = sToolTipText;
    sPixmap = "MacroFolder";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:189-199
void StdCmdMacrosFolder::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (open macros folder backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:201-204
bool StdCmdMacrosFolder::isActive()
{
    return false;  // deferred — Step 3+ (open macros folder backend)
}

// =====================================================================
// Registration helper — called by CreateStdCommands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandMacro.cpp:206-218 (CreateMacroCommands)
void createMacroCommands(CommandManager& mgr)
{
    mgr.addCommand(new StdCmdDlgMacroRecord());
    mgr.addCommand(new StdCmdDlgMacroExecute());
    mgr.addCommand(new StdCmdMacrosFolder());
    mgr.addCommand(new StdCmdDlgMacroExecuteDirect());
    mgr.addCommand(new StdCmdMacroAttachDebugger());
}

} // namespace Gui
