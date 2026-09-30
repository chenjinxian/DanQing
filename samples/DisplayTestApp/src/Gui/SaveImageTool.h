// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — SaveImage tool
// Ported from: itwinjs-core display-test-app SaveImageTool.ts (toolId "SaveImage",
// minArgs 0 / maxArgs 4; reads the selected viewport's image and opens it in a new
// window, or copies it to the clipboard with the c flag).
//
// DanQing output-channel adaptation (§11.10 EQUIVALENCE registration): the
// reference surfaces the PNG via openImageDataUrlInNewWindow (browser window
// semantics); the desktop equivalent for persisting a frame is a file write —
// run() asks for the destination with QFileDialog (default "SavedView.png") and
// writes a PNG. The reference's width/height/omitCanvasDecorations arguments
// (SaveImageTool.ts:96-108) require target-side sized reads
// (Viewport.readImageBuffer({size}) — Target.readImageBuffer's size branch) which
// DanQing has not ported; only the clipboard flag is consumed (registered TODO
// with the size-read port).
#pragma once

#include <QString>

#include <dqApp/ToolAdmin.h>

namespace dqApp {
class Viewport;
}

namespace Gui {

class SaveImageTool final : public dqApp::InteractiveTool
{
public:
    // Ported from: SaveImageTool.toolId (SaveImageTool.ts:19).
    const char* getToolId() const override { return "SaveImage"; }
    // Ported from: SaveImageTool.minArgs / maxArgs (SaveImageTool.ts:20-23).
    int minArgs() const override { return 0; }
    int maxArgs() const override { return 4; }
    // Ported from: Tool.get englishKeyin — SVTTools.json "tools.SaveImage.keyin".
    std::string englishKeyin() const override { return "dta save image"; }

    // Reads the active viewport's image and writes it to `path` as a PNG.
    // Returns false when the read or the write fails. This is run()'s file-write
    // half factored out (the reference's canvas → dataURL pipeline — SaveImageTool.
    // ts:44-50); tests drive it directly since the reference has no file dialog.
    static bool writeFrameToFile(dqApp::Viewport& vp, QString const& path);

    // Ported from: SaveImageTool.run (SaveImageTool.ts:25-56) — read the selected
    // viewport's image; clipboard (c flag) or file output.
    bool run() override;
    // Ported from: SaveImageTool.parseAndRun (SaveImageTool.ts:92-108) — c=copy to
    // clipboard (w/h/d/o consume the reference's arg surface but the sized-read
    // target port is pending — see the header note).
    bool parseAndRun(std::vector<std::string> const& args) override;

private:
    bool m_copyToClipboard = false;
};

}  // namespace Gui
