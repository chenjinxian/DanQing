// Ported from: FreeCAD src/Gui/CommandDoc.cpp + CommandStd.cpp +
//              Mod/Measure/Gui/Command.cpp + Gui/AddonManager
//              Tools-domain commands (module-level stubs + dependency graph).
//              Each command ctor metadata is ported verbatim from FreeCAD source;
//              activated() bodies are stubs until backend is wired.
#include "CommandTools.h"
#include "Command.h"

namespace Gui {

// =====================================================================
// Module-level stubs (commands defined in FreeCAD modules, not src/Gui/)
// =====================================================================

//===========================================================================
// Std_AddonMgr
//===========================================================================

// Ported from: Authored — FreeCAD Std_AddonMgr is in AddonManager module, not src/Gui
class StdCmdAddonMgr : public Gui::Command {
public:
    StdCmdAddonMgr();
    ~StdCmdAddonMgr() override = default;
    const char* className() const override { return "StdCmdAddonMgr"; }

protected:
    void activated(int iMsg) override;
};

// Ported from: Authored — metadata from FreeCAD AddonManager Cmd.cpp
StdCmdAddonMgr::StdCmdAddonMgr()
    : Command("Std_AddonMgr")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("&Addon Manager");
    sToolTipText = QT_TR_NOOP("Opens the Addon Manager to install workbenches, macros, and preference packs");
    sWhatsThis = "Std_AddonMgr";
    sStatusTip = sToolTipText;
    sPixmap = "Std_AddonMgr";
    eType = 0;
}

// Ported from: Authored — no direct backend in DisplayTestApp
void StdCmdAddonMgr::activated(int /*iMsg*/)
{
    // TODO: wire to Addon Manager dialog when backend is available
}

//===========================================================================
// Std_Measure
//===========================================================================

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:45-80
class StdCmdMeasure : public Gui::Command {
public:
    StdCmdMeasure();
    ~StdCmdMeasure() override = default;
    const char* className() const override { return "StdCmdMeasure"; }

protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:47-56
StdCmdMeasure::StdCmdMeasure()
    : Command("Std_Measure")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("&Measure");
    sToolTipText = QT_TR_NOOP("Measures a feature");
    sWhatsThis = "Std_Measure";
    sStatusTip = QT_TR_NOOP("Measures a feature");
    sPixmap = "umf-measurement";
}

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:58-65
void StdCmdMeasure::activated(int /*iMsg*/)
{
    // TODO: wire to TaskMeasure when backend is available
}

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:67-80
bool StdCmdMeasure::isActive()
{
    return true;  // TODO: wire to App::GetApplication().getActiveDocument() + selection check
}

//===========================================================================
// Std_MassProperties
//===========================================================================

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:91-120
class StdCmdMassProperties : public Gui::Command {
public:
    StdCmdMassProperties();
    ~StdCmdMassProperties() override = default;
    const char* className() const override { return "StdCmdMassProperties"; }

protected:
    void activated(int iMsg) override;
    bool isActive() override;
};

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:93-102
StdCmdMassProperties::StdCmdMassProperties()
    : Command("Std_MassProperties")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Mass Properties");
    sToolTipText = QT_TR_NOOP("Calculates mass properties of selected objects");
    sWhatsThis = "Std_MassProperties";
    sStatusTip = sToolTipText;
    sPixmap = "MassPropertiesIcon";
}

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:104-110
void StdCmdMassProperties::activated(int /*iMsg*/)
{
    // TODO: wire to TaskMassProperties when backend is available
}

// Ported from: FreeCAD src/Mod/Measure/Gui/Command.cpp:113-120
bool StdCmdMassProperties::isActive()
{
    return true;  // TODO: wire to active document + control check
}

// =====================================================================
// Dependency graph commands (from FreeCAD CommandDoc.cpp)
// =====================================================================

//===========================================================================
// Std_DependencyGraph
//===========================================================================

DEF_STD_CMD_A(StdCmdDependencyGraph)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:658-670
StdCmdDependencyGraph::StdCmdDependencyGraph()
    : Command("Std_DependencyGraph")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Dependency Gra&ph");
    sToolTipText = QT_TR_NOOP("Shows the dependency graph of the objects in the active document");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_DependencyGraph";
    eType = 0;
    sPixmap = "Std_DependencyGraph";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:672-679
void StdCmdDependencyGraph::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (GraphvizView dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:681-684
bool StdCmdDependencyGraph::isActive()
{
    return false;  // deferred — Step 3+ (GraphvizView dialog backend)
}

//===========================================================================
// Std_ExportDependencyGraph
//===========================================================================

DEF_STD_CMD_A(StdCmdExportDependencyGraph)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:692-702
StdCmdExportDependencyGraph::StdCmdExportDependencyGraph()
    : Command("Std_ExportDependencyGraph")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Export Dependency &Graph...");
    sToolTipText = QT_TR_NOOP("Exports the dependency graph as a Graphviz (.gv) file");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ExportDependencyGraph";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:704-724
void StdCmdExportDependencyGraph::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (Graphviz export dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:726-729
bool StdCmdExportDependencyGraph::isActive()
{
    return false;  // deferred — Step 3+ (Graphviz export dialog backend)
}

//===========================================================================
// Std_ProjectUtil
//===========================================================================

DEF_STD_CMD_A(StdCmdProjectUtil)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:969-980
StdCmdProjectUtil::StdCmdProjectUtil()
    : Command("Std_ProjectUtil")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Do&cument Utility");
    sToolTipText = QT_TR_NOOP("Extracts or creates document files");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_ProjectUtil";
    sPixmap = "Std_ProjectUtil";
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:982-987
void StdCmdProjectUtil::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (project utility dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:989-992
bool StdCmdProjectUtil::isActive()
{
    return false;  // deferred — Step 3+ (project utility dialog backend)
}

// =====================================================================
// Registration helper — called by CreateStdCommands
// =====================================================================

// Ported from: Authored — combines tool commands from FreeCAD CommandDoc.cpp,
//              CommandStd.cpp, and Mod/Measure/Gui/Command.cpp
void createToolsCommands(CommandManager& mgr)
{
    // Module-level stubs
    mgr.addCommand(new StdCmdAddonMgr());
    mgr.addCommand(new StdCmdMeasure());
    mgr.addCommand(new StdCmdMassProperties());

    // Dependency graph (from CommandDoc.cpp)
    mgr.addCommand(new StdCmdDependencyGraph());
    mgr.addCommand(new StdCmdExportDependencyGraph());

    // Project utility (from CommandStd.cpp)
    mgr.addCommand(new StdCmdProjectUtil());
}

} // namespace Gui
