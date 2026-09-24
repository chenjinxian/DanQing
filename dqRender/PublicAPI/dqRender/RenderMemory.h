// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — render-system memory statistics (type surface only).
//
// Ported from: itwinjs-core core/frontend/src/render/RenderMemory.ts
//
// Type-only port: Consumers/Buffers/Statistics structs + BufferType/ConsumerType
// enums match the itwinjs-core surface field-by-field. Helper logic is TODO
// (collectStatistics integration comes when Viewport lands).
//
// Faithful to the TS `export namespace RenderMemory { ... }` — types are nested
// (Consumers/BufferType/Buffers/ConsumerType/Statistics), not flat-prefixed (§3.5).
#pragma once

#include "Export.h"

#include <cstdint>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace RenderMemory {

// Memory consumed by a particular type of resource.
// Ported from: itwinjs-core RenderMemory.Consumers
//              (internal, but surfaced because addConsumer/addBuffer need it)
struct DQ_RENDER_EXPORT Consumers {
    uint64_t totalBytes = 0;  // total bytes consumed by all consumers
    uint64_t maxBytes = 0;    // largest bytes consumed by a single consumer
    uint32_t count = 0;       // number of consumers of this type

    // add a single consumer of `numBytes` bytes.
    // Ported from: itwinjs-core RenderMemory.Consumers.addConsumer()
    void addConsumer(uint64_t numBytes) noexcept {
        totalBytes += numBytes;
        if (numBytes > maxBytes) maxBytes = numBytes;
        ++count;
    }

    // Reset to zero.
    // Ported from: itwinjs-core RenderMemory.Consumers.clear()
    void clear() noexcept {
        totalBytes = 0;
        maxBytes = 0;
        count = 0;
    }
};

// Type of a GPU-allocated buffer tracked by RenderMemory.Buffers.
// Ported from: itwinjs-core RenderMemory.BufferType
enum class BufferType : uint8_t {
    Surfaces = 0,
    VisibleEdges,
    SilhouetteEdges,
    PolylineEdges,
    IndexedEdges,
    Polylines,
    PointStrings,
    PointClouds,
    Instances,
    Terrain,
    RealityMesh,
    COUNT,
};

// Statistics about GPU-allocated buffers, broken down by BufferType.
// Ported from: itwinjs-core RenderMemory.Buffers
struct DQ_RENDER_EXPORT Buffers : public Consumers {
    std::vector<Consumers> consumers;  // indexed by BufferType

    Buffers()
        : consumers(static_cast<size_t>(BufferType::COUNT)) {}

    // Reset all per-type consumers and aggregate.
    // Ported from: itwinjs-core RenderMemory.Buffers.clear()
    void clear() noexcept {
        for (auto& c : consumers) c.clear();
        Consumers::clear();
    }

    // add `numBytes` for the given buffer type (also bumps the aggregate).
    // Ported from: itwinjs-core RenderMemory.Buffers.addBuffer()
    void addBuffer(BufferType type, uint64_t numBytes) noexcept {
        addConsumer(numBytes);
        consumers[static_cast<size_t>(type)].addConsumer(numBytes);
    }
};

// Type of a render-system consumer tracked by RenderMemory.Statistics.
// Ported from: itwinjs-core RenderMemory.ConsumerType
enum class ConsumerType : uint8_t {
    Textures = 0,
    VertexTables,
    EdgeTables,
    FeatureTables,
    FeatureOverrides,
    ClipVolumes,
    PlanarClassifiers,
    ShadowMaps,
    TextureAttachments,
    ThematicTextures,
    COUNT,
};

// Statistics about the amount and type of memory consumed by the RenderSystem.
// Ported from: itwinjs-core RenderMemory.Statistics
struct DQ_RENDER_EXPORT Statistics {
    uint64_t totalBytes = 0;
    std::vector<Consumers> consumers;  // indexed by ConsumerType
    Buffers buffers;

    Statistics()
        : consumers(static_cast<size_t>(ConsumerType::COUNT)) {}

    // create a new, empty Statistics object.
    // Ported from: itwinjs-core RenderMemory.Statistics.create()
    static Statistics create() noexcept { return Statistics{}; }

    // Reset everything.
    // Ported from: itwinjs-core RenderMemory.Statistics.clear()
    void clear() noexcept {
        totalBytes = 0;
        buffers.clear();
        for (auto& c : consumers) c.clear();
    }

    // add `numBytes` for a buffer type (bumps totalBytes + buffers).
    // Ported from: itwinjs-core RenderMemory.Statistics.addBuffer()
    void addBuffer(BufferType type, uint64_t numBytes) noexcept {
        totalBytes += numBytes;
        buffers.addBuffer(type, numBytes);
    }

    // add `numBytes` for a consumer type (bumps totalBytes + consumers[type]).
    // Ported from: itwinjs-core RenderMemory.Statistics.addConsumer()
    void addConsumer(ConsumerType type, uint64_t numBytes) noexcept {
        totalBytes += numBytes;
        consumers[static_cast<size_t>(type)].addConsumer(numBytes);
    }

    // Convenience accessors — match RenderMemory.Statistics getters.
    // Ported from: itwinjs-core RenderMemory.Statistics.{addTexture,...,addInstances}()
    void addTexture(uint64_t n) noexcept { addConsumer(ConsumerType::Textures, n); }
    void addVertexTable(uint64_t n) noexcept { addConsumer(ConsumerType::VertexTables, n); }
    void addEdgeTable(uint64_t n) noexcept { addConsumer(ConsumerType::EdgeTables, n); }
    void addFeatureTable(uint64_t n) noexcept { addConsumer(ConsumerType::FeatureTables, n); }
    void addThematicTexture(uint64_t n) noexcept { addConsumer(ConsumerType::ThematicTextures, n); }
    void addFeatureOverrides(uint64_t n) noexcept { addConsumer(ConsumerType::FeatureOverrides, n); }
    void addContours(uint64_t n) noexcept { addConsumer(ConsumerType::FeatureOverrides, n); }
    void addClipVolume(uint64_t n) noexcept { addConsumer(ConsumerType::ClipVolumes, n); }
    void addPlanarClassifier(uint64_t n) noexcept { addConsumer(ConsumerType::PlanarClassifiers, n); }
    void addShadowMap(uint64_t n) noexcept { addConsumer(ConsumerType::ShadowMaps, n); }
    void addTextureAttachment(uint64_t n) noexcept { addConsumer(ConsumerType::TextureAttachments, n); }

    void addSurface(uint64_t n) noexcept { addBuffer(BufferType::Surfaces, n); }
    void addVisibleEdges(uint64_t n) noexcept { addBuffer(BufferType::VisibleEdges, n); }
    void addIndexedEdges(uint64_t n) noexcept { addBuffer(BufferType::IndexedEdges, n); }
    void addSilhouetteEdges(uint64_t n) noexcept { addBuffer(BufferType::SilhouetteEdges, n); }
    void addPolylineEdges(uint64_t n) noexcept { addBuffer(BufferType::PolylineEdges, n); }
    void addPolyline(uint64_t n) noexcept { addBuffer(BufferType::Polylines, n); }
    void addPointString(uint64_t n) noexcept { addBuffer(BufferType::PointStrings, n); }
    void addPointCloud(uint64_t n) noexcept { addBuffer(BufferType::PointClouds, n); }
    void addTerrain(uint64_t n) noexcept { addBuffer(BufferType::Terrain, n); }
    void addRealityMesh(uint64_t n) noexcept { addBuffer(BufferType::RealityMesh, n); }
    void addInstances(uint64_t n) noexcept { addBuffer(BufferType::Instances, n); }
};

}  // namespace RenderMemory

END_DQ_RENDER_NAMESPACE
