// Ported from: FreeCAD src/Gui/CommandStd.cpp
//              Std-domain remainder. M-L(2) 清理（分析报告 §3.1/§3.5）：Help 域
//              12 个 FreeCAD 生态链接命令（About/WhatsThis/RestartInSafeMode/
//              PythonHelp/OnlineHelp×2/FreeCADWebsite/Donation/UserHub/Forum/
//              ReportBug/DevHandbook）与 Tools 域 7 个存根（DlgParameter/
//              DlgPreferences/DlgCustomize/CommandLine/UnitsCalculator/
//              TextDocument/AnnotationLabel）+ View 域 ReloadStyleSheet +
//              Macro 域 RecentMacros 全删——激活体均为 TODO 存根（无 DTA 对应
//              物）。仅 Std_AboutQt 真功能保留（qApp->aboutQt），归 Tools 域
//              （原 Help 菜单整体删除后 Tools 是其唯一可达域）。
#include "CommandStd.h"
#include "Command.h"

#include <QApplication>

namespace Gui {

//===========================================================================
// Std_AboutQt
//===========================================================================
DEF_STD_CMD_A(StdCmdAboutQt)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:295-304
StdCmdAboutQt::StdCmdAboutQt()
    : Command("Std_AboutQt")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("About &Qt");
    sToolTipText = QT_TR_NOOP("Displays information about Qt");
    sWhatsThis = "Std_AboutQt";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:306-310
void StdCmdAboutQt::activated(int /*iMsg*/)
{
    qApp->aboutQt();
}

bool StdCmdAboutQt::isActive()
{
    return true;
}

// =====================================================================
// Registration helper — called by CreateStdCommands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
void createStdDomainCommands(CommandManager& mgr)
{
    // Tools（原 Help 域唯一真功能项——M-L(2) 归域）
    mgr.addCommand(new StdCmdAboutQt());
}

} // namespace Gui
