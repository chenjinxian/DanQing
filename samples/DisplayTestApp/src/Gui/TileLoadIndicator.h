// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — tile load progress indicator
// Ported from: itwinjs-core display-test-app TileLoadIndicator.ts (a <progress>
// element in the status-bar container; updated on every viewManager.onFinishRender
// with ready/(ready+requested) aggregated over all viewports).
//
// Qt mapping (§3.4): HTMLProgressElement → QProgressBar (min 0 max 1000 — the
// reference sets fractional value; QProgressBar takes integers, so 0.1% steps);
// the mobile firstRenderFinished notification branch (TileLoadIndicator.ts:25-35)
// has no DanQing counterpart (no mobile messenger) and is omitted.
#pragma once

#include <QProgressBar>

#include <dqBase/DqEvent.h>

namespace dqApp {
class Viewport;
}

namespace Gui {

class TileLoadIndicator : public QProgressBar
{
    Q_OBJECT
public:
    explicit TileLoadIndicator(QWidget* parent = nullptr);
    ~TileLoadIndicator() override;

private:
    // Aggregate numReadyTiles / (numReadyTiles + numRequestedTiles) across all
    // viewports and set the progress value (1.0 when nothing is pending).
    // Ported from: TileLoadIndicator.update (TileLoadIndicator.ts:17-39).
    void update();

    dqBase::DqEventScope m_renderEventScope;
};

}  // namespace Gui
