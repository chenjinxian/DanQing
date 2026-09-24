// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/geometry/src/polyface/multiclip/BuildAverageNormalsContext.ts
// DanQing dqGeom — BuildAverageNormalsContext implementation (per-vertex normal averaging)
#include "BuildAverageNormalsContext.h"

#include "dqGeom/PolyfaceVisitor.h"
#include "dqGeom/PolygonOps.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

BEGIN_DQ_GEOM_NAMESPACE

namespace {

// 1:1 IndexedAreaAndNormal — a normal vector with area (weight) and a source index.
struct IndexedAreaAndNormal {
    int32_t index = 0;
    double area = 0.0;
    Vector3d normal;

    IndexedAreaAndNormal() = default;
    IndexedAreaAndNormal(int32_t idx, double a, Vector3d n) : index(idx), area(a), normal(n) {}

    // 1:1 addWeightedNormal(weight, normal): area += weight; normal += weight*normal.
    void addWeightedNormal(double weight, const Vector3d& n)
    {
        area += weight;
        normal.x += n.x * weight;
        normal.y += n.y * weight;
        normal.z += n.z * weight;
    }

    // 1:1 divideNormalByArea: normal *= 1/area.
    void divideNormalByArea()
    {
        if (area > 0.0)
            normal.Scale(1.0 / area);
    }
};

// 1:1 SectorData — index data for one sector of a facet (facetData copy + sector/vertex indices + cluster).
struct SectorData {
    IndexedAreaAndNormal facetData;       // 1:1 facetData (by value; shared per-facet via copies)
    int32_t sectorIndex = 0;
    int32_t vertexIndex = 0;
    IndexedAreaAndNormal* clusterData = nullptr;  // 1:1 sectorClusterData (ref into clusters[])
};

}  // namespace

// 1:1 BuildAverageNormalsContext.buildFastAverageNormals.
void BuildAverageNormalsContext::BuildFastAverageNormals(IndexedPolyface& polyface, double toleranceAngleRadians)
{
    auto visitor = polyface.CreateVisitor(0);
    const Vector3d defaultNormal = Vector3d::From(0.0, 0.0, 1.0);
    const double smallArea = kSmallMetricDistanceSquared;  // reference: "I DO NOT LIKE THIS TOLERANCE"

    std::vector<SectorData> sectors;
    int32_t facetIndex = 0;
    int32_t sectorIndex = 0;

    // One IndexedAreaAndNormal per facet; each sector records (facetData, sectorIndex, clientPointIndex).
    while (visitor->MoveToNextFacet()) {
        std::vector<Point3d> facetPoints;
        const size_t pointCount = visitor->PointCount();
        facetPoints.reserve(pointCount);
        for (size_t i = 0; i < pointCount; ++i)
            facetPoints.push_back(visitor->GetPoint(i));

        Vector3d facetNormal = defaultNormal;
        double area = 0.0;
        if (const auto an = PolygonOps::areaNormalGo(facetPoints)) {
            area = an->Magnitude();
            if (area < smallArea) {
                facetNormal = defaultNormal;
                area = 0.0;
            } else {
                facetNormal = *an;
                facetNormal.Scale(1.0 / area);  // unit facet normal
            }
        }
        const IndexedAreaAndNormal facetData(facetIndex++, area, facetNormal);
        for (size_t i = 0; i < pointCount; ++i)
            sectors.push_back(SectorData{facetData, sectorIndex++,
                                         static_cast<int32_t>(visitor->ClientPointIndex(i))});
    }

    // Sort by vertex index so all sectors around each vertex are clustered.
    std::sort(sectors.begin(), sectors.end(),
              [](const SectorData& a, const SectorData& b) { return a.vertexIndex < b.vertexIndex; });

    // Walk sectors around each vertex; for each unassigned sector, accumulate near-parallel normals.
    std::vector<IndexedAreaAndNormal> clusters;
    clusters.reserve(sectors.size());  // stable addresses — clusterData pointers won't invalidate.
    double toleranceRadians = toleranceAngleRadians;
    if (toleranceRadians < 0.0001)
        toleranceRadians = 0.0001;
    int32_t clusterIndex = 0;

    for (size_t baseSectorIndex = 0; baseSectorIndex < sectors.size(); ++baseSectorIndex) {
        if (sectors[baseSectorIndex].clusterData != nullptr)
            continue;
        const int32_t vertexIndex = sectors[baseSectorIndex].vertexIndex;
        const Vector3d& baseNormal = sectors[baseSectorIndex].facetData.normal;

        clusters.emplace_back(clusterIndex++, 0.0, Vector3d::FromZero());
        IndexedAreaAndNormal* clusterNormal = &clusters.back();
        for (size_t candidateSectorIndex = baseSectorIndex; candidateSectorIndex < sectors.size();
             ++candidateSectorIndex) {
            SectorData& candidate = sectors[candidateSectorIndex];
            if (candidate.vertexIndex != vertexIndex)
                break;
            if (candidate.facetData.normal.AngleTo(baseNormal) > toleranceRadians)
                continue;
            if (candidate.clusterData == nullptr) {
                clusterNormal->addWeightedNormal(1.0, candidate.facetData.normal);
                candidate.clusterData = clusterNormal;
            }
        }
    }

    // Re-sort by original sector index.
    std::sort(sectors.begin(), sectors.end(),
              [](const SectorData& a, const SectorData& b) { return a.sectorIndex < b.sectorIndex; });

    // Normalize the sums and emplace in the polyface; record each cluster's normal index.
    PolyfaceData& data = polyface.Data();
    data.normalIndex.clear();
    data.normals.clear();
    for (auto& cluster : clusters) {
        cluster.divideNormalByArea();
        cluster.index = static_cast<int32_t>(data.normals.size());
        data.normals.push_back(cluster.normal);
    }
    // Emplace the per-sector normal indices.
    for (const auto& sector : sectors) {
        assert(sector.clusterData != nullptr);
        if (sector.clusterData != nullptr)
            data.normalIndex.push_back(sector.clusterData->index);
    }
}

// 1:1 BuildAverageNormalsContext.buildPerFaceNormals.
void BuildAverageNormalsContext::BuildPerFaceNormals(IndexedPolyface& polyface)
{
    auto visitor = polyface.CreateVisitor(0);
    const Vector3d defaultNormal = Vector3d::From(0.0, 0.0, 1.0);

    std::vector<Vector3d> newNormals;
    std::vector<int32_t> newIndices;
    while (visitor->MoveToNextFacet()) {
        const int32_t thisNormalIndex = static_cast<int32_t>(newNormals.size());
        std::vector<Point3d> fp;
        const size_t pointCount = visitor->PointCount();
        fp.reserve(pointCount);
        for (size_t i = 0; i < pointCount; ++i)
            fp.push_back(visitor->GetPoint(i));

        Vector3d facetNormal;
        if (PolygonOps::unitNormal(fp, facetNormal))
            newNormals.push_back(facetNormal);
        else
            newNormals.push_back(defaultNormal);
        for (size_t i = 0; i < pointCount; ++i)
            newIndices.push_back(thisNormalIndex);
    }
    polyface.Data().normalIndex = std::move(newIndices);
    polyface.Data().normals = std::move(newNormals);
}

END_DQ_GEOM_NAMESPACE
