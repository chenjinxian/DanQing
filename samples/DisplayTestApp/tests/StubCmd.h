// Ported from: Authored — test-only stub for Command framework tests
#pragma once
#include "Gui/Command.h"
#include "Gui/Action.h"

// Ported from: Authored — minimal DEF_STD_CMD stub for testing
DEF_STD_CMD(StubCmd)

// Ported from: Authored — StubCmd ctor sets metadata for testing
inline StubCmd::StubCmd()
    : Gui::Command("Stub_Cmd")
{
    sMenuText    = QT_TR_NOOP("&Stub Command");
    sToolTipText = QT_TR_NOOP("A stub command for testing");
    sStatusTip   = QT_TR_NOOP("Stub status");
    sPixmap      = "StubIcon";
    sAccel       = "Ctrl+N";
}

// Ported from: Authored — empty activated implementation
inline void StubCmd::activated(int)
{
    // no-op for testing
}

// Ported from: Authored — DEF_STD_CMD_A variant for isActive() testing
DEF_STD_CMD_A(StubActiveCmd)

inline StubActiveCmd::StubActiveCmd()
    : Gui::Command("StubActive_Cmd")
{
    sMenuText    = QT_TR_NOOP("&Stub Active");
    sToolTipText = QT_TR_NOOP("A stub active command");
    sAccel       = "Ctrl+S";
}

inline void StubActiveCmd::activated(int)
{
    // no-op
}

inline bool StubActiveCmd::isActive()
{
    return false;  // always inactive for testing
}
