// Ported from: FreeCAD src/Gui/CommandDoc.cpp + CommandStd.cpp + CommandWindow.cpp
//              + CommandView.cpp + CommandFeat.cpp
// File + Edit domain commands. Each command ctor metadata is ported verbatim from
// FreeCAD source; activated()/isActive() bodies are stubs until backend is wired.
#include "Command.h"
#include "Action.h"
#include "CommandDoc.h"
#include "MainWindow.h"
#include "Application.h"

#include <QApplication>
#include <QActionGroup>
#include <QFileDialog>

// View3DInventor (real dqApp viewport) is only compiled/linked into the
// DisplayTestApp application target — DisplayTestAppTest compiles this file
// without the dqBase/dqApp include paths and without View3DInventor.cpp (the
// command layer's established pattern, see the injectable-factory note on
// StdCmdNew below). Detect the application target by probing for the dqBase
// header View3DInventor.h requires, and only reference View3DInventor when it
// is available.
#if defined(__has_include)
#  if __has_include(<dqBase/RefCounted.h>)
#    include "View3DInventor.h"
#    define DTA_HAS_VIEW3DINVENTOR 1
#  endif
#endif

namespace Gui {

// =====================================================================
// File domain
// =====================================================================

//===========================================================================
// Std_New
//===========================================================================

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:737-748
StdCmdNew::StdCmdNew()
    : Command("Std_New")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&New Document");
    sToolTipText = QT_TR_NOOP("Creates a new empty document");
    sWhatsThis = "Std_New";
    sStatusTip = sToolTipText;
    sPixmap = "document-new";
    sAccel = keySequenceToAccel(QKeySequence::New);
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:750-764 → Application::newDocument.
// DTA: a "new document" is a blank 3D connection, created through the facade's injectable
// factory (main.cpp registers the real BlankConnection View3DInventor creator) so this
// command never references real-dqApp View3DInventor directly.
void StdCmdNew::activated(int /*iMsg*/)
{
    if (auto* app = getGuiApplication()) {
        app->newDocument();
        if (auto* mw = app->getMainWindow())
            mw->showStatus(0, QObject::tr("Blank Connection opened"));
    }
}

//===========================================================================
// Std_Open
//===========================================================================
DEF_STD_CMD_A(StdCmdOpen)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:83-95
StdCmdOpen::StdCmdOpen()
    : Command("Std_Open")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Open...");
    sToolTipText = QT_TR_NOOP("Opens a document or imports files");
    sWhatsThis = "Std_Open";
    sStatusTip = sToolTipText;
    sPixmap = "document-open";
    sAccel = keySequenceToAccel(QKeySequence::Open);
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:97-200
void StdCmdOpen::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp (isActive implicit)
bool StdCmdOpen::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_RecentFiles
//===========================================================================
DEF_STD_CMD_C(StdCmdRecentFiles)

// Ported from: FreeCAD src/Gui/CommandStd.cpp:143-153
StdCmdRecentFiles::StdCmdRecentFiles()
    : Command("Std_RecentFiles")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Open &Recent");
    sToolTipText = QT_TR_NOOP("Displays the list of recently opened files");
    sWhatsThis = "Std_RecentFiles";
    sStatusTip = sToolTipText;
    sPixmap = "Std_RecentFiles";
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:160-166
void StdCmdRecentFiles::activated(int iMsg)
{
    auto act = qobject_cast<RecentFilesAction*>(m_pcAction);
    if (act) {
        act->activateFile(iMsg);
    }
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:171-179
Action* StdCmdRecentFiles::createAction()
{
    auto pcAction = new RecentFilesAction(this, getMainWindow());
    pcAction->setObjectName(QLatin1String("recentFiles"));
    pcAction->setDropDownMenu(true);
    pcAction->setRememberLast(false);
    applyCommandData(this->className(), pcAction);
    return pcAction;
}

//===========================================================================
// Std_CloseActiveWindow
//===========================================================================
DEF_STD_CMD_A(StdCmdCloseActiveWindow)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:104-118
StdCmdCloseActiveWindow::StdCmdCloseActiveWindow()
    : Command("Std_CloseActiveWindow")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("&Close");
    sToolTipText = QT_TR_NOOP("Closes the active window");
    sWhatsThis = "Std_CloseActiveWindow";
    sStatusTip = sToolTipText;
    sAccel = keySequenceToAccel(QKeySequence::Close);
    sPixmap = "Std_CloseActiveWindow";
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:120-124
void StdCmdCloseActiveWindow::activated(int /*iMsg*/)
{
    // TODO: wire to getMainWindow()->closeActiveWindow() when MainWindow provides it
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:126-129
bool StdCmdCloseActiveWindow::isActive()
{
    return true;  // TODO: wire to !getMainWindow()->windows().isEmpty()
}

//===========================================================================
// Std_CloseAllWindows
//===========================================================================
DEF_STD_CMD_A(StdCmdCloseAllWindows)

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:136-146
StdCmdCloseAllWindows::StdCmdCloseAllWindows()
    : Command("Std_CloseAllWindows")
{
    sGroup = "Window";
    sMenuText = QT_TR_NOOP("Close A&ll");
    sToolTipText = QT_TR_NOOP("Closes all windows");
    sWhatsThis = "Std_CloseAllWindows";
    sStatusTip = sToolTipText;
    sPixmap = "Std_CloseAllWindows";
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:148-152
void StdCmdCloseAllWindows::activated(int /*iMsg*/)
{
    // TODO: wire to getMainWindow()->closeAllDocuments() when MainWindow provides it
}

// Ported from: FreeCAD src/Gui/CommandWindow.cpp:154-157
bool StdCmdCloseAllWindows::isActive()
{
    return true;  // TODO: wire to !getMainWindow()->windows().isEmpty()
}

//===========================================================================
// Std_Save
//===========================================================================
DEF_STD_CMD_A(StdCmdSave)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:771-782
StdCmdSave::StdCmdSave()
    : Command("Std_Save")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Save");
    sToolTipText = QT_TR_NOOP("Saves the active document");
    sWhatsThis = "Std_Save";
    sStatusTip = sToolTipText;
    sPixmap = "document-save";
    sAccel = keySequenceToAccel(QKeySequence::Save);
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:784-797
void StdCmdSave::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:799-802
bool StdCmdSave::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_SaveAs
//===========================================================================
DEF_STD_CMD_A(StdCmdSaveAs)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:809-820
StdCmdSaveAs::StdCmdSaveAs()
    : Command("Std_SaveAs")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Save &As...");
    sToolTipText = QT_TR_NOOP("Saves the active document under a new file name");
    sWhatsThis = "Std_SaveAs";
    sStatusTip = sToolTipText;
    sPixmap = "document-save-as";
    sAccel = keySequenceToAccel(QKeySequence::SaveAs);
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:822-826
void StdCmdSaveAs::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:828-831
bool StdCmdSaveAs::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_SaveCopy
//===========================================================================
DEF_STD_CMD_A(StdCmdSaveCopy)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:839-850
StdCmdSaveCopy::StdCmdSaveCopy()
    : Command("Std_SaveCopy")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Save a Cop&y...");
    sToolTipText = QT_TR_NOOP("Saves a copy of the active document under a new file name");
    sWhatsThis = "Std_SaveCopy";
    sStatusTip = sToolTipText;
    sPixmap = "Std_SaveCopy";
    sAccel = "Ctrl+Alt+Shift+S";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:853-857
void StdCmdSaveCopy::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:859-862
bool StdCmdSaveCopy::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_SaveAll
//===========================================================================
DEF_STD_CMD_A(StdCmdSaveAll)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:869-878
StdCmdSaveAll::StdCmdSaveAll()
    : Command("Std_SaveAll")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Sa&ve All");
    sToolTipText = QT_TR_NOOP("Saves all open documents");
    sWhatsThis = "Std_SaveAll";
    sStatusTip = sToolTipText;
    sPixmap = "Std_SaveAll";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:880-884
void StdCmdSaveAll::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:886-889
bool StdCmdSaveAll::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Revert
//===========================================================================
DEF_STD_CMD_A(StdCmdRevert)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:897-907
StdCmdRevert::StdCmdRevert()
    : Command("Std_Revert")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Rever&t");
    sToolTipText = QT_TR_NOOP("Reverts to the saved version of this file");
    sWhatsThis = "Std_Revert";
    sStatusTip = sToolTipText;
    sPixmap = "Std_Revert";
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:909-925
void StdCmdRevert::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:927-930
bool StdCmdRevert::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Import
//===========================================================================
DEF_STD_CMD_A(StdCmdImport)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:210-221
StdCmdImport::StdCmdImport()
    : Command("Std_Import")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Import...");
    sToolTipText = QT_TR_NOOP("Imports a file into the active document");
    sWhatsThis = "Std_Import";
    sStatusTip = sToolTipText;
    sPixmap = "Std_Import";
    sAccel = "Ctrl+Shift+I";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:223-296 (StdCmdImport::activated)
//               + itwinjs-core GltfDecoration.ts:157 queryAsset (file picker).
void StdCmdImport::activated(int /*iMsg*/)
{
    auto* mw = Gui::getMainWindow();
    if (!mw)
        return;

    // File dialog — glTF/GLB only (Step 4 scope).
    const QString filter = QObject::tr("glTF models (*.gltf *.glb);;All files (*)");
    QString path = QFileDialog::getOpenFileName(mw, QObject::tr("Import glTF"),
                                                QString(), filter);
    if (path.isEmpty())
        return;  // user cancelled

#ifdef DTA_HAS_VIEW3DINVENTOR
    // Ensure a 3D view is active; create one if the active MDI view is not a View3DInventor.
    auto* app = Gui::Application::Instance();
    Gui::MDIView* active = app->activeView();
    auto* view3d = qobject_cast<Gui::View3DInventor*>(active);
    if (!view3d) {
        view3d = qobject_cast<Gui::View3DInventor*>(app->newDocument());
        if (!view3d)
            return;
    }

    if (!view3d->loadGltf(path))
        mw->showStatus(0, QObject::tr("Could not import %1").arg(path));
#else
    // Test-target build: View3DInventor (real dqApp) is not compiled in, so the
    // import cannot reach a viewport — stop after the file dialog.
#endif
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:298-301 — active whenever the app is running.
bool StdCmdImport::isActive()
{
    return true;
}

//===========================================================================
// Std_Export
//===========================================================================
DEF_STD_CMD_A(StdCmdExport)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:310-322
StdCmdExport::StdCmdExport()
    : Command("Std_Export")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Export...");
    sToolTipText = QT_TR_NOOP("Exports an object in the active document");
    sWhatsThis = "Std_Export";
    sStatusTip = sToolTipText;
    sAccel = "Ctrl+E";
    sPixmap = "Std_Export";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:324+
void StdCmdExport::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp (isActive implicit)
bool StdCmdExport::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_MergeProjects
//===========================================================================
DEF_STD_CMD_A(StdCmdMergeProjects)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:598-610
StdCmdMergeProjects::StdCmdMergeProjects()
    : Command("Std_MergeProjects")
{
    sAppModule = "File";
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Merge Document");
    sToolTipText = QT_TR_NOOP("Merges another FreeCAD document into the active one");
    sWhatsThis = "Std_MergeProjects";
    sStatusTip = sToolTipText;
    sPixmap = "Std_MergeProjects";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:612-645
void StdCmdMergeProjects::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:647-650
bool StdCmdMergeProjects::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_ProjectInfo
//===========================================================================
DEF_STD_CMD_A(StdCmdProjectInfo)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:938-949
StdCmdProjectInfo::StdCmdProjectInfo()
    : Command("Std_ProjectInfo")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Doc&ument Information");
    sToolTipText = QT_TR_NOOP("Shows information about the active document");
    sWhatsThis = "Std_ProjectInfo";
    sStatusTip = sToolTipText;
    sPixmap = "document-properties";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:951-956
void StdCmdProjectInfo::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:958-961
bool StdCmdProjectInfo::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Print
//===========================================================================
DEF_STD_CMD_A(StdCmdPrint)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:999-1011
StdCmdPrint::StdCmdPrint()
    : Command("Std_Print")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("&Print");
    sToolTipText = QT_TR_NOOP("Prints the active document");
    sWhatsThis = "Std_Print";
    sStatusTip = sToolTipText;
    sPixmap = "document-print";
    sAccel = keySequenceToAccel(QKeySequence::Print);
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1013-1020
void StdCmdPrint::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1022-1025
bool StdCmdPrint::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_PrintPreview
//===========================================================================
DEF_STD_CMD_A(StdCmdPrintPreview)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1032-1043
StdCmdPrintPreview::StdCmdPrintPreview()
    : Command("Std_PrintPreview")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Print Previe&w");
    sToolTipText = QT_TR_NOOP("Previews the active document before printing");
    sWhatsThis = "Std_PrintPreview";
    sStatusTip = sToolTipText;
    sPixmap = "document-print-preview";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1045-1050
void StdCmdPrintPreview::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1053-1056
bool StdCmdPrintPreview::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_PrintPdf
//===========================================================================
DEF_STD_CMD_A(StdCmdPrintPdf)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1063-1074
StdCmdPrintPdf::StdCmdPrintPdf()
    : Command("Std_PrintPdf")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("Export P&DF");
    sToolTipText = QT_TR_NOOP("Exports the active document as a PDF file");
    sWhatsThis = "Std_PrintPdf";
    sStatusTip = sToolTipText;
    sPixmap = "Std_PrintPdf";
    eType = 0;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1076-1083
void StdCmdPrintPdf::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1085-1088
bool StdCmdPrintPdf::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Quit
//===========================================================================
DEF_STD_CMD_C(StdCmdQuit)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1096-1107
StdCmdQuit::StdCmdQuit()
    : Command("Std_Quit")
{
    sGroup = "File";
    sMenuText = QT_TR_NOOP("E&xit");
    sToolTipText = QT_TR_NOOP("Quits the application");
    sWhatsThis = "Std_Quit";
    sStatusTip = sToolTipText;
    sPixmap = "application-exit";
    sAccel = keySequenceToAccel(QKeySequence::Quit);
    eType = NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1109-1115
Action* StdCmdQuit::createAction()
{
    Action* pcAction = Command::createAction();
    pcAction->setMenuRole(QAction::QuitRole);
    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1117-1122
void StdCmdQuit::activated(int /*iMsg*/)
{
    if (auto* mw = getMainWindow())
        mw->close();
}

// =====================================================================
// Edit domain
// =====================================================================

//===========================================================================
// Std_Undo
//===========================================================================
DEF_STD_CMD_AC(StdCmdUndo)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1130-1141
StdCmdUndo::StdCmdUndo()
    : Command("Std_Undo")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Undo");
    sToolTipText = QT_TR_NOOP("Undoes the previous action");
    sWhatsThis = "Std_Undo";
    sStatusTip = sToolTipText;
    sPixmap = "edit-undo";
    sAccel = keySequenceToAccel(QKeySequence::Undo);
    eType = ForEdit | NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1143-1148
void StdCmdUndo::activated(int /*iMsg*/)
{
    // TODO: wire to getGuiApplication()->sendMsgToActiveView("Undo")
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1150-1153
bool StdCmdUndo::isActive()
{
    return true;  // TODO: wire to getGuiApplication()->sendHasMsgToActiveView("Undo")
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1155-1167
// C++ adaptation: UndoAction not yet ported; return plain Action
Action* StdCmdUndo::createAction()
{
    Action* pcAction = Command::createAction();
    return pcAction;
}

//===========================================================================
// Std_Redo
//===========================================================================
DEF_STD_CMD_AC(StdCmdRedo)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1175-1186
StdCmdRedo::StdCmdRedo()
    : Command("Std_Redo")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Redo");
    sToolTipText = QT_TR_NOOP("Redoes a previously undone action");
    sWhatsThis = "Std_Redo";
    sStatusTip = sToolTipText;
    sPixmap = "edit-redo";
    sAccel = keySequenceToAccel(QKeySequence::Redo);
    eType = ForEdit | NoTransaction;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1188-1193
void StdCmdRedo::activated(int /*iMsg*/)
{
    // TODO: wire to getGuiApplication()->sendMsgToActiveView("Redo")
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1195-1198
bool StdCmdRedo::isActive()
{
    return true;  // TODO: wire to getGuiApplication()->sendHasMsgToActiveView("Redo")
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1200-1212
// C++ adaptation: RedoAction not yet ported; return plain Action
Action* StdCmdRedo::createAction()
{
    Action* pcAction = Command::createAction();
    return pcAction;
}

//===========================================================================
// Std_Cut
//===========================================================================
DEF_STD_CMD_A(StdCmdCut)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1219-1229
StdCmdCut::StdCmdCut()
    : Command("Std_Cut")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Cu&t");
    sToolTipText = QT_TR_NOOP("Removes the selection and copies it to the clipboard");
    sWhatsThis = "Std_Cut";
    sStatusTip = sToolTipText;
    sPixmap = "edit-cut";
    sAccel = keySequenceToAccel(QKeySequence::Cut);
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1231-1235
void StdCmdCut::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1237-1240
bool StdCmdCut::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Copy
//===========================================================================
DEF_STD_CMD_A(StdCmdCopy)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1247-1257
StdCmdCopy::StdCmdCopy()
    : Command("Std_Copy")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Copy");
    sToolTipText = QT_TR_NOOP("Copies the selection to the clipboard");
    sWhatsThis = "Std_Copy";
    sStatusTip = sToolTipText;
    sPixmap = "edit-copy";
    sAccel = keySequenceToAccel(QKeySequence::Copy);
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1259-1268
void StdCmdCopy::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1270-1276
bool StdCmdCopy::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Paste
//===========================================================================
DEF_STD_CMD_A(StdCmdPaste)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1283-1293
StdCmdPaste::StdCmdPaste()
    : Command("Std_Paste")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Paste");
    sToolTipText = QT_TR_NOOP("Pastes the contents of the clipboard");
    sWhatsThis = "Std_Paste";
    sStatusTip = sToolTipText;
    sPixmap = "edit-paste";
    sAccel = keySequenceToAccel(QKeySequence::Paste);
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1295-1307
void StdCmdPaste::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1309-1320
bool StdCmdPaste::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_DuplicateSelection
//===========================================================================
DEF_STD_CMD_A(StdCmdDuplicateSelection)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1324-1334
StdCmdDuplicateSelection::StdCmdDuplicateSelection()
    : Command("Std_DuplicateSelection")
{
    sAppModule = "Edit";
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Duplicate Selecti&on");
    sToolTipText = QT_TR_NOOP("Duplicates the selected objects to the active document");
    sWhatsThis = "Std_DuplicateSelection";
    sStatusTip = sToolTipText;
    sPixmap = "Std_DuplicateSelection";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1336-1415
void StdCmdDuplicateSelection::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1417-1420
bool StdCmdDuplicateSelection::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_SelectAll
//===========================================================================
DEF_STD_CMD_A(StdCmdSelectAll)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1428-1441
StdCmdSelectAll::StdCmdSelectAll()
    : Command("Std_SelectAll")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Select &All");
    sToolTipText = QT_TR_NOOP("Selects all objects in the active document");
    sWhatsThis = "Std_SelectAll";
    sStatusTip = sToolTipText;
    sPixmap = "edit-select-all";
    sAccel = "Ctrl+A";  // supersedes shortcuts for text edits
    eType = AlterSelection;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1443-1470
void StdCmdSelectAll::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1472-1475
bool StdCmdSelectAll::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Delete
//===========================================================================
DEF_STD_CMD_A(StdCmdDelete)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1482-1493
StdCmdDelete::StdCmdDelete()
    : Command("Std_Delete")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Delete");
    sToolTipText = QT_TR_NOOP("Deletes the selected objects");
    sWhatsThis = "Std_Delete";
    sStatusTip = sToolTipText;
    sPixmap = "edit-delete";
    // FreeCAD uses QtTools::deleteKeySequence(); use Del as fallback
    sAccel = "Del";
    eType = ForEdit;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1495+
void StdCmdDelete::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp (isActive implicit)
bool StdCmdDelete::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Refresh  (sMenuText = "Recompute")
//===========================================================================
DEF_STD_CMD_A(StdCmdRefresh)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1702-1725
StdCmdRefresh::StdCmdRefresh()
    : Command("Std_Refresh")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Recompute");
    sToolTipText = QT_TR_NOOP("Recomputes the active document");
    sWhatsThis = "Std_Refresh";
    sStatusTip = sToolTipText;
    sPixmap = "view-refresh";
    sAccel = keySequenceToAccel(QKeySequence::Refresh);
    eType = AlterDoc | Alter3DView | AlterSelection | ForEdit;
    bCanLog = false;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1789+
void StdCmdRefresh::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp (isActive implicit)
bool StdCmdRefresh::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_BoxSelection
//===========================================================================
DEF_STD_CMD_A(StdCmdBoxSelection)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2856-2867
StdCmdBoxSelection::StdCmdBoxSelection()
    : Command("Std_BoxSelection")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("&Box Selection");
    sToolTipText = QT_TR_NOOP("Activates the box selection tool");
    sWhatsThis = "Std_BoxSelection";
    sStatusTip = sToolTipText;
    sPixmap = "edit-select-box";
    sAccel = "Shift+B";
    eType = AlterSelection;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:2881+
void StdCmdBoxSelection::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandView.cpp (isActive implicit)
bool StdCmdBoxSelection::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_BoxElementSelection
//===========================================================================
DEF_STD_CMD_A(StdCmdBoxElementSelection)

// Ported from: FreeCAD src/Gui/CommandView.cpp:2923-2934
StdCmdBoxElementSelection::StdCmdBoxElementSelection()
    : Command("Std_BoxElementSelection")
{
    sGroup = "Standard-View";
    sMenuText = QT_TR_NOOP("Bo&x Element Selection");
    sToolTipText = QT_TR_NOOP("Activates box element selection");
    sWhatsThis = "Std_BoxElementSelection";
    sStatusTip = sToolTipText;
    sPixmap = "edit-element-select-box";
    sAccel = "Shift+E";
    eType = AlterSelection;
}

// Ported from: FreeCAD src/Gui/CommandView.cpp:2936+
void StdCmdBoxElementSelection::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandView.cpp (isActive implicit)
bool StdCmdBoxElementSelection::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_SendToPythonConsole
//===========================================================================
DEF_STD_CMD_A(StdCmdSendToPythonConsole)

// Ported from: FreeCAD src/Gui/CommandFeat.cpp:294-305
StdCmdSendToPythonConsole::StdCmdSendToPythonConsole()
    : Command("Std_SendToPythonConsole")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("&Send to Python Console");
    sToolTipText = QT_TR_NOOP("Sends the selected object to the Python console");
    sWhatsThis = "Std_SendToPythonConsole";
    sStatusTip = sToolTipText;
    sPixmap = "applications-python";
    sAccel = "Ctrl+Shift+P";
}

// Ported from: FreeCAD src/Gui/CommandFeat.cpp:313+
void StdCmdSendToPythonConsole::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandFeat.cpp:307-311
bool StdCmdSendToPythonConsole::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Placement
//===========================================================================
DEF_STD_CMD_A(StdCmdPlacement)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1858-1871
StdCmdPlacement::StdCmdPlacement()
    : Command("Std_Placement")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("P&lacement");
    sToolTipText = QT_TR_NOOP(
        "Opens the placement editor to adjust the placement of the selected object"
    );
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_Placement";
    sPixmap = "Std_Placement";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1873-1901
void StdCmdPlacement::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1903-1914
bool StdCmdPlacement::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_TransformManip
//===========================================================================
DEF_STD_CMD_A(StdCmdTransformManip)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1921-1930
StdCmdTransformManip::StdCmdTransformManip()
    : Command("Std_TransformManip")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Trans&form");
    sToolTipText = QT_TR_NOOP("Transforms the selected object in the 3D view");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_TransformManip";
    sPixmap = "Std_TransformManip";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1932-1949
void StdCmdTransformManip::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1951-1962
bool StdCmdTransformManip::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Alignment
//===========================================================================
DEF_STD_CMD_A(StdCmdAlignment)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1969-1979
StdCmdAlignment::StdCmdAlignment()
    : Command("Std_Alignment")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Ali&gn To...");
    sToolTipText = QT_TR_NOOP("Aligns the selected objects");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_Alignment";
    sPixmap = "Std_Alignment";
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:1981-2027
void StdCmdAlignment::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2029-2035
bool StdCmdAlignment::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Edit
//===========================================================================
DEF_STD_CMD_A(StdCmdEdit)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2042-2053
StdCmdEdit::StdCmdEdit()
    : Command("Std_Edit")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Toggle &Edit Mode");
    sToolTipText = QT_TR_NOOP("Toggles the selected object's edit mode");
    sWhatsThis = "Std_Edit";
    sStatusTip = sToolTipText;
    sAccel = "";
    sPixmap = "edit-edit";
    eType = ForEdit;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2055-2071
void StdCmdEdit::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2073-2077
bool StdCmdEdit::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_Properties
//===========================================================================
DEF_STD_CMD_A(StdCmdProperties)

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2084-2097
StdCmdProperties::StdCmdProperties()
    : Command("Std_Properties")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Propert&ies");
    sToolTipText = QT_TR_NOOP(
        "Shows the property view, which displays the properties of the selected object."
    );
    sWhatsThis = "Std_Properties";
    sStatusTip = sToolTipText;
    sAccel = "Alt+Return";
    sPixmap = "document-properties";
    eType = Alter3DView;
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2099-2111
void StdCmdProperties::activated(int /*iMsg*/)
{
    // TODO: deferred — Step 4 (file/element I/O); see CLAUDE.md §0. Backend not yet implemented.
}

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:2113-2116
bool StdCmdProperties::isActive()
{
    return false;  // deferred — Step 4 (file/element I/O)
}

//===========================================================================
// Std_UserEditMode
//===========================================================================

// Ported from: FreeCAD src/Gui/CommandStd.cpp:843-859 (class decl)
class StdCmdUserEditMode : public Gui::Command {
public:
    StdCmdUserEditMode();
    ~StdCmdUserEditMode() override = default;
    const char* className() const override { return "StdCmdUserEditMode"; }

protected:
    void activated(int iMsg) override;
    bool isActive() override;
    Gui::Action* createAction() override;
};

// Ported from: FreeCAD src/Gui/CommandStd.cpp:861-875
StdCmdUserEditMode::StdCmdUserEditMode()
    : Command("Std_UserEditMode")
{
    sGroup = "Edit";
    sMenuText = QT_TR_NOOP("Edit &Mode");
    sToolTipText = QT_TR_NOOP("Defines behavior when editing an object from the tree view");
    sStatusTip = sToolTipText;
    sWhatsThis = "Std_UserEditMode";
    sPixmap = "Std_UserEditModeDefault";
    eType = ForEdit;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:877-910
// C++ adaptation: simplified — 4 hardcoded edit modes instead of dynamic list
Gui::Action* StdCmdUserEditMode::createAction()
{
    auto pcAction = new Gui::ActionGroup(this, Gui::getMainWindow());
    pcAction->setDropDownMenu(true);
    pcAction->setIsMode(true);
    applyCommandData(this->className(), pcAction);

    // FreeCAD uses Application::Instance->listUserEditModes(); we hardcode the 4 defaults
    const char* modeNames[] = {"Default", "Sketcher", "PartDesign", "Draft"};
    for (int i = 0; i < 4; ++i) {
        QAction* act = pcAction->addAction(QString());
        act->setCheckable(true);
        act->setObjectName(QStringLiteral("Std_UserEditMode") + QLatin1String(modeNames[i]));
        act->setWhatsThis(QString::fromLatin1(getWhatsThis()));
        act->setToolTip(QStringLiteral("Edit mode: %1").arg(QLatin1String(modeNames[i])));
        if (i == 0) {
            pcAction->setIcon(act->icon());
            act->setChecked(true);
        }
    }

    return pcAction;
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:939-945
void StdCmdUserEditMode::activated(int iMsg)
{
    // TODO: wire to App::GetApplication().GetParameterGroup + setUserEditMode
    Q_UNUSED(iMsg)
}

// Ported from: FreeCAD src/Gui/CommandStd.cpp:947-950
bool StdCmdUserEditMode::isActive()
{
    return true;
}

// =====================================================================
// Registration helper — called by Task 10 (CreateStdCommands)
// =====================================================================

void createFileEditCommands(CommandManager& mgr)
{
    // File
    mgr.addCommand(new StdCmdNew());
    mgr.addCommand(new StdCmdOpen());
    mgr.addCommand(new StdCmdRecentFiles());
    mgr.addCommand(new StdCmdCloseActiveWindow());
    mgr.addCommand(new StdCmdCloseAllWindows());
    mgr.addCommand(new StdCmdSave());
    mgr.addCommand(new StdCmdSaveAs());
    mgr.addCommand(new StdCmdSaveCopy());
    mgr.addCommand(new StdCmdSaveAll());
    mgr.addCommand(new StdCmdRevert());
    mgr.addCommand(new StdCmdImport());
    mgr.addCommand(new StdCmdExport());
    mgr.addCommand(new StdCmdMergeProjects());
    mgr.addCommand(new StdCmdProjectInfo());
    mgr.addCommand(new StdCmdPrint());
    mgr.addCommand(new StdCmdPrintPreview());
    mgr.addCommand(new StdCmdPrintPdf());
    mgr.addCommand(new StdCmdQuit());

    // Edit
    mgr.addCommand(new StdCmdUndo());
    mgr.addCommand(new StdCmdRedo());
    mgr.addCommand(new StdCmdCut());
    mgr.addCommand(new StdCmdCopy());
    mgr.addCommand(new StdCmdPaste());
    mgr.addCommand(new StdCmdDuplicateSelection());
    mgr.addCommand(new StdCmdSelectAll());
    mgr.addCommand(new StdCmdDelete());
    mgr.addCommand(new StdCmdRefresh());
    mgr.addCommand(new StdCmdBoxSelection());
    mgr.addCommand(new StdCmdBoxElementSelection());
    mgr.addCommand(new StdCmdSendToPythonConsole());
    mgr.addCommand(new StdCmdPlacement());
    mgr.addCommand(new StdCmdTransformManip());
    mgr.addCommand(new StdCmdAlignment());
    mgr.addCommand(new StdCmdEdit());
    mgr.addCommand(new StdCmdProperties());
    mgr.addCommand(new StdCmdUserEditMode());
}

} // namespace Gui
