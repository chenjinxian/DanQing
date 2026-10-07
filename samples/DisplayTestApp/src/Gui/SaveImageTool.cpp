// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — SaveImage tool implementation
// Ported from: itwinjs-core display-test-app SaveImageTool.ts
#include "SaveImageTool.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QImage>

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ParseArgs.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

namespace Gui {

bool SaveImageTool::writeFrameToFile(dqApp::Viewport& vp, QString const& path)
{
    // Ported from: SaveImageTool.run (SaveImageTool.ts:44-50) —
    //   const buffer = vp.readImageBuffer(...); const canvas = vp.readImageToCanvas
    //   (...); const url = canvas.toDataURL(); — the read + PNG-encode pipeline.
    // DanQing: Viewport::readImageBuffer (Viewport.ts:2803 readImageBuffer port)
    // + QImage::save (the canvas/dataURL half has no DOM here).
    //
    // 登记偏差（M-L(3) 终审 Minor-⑤）：参考 readImageBuffer 前有
    // `await vp.waitForSceneCompletion()`（SaveImageTool.ts:36——在途瓦全部
    // 落地再读帧）；DanQing 无该 API（登记），读帧取"最近一帧"——在途瓦
    // 未落地时产物缺该批内容。前置补位随 waitForSceneCompletion 移植落地。
    // 失败消息按参考两段区分：readImageBuffer 失败 = "Failed to read image"
    //（:39）；PNG 编码/写文件失败 = "Failed to produce PNG"（:49）。
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    if (!vp.readImageBuffer(rgba, w, h) || 0 == w || 0 == h) {
        dqApp::Application::Get().GetNotificationManager().OutputMessage(
            dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Error,
                                        "Failed to read image"));
        return false;
    }

    QImage image(w, h, QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < h; ++y) {
        // glReadPixels rows are bottom-up (GL origin); QImage rows are top-down.
        memcpy(image.scanLine(static_cast<int>(h - 1 - y)), &rgba[static_cast<size_t>(y) * w * 4],
               static_cast<size_t>(w) * 4);
    }
    if (!image.save(path, "PNG")) {
        dqApp::Application::Get().GetNotificationManager().OutputMessage(
            dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Error,
                                        "Failed to produce PNG"));
        return false;
    }
    return true;
}

bool SaveImageTool::run()
{
    // Ported from: SaveImageTool.run (SaveImageTool.ts:25-56).
    dqApp::Viewport* vp = dqApp::Application::Get().GetViewManager().GetActiveViewport();
    if (nullptr == vp)
        return false;

    if (!m_copyToClipboard) {
        // Reference: openImageDataUrlInNewWindow(url, "Saved View") — browser
        // window semantics (see the header's EQUIVALENCE note): the desktop
        // equivalent is a file write via the save dialog.
        QString const path = QFileDialog::getSaveFileName(
            nullptr, QStringLiteral("Save Image"), QStringLiteral("SavedView.png"),
            QStringLiteral("PNG image (*.png)"));
        if (path.isEmpty())
            return true;  // user cancelled — not a failure
        // 失败告警由 writeFrameToFile 内部单次发出（SaveImageTool.ts:39-41 单次
        // alert——2026-10-07 审计 B7：原调用方再发一次 = "Failed to read image"
        // 双告警）。
        if (!writeFrameToFile(*vp, path))
            return true;
        return true;
    }

    // Reference: navigator.clipboard.write (SaveImageTool.ts:60-79) → Qt clipboard.
    std::vector<uint8_t> rgba;
    uint32_t w = 0, h = 0;
    if (!vp->readImageBuffer(rgba, w, h) || 0 == w || 0 == h) {
        dqApp::Application::Get().GetNotificationManager().OutputMessage(
            dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Error,
                                        "Failed to read image"));
        return true;
    }
    QImage image(w, h, QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < h; ++y)
        memcpy(image.scanLine(static_cast<int>(h - 1 - y)), &rgba[static_cast<size_t>(y) * w * 4],
               static_cast<size_t>(w) * 4);
    QGuiApplication::clipboard()->setImage(image);
    return true;
}

bool SaveImageTool::parseAndRun(std::vector<std::string> const& args)
{
    // Ported from: SaveImageTool.parseAndRun (SaveImageTool.ts:92-108) —
    //   c → copyToClipboard; d/w/h → dimensions; o → omitCanvasDecorations.
    // The dimension / omit-decorations branches consume the reference's argument
    // surface but their target-side sized-read port is pending (see the header
    // note) — only the clipboard flag is honored.
    auto const opts = dqApp::parseArgs(args);
    if (opts.getBoolean("c").has_value())
        m_copyToClipboard = *opts.getBoolean("c");

    return run();
}

}  // namespace Gui
