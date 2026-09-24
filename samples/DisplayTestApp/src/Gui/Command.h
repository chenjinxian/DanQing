// Ported from: FreeCAD src/Gui/Command.h
#pragma once

#include <map>
#include <string>
#include <QString>

class QWidget;
class QAction;

namespace Gui {

class Action;
class Command;
class CommandManager;
class MenuItem;

// Ported from: FreeCAD src/Gui/Command.h:708-715
enum CmdType {
    AlterDoc       = 1,
    Alter3DView    = 2,
    AlterSelection = 4,
    ForEdit        = 8,
    NoTransaction  = 16,
};

// Ported from: FreeCAD src/Gui/Command.h:256-346 (CommandBase metadata)
class CommandBase {
public:
    CommandBase(const char* sMenu, const char* sToolTip, const char* sWhat,
                const char* sStatus, const char* sPixmap, const char* sAcc);
    virtual ~CommandBase() = default;

    // Ported from: FreeCAD src/Gui/Command.h:292-315
    const char* getMenuText()    const { return sMenuText; }
    const char* getToolTipText() const { return sToolTipText; }
    const char* getWhatsThis()   const { return sWhatsThis; }
    const char* getStatusTip()   const { return sStatusTip; }
    const char* getPixmap()      const { return sPixmap; }
    const char* getAccel()       const { return sAccel; }

    // Ported from: FreeCAD src/Gui/Command.h:320-326
    void setMenuText(const char* s)    { sMenuText = s; }
    void setToolTipText(const char* s) { sToolTipText = s; }
    void setWhatsThis(const char* s)   { sWhatsThis = s; }
    void setStatusTip(const char* s)   { sStatusTip = s; }
    void setPixmap(const char* s)      { sPixmap = s; }
    void setAccel(const char* s)       { sAccel = s; }

protected:
    const char* sMenuText    = nullptr;
    const char* sToolTipText = nullptr;
    const char* sWhatsThis   = nullptr;
    const char* sStatusTip   = nullptr;
    const char* sPixmap      = nullptr;
    const char* sAccel       = nullptr;
    Action* m_pcAction       = nullptr;  // Ported from: _pcAction (FreeCAD Command.h:344)
    friend class Command;
    friend class CommandManager;
};

// Ported from: FreeCAD src/Gui/Command.h:363-739 (Command)
class Command : public CommandBase {
public:
    // Ported from: FreeCAD src/Gui/Command.h:366
    explicit Command(const char* name);
    ~Command() override = default;

    // Ported from: FreeCAD src/Gui/Command.h:670-679
    const char* getName()      const { return sName; }
    const char* getGroupName() const { return sGroup; }
    int  getType() const { return eType; }

    // Ported from: FreeCAD src/Gui/Command.h:374-389
    virtual void   activated(int iMsg) = 0;
    virtual bool   isActive() { return true; }
    virtual const char* className() const = 0;

    // Ported from: FreeCAD src/Gui/Command.h:376-381 (createAction override)
    virtual Action* createAction();
    Action* getAction() const { return m_pcAction; }
    void    initAction();
    void    addTo(QWidget* pcWidget);
    void    setShortcut(const QString&);
    void    testActive();

protected:
    // Ported from: FreeCAD src/Gui/Command.h:378
    void applyCommandData(const char* context, Action* action);

    // Ported from: FreeCAD src/Gui/Command.h:722-730
    const char* sName      = nullptr;
    const char* sAppModule = "FreeCAD";
    const char* sGroup     = "Standard";
    const char* sHelpUrl   = nullptr;
    int  eType = AlterDoc | Alter3DView | AlterSelection;
    bool bCanLog = true;
    bool bEnabled = true;
};

// Ported from: FreeCAD src/Gui/Command.h:1043-1065 (keySequenceToAccel as free function)
// Returns cached const char* — sAccel is const char*, so the result must outlive the caller.
const char* keySequenceToAccel(int stdKey);

// Ported from: FreeCAD src/Gui/Command.h:973-1063 (CommandManager)
class CommandManager {
public:
    void addCommand(Command* pCom);
    Command* getCommandByName(const char* sName) const;
    bool addTo(const char* Name, QWidget* pcWidget);
    void testActive();
    const std::map<std::string, Command*>& getAllCommands() const { return m_sCommands; }

    // Ported from: FreeCAD src/Gui/Application.cpp:2226-2252
    //              (Gui::Application::setupContextMenu — DTA adaptation)
    // Dispatch entry point for context-menu population. FreeCAD routes this
    // through Gui::Application::Instance->setupContextMenu, which iterates active
    // workbenches via WorkbenchManager. DTA has no Gui::Application with this
    // method; dispatch is placed on CommandManager so callers (View3DInventor,
    // future Tree context menus) reach the active workbench through the existing
    // MainWindow::commandManager() singleton accessor with minimal new surface.
    // No-op when no workbench has been activated (graceful null-active guard).
    void setupContextMenu(const char* recipient, MenuItem* item) const;

private:
    std::map<std::string, Command*> m_sCommands;  // Ported from: _sCommands
};

} // namespace Gui

// =====================================================================
// DEF_STD_CMD macros. Ported from: FreeCAD src/Gui/Command.h:1073-1156
// =====================================================================

#define DEF_STD_CMD(X) \
    class X : public Gui::Command { \
    public: X(); virtual ~X() {} \
        virtual const char* className() const { return #X; } \
    protected: virtual void activated(int iMsg); \
    private: X(const X&) = delete; X(X&&) = delete; \
        X& operator=(const X&) = delete; X& operator=(X&&) = delete; };

#define DEF_STD_CMD_A(X) \
    class X : public Gui::Command { \
    public: X(); virtual ~X() {} \
        virtual const char* className() const { return #X; } \
    protected: virtual void activated(int iMsg); virtual bool isActive(); \
    private: X(const X&) = delete; X(X&&) = delete; \
        X& operator=(const X&) = delete; X& operator=(X&&) = delete; };

#define DEF_STD_CMD_AC(X) \
    class X : public Gui::Command { \
    public: X(); virtual ~X() {} \
        virtual const char* className() const { return #X; } \
    protected: virtual void activated(int iMsg); virtual bool isActive(); \
        virtual Gui::Action* createAction(); \
    private: X(const X&) = delete; X(X&&) = delete; \
        X& operator=(const X&) = delete; X& operator=(X&&) = delete; };

// Ported from: FreeCAD src/Gui/Command.h:1129-1140 (DEF_STD_CMD_C)
// Commands that override createAction (not isActive).
#define DEF_STD_CMD_C(X) \
    class X : public Gui::Command { \
    public: X(); virtual ~X() {} \
        virtual const char* className() const { return #X; } \
    protected: virtual void activated(int iMsg); \
        virtual Gui::Action* createAction(); \
    private: X(const X&) = delete; X(X&&) = delete; \
        X& operator=(const X&) = delete; X& operator=(X&&) = delete; };
