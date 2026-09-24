// Ported from: FreeCAD src/Gui/CommandStructure.cpp + CommandLink.cpp:1040
//              Structure domain commands. Each command ctor metadata is ported verbatim
//              from FreeCAD source; activated() bodies are stubs until backend is wired.
#include "Command.h"
#include "CommandStructure.h"

namespace Gui {

// =====================================================================
// Std_Part
// ============================================================================

DEF_STD_CMD_A(StdCmdPart)

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:45-58
StdCmdPart::StdCmdPart()
    : Command("Std_Part")
{
    sGroup = "Structure";
    sMenuText = QT_TR_NOOP("New Part");
    sToolTipText = QT_TR_NOOP(
        "Creates a part, which is a general-purpose container to group objects so they "
        "act as a unit in the 3D view. It is intended to arrange objects that have a part "
        "TopoShape, like part primitives, Part Design bodies, and other parts."
    );
    sWhatsThis = "Std_Part";
    sStatusTip = sToolTipText;
    sPixmap = "Geofeaturegroup";
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:60-116
void StdCmdPart::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (App::Part creation backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:118-121
bool StdCmdPart::isActive()
{
    return false;  // deferred — Step 3+ (App::Part creation backend)
}

// =====================================================================
// Std_Group
// ============================================================================

DEF_STD_CMD_A(StdCmdGroup)

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:128-141
StdCmdGroup::StdCmdGroup()
    : Command("Std_Group")
{
    sGroup = "Structure";
    sMenuText = QT_TR_NOOP("New Group");
    sToolTipText = QT_TR_NOOP(
        "Creates a group, which is a general-purpose container to group objects in the "
        "tree view, regardless of their data type. It is a simple folder to organize "
        "the objects in a model."
    );
    sWhatsThis = "Std_Group";
    sStatusTip = sToolTipText;
    sPixmap = "folder";
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:143-188
void StdCmdGroup::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (App::DocumentObjectGroup creation backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:190-193
bool StdCmdGroup::isActive()
{
    return false;  // deferred — Step 3+ (App::DocumentObjectGroup creation backend)
}

// =====================================================================
// Std_VarSet
// ============================================================================

DEF_STD_CMD_A(StdCmdVarSet)

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:200-210
StdCmdVarSet::StdCmdVarSet()
    : Command("Std_VarSet")
{
    sGroup = "Structure";
    sMenuText = QT_TR_NOOP("Variable Set");
    sToolTipText
        = QT_TR_NOOP("Creates a variable set, which is an object that maintains a set of properties to be used as variables");
    sWhatsThis = "Std_VarSet";
    sStatusTip = sToolTipText;
    sPixmap = "VarSet";
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:212-247
void StdCmdVarSet::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (App::VarSet creation backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:249-252
bool StdCmdVarSet::isActive()
{
    return false;  // deferred — Step 3+ (App::VarSet creation backend)
}

// =====================================================================
// Std_LinkActions (stub — FreeCAD uses GroupCommand, not available in DisplayTestApp)
// ============================================================================

DEF_STD_CMD_A(StdCmdLinkActions)

// Ported from: FreeCAD src/Gui/CommandLink.cpp:1043-1049
StdCmdLinkActions::StdCmdLinkActions()
    : Command("Std_LinkActions")
{
    sGroup = "Structure";
    sMenuText = QT_TR_NOOP("Link Actions");
    sToolTipText = QT_TR_NOOP("Actions for managing links between objects");
    sWhatsThis = "Std_LinkActions";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandLink.cpp:1067-1071
void StdCmdLinkActions::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (link actions dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdLinkActions::isActive()
{
    return false;  // deferred — Step 3+ (link actions dialog backend)
}

// =====================================================================
// Registration helper — called by CreateStdCommands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandStructure.cpp:254-266 (CreateStructureCommands)
void createStructureCommands(CommandManager& mgr)
{
    mgr.addCommand(new StdCmdPart());
    mgr.addCommand(new StdCmdGroup());
    mgr.addCommand(new StdCmdVarSet());
    mgr.addCommand(new StdCmdLinkActions());
}

} // namespace Gui
