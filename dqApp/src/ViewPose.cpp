// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ViewPose 实现（equalState）
// Ported from: itwinjs-core core/frontend/src/ViewPose.ts
#include "dqApp/ViewPose.h"
#include "dqApp/ViewState.h"

namespace dqApp {

// Ported from: itwinjs-core ViewPose3d.equalState (ViewPose.ts:108-117)：
// cameraOn === view.isCameraOn && origin/extents/rotation AlmostEqual &&
// (!cameraOn || camera.equals(view.camera))。
bool ViewPose3d::equalState(const ViewState& view) const
{
    auto const* v3d = view.AsViewState3d();
    if (!v3d)
        return false;
    return IsCameraOn() == v3d->IsCameraOn() &&
           origin.AlmostEqual(v3d->GetOrigin()) &&
           extents.AlmostEqual(v3d->GetExtents()) &&
           rotation.IsAlmostEqual(v3d->getRotation()) &&
           (!IsCameraOn() || camera.equals(v3d->GetCamera()));
}

// DanQing 无 2D 视图类（ViewState2d 未移植）——2d pose 与任何现存视图均不等价。
// 参考 ViewPose2d.equalState（ViewPose.ts:157-164）在 2D 视图落地时一并移植。
bool ViewPose2d::equalState(const ViewState& /*view*/) const
{
    return false;
}

}  // namespace dqApp
