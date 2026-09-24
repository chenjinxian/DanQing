// Ported from: FreeCAD src/Gui/CommandStd.cpp
//              ALL Std-domain commands (Help + Tools + View + Macro items)
//              not already in CommandDoc/CommandView. In FreeCAD, these are all
//              defined in CommandStd.cpp and registered via CreateStdCommands().
//              Each command ctor metadata is ported verbatim from FreeCAD source;
//              activated() bodies are stubs until backend is wired.
#include "CommandStd.h"
#include "Command.h"
#include "Action.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QKeySequence>
#include <QUrl>

namespace Gui {

// =====================================================================
// Help domain (from FreeCAD CommandStd.cpp + OnlineDocumentation.cpp)
// =====================================================================

//===========================================================================
// Std_About
//===========================================================================

// Ported from: FreeCAD src/Gui/CommandStd.cpp:229-288
class StdCmdAbout : public Gui::Command {
public:
    StdCmdAbout();
    ~StdCmdAbout() override = default;
    const char* className() const override { return "StdCmdAbout"; }

protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Gui::Action* createAction() override;
};

// Ported from: FreeCAD src/Gui/CommandStd.cpp:231-240
StdCmdAbout::StdCmdAbout()
    : Command("Std_About")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("&About %1");
    sToolTipText = QT_TR_NOOP("Displays information about %1");
    sWhatsThis = "Std_About";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:242-257
Gui::Action* StdCmdAbout::createAction()
{
    auto* pcAction = new Gui::Action(this, getMainWindow());
    QString exe = qApp->applicationName();
    pcAction->setText(QCoreApplication::translate(this->className(), getMenuText()).arg(exe));
    pcAction->setToolTip(QCoreApplication::translate(this->className(), getToolTipText()).arg(exe));
    pcAction->setStatusTip(QCoreApplication::translate(this->className(), getStatusTip()).arg(exe));
    pcAction->setWhatsThis(QLatin1String(getWhatsThis()));
    pcAction->setIcon(QApplication::windowIcon());
    pcAction->setShortcut(QString::fromLatin1(getAccel()));
    pcAction->setMenuRole(QAction::AboutRole);
    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:259-262
bool StdCmdAbout::isActive()
{
    return true;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:267-273
void StdCmdAbout::activated(int /*iMsg*/)
{
    // TODO: wire to AboutDialog when backend is available
}

//===========================================================================
// Std_AboutQt
//===========================================================================
DEF_STD_CMD_A(StdCmdAboutQt)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:295-304
StdCmdAboutQt::StdCmdAboutQt()
    : Command("Std_AboutQt")
{
    sGroup = "Help";
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

//===========================================================================
// Std_WhatsThis
//===========================================================================
DEF_STD_CMD_A(StdCmdWhatsThis)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:317-328
StdCmdWhatsThis::StdCmdWhatsThis()
    : Command("Std_WhatsThis")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("&What's This?");
    sToolTipText = QT_TR_NOOP("Opens the documentation for the selected command");
    sWhatsThis = "Std_WhatsThis";
    sStatusTip = sToolTipText;
    sAccel = keySequenceToAccel(QKeySequence::WhatsThis);
    sPixmap = "WhatsThis";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:330-334
void StdCmdWhatsThis::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (What's This help backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdWhatsThis::isActive()
{
    return false;  // deferred — Step 3+ (What's This help backend)
}

//===========================================================================
// Std_RestartInSafeMode
//===========================================================================
DEF_STD_CMD_A(StdCmdRestartInSafeMode)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:341-351
StdCmdRestartInSafeMode::StdCmdRestartInSafeMode()
    : Command("Std_RestartInSafeMode")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("Restart in Safe Mode");
    sToolTipText = QT_TR_NOOP("Starts FreeCAD without any modules or plugins loaded");
    sWhatsThis = "Std_RestartInSafeMode";
    sStatusTip = sToolTipText;
    sPixmap = "safe-mode-restart";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:353-382
void StdCmdRestartInSafeMode::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (restart-in-safe-mode backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdRestartInSafeMode::isActive()
{
    return false;  // deferred — Step 3+ (restart-in-safe-mode backend)
}

//===========================================================================
// Std_PythonHelp
//===========================================================================

// Ported from: FreeCAD src/Gui/OnlineDocumentation.cpp:320-332
class StdCmdPythonHelp : public Gui::Command {
public:
    StdCmdPythonHelp();
    ~StdCmdPythonHelp() override = default;
    const char* className() const override { return "StdCmdPythonHelp"; }

protected:
    void activated(int iMsg) override;
};

// Ported from: FreeCAD src/Gui/OnlineDocumentation.cpp:320-332
StdCmdPythonHelp::StdCmdPythonHelp()
    : Command("Std_PythonHelp")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("&Python Documentation");
    sToolTipText = QT_TR_NOOP("Opens the Python documentation");
    sWhatsThis = "Std_PythonHelp";
    sStatusTip = sToolTipText;
    sPixmap = "applications-python";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/OnlineDocumentation.cpp:340-345
void StdCmdPythonHelp::activated(int /*iMsg*/)
{
    // TODO: wire to Python documentation viewer
}

//===========================================================================
// Std_OnlineHelp
//===========================================================================
DEF_STD_CMD_A(StdCmdOnlineHelp)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:539-550
StdCmdOnlineHelp::StdCmdOnlineHelp()
    : Command("Std_OnlineHelp")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("&Help");
    sToolTipText = QT_TR_NOOP("Opens the Help documentation");
    sWhatsThis = "Std_OnlineHelp";
    sStatusTip = sToolTipText;
    sPixmap = "help-browser";
    sAccel = keySequenceToAccel(QKeySequence::HelpContents);
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:552-556
void StdCmdOnlineHelp::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (documentation viewer backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdOnlineHelp::isActive()
{
    return false;  // deferred — Step 3+ (documentation viewer backend)
}

//===========================================================================
// Std_OnlineHelpWebsite
//===========================================================================
DEF_STD_CMD_A(StdCmdOnlineHelpWebsite)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:564-573
StdCmdOnlineHelpWebsite::StdCmdOnlineHelpWebsite()
    : Command("Std_OnlineHelpWebsite")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("Help Website");
    sToolTipText = QT_TR_NOOP("Opens the help documentation");
    sWhatsThis = "Std_OnlineHelpWebsite";
    sStatusTip = sToolTipText;
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:575-587
void StdCmdOnlineHelpWebsite::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(help URL) —
// suppressed alongside the other Help items so the menu greys out uniformly until the Help
// backend (URL dispatch + offline fallback) lands in Step 3.
bool StdCmdOnlineHelpWebsite::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_FreeCADWebsite
//===========================================================================
DEF_STD_CMD_A(StdCmdFreeCADWebsite)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:659-669
StdCmdFreeCADWebsite::StdCmdFreeCADWebsite()
    : Command("Std_FreeCADWebsite")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("FreeCAD W&ebsite");
    sToolTipText = QT_TR_NOOP("Navigates to the official FreeCAD website");
    sWhatsThis = "Std_FreeCADWebsite";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:671-682
void StdCmdFreeCADWebsite::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdFreeCADWebsite::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_FreeCADDonation
//===========================================================================
DEF_STD_CMD_A(StdCmdFreeCADDonation)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:595-605
StdCmdFreeCADDonation::StdCmdFreeCADDonation()
    : Command("Std_FreeCADDonation")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("Donate to FreeCA&D");
    sToolTipText = QT_TR_NOOP("Opens the FreeCAD donation page");
    sWhatsThis = "Std_FreeCADDonation";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:607-616
void StdCmdFreeCADDonation::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdFreeCADDonation::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_FreeCADUserHub
//===========================================================================
DEF_STD_CMD_A(StdCmdFreeCADUserHub)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:690-700
StdCmdFreeCADUserHub::StdCmdFreeCADUserHub()
    : Command("Std_FreeCADUserHub")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("&User Documentation");
    sToolTipText = QT_TR_NOOP("Opens the documentation for users");
    sWhatsThis = "Std_FreeCADUserHub";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:702-714
void StdCmdFreeCADUserHub::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdFreeCADUserHub::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_FreeCADForum
//===========================================================================
DEF_STD_CMD_A(StdCmdFreeCADForum)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:722-732
StdCmdFreeCADForum::StdCmdFreeCADForum()
    : Command("Std_FreeCADForum")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("FreeCAD &Forum");
    sToolTipText = QT_TR_NOOP("Opens the FreeCAD forum to find help from other users");
    sWhatsThis = "Std_FreeCADForum";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:734-745
void StdCmdFreeCADForum::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdFreeCADForum::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_ReportBug
//===========================================================================
DEF_STD_CMD_A(StdCmdReportBug)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:753-763
StdCmdReportBug::StdCmdReportBug()
    : Command("Std_ReportBug")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("Report an &Issue");
    sToolTipText = QT_TR_NOOP("Opens the bugtracker to report an issue");
    sWhatsThis = "Std_ReportBug";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:765-774
void StdCmdReportBug::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdReportBug::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

//===========================================================================
// Std_DevHandbook
//===========================================================================
DEF_STD_CMD_A(StdCmdDevHandbook)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:625-638
StdCmdDevHandbook::StdCmdDevHandbook()
    : Command("Std_DevHandbook")
{
    sGroup = "Help";
    sMenuText = QT_TR_NOOP("Developers Handbook");
    sToolTipText = QT_TR_NOOP("Opens the FreeCAD developers handbook");
    sWhatsThis = "Std_DevHandbook";
    sStatusTip = sToolTipText;
    sPixmap = "internet-web-browser";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:640-651
void StdCmdDevHandbook::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (external-link Help backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Deferred: Step 3+ (external-link Help backend). Was QDesktopServices::openUrl(URL).
bool StdCmdDevHandbook::isActive()
{
    return false;  // deferred — Step 3+ (external-link Help backend)
}

// =====================================================================
// Tools domain (from FreeCAD CommandStd.cpp)
// =====================================================================

//===========================================================================
// Std_DlgParameter
//===========================================================================
DEF_STD_CMD_A(StdCmdDlgParameter)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:389-399
StdCmdDlgParameter::StdCmdDlgParameter()
    : Command("Std_DlgParameter")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("E&dit Parameters");
    sToolTipText = QT_TR_NOOP("Opens a dialog to edit the parameters");
    sWhatsThis = "Std_DlgParameter";
    sStatusTip = sToolTipText;
    sPixmap = "Std_DlgParameter";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:402-408
void StdCmdDlgParameter::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (parameter editor dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdDlgParameter::isActive()
{
    return false;  // deferred — Step 3+ (parameter editor dialog backend)
}

//===========================================================================
// Std_DlgPreferences
//===========================================================================
DEF_STD_CMD_C(StdCmdDlgPreferences)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:415-427
StdCmdDlgPreferences::StdCmdDlgPreferences()
    : Command("Std_DlgPreferences")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Prefere&nces");
    sToolTipText = QT_TR_NOOP("Opens a dialog to edit the preferences");
    sWhatsThis = "Std_DlgPreferences";
    sStatusTip = sToolTipText;
    sPixmap = "preferences-system";
    eType = 0;
    sAccel = "Ctrl+,";
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:429-435
Gui::Action* StdCmdDlgPreferences::createAction()
{
    Gui::Action* pcAction = Command::createAction();
    pcAction->setMenuRole(QAction::PreferencesRole);
    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:437-455
void StdCmdDlgPreferences::activated(int /*iMsg*/)
{
    // TODO: wire to DlgPreferencesImp
}

//===========================================================================
// Std_DlgCustomize
//===========================================================================
DEF_STD_CMD_A(StdCmdDlgCustomize)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:462-472
StdCmdDlgCustomize::StdCmdDlgCustomize()
    : Command("Std_DlgCustomize")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Cu&stomize");
    sToolTipText = QT_TR_NOOP("Opens a dialog to edit toolbars, shortcuts, and macros");
    sWhatsThis = "Std_DlgCustomize";
    sStatusTip = sToolTipText;
    sPixmap = "applications-accessories";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:474-483
void StdCmdDlgCustomize::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (customize dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdDlgCustomize::isActive()
{
    return false;  // deferred — Step 3+ (customize dialog backend)
}

//===========================================================================
// Std_CommandLine
//===========================================================================
DEF_STD_CMD_A(StdCmdCommandLine)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:490-501
StdCmdCommandLine::StdCmdCommandLine()
    : Command("Std_CommandLine")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Command &Line");
    sToolTipText = QT_TR_NOOP("Opens a command line interface in the console");
    sWhatsThis = "Std_CommandLine";
    sStatusTip = sToolTipText;
    sPixmap = "utilities-terminal";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:503-531
void StdCmdCommandLine::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (command-line console backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdCommandLine::isActive()
{
    return false;  // deferred — Step 3+ (command-line console backend)
}

//===========================================================================
// Std_UnitsCalculator
//===========================================================================
DEF_STD_CMD_A(StdCmdUnitsCalculator)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:820-833
StdCmdUnitsCalculator::StdCmdUnitsCalculator()
    : Command("Std_UnitsCalculator")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("&Units Converter");
    sToolTipText = QT_TR_NOOP("Converts between different unit systems");
    sWhatsThis = "Std_UnitsCalculator";
    sStatusTip = sToolTipText;
    sPixmap = "Std_UnitsCalculator";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp (activated implicit)
void StdCmdUnitsCalculator::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (units calculator dialog backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdUnitsCalculator::isActive()
{
    return false;  // deferred — Step 3+ (units calculator dialog backend)
}

//===========================================================================
// Std_TextDocument
//===========================================================================
DEF_STD_CMD_A(StdCmdTextDocument)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:782-792
StdCmdTextDocument::StdCmdTextDocument()
    : Command("Std_TextDocument")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Te&xt Document");
    sToolTipText = QT_TR_NOOP("Adds a text document to the active document");
    sWhatsThis = "Std_TextDocument";
    sStatusTip = sToolTipText;
    sPixmap = "TextDocument";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:794-808
void StdCmdTextDocument::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (App::TextDocument creation backend); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:810-813
bool StdCmdTextDocument::isActive()
{
    return false;  // deferred — Step 3+ (App::TextDocument creation backend)
}

//===========================================================================
// Std_AnnotationLabel
//===========================================================================
DEF_STD_CMD_A(StdCmdAnnotationLabel)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:979-990
StdCmdAnnotationLabel::StdCmdAnnotationLabel()
    : Command("Std_AnnotationLabel")
{
    sGroup = "Tools";
    sMenuText = QT_TR_NOOP("Annotation Label");
    sToolTipText = QT_TR_NOOP("Creates a new annotation label at the picked location in the 3D view");
    sWhatsThis = "Std_AnnotationLabel";
    sStatusTip = sToolTipText;
    sPixmap = "Tree_Annotation";
    eType = AlterDoc;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:992-998
bool StdCmdAnnotationLabel::isActive()
{
    return false;  // deferred — Step 3+ (annotation label creation backend)
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:1000+
void StdCmdAnnotationLabel::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (annotation label creation backend); see CLAUDE.md §0. Backend not yet implemented.
}

// =====================================================================
// View domain (from FreeCAD CommandStd.cpp)
// =====================================================================

//===========================================================================
// Std_ReloadStyleSheet
//===========================================================================
DEF_STD_CMD_A(StdCmdReloadStyleSheet)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:957-967
StdCmdReloadStyleSheet::StdCmdReloadStyleSheet()
    : Command("Std_ReloadStyleSheet")
{
    sGroup = "View";
    sMenuText = QT_TR_NOOP("&Reload Stylesheet");
    sToolTipText = QT_TR_NOOP("Reloads the current stylesheet");
    sWhatsThis = "Std_ReloadStyleSheet";
    sStatusTip = sToolTipText;
    sPixmap = "view-refresh";
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:969-972
void StdCmdReloadStyleSheet::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 3+ (stylesheet reload backend); see CLAUDE.md §0. Backend not yet implemented.
}

bool StdCmdReloadStyleSheet::isActive()
{
    return false;  // deferred — Step 3+ (stylesheet reload backend)
}

// =====================================================================
// Macro domain (from FreeCAD CommandStd.cpp)
// =====================================================================

//===========================================================================
// Std_RecentMacros
//===========================================================================
DEF_STD_CMD_C(StdCmdRecentMacros)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:187-197
StdCmdRecentMacros::StdCmdRecentMacros()
    : Command("Std_RecentMacros")
{
    sGroup = "Macro";
    sMenuText = QT_TR_NOOP("&Recent Macros");
    sToolTipText = QT_TR_NOOP("Displays the list of recently used macros");
    sWhatsThis = "Std_RecentMacros";
    sStatusTip = sToolTipText;
    sPixmap = "Std_RecentMacros";
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:204-210
void StdCmdRecentMacros::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    // TODO: wire to RecentMacrosAction
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:215-223
Action* StdCmdRecentMacros::createAction()
{
    // C++ adaptation: simplified — no RecentMacrosAction in DisplayTestApp yet
    auto* pcAction = Command::createAction();
    return pcAction;
}

// =====================================================================
// Registration helper — called by CreateStdCommands
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandStd.cpp:1052-1084 (CreateStdCommands)
// Registers ALL Std-domain commands (Help + Tools + View + Macro items)
// not already in CommandDoc/CommandView.
// Std_Workbench → CommandView, Std_RecentFiles → CommandDoc, Std_UserEditMode → CommandDoc.
void createStdDomainCommands(CommandManager& mgr)
{
    // Help
    mgr.addCommand(new StdCmdAbout());
    mgr.addCommand(new StdCmdAboutQt());
    mgr.addCommand(new StdCmdWhatsThis());
    mgr.addCommand(new StdCmdRestartInSafeMode());
    mgr.addCommand(new StdCmdPythonHelp());
    mgr.addCommand(new StdCmdOnlineHelp());
    mgr.addCommand(new StdCmdOnlineHelpWebsite());
    mgr.addCommand(new StdCmdFreeCADWebsite());
    mgr.addCommand(new StdCmdFreeCADDonation());
    mgr.addCommand(new StdCmdFreeCADUserHub());
    mgr.addCommand(new StdCmdFreeCADForum());
    mgr.addCommand(new StdCmdReportBug());
    mgr.addCommand(new StdCmdDevHandbook());

    // Tools
    mgr.addCommand(new StdCmdDlgParameter());
    mgr.addCommand(new StdCmdDlgPreferences());
    mgr.addCommand(new StdCmdDlgCustomize());
    mgr.addCommand(new StdCmdCommandLine());
    mgr.addCommand(new StdCmdUnitsCalculator());
    mgr.addCommand(new StdCmdTextDocument());
    mgr.addCommand(new StdCmdAnnotationLabel());

    // View
    mgr.addCommand(new StdCmdReloadStyleSheet());

    // Macro (from CommandStd.cpp)
    mgr.addCommand(new StdCmdRecentMacros());
}

} // namespace Gui
