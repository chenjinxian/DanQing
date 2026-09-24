// Ported from: FreeCAD src/Gui/Command.cpp
#include "Command.h"
#include "Action.h"
#include "MenuManager.h"  // MenuItem definition (forward-declared in Command.h)
#include "Workbench.h"    // Workbench::activeWorkbench()
#include <QKeySequence>
#include <QCoreApplication>
#include <QIcon>
#include <QWidget>

namespace Gui {

// Ported from: FreeCAD src/Gui/Command.cpp:162-177
CommandBase::CommandBase(
    const char* sMenu,
    const char* sToolTip,
    const char* sWhat,
    const char* sStatus,
    const char* sPixmap,
    const char* sAcc
)
    : sMenuText(sMenu)
    , sToolTipText(sToolTip)
    , sWhatsThis(sWhat ? sWhat : sToolTip)
    , sStatusTip(sStatus ? sStatus : sToolTip)
    , sPixmap(sPixmap)
    , sAccel(sAcc)
{}

// Ported from: FreeCAD src/Gui/Command.cpp:239-249
Command::Command(const char* name)
    : CommandBase(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)
{
    sName = name;
    sAppModule = "FreeCAD";
    sGroup = "Standard";
    eType = AlterDoc | Alter3DView | AlterSelection;
    bEnabled = true;
    bCanLog = true;
}

// Ported from: FreeCAD src/Gui/Command.cpp:1043-1065 (as free function)
// Caches result in static map so returned const char* remains valid.
const char* keySequenceToAccel(int stdKey)
{
    static std::map<int, std::string> strings;
    auto it = strings.find(stdKey);
    if (it != strings.end())
        return it->second.c_str();

    auto type = static_cast<QKeySequence::StandardKey>(stdKey);
    std::string s = QKeySequence(type).toString().toStdString();
    auto [ins, _] = strings.emplace(stdKey, std::move(s));
    return ins->second.c_str();
}

// Ported from: FreeCAD src/Gui/Command.cpp:1023-1040
void Command::applyCommandData(const char* context, Action* action)
{
    QAction* a = action->action();
    a->setText(QCoreApplication::translate(context, getMenuText() ? getMenuText() : ""));
    a->setToolTip(QCoreApplication::translate(context, getToolTipText() ? getToolTipText() : ""));
    a->setWhatsThis(QCoreApplication::translate(context, getWhatsThis() ? getWhatsThis() : ""));
    if (getStatusTip())
        a->setStatusTip(QCoreApplication::translate(context, getStatusTip()));
    else
        a->setStatusTip(QCoreApplication::translate(context, getToolTipText() ? getToolTipText() : ""));
    if (a->menuRole() == QAction::TextHeuristicRole)
        a->setMenuRole(QAction::NoRole);
}

// Ported from: FreeCAD src/Gui/Command.cpp:1081-1090
// C++ adaptation: BitmapFactory -> QIcon(":/icons/"+sPixmap)
Action* Command::createAction()
{
    auto* pcAction = new Action(this);
    applyCommandData(this->className(), pcAction);
    if (sPixmap)
        pcAction->setIcon(QIcon(QStringLiteral(":/icons/") + QString::fromLatin1(sPixmap)));
    return pcAction;
}

// Ported from: FreeCAD src/Gui/Command.cpp:286-298
// C++ adaptation: skip ShortcutManager (does not exist in DisplayTestApp)
void Command::initAction()
{
    if (!m_pcAction) {
        m_pcAction = createAction();
        setShortcut(QString::fromLatin1(getAccel() ? getAccel() : ""));
        testActive();
    }
}

// Ported from: FreeCAD src/Gui/Command.cpp:253-258
// C++ adaptation: skip ShortcutManager
void Command::setShortcut(const QString& accel)
{
    if (!m_pcAction) return;
    if (!accel.isEmpty())
        m_pcAction->action()->setShortcut(QKeySequence(accel));
}

// Ported from: FreeCAD src/Gui/Command.cpp:300-304
void Command::addTo(QWidget* pcWidget)
{
    initAction();
    m_pcAction->addTo(pcWidget);
}

// Ported from: FreeCAD src/Gui/Command.cpp:533-573
// C++ adaptation: drop ActionGroup branch + Control().isAllowedAlter* gates
// (no Control singleton in DisplayTestApp)
void Command::testActive()
{
    if (!m_pcAction) return;
    if (!bEnabled) { m_pcAction->setEnabled(false); return; }
    bool bActive = isActive();
    m_pcAction->setEnabled(bActive);
}

// =====================================================================
// CommandManager
// =====================================================================

// Ported from: FreeCAD src/Gui/Command.cpp:2041-2055
void CommandManager::addCommand(Command* pCom)
{
    auto& slot = m_sCommands[pCom->getName()];
    if (slot) return;  // duplicate — ignore
    slot = pCom;
}

// Ported from: FreeCAD src/Gui/Command.cpp:2168-2172
Command* CommandManager::getCommandByName(const char* sName) const
{
    auto it = m_sCommands.find(sName);
    return it != m_sCommands.end() ? it->second : nullptr;
}

// Ported from: FreeCAD src/Gui/Command.cpp:2103-2123
bool CommandManager::addTo(const char* Name, QWidget* pcWidget)
{
    auto it = m_sCommands.find(Name);
    if (it == m_sCommands.end()) return false;
    it->second->addTo(pcWidget);
    return true;
}

// Ported from: FreeCAD src/Gui/Command.cpp:2183-2189
void CommandManager::testActive()
{
    for (auto& kv : m_sCommands)
        kv.second->testActive();
}

// Ported from: FreeCAD src/Gui/Application.cpp:2226-2252
//              (Gui::Application::setupContextMenu — DTA adaptation)
// Dispatch entry point: delegates to the active workbench's setupContextMenu.
// FreeCAD reaches the active workbench via WorkbenchManager::instance()->active();
// DTA reaches it via Workbench::activeWorkbench() (set by Workbench::activate()).
// Null-active guard: a fresh CommandManager with no activated workbench is safe
// (e.g. unit tests that construct CommandManager directly).
void CommandManager::setupContextMenu(const char* recipient, MenuItem* item) const
{
    const Workbench* actWb = Workbench::activeWorkbench();
    if (actWb) {
        actWb->setupContextMenu(recipient, item);
    }
}

} // namespace Gui
