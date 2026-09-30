// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — tile load progress indicator implementation
// Ported from: itwinjs-core display-test-app TileLoadIndicator.ts
#include "TileLoadIndicator.h"

#include <dqApp/Application.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>

namespace Gui {

TileLoadIndicator::TileLoadIndicator(QWidget* parent)
    : QProgressBar(parent)
{
    setObjectName(QStringLiteral("DTA.TileLoadIndicator"));
    setRange(0, 1000);              // HTMLProgressElement takes fractional values;
                                    // QProgressBar integers — 0.1% resolution.
    setTextVisible(false);
    setFixedWidth(120);

    // Ported from: TileLoadIndicator ctor (TileLoadIndicator.ts:14-18) —
    // IModelApp.viewManager.onFinishRender.addListener(() => this.update()).
    m_renderEventScope.add(
        dqApp::Application::Get().GetViewManager().OnFinishRender.AddListener(
            [this]() { update(); }));
    update();
}

TileLoadIndicator::~TileLoadIndicator()
{
    m_renderEventScope.DisconnectAll();
}

void TileLoadIndicator::update()
{
    // Ported from: TileLoadIndicator.update (TileLoadIndicator.ts:21-39).
    long long ready = 0;
    long long total = 0;
    auto& viewMgr = dqApp::Application::Get().GetViewManager();
    for (size_t i = 0; i < viewMgr.GetViewportCount(); ++i) {
        dqApp::Viewport* vp = viewMgr.GetViewport(i);
        if (!vp)
            continue;
        ready += static_cast<long long>(vp->numReadyTiles());
        total += static_cast<long long>(vp->numReadyTiles() + vp->numRequestedTiles());
    }

    double const pctComplete = (total > 0) ? static_cast<double>(ready) / static_cast<double>(total)
                                           : 1.0;
    setValue(static_cast<int>(pctComplete * 1000.0));
}

}  // namespace Gui
