// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — View state (what to view and how)
// Ported from: itwinjs-core core/frontend/src/ViewState.ts + SpatialViewState.ts
#pragma once

#include "DisplayStyle.h"
#include "Decorator.h"
#include "MarginOptions.h"
#include "ViewStatus.h"

#include <dqBase/RefCounted.h>
#include <dqCommon/Camera.h>
#include <dqCommon/Cartographic.h>
#include <dqCommon/CategorySelectorState.h>
#include <dqCommon/Frustum.h>
#include <dqCommon/GridOrientationType.h>
#include <dqCommon/ViewFlags.h>
#include <dqCommon/ModelSelectorState.h>
#include <dqGeom/Map4d.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>
#include <dqGeom/Range3d.h>
#include <dqGeom/Vector3d.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace dqApp {

class IModelConnection;
class Viewport;
class ViewPose;  // forward declaration for savePose/applyPose
class ViewState;  // forward declaration for GridDecorator
class ViewState3d;  // forward declaration for type guard
class SpatialViewState;  // forward declaration for type guard

// ExtentLimits — describes the largest and smallest values allowed for the
// extents of a ViewState. Attempts to exceed these limits in any dimension will
// fail, preserving the previous extents.
// Ported from: itwinjs-core ViewState.ts ExtentLimits (ViewState.ts:70-75)。
struct ExtentLimits {
    double min;  // The smallest allowed extent in any dimension.
    double max;  // The largest allowed extent in any dimension.
};

// OnViewExtentsError — a method to be called if an error occurs while adjusting
// a ViewState's extents (view too large or too small).
// Ported from: itwinjs-core ViewAnimation.ts OnViewExtentsError (ViewAnimation.ts:64-67)。
struct OnViewExtentsError {
    std::function<ViewStatus(ViewStatus)> onExtentsError;  // 可空
};

// LookAtArgs — ViewState3d::lookAt 的参数。
// Ported from: itwinjs-core LookAtArgs/LookAtPerspectiveArgs/LookAtOrthoArgs/
//              LookAtUsingLensAngle（ViewState.ts:125-180）。§3.4 适配：TS 三接口
// 联合（targetPoint/viewDirection/lensAngle 互斥判别）→ 单 struct + optional
// 字段；判别语义与参考一致：targetPoint 有值=perspective，仅 viewDirection=
// orthographic，lensAngleRadians 联合 targetPoint=UsingLensAngle 变体。
struct LookAtArgs {
    dqGeom::Point3d eyePoint;                       // :129 眼点
    dqGeom::Vector3d upVector;                      // :131 up 向量（不得平行视线）
    std::optional<dqGeom::Point2d> newExtents;      // :133 焦平面视域（缺省保持现值）
    std::optional<double> frontDistance;            // :135 眼→前平面距离
    std::optional<double> backDistance;             // :137 眼→后平面距离
    OnViewExtentsError const* opts = nullptr;       // :139
    std::optional<dqGeom::Point3d> targetPoint;     // :148（perspective）
    std::optional<dqGeom::Vector3d> viewDirection;  // :160（ortho）
    std::optional<double> lensAngleRadians;         // :172（UsingLensAngle，Angle→弧度）
};

// GlobalLocation — lookAtGlobalLocation 的定位参数。
// Ported from: itwinjs-core GlobalLocation（ViewGlobalLocation.ts）。
struct GlobalLocation {
    dqCommon::Cartographic center;  // 定位中心
    std::optional<double> area;     // 可选面积（触发 areaToEyeHeight，未移植）
};

// ---------------------------------------------------------------------------
// GridDecorator — decorates the viewport with the view's grid
// Ported from: itwinjs-core ViewState.ts GridDecorator (ViewState.ts:177-207)。
// Graphics are cached as long as scene remains valid (useCachedDecorations=true)。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT GridDecorator : public IDecorator {
public:
    explicit GridDecorator(ViewState* view) : m_view(view) {}

    // ← ViewState.ts:181 — public readonly useCachedDecorations = true。
    // DanQing 适配（所有权偏差）：参考经 DecorationsCache 缓存网格图形依赖 TS GC
    // 持有——C++ 移植的 DecorationsCache::clear() 会 delete entry.graphic，与
    // Viewport::m_gridGraphic 的 GL 句柄复用所有权冲突（InvalidateScene 每帧
    // 清缓存 → 悬空）。网格图形改由 Viewport 持有（create-once +
    // updatePlanarGridFrustum 原地更新），此处返回 false 使缓存不接管所有权；
    // 每次收集重跑 Decorate 的行为与缓存 miss 重放等价。
    bool UseCachedDecorations() const override { return false; }

    // ← ViewState.ts:183-206 decorate。
    void Decorate(DecorateContext& context) override;

    bool TestDecorationHit(uint32_t /*featureId*/) const override { return false; }
    QString GetDecorationToolTip(uint32_t /*featureId*/) const override { return {}; }

private:
    ViewState* m_view;  // not owned（持有者为所属 ViewState，生命周期一致）
};

// ---------------------------------------------------------------------------
// ViewState — defines what the viewport shows
// Ported from: itwinjs-core ViewState.ts (abstract base)
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewState : public dqBase::RefCounted<ViewState>, public IDecorator {
public:
    ViewState();
    ~ViewState() override;

    // --- Decorator（参考 ViewState implements Decorator）---
    // Add view-specific decorations. The base implementation draws the grid.
    // Ported from: itwinjs-core ViewState.decorate (ViewState.ts:641-645)。
    // Subclasses must invoke the base implementation（参考注释 :641-643）。
    void Decorate(DecorateContext& context) override;
    bool TestDecorationHit(uint32_t /*featureId*/) const override { return false; }
    QString GetDecorationToolTip(uint32_t /*featureId*/) const override { return {}; }

    // 网格装饰入口（参考 ViewState.drawGrid，ViewState.ts:678-680）：
    // context.addFromDecorator(this._gridDecorator)。
    void drawGrid(DecorateContext& context);

    // Events (← itwinjs-core ViewState events)
    // Fired when the corresponding property changes.
    dqBase::DqEvent<> OnDisplayStyleChanged;
    dqBase::DqEvent<> OnViewedCategoriesChanged;
    dqBase::DqEvent<> OnViewedModelsChanged;
    dqBase::DqEvent<> OnClipVectorChanged;
    // Additional events (← itwinjs-core ViewState.ts)
    dqBase::DqEvent<> OnCategorySelectorChanged;
    dqBase::DqEvent<> OnModelSelectorChanged;
    dqBase::DqEvent<> OnRenderModeChanged;
    dqBase::DqEvent<> OnDetailsChanged;
    dqBase::DqEvent<> OnGridOrientationChanged;  // ViewState3d only

    // Viewport binding (← itwinjs-core: view.attachToViewport/detachFromViewport)
    // ViewState maintains a list of attached Viewports so it can notify them
    // when properties change.
    void AttachToViewport(Viewport* vp);
    void DetachFromViewport(Viewport* vp);
    bool IsAttachedToViewport(Viewport* vp) const;
    std::vector<Viewport*> const& GetAttachedViewports() const { return m_attachedViewports; }

    // IModel connection (← ViewState._iModel)
    IModelConnection* GetIModel() const { return m_iModel; }
    void SetIModel(IModelConnection* imodel) {
        m_iModel = imodel;
        // DisplayStyleState 的 iModel 绑定（DisplayStyleState.ts:763 getBackgroundMapGeometry
        // 依赖）；DanQing 的 DisplayStyle 是值成员，此处系挂。
        m_displayStyle.setIModel(imodel);
    }

    // Frustum definition
    dqGeom::Point3d GetOrigin() const { return m_origin; }
    dqGeom::Vector3d GetExtents() const { return m_extents; }
    void SetOrigin(dqGeom::Point3d const& origin) {
        m_origin = origin;
        NotifyViewportsNeedRedraw();
    }
    void SetExtents(dqGeom::Vector3d const& extents) {
        m_extents = extents;
        NotifyViewportsNeedRedraw();
    }

    // Display style
    DisplayStyle& GetDisplayStyle() { return m_displayStyle; }
    DisplayStyle const& GetDisplayStyle() const { return m_displayStyle; }

    // Category selector — which categories are visible (← itwinjs-core ViewState
    // holds the CategorySelectorState; SpatialViewState.ts:79 createBlank makes an
    // empty one). Empty by default for a blank view.
    dqCommon::CategorySelectorState& GetCategorySelector() { return m_categorySelector; }
    dqCommon::CategorySelectorState const& GetCategorySelector() const { return m_categorySelector; }

    // View flags (convenience) — const& because dqCommon::ViewFlags is immutable
    // (mutate via DisplayStyle::setViewFlags).
    dqCommon::ViewFlags const& getViewFlags() const noexcept { return m_displayStyle.getViewFlags(); }

    // Description
    std::string const& getDescription() const { return m_description; }
    void SetDescription(std::string const& desc) { m_description = desc; }

    // Load (no-op for blank connections)
    virtual void Load() {}

    // Clone — create a deep copy of this ViewState.
    // Ported from: itwinjs-core ViewState.clone()
    // Events and attached viewports are NOT cloned.
    virtual dqBase::RefPtr<ViewState> Clone() const;

    // Type guard for 3D views (avoids dynamic_cast, which requires RTTI)
    virtual ViewState3d* AsViewState3d() { return nullptr; }
    // const overload — needed by ViewPose3d::equalState(const ViewState&)
    // (ViewPose.ts:108-117 reads the view without mutating it).
    virtual ViewState3d const* AsViewState3d() const { return nullptr; }
    // Ported from: itwinjs-core ViewState.isSpatialView()
    virtual bool isSpatialView() const noexcept { return false; }

    // --- Extent limits ---
    // Get the largest and smallest values allowed for the extents of this
    // ViewState (ViewState.ts:851; reference resolves _extentLimits ??
    // defaultExtentLimits — DanQing stores the resolved value directly, see
    // m_extentLimits).
    // Ported from: itwinjs-core ViewState.extentLimits。
    ExtentLimits const& extentLimits() const noexcept { return m_extentLimits; }

    // Get the aspect ratio skew (x/y, usually 1.0) that is used to exaggerate
    // the y axis of the view.
    // Ported from: itwinjs-core ViewState.getAspectRatioSkew (ViewState.ts:971-973)。
    // ViewDetails/aspectRatioSkew 未移植——恒 1.0 = 参考 skew 缺失时的默认行为
    // (core-common ViewDetails.ts:100 `JsonUtils.asDouble(_json.aspectSkew, 1.0)`)。
    double getAspectRatioSkew() const noexcept { return 1.0; }

    // Determine whether this ViewState has the same coordinate system as another
    // one. They must be from the same iModel, and view a model in common.
    // Ported from: itwinjs-core ViewState.hasSameCoordinates (ViewState.ts:1221-1240)。
    // DanQing 无 2D 视图/DrawingView——参考其余分支（:1229+ 模型比较）随 2D 落地时补。
    bool hasSameCoordinates(const ViewState& other) const;

    // Adjust the supplied delta/origin so the extents lie within extentLimits and
    // (when aspect is supplied) match the requested aspect ratio, shifting origin
    // by half the delta change to keep the view centered.
    // Ported from: itwinjs-core ViewState.adjustViewDelta (ViewState.ts:889-920)。
    ViewStatus adjustViewDelta(dqGeom::Vector3d& delta, dqGeom::Point3d& origin,
                               dqGeom::Matrix3d const& rot, std::optional<double> aspect,
                               OnViewExtentsError const* opts);

protected:
    // Notify all attached viewports that they need to redraw.
    void NotifyViewportsNeedRedraw();

    IModelConnection* m_iModel = nullptr;  // not owned
    dqGeom::Point3d m_origin = {0, 0, 0};
    dqGeom::Vector3d m_extents = {1000, 1000, 1000};
    // 参考 ViewState 基类的 defaultExtentLimits 是 abstract（ViewState.ts:861），
    // 具体值由子类提供；DanQing 唯一的具体视图是 SpatialViewState，故基类成员直接
    // 取 SpatialViewState.ts:117 的空间视图默认 {Constant.oneMillimeter,
    // 3*Constant.diameterOfEarth}（core-geometry Constant.ts:16/24 → 0.001 /
    // 3*12742000）。Drawing/Sheet 视图落地时各子类再覆盖（参考
    // DrawingViewState.ts:455-456 / SheetViewState.ts:242）。
    ExtentLimits m_extentLimits = {0.001, 38226000.0};
    dqCommon::CategorySelectorState m_categorySelector;
    DisplayStyle m_displayStyle;
    std::string m_description;
    std::vector<Viewport*> m_attachedViewports;  // not owned
    // ← ViewState.ts:247 — private readonly _gridDecorator（每视图一个，参考
    // 构造于 ViewState ctor :307）。
    std::unique_ptr<GridDecorator> m_gridDecorator;
};

// ---------------------------------------------------------------------------
// ViewState3d — 3D view state with rotation and camera
// Ported from: itwinjs-core ViewState.ts ViewState3d class
//
// Holds the rotation matrix (orthonormal 3x3 whose rows are the view's
// X, Y, Z axis unit vectors) and camera parameters (eye point, focus
// distance, lens angle).  Provides camera positioning via lookAt() and
// coordinate queries via getCenter/GetTargetPoint/GetZVec.
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ViewState3d : public ViewState {
public:
    ViewState3d();
    ~ViewState3d() override;

    // --- Rotation ---
    dqGeom::Matrix3d const& getRotation() const { return m_rotation; }
    void SetRotation(dqGeom::Matrix3d const& rot) { m_rotation = rot; }

    // --- Camera ---
    dqCommon::Camera const& GetCamera() const { return m_camera; }
    dqCommon::Camera& GetCamera() { return m_camera; }
    bool IsCameraOn() const { return m_cameraOn; }
    void EnableCamera() { m_cameraOn = true; }
    void TurnCameraOff() { m_cameraOn = false; }

    void setEyePoint(dqGeom::Point3d const& eye) { m_camera.setEyePoint(eye); }
    dqGeom::Point3d getEyePoint() const { return m_camera.getEyePoint(); }
    void setFocusDistance(double dist) { m_camera.setFocusDistance(dist); }
    double getFocusDistance() const { return m_camera.getFocusDistance(); }
    void SetLensAngle(double radians) { m_camera.setLensRadians(radians); }
    double GetLensAngle() const { return m_camera.getLensRadians(); }

    // --- Derived queries ---
    // Ported from: itwinjs-core ViewState3d.getCenter()
    // Center of the frustum in world coordinates.
    dqGeom::Point3d getCenter() const;

    // Ported from: itwinjs-core ViewState.setCenter (ViewState.ts:524-527)：
    // origin += center - getCenter()。动画器 setCenter 调用点
    // （FrustumAnimator.ts:133，须在 extents/rotation 之后调用）。
    void setCenter(dqGeom::Point3d const& center);

    // Ported from: itwinjs-core ViewState3d.getTargetPoint (ViewState.ts:1877-1884)
    // camera off + geo-located Ellipsoid → getEarthFocalPoint()（或回退 getCenter()）；
    // camera on → eye + zVec * (-focusDist)。
    dqGeom::Point3d GetTargetPoint() const;

    // Ported from: itwinjs-core ViewState3d.getEarthFocalPoint (ViewState.ts:2272-2293)：
    // 非地理定位/非 Ellipsoid globe/无 BackgroundMapGeometry → undefined；否则沿
    // viewZ 从 center+diameterOfEarth·viewZ 反向打向地球椭球，取最大 fraction<0
    // 的出射点（相机关闭视图的旋转锚点——StandardViewTool/ViewManip 依赖它让
    // 链式标准视图切换的枢轴锚定在地表项目附近）。
    std::optional<dqGeom::Point3d> getEarthFocalPoint() const;

    // Ported from: itwinjs-core ViewState.getUpVector (ViewState.ts:1246-1255)：
    // 非地理定位/非 Ellipsoid/点在项目范围内 → unitZ；否则返回地球心→point 的
    // 归一化向量（地理上向量）。"Earth center" = getMapEcefToDb(0).origin
    // （IModelConnection.ts:571-583 = ecefLocation.getTransform().inverse().origin）。
    dqGeom::Vector3d getUpVector(dqGeom::Point3d const& point) const;

    // =========================================================================
    // 相机开启链（lookAt 家族）
    // Ported from: itwinjs-core ViewState3d（ViewState.ts:1813-2170）
    // =========================================================================

    /// minimumFrontDistance 的一并下限（参考 :1482 public 字段，默认 0）。
    double forceMinFrontDist = 0.0;

    /// Ported from: itwinjs-core ViewState3d.supportsCamera (:1833-1835)
    /// = allow3dManipulations()（globe 限制检查未移植——见 allow3dManipulations 注）。
    bool supportsCamera() const;

    /// Ported from: itwinjs-core ViewState3d.calcLensAngle (:1871-1873)：
    /// 2·atan2(extents.x/2, focusDist)。参考返回 Angle；DanQing Camera API 为弧度制，
    /// 返回弧度（§3.4 类型适配）。
    double calcLensAngle() const;

    /// Ported from: itwinjs-core ViewState3d.minimumFrontDistance (:1837-1838)
    /// = max(15.2·oneCentimeter, forceMinFrontDist)。
    double minimumFrontDistance() const;

    /// Ported from: itwinjs-core ViewState3d.getFrontDistance (:2096)
    /// = getBackDistance() - extents.z。
    double getFrontDistance() const;

    /// Ported from: itwinjs-core ViewState3d.getBackDistance (:2099-2105)：
    /// origin→eye 向量经 rotation 旋转后的 z 分量。
    double getBackDistance() const;

    /// Ported from: itwinjs-core ViewState3d.centerEyePoint (:2111-2115)：
    /// 把眼点置于视域中心正后方 backDistance（消除一点透视歪斜）。
    void centerEyePoint(std::optional<double> backDistance = std::nullopt);

    /// Ported from: itwinjs-core ViewState3d.verifyFocusPlane (:2131-2166)：
    /// 确保焦平面位于前后平面之间，否则居中并按比例调整 origin/extents。
    void verifyFocusPlane();

    /// Ported from: itwinjs-core ViewState3d.getEyeOrOrthographicViewPoint
    /// (:2148-2159)：相机开启→eye；关闭→按镜头角推算的等效正交眼点。
    dqGeom::Point3d getEyeOrOrthographicViewPoint();

    /// Ported from: itwinjs-core ViewState3d.rootToCartographic (:1602-1606)：
    /// 经 BackgroundMapGeometry.dbToCartographic（无 backgroundMap → nullopt）。
    std::optional<dqCommon::Cartographic> rootToCartographic(dqGeom::Point3d const& root) const;

    /// Ported from: itwinjs-core ViewState3d.cartographicToRoot (:1611-1614)。
    dqGeom::Point3d cartographicToRoot(dqCommon::Cartographic const& cartographic) const;

    /// Ported from: itwinjs-core ViewState3d.lookAt (:1891-1990)：透视/正交/镜头角
    /// 三变体统一入口。失败路径返回相应 ViewStatus（InvalidTargetPoint/InvalidLens/
    /// InvalidUpVector/InvalidDirection/NotCameraView/MaxDisplayDepth）。
    ViewStatus lookAt(LookAtArgs const& args);

    /// Ported from: itwinjs-core ViewState3d.lookAtGlobalLocation (:1637-1654)：
    /// 在地理定位上放置相机（eyeHeight 米高、俯仰 pitchAngleRadians）。
    /// 返回新/旧眼点距离；非地理定位返回 0。
    double lookAtGlobalLocation(double eyeHeight, double pitchAngleRadians = 0.0,
                                std::optional<GlobalLocation> location = std::nullopt,
                                dqGeom::Point3d const* eyePoint = nullptr);

protected:
    /// Ported from: itwinjs-core ViewState3d.enableCamera (:1828-1831)：
    /// 仅当 supportsCamera() 时开启。
    void enableCamera();

    /// Ported from: itwinjs-core ViewState3d.calculateMaxDepth (:1813-1820)。
    static double calculateMaxDepth(dqGeom::Vector3d const& delta, dqGeom::Vector3d const& zVec);

private:
    /// Ported from: itwinjs-core ViewState3d.finishLookAtGlobalLocation (:1683-1710)。
    double finishLookAtGlobalLocation(dqCommon::Cartographic targetPointCartographic,
                                      dqGeom::Point3d origEyePoint, dqGeom::Point3d eyePoint,
                                      dqGeom::Point3d targetPoint, double pitchAngleRadians);

public:

    // Ported from: itwinjs-core ViewState3d.getZVector()
    // Third row of the rotation matrix (the look direction, inverted).
    dqGeom::Vector3d GetZVec() const { return m_rotation.RowZ(); }

    // Ported from: itwinjs-core ViewState3d.getXVector()
    dqGeom::Vector3d GetXVec() const { return m_rotation.RowX(); }

    // Ported from: itwinjs-core ViewState3d.getYVector()
    dqGeom::Vector3d GetYVec() const { return m_rotation.RowY(); }

    // --- World→NPC map + frustum derivation ---
    // Ported from: itwinjs-core ViewState.computeWorldToNpc (ViewState.ts:625-701).
    // Builds the world→NPC[0,1]^3 Map4d from rotation/origin/extents (orthographic)
    // or from the camera lens/focus/eye (perspective). `viewRot`/`inOrigin`/`delta`
    // default to this view's rotation/origin/extents when null. When
    // `enforceFrontToBackRatio` is true the perspective front/back ratio is clamped
    // to the z-buffer limit (0.0003) — pass false when displaying a background map
    // (no z-buffer). Returns {map, frustFraction}; map is nullopt for a degenerate
    // (non-invertible) frustum. frustFraction is 1.0 (ortho) or frontSize/rearSize.
    struct WorldToNpcResult {
        std::optional<dqGeom::Map4d> map;
        double frustFraction = 1.0;
    };
    WorldToNpcResult computeWorldToNpc(
        dqGeom::Matrix3d const* viewRot = nullptr,
        dqGeom::Point3d const* inOrigin = nullptr,
        dqGeom::Vector3d const* delta = nullptr,
        bool enforceFrontToBackRatio = true) const;

    // Ported from: itwinjs-core ViewState.calculateFrustum (ViewState.ts:707-715).
    // Fills `out` with the 8 world-space frustum corners by applying
    // computeWorldToNpc().map.transform1 to the NPC cube. Returns false if the
    // frustum is degenerate (map is nullopt); `out` is left as the NPC cube.
    bool calculateFrustum(dqCommon::Frustum& out) const;

    // Ported from: itwinjs-core ViewingSpace.getFrustum (ViewingSpace.ts:471-503).
    // includeOrientation=true → world coords (delegates to calculateFrustum, which
    //   handles the perspective case via computeWorldToNpc — closes D10).
    // includeOrientation=false → view-local axis-aligned extents box [0..extents].
    void GetFrustum(dqCommon::Frustum& out, bool includeOrientation = true) const;

    // Ported from: itwinjs-core ViewState.setupFromFrustum() (ViewState.ts:791-841)
    // Inverse of GetFrustum: recompute origin/extents/rotation from the 8
    // corners of a frustum.  Returns true on success, false if the frustum is
    // degenerate (zero-length X edge, collinear X/Y, or left-handed).
    // NOTE: adjustViewDelta itself IS ported (ViewState.cpp, ViewState.ts:889-920);
    // only its clamp CALL SITE in setupFromFrustum (ViewState.ts:833-835 —
    // status = adjustViewDelta(...); non-Success → early return) remains unwired
    // (unreachable in blank-connection views) — see .cpp TODO.
    // (StandardView.adjustToStandardRotation IS ported — see the call site below.)
    bool SetupFromFrustum(dqCommon::Frustum const& in);

    // Ported from: itwinjs-core ViewState3d.moveCameraWorld() (ViewState.ts:1972-1981)
    // Move the view by `dist` in world coordinates.
    // Ported from: itwinjs-core ViewState3d.moveCameraWorld (ViewState.ts:2029-2040)：
    // 相机关闭 → origin += dist；相机开启 → lookAt({eye+=dist, target+=dist, up=y})。
    ViewStatus MoveCameraWorld(dqGeom::Vector3d const& dist);

    // Ported from: itwinjs-core ViewState3d.is3d() (ViewState.ts:1505)
    bool is3d() const noexcept { return true; }

    // Ported from: itwinjs-core ViewState3d.allow3dManipulations()
    // (ViewState.ts:1433-1435). Step 3 simplification returns is3d(); the full
    // globe/ViewDetails.allow3dManipulations check is deferred.
    // TODO: globe/allow3dManipulations full check — out of scope Step 3
    bool Allow3dManipulations() const noexcept { return is3d(); }

    // Ported from: itwinjs-core ViewState3d.setupView()
    // Set eye, target, and up to configure the view.
    void SetupView(dqGeom::Point3d const& eye,
                   dqGeom::Point3d const& target,
                   dqGeom::Vector3d const& up);

    // Ported from: itwinjs-core ViewState3d.lookAtVolume (ViewState.ts:1083) ->
    //              lookAtViewAlignedVolume (:1096-1177). Fit the view to a world-space
    //              bounding box. `aspect` (viewport window aspect) + `options` (padding/
    //              margin) mirror the reference; both default to null (default x1.04
    //              dilation). NOTE: adjustViewDelta itself IS ported (ViewState.cpp,
    //              ViewState.ts:889-920 — aspect branch included); the clamp CALL SITE in
    //              lookAtViewAlignedVolume (ViewState.ts:1159) + verifyFocusPlane remain
    //              unwired — see .cpp TODOs.
    void LookAtVolume(dqGeom::Range3d const& volume,
                      double const* aspect = nullptr,
                      MarginOptions const* options = nullptr);

    // Ported from: itwinjs-core StandardViewTool.onViewChanged() (ViewTool.ts:3496-3509)
    //              StandardView.getStandardRotation() (StandardView.ts:73-75)
    // Set the view rotation to one of the standard orientations (0-7).
    // Index maps to StandardViewId: 0=Top, 1=Bottom, 2=Left, 3=Right,
    // 4=Front, 5=Back, 6=Iso, 7=RightIso.
    void SetStandardView(int viewIndex);

    // Ported from: itwinjs-core ViewState.fixAspectRatio()
    // Adjust the Y extent of the view volume so the view aspect ratio
    // matches the window aspect ratio. Keeps the view centered.
    void FixAspectRatio(float windowAspect);

    // Capture a copy of the viewed volume and camera parameters.
    // Ported from: itwinjs-core ViewState3d.savePose (ViewState.ts:1533)。
    // 注：参考在 ViewState 基类声明抽象 savePose/applyPose（ViewState.ts:625/632，
    // 其基类本就抽象）；DanQing 的 ViewState 可被 ViewState::Clone() 直接实例化
    // （ViewState.cpp:68），故方法放在 3d 层（DanQing 无 2D 视图类）。
    std::unique_ptr<ViewPose> savePose() const;
    // See ViewState.applyPose. Ported from: itwinjs-core ViewState3d.applyPose
    // (ViewState.ts:1536-1547)。
    void applyPose(const ViewPose& val);

    // --- Grid settings ---
    // Ported from: itwinjs-core ViewState grid-settings API:
    //   getGridOrientation/getGridsPerRef/getGridSpacing (ViewState.ts:1008-1017),
    //   setGridSettings (ViewState.ts:953-966), getGridSettings (ViewState.ts:968-1006).
    // itwinjs stores these on viewDetails (ViewDetails.ts); DanQing keeps them on
    // ViewState3d (2-D views are not yet supported). Defaults match the reference:
    // orientation=WorldXY, gridsPerRef=10, spacing={1.0,1.0} (ViewDetails.ts:24-31).
    dqCommon::GridOrientationType getGridOrientation() const noexcept { return m_gridOrientation; }
    int32_t getGridsPerRef() const noexcept { return m_gridsPerRef; }
    dqGeom::Point2d getGridSpacing() const noexcept { return m_gridSpacing; }
    // Ported from: itwinjs-core ViewState.setGridSettings (ViewState.ts:953-966)
    void setGridSettings(dqCommon::GridOrientationType orientation, dqGeom::Point2d spacing, int32_t gridsPerRef);
    // Ported from: itwinjs-core ViewState.getGridSettings (ViewState.ts:968-1006).
    // Populates origin/rMatrix for the grid plane from the orientation. The View
    // branch (:1032-1038, needs vp.npcToView + vp.rotation) uses the vp overload;
    // 本无视口重载的 View 分支保持恒等（便捷路径，GridSettingsTest 用）。
    void getGridSettings(dqGeom::Point3d& origin, dqGeom::Matrix3d& rMatrix,
                         dqCommon::GridOrientationType orientation) const;
    // 参考签名版（ViewState.ts:1026 getGridSettings(vp, origin, rMatrix, orientation)）。
    // GridDecorator 经此取 View 方向的视平面网格（:1032-1038）。
    void getGridSettings(Viewport const& vp, dqGeom::Point3d& origin, dqGeom::Matrix3d& rMatrix,
                         dqCommon::GridOrientationType orientation) const;

    // Type guard
    ViewState3d* AsViewState3d() override { return this; }
    ViewState3d const* AsViewState3d() const override { return this; }
    // Type guard for spatial views (avoids dynamic_cast, which requires RTTI).
    virtual SpatialViewState* AsSpatialViewState() { return nullptr; }

    // Clone — deep copy including rotation and camera.
    dqBase::RefPtr<ViewState> Clone() const override;

protected:
    dqGeom::Matrix3d m_rotation = dqGeom::Matrix3d::CreateIdentity();
    dqCommon::Camera m_camera;
    bool m_cameraOn = false;
    // Grid settings (← itwinjs ViewDetails; defaults ViewDetails.ts:24-31)
    dqCommon::GridOrientationType m_gridOrientation = dqCommon::GridOrientationType::WorldXY;
    dqGeom::Point2d m_gridSpacing{1.0, 1.0};
    int32_t m_gridsPerRef = 10;
};

// ---------------------------------------------------------------------------
// SpatialViewState — 3D spatial view
// Ported from: itwinjs-core SpatialViewState.ts
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT SpatialViewState : public ViewState3d {
public:
    SpatialViewState();
    ~SpatialViewState() override;

    // --- Tile tree references (← SpatialViewState.ts:109 _treeRefs +
    //     getModelTreeRefs :544-548) ---
    // Constructed via the overridable SpatialTileTreeReferences::create
    // factory seam (PrimaryTileTree.ts:601-606 — the seam frontend-tiles
    // replaces in initializeFrontendTiles, FrontendTiles.ts:215).
    class SpatialTileTreeReferences& GetTileTreeRefs() const noexcept;
    // <- SpatialViewState.getModelTreeRefs (:544-548) — iterate the view's
    // tile tree references (implementation in .cpp; complete type there).
    void ForEachModelTreeRef(std::function<void(class TileTreeReference&)> const& func) const;

    // Factory: create a blank spatial view linked to a connection
    // ← SpatialViewState.createBlank(iModel, origin, extents, rotation?)
    static dqBase::RefPtr<SpatialViewState> CreateBlank(
        IModelConnection* iModel,
        dqGeom::Point3d const& origin,
        dqGeom::Vector3d const& extents,
        std::optional<dqGeom::Matrix3d> rotation = std::nullopt);

    // Type guard override (avoids dynamic_cast / RTTI).
    SpatialViewState* AsSpatialViewState() override { return this; }
    // Ported from: itwinjs-core SpatialViewState.isSpatialView()
    bool isSpatialView() const noexcept override { return true; }

    // Model selector — controls which models are visible
    // Ported from: itwinjs-core SpatialViewState.modelSelector
    dqCommon::ModelSelectorState& GetModelSelector() { return m_modelSelector; }
    dqCommon::ModelSelectorState const& GetModelSelector() const { return m_modelSelector; }

    // Convenience methods
    void AddViewedModel(dqBase::DqId id) { m_modelSelector.addModel(id); }
    void RemoveViewedModel(dqBase::DqId id) { m_modelSelector.dropModel(id); }
    bool ViewsModel(dqBase::DqId id) const { return m_modelSelector.containsModel(id); }

    // Compute a world-space volume tightly encompassing the view's contents.
    // Ported from: itwinjs-core SpatialViewState.computeFitRange (SpatialViewState.ts:145-160)
    // — falls back to ComputeBaseExtents when no tile-tree range is available (blank view).
    dqGeom::Range3d ComputeFitRange() const;
    // Ported from: itwinjs-core SpatialViewState.computeBaseExtents (SpatialViewState.ts:124-134)
    dqGeom::Range3d ComputeBaseExtents() const;

    // Clone — deep copy including model selector.
    dqBase::RefPtr<ViewState> Clone() const override;

private:
    dqCommon::ModelSelectorState m_modelSelector;
    std::unique_ptr<class SpatialTileTreeReferences> m_treeRefs;
};

}  // namespace dqApp
