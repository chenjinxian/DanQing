// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — View flags (immutable)
//
// Ported from: itwinjs-core core/common/src/ViewFlags.ts
// Flags controlling how graphics appear within a view.
#pragma once

#include "Export.h"
#include "RenderMode.h"
#include "DqCommon.h"

#include <cstdint>
#include <optional>

BEGIN_DQ_COMMON_NAMESPACE

// Forward declarations for ViewFlagProps (JSON representation)
struct ViewFlagProps;

// Properties struct — all fields of ViewFlags without methods.
// Ported from: itwinjs-core ViewFlagsProperties
struct ViewFlagsProperties {
    RenderMode renderMode = RenderMode::Wireframe;
    bool dimensions = true;
    bool patterns = true;
    bool weights = true;
    bool styles = true;
    bool transparency = true;
    bool fill = true;
    bool textures = true;
    bool materials = true;
    bool acsTriad = false;
    bool grid = false;
    bool visibleEdges = false;
    bool hiddenEdges = false;
    bool shadows = false;
    bool clipVolume = true;
    bool constructions = false;
    bool monochrome = false;
    bool backgroundMap = false;
    bool ambientOcclusion = false;
    bool thematicDisplay = false;
    bool wiremesh = false;
    bool forceSurfaceDiscard = false;
    bool whiteOnWhiteReversal = true;
    bool lighting = false;
};

// JSON representation of ViewFlags (persistence format).
// Ported from: itwinjs-core ViewFlagProps
struct DQ_COMMON_EXPORT ViewFlagProps {
    std::optional<bool> noConstruct;
    std::optional<bool> noDim;
    std::optional<bool> noPattern;
    std::optional<bool> noWeight;
    std::optional<bool> noStyle;
    std::optional<bool> noTransp;
    std::optional<bool> noFill;
    std::optional<bool> grid;
    std::optional<bool> acs;
    std::optional<bool> noTexture;
    std::optional<bool> noMaterial;
    std::optional<bool> noCameraLights;
    std::optional<bool> noSourceLights;
    std::optional<bool> noSolarLight;
    std::optional<bool> visEdges;
    std::optional<bool> hidEdges;
    std::optional<bool> shadows;
    std::optional<bool> clipVol;
    std::optional<bool> monochrome;
    std::optional<bool> backgroundMap;
    std::optional<bool> ambientOcclusion;
    std::optional<bool> thematicDisplay;
    std::optional<bool> wiremesh;
    std::optional<bool> forceSurfaceDiscard;
    std::optional<bool> noWhiteOnWhiteReversal;
    std::optional<RenderMode> renderMode;
};

// Flags controlling how graphics appear within a view.
// Immutable — use copy/override/With to produce modified copies.
// Ported from: itwinjs-core core/common/src/ViewFlags.ts
class DQ_COMMON_EXPORT ViewFlags {
public:
    // Properties — ported from: itwinjs-core ViewFlags fields
    RenderMode renderMode() const noexcept { return m_props.renderMode; }
    bool dimensions() const noexcept { return m_props.dimensions; }
    bool patterns() const noexcept { return m_props.patterns; }
    bool weights() const noexcept { return m_props.weights; }
    bool styles() const noexcept { return m_props.styles; }
    bool transparency() const noexcept { return m_props.transparency; }
    bool fill() const noexcept { return m_props.fill; }
    bool textures() const noexcept { return m_props.textures; }
    bool materials() const noexcept { return m_props.materials; }
    bool acsTriad() const noexcept { return m_props.acsTriad; }
    bool grid() const noexcept { return m_props.grid; }
    bool visibleEdges() const noexcept { return m_props.visibleEdges; }
    bool hiddenEdges() const noexcept { return m_props.hiddenEdges; }
    bool shadows() const noexcept { return m_props.shadows; }
    bool clipVolume() const noexcept { return m_props.clipVolume; }
    bool constructions() const noexcept { return m_props.constructions; }
    bool monochrome() const noexcept { return m_props.monochrome; }
    bool backgroundMap() const noexcept { return m_props.backgroundMap; }
    bool ambientOcclusion() const noexcept { return m_props.ambientOcclusion; }
    bool thematicDisplay() const noexcept { return m_props.thematicDisplay; }
    bool wiremesh() const noexcept { return m_props.wiremesh; }
    bool forceSurfaceDiscard() const noexcept { return m_props.forceSurfaceDiscard; }
    bool whiteOnWhiteReversal() const noexcept { return m_props.whiteOnWhiteReversal; }
    bool lighting() const noexcept { return m_props.lighting; }

    // Constructor — ported from: itwinjs-core ViewFlags constructor
    explicit ViewFlags(const ViewFlagsProperties& props = ViewFlagsProperties{}) noexcept
        : m_props(props)
    {
    }

    // copy with changed properties — ported from: itwinjs-core ViewFlags.copy()
    ViewFlags copy(const ViewFlagsProperties& changedFlags) const;

    // override properties (undefined = retain) — ported from: itwinjs-core ViewFlags.override()
    ViewFlags override(const ViewFlagsProperties& overrides) const;

    // copy with single boolean changed — ported from: itwinjs-core ViewFlags.with()
    // Note: uses a string key approach; for C++ we provide typed setters instead.
    ViewFlags withRenderMode(RenderMode mode) const noexcept;

    // Normalize based on render mode — ported from: itwinjs-core ViewFlags.normalize()
    ViewFlags normalize() const;

    // Returns true if hidden edges are visible for current render mode.
    // Ported from: itwinjs-core ViewFlags.hiddenEdgesVisible()
    bool hiddenEdgesVisible() const noexcept;

    // Returns true if edges of surfaces should be displayed.
    // Ported from: itwinjs-core ViewFlags.edgesRequired()
    bool edgesRequired() const noexcept;

    // Equality — ported from: itwinjs-core ViewFlags.equals()
    bool equals(const ViewFlags& other) const noexcept;
    bool equals(const ViewFlagsProperties& other) const noexcept;

    // Static defaults — ported from: itwinjs-core ViewFlags.defaults
    static const ViewFlags& defaults() noexcept;

    // create from properties — ported from: itwinjs-core ViewFlags.create()
    static ViewFlags create(const ViewFlagsProperties& props = ViewFlagsProperties{});

    // create from JSON — ported from: itwinjs-core ViewFlags.fromJSON()
    static ViewFlags fromJSON(const ViewFlagProps* json);

    // Convert to JSON — ported from: itwinjs-core ViewFlags.toJSON()
    ViewFlagProps toJSON() const;

    // Convert to fully defined JSON — ported from: itwinjs-core ViewFlags.toFullyDefinedJSON()
    ViewFlagProps toFullyDefinedJSON() const;

    // Access underlying properties
    const ViewFlagsProperties& Properties() const noexcept { return m_props; }

private:
    ViewFlagsProperties m_props;
};

END_DQ_COMMON_NAMESPACE
