// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceQuery (read-only mesh queries)
//
// Ported from: itwinjs-core core/geometry/src/polyface/PolyfaceQuery.ts
//              imodel-native iModelCore/GeomLibs/PublicAPI/Geom/Polyface.h (PolyfaceQuery)
//
// Static utility class for read-only queries on IndexedPolyface.
// This is the interface that dqRender consumes (via PolyfaceQuery::PointRange, etc.).
#pragma once

#include "IndexedPolyface.h"

BEGIN_DQ_GEOM_NAMESPACE

// ---------------------------------------------------------------------------
// PolyfaceQuery — static read-only queries on polyface meshes
// ---------------------------------------------------------------------------
class DQ_GEOM_EXPORT PolyfaceQuery {
public:
    PolyfaceQuery() = delete;  // static-only class

    // --- Geometry queries ---
    /// Compute the bounding range of all points.
    static Range3d PointRange(IndexedPolyface const& polyface);

    /// Get the number of facets.
    static size_t GetNumFacet(IndexedPolyface const& polyface);

    /// Get the number of vertices.
    static size_t GetNumVertex(IndexedPolyface const& polyface);

    /// Check if normals are available.
    static bool HasNormals(IndexedPolyface const& polyface);

    /// Check if colors are available.
    static bool HasColors(IndexedPolyface const& polyface);

    /// Check if the mesh has any facets.
    static bool HasFacets(IndexedPolyface const& polyface);

    // --- Area queries ---
    /// Compute total surface area (sum of facet areas).
    static double SumFacetAreas(IndexedPolyface const& polyface);

    /// Compute the unit normal of a single facet (assumes planar).
    static bool ComputeFacetUnitNormal(IndexedPolyface const& polyface,
                                        size_t facetIndex, Vector3d& normal);

    // --- Normal generation ---
    /// Compute and attach averaged vertex normals to a normal-less polyface (angle-based smoothing groups).
    /// Ported from: itwinjs-core PolyfaceQuery.buildAverageNormals → BuildAverageNormalsContext.buildFastAverageNormals
    ///   (core/geometry/src/polyface/multiclip/BuildAverageNormalsContext.ts, 184 lines; default tolerance 31°).
    static void BuildAverageNormals(IndexedPolyface& polyface,
                                    double toleranceAngleRadians = 0.5410520681182421 /* 31 degrees */);


    // --- Topology queries ---
    /// Check if the mesh is closed (every edge shared by exactly 2 facets).
    static bool IsClosedByEdgePairing(IndexedPolyface const& polyface);

private:
    /// Helper: compute triangle area from 3 points.
    static double TriangleArea(Point3d const& a, Point3d const& b, Point3d const& c);
};

END_DQ_GEOM_NAMESPACE
