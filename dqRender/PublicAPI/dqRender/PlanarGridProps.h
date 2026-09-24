// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender - PlanarGridProps (planar grid display settings)
// Ported from: itwinjs-core core/frontend/src/render/RenderSystem.ts:68-96
//              (PlanarGridTransparency L71-78, PlanarGridProps L83-96)
//
// Settings consumed by PlanarGridGraphic (the procedural ground grid). origin + rMatrix
// place the grid (rMatrix rowX/rowY are the in-plane axes, rowZ the normal); spacing is
// the line spacing in X/Y; gridsPerRef is the reference-line period; color + transparency
// drive u_gridColor / u_gridProps.
#pragma once

#include <dqCommon/ColorDef.h>
#include <dqGeom/Matrix3d.h>
#include <dqGeom/Point2d.h>
#include <dqGeom/Point3d.h>

#include <optional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

/// Transparency settings for planar grid display.
/// Ported from: itwinjs-core PlanarGridTransparency (RenderSystem.ts:71-78)
struct PlanarGridTransparency {
    /// Transparency for the grid plane (high, to avoid obscuring geometry).
    float planeTransparency = 0.9f;
    /// Transparency of the grid lines (higher than plane, less than reference).
    float lineTransparency = 0.75f;
    /// Transparency of the reference lines (lower, so they are more prominent).
    float refTransparency = 0.5f;
};

/// Settings for planar grid display.
/// Ported from: itwinjs-core PlanarGridProps (RenderSystem.ts:83-96)
struct PlanarGridProps {
    /// The grid origin.
    dqGeom::Point3d origin;
    /// The grid orientation; rowX/rowY are the in-plane axes, rowZ the normal.
    dqGeom::Matrix3d rMatrix = dqGeom::Matrix3d::CreateIdentity();
    /// The spacing between grid lines in the X and Y direction.
    dqGeom::Point2d spacing;
    /// Grid lines per reference. If zero, no reference lines are displayed.
    double gridsPerRef = 0.0;
    /// Grid color.
    dqCommon::ColorDef color = dqCommon::ColorDef::create();
    /// Transparency settings. If unset, PlanarGridTransparency defaults are used.
    std::optional<PlanarGridTransparency> transparency;
};

END_DQ_RENDER_NAMESPACE
