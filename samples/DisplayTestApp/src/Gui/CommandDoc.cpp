// Ported from: FreeCAD src/Gui/CommandDoc.cpp
// File domain commands. M-L(2) 清理（分析报告 §3.1/§3.5）：Edit 域整族与
// File 域文档-I/O 存根（Open/Save×4/Revert/Export/MergeProjects/ProjectInfo/
// Print×3/Cut/Copy/Paste/DuplicateSelection/SelectAll/Delete/Refresh/
// BoxSelection/BoxElementSelection/SendToPythonConsole/Placement/
// TransformManip/Alignment/Edit/Properties/UserEditMode）与 Std_Undo/Std_Redo
// （文档语义存根——视图 undo/redo 已在 Analysis 工具栏）与 Std_CloseActiveWindow/
// Std_CloseAllWindows（可点无效）与 Std_RecentFiles（永久空子菜单——无文件
// 系统入口）全删。保留三个真功能命令：Std_New（blank connection）、
// Std_Import（glTF 导入）、Std_Quit。
#include "Command.h"
#include "Action.h"
#include "CommandDoc.h"
#include "MainWindow.h"
#include "Application.h"

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
// Registration helper — called by Task 10 (CreateStdCommands)
// =====================================================================

void createFileEditCommands(CommandManager& mgr)
{
    // File（真功能三项——M-L(2) 清理后余量）
    mgr.addCommand(new StdCmdNew());
    mgr.addCommand(new StdCmdImport());
    mgr.addCommand(new StdCmdQuit());
}

} // namespace Gui
