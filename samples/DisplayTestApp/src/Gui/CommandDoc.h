// Ported from: FreeCAD src/Gui/CommandDoc.cpp (header for registration + class declarations)
// Declares command classes + createFileEditCommands() — registers all File + Edit domain commands.
#pragma once

#include "Command.h"

namespace Gui {

class CommandManager;

// =====================================================================
// File domain — New command (exposed for unit-test dispatch, mirroring
// CommandView.h's exposure of StdCmdViewFitAll etc.).
// =====================================================================

// Ported from: FreeCAD src/Gui/CommandDoc.cpp:737-748 (DEF_STD_CMD)
class StdCmdNew : public Command {
public:
    StdCmdNew();
    ~StdCmdNew() override = default;
    const char* className() const override { return "StdCmdNew"; }
protected:
    void activated(int iMsg) override;
};

/// Register all File + Edit domain commands with the given manager.
/// Ported from: Authored — FreeCAD registers via createStdCommands() in CommandDoc.cpp
void createFileEditCommands(CommandManager& mgr);

} // namespace Gui
