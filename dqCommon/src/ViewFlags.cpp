// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ViewFlags implementation
//
// Ported from: itwinjs-core core/common/src/ViewFlags.ts
#include "dqCommon/ViewFlags.h"

BEGIN_DQ_COMMON_NAMESPACE

// Ported from: itwinjs-core ViewFlags.defaults
static const ViewFlagsProperties s_defaultProps{};
static const ViewFlags s_defaults{s_defaultProps};

const ViewFlags& ViewFlags::defaults() noexcept
{
    return s_defaults;
}

// Ported from: itwinjs-core ViewFlags.create()
ViewFlags ViewFlags::create(const ViewFlagsProperties& props)
{
    // In TS: returns defaults if all props are default/empty.
    // For simplicity, always construct (matches TS behavior when props differ).
    return ViewFlags(props);
}

// Ported from: itwinjs-core ViewFlags.copy()
ViewFlags ViewFlags::copy(const ViewFlagsProperties& changedFlags) const
{
    ViewFlagsProperties merged = m_props;
    merged.renderMode = changedFlags.renderMode;
    merged.dimensions = changedFlags.dimensions;
    merged.patterns = changedFlags.patterns;
    merged.weights = changedFlags.weights;
    merged.styles = changedFlags.styles;
    merged.transparency = changedFlags.transparency;
    merged.fill = changedFlags.fill;
    merged.textures = changedFlags.textures;
    merged.materials = changedFlags.materials;
    merged.acsTriad = changedFlags.acsTriad;
    merged.grid = changedFlags.grid;
    merged.visibleEdges = changedFlags.visibleEdges;
    merged.hiddenEdges = changedFlags.hiddenEdges;
    merged.shadows = changedFlags.shadows;
    merged.clipVolume = changedFlags.clipVolume;
    merged.constructions = changedFlags.constructions;
    merged.monochrome = changedFlags.monochrome;
    merged.backgroundMap = changedFlags.backgroundMap;
    merged.ambientOcclusion = changedFlags.ambientOcclusion;
    merged.thematicDisplay = changedFlags.thematicDisplay;
    merged.wiremesh = changedFlags.wiremesh;
    merged.forceSurfaceDiscard = changedFlags.forceSurfaceDiscard;
    merged.whiteOnWhiteReversal = changedFlags.whiteOnWhiteReversal;
    merged.lighting = changedFlags.lighting;
    return ViewFlags(merged);
}

// Ported from: itwinjs-core ViewFlags.override()
// In TS, undefined means "retain existing value". In C++ ViewFlagsProperties,
// all fields have values, so this delegates to copy. Callers should set only
// the fields they want to change on the properties struct.
ViewFlags ViewFlags::override(const ViewFlagsProperties& overrides) const
{
    return copy(overrides);
}

// Ported from: itwinjs-core ViewFlags.withRenderMode()
ViewFlags ViewFlags::withRenderMode(RenderMode mode) const noexcept
{
    if (mode == m_props.renderMode)
        return *this;
    ViewFlagsProperties props = m_props;
    props.renderMode = mode;
    return ViewFlags(props);
}

// Ported from: itwinjs-core edgesRequired() helper
static bool edgesRequired(RenderMode renderMode, bool visibleEdges)
{
    return visibleEdges || RenderMode::SmoothShade != renderMode;
}

// Ported from: itwinjs-core ViewFlags.normalize()
ViewFlags ViewFlags::normalize() const
{
    switch (m_props.renderMode) {
    case RenderMode::Wireframe:
        if (m_props.visibleEdges || m_props.hiddenEdges) {
            ViewFlagsProperties props = m_props;
            props.visibleEdges = false;
            props.hiddenEdges = false;
            return ViewFlags(props);
        }
        break;
    case RenderMode::SmoothShade:
        if (!m_props.visibleEdges) {
            ViewFlagsProperties props = m_props;
            props.hiddenEdges = false;
            return ViewFlags(props);
        }
        break;
    case RenderMode::HiddenLine:
    case RenderMode::SolidFill:
        if (!m_props.visibleEdges || m_props.transparency) {
            ViewFlagsProperties props = m_props;
            props.visibleEdges = true;
            props.transparency = false;
            return ViewFlags(props);
        }
        break;
    }
    return *this;
}

// Ported from: itwinjs-core ViewFlags.hiddenEdgesVisible()
bool ViewFlags::hiddenEdgesVisible() const noexcept
{
    switch (m_props.renderMode) {
    case RenderMode::SolidFill:
    case RenderMode::HiddenLine:
        return m_props.hiddenEdges;
    case RenderMode::SmoothShade:
        return m_props.visibleEdges && m_props.hiddenEdges;
    default:  // Wireframe
        return true;
    }
}

// Ported from: itwinjs-core ViewFlags.edgesRequired()
bool ViewFlags::edgesRequired() const noexcept
{
    return ::dqCommon::edgesRequired(m_props.renderMode, m_props.visibleEdges);
}

// Ported from: itwinjs-core ViewFlags.equals()
bool ViewFlags::equals(const ViewFlags& other) const noexcept
{
    return equals(other.m_props);
}

bool ViewFlags::equals(const ViewFlagsProperties& o) const noexcept
{
    return m_props.renderMode == o.renderMode && m_props.dimensions == o.dimensions &&
           m_props.patterns == o.patterns && m_props.weights == o.weights && m_props.styles == o.styles &&
           m_props.transparency == o.transparency && m_props.fill == o.fill && m_props.textures == o.textures &&
           m_props.materials == o.materials && m_props.acsTriad == o.acsTriad && m_props.grid == o.grid &&
           m_props.visibleEdges == o.visibleEdges && m_props.hiddenEdges == o.hiddenEdges &&
           m_props.lighting == o.lighting && m_props.shadows == o.shadows && m_props.clipVolume == o.clipVolume &&
           m_props.constructions == o.constructions && m_props.monochrome == o.monochrome &&
           m_props.backgroundMap == o.backgroundMap && m_props.ambientOcclusion == o.ambientOcclusion &&
           m_props.thematicDisplay == o.thematicDisplay && m_props.wiremesh == o.wiremesh &&
           m_props.forceSurfaceDiscard == o.forceSurfaceDiscard &&
           m_props.whiteOnWhiteReversal == o.whiteOnWhiteReversal;
}

// Ported from: itwinjs-core ViewFlags.fromJSON()
ViewFlags ViewFlags::fromJSON(const ViewFlagProps* json)
{
    if (json == nullptr)
        return defaults();

    auto asBool = [](const std::optional<bool>& v) -> bool { return v.value_or(false); };
    auto asInt = [](const std::optional<RenderMode>& v) -> int {
        return v.has_value() ? static_cast<int>(v.value()) : 0;
    };

    const int renderModeValue = asInt(json->renderMode);
    RenderMode renderMode;
    if (renderModeValue < static_cast<int>(RenderMode::HiddenLine))
        renderMode = RenderMode::Wireframe;
    else if (renderModeValue > static_cast<int>(RenderMode::SolidFill))
        renderMode = RenderMode::SmoothShade;
    else
        renderMode = static_cast<RenderMode>(renderModeValue);

    // Ported from: itwinjs-core lighting = !noCameraLights || !noSourceLights || !noSolarLight
    const bool lighting = !asBool(json->noCameraLights) || !asBool(json->noSourceLights) ||
                          !asBool(json->noSolarLight);

    ViewFlagsProperties props;
    props.renderMode = renderMode;
    props.lighting = lighting;
    props.constructions = !asBool(json->noConstruct);
    props.dimensions = !asBool(json->noDim);
    props.patterns = !asBool(json->noPattern);
    props.weights = !asBool(json->noWeight);
    props.styles = !asBool(json->noStyle);
    props.transparency = !asBool(json->noTransp);
    props.fill = !asBool(json->noFill);
    props.grid = asBool(json->grid);
    props.acsTriad = asBool(json->acs);
    props.textures = !asBool(json->noTexture);
    props.materials = !asBool(json->noMaterial);
    props.visibleEdges = asBool(json->visEdges);
    props.hiddenEdges = asBool(json->hidEdges);
    props.shadows = asBool(json->shadows);
    props.clipVolume = asBool(json->clipVol);
    props.monochrome = asBool(json->monochrome);
    props.backgroundMap = asBool(json->backgroundMap);
    props.ambientOcclusion = asBool(json->ambientOcclusion);
    props.thematicDisplay = asBool(json->thematicDisplay);
    props.wiremesh = asBool(json->wiremesh);
    props.forceSurfaceDiscard = asBool(json->forceSurfaceDiscard);
    props.whiteOnWhiteReversal = !asBool(json->noWhiteOnWhiteReversal);

    return ViewFlags(props);
}

// Ported from: itwinjs-core ViewFlags.toJSON()
ViewFlagProps ViewFlags::toJSON() const
{
    ViewFlagProps out;
    if (!m_props.constructions)
        out.noConstruct = true;
    if (!m_props.dimensions)
        out.noDim = true;
    if (!m_props.patterns)
        out.noPattern = true;
    if (!m_props.weights)
        out.noWeight = true;
    if (!m_props.styles)
        out.noStyle = true;
    if (!m_props.transparency)
        out.noTransp = true;
    if (!m_props.fill)
        out.noFill = true;
    if (m_props.grid)
        out.grid = true;
    if (m_props.acsTriad)
        out.acs = true;
    if (!m_props.textures)
        out.noTexture = true;
    if (!m_props.materials)
        out.noMaterial = true;
    if (!m_props.lighting) {
        out.noCameraLights = true;
        out.noSourceLights = true;
        out.noSolarLight = true;
    }
    if (m_props.visibleEdges)
        out.visEdges = true;
    if (m_props.hiddenEdges)
        out.hidEdges = true;
    if (m_props.shadows)
        out.shadows = true;
    if (m_props.clipVolume)
        out.clipVol = true;
    if (m_props.monochrome)
        out.monochrome = true;
    if (m_props.backgroundMap)
        out.backgroundMap = true;
    if (m_props.ambientOcclusion)
        out.ambientOcclusion = true;
    if (m_props.thematicDisplay)
        out.thematicDisplay = true;
    if (m_props.wiremesh)
        out.wiremesh = true;
    if (m_props.forceSurfaceDiscard)
        out.forceSurfaceDiscard = true;
    if (!m_props.whiteOnWhiteReversal)
        out.noWhiteOnWhiteReversal = true;
    out.renderMode = m_props.renderMode;
    return out;
}

// Ported from: itwinjs-core ViewFlags.toFullyDefinedJSON()
ViewFlagProps ViewFlags::toFullyDefinedJSON() const
{
    ViewFlagProps out;
    out.renderMode = m_props.renderMode;
    out.noConstruct = !m_props.constructions;
    out.noDim = !m_props.dimensions;
    out.noPattern = !m_props.patterns;
    out.noWeight = !m_props.weights;
    out.noStyle = !m_props.styles;
    out.noTransp = !m_props.transparency;
    out.noFill = !m_props.fill;
    out.grid = m_props.grid;
    out.acs = m_props.acsTriad;
    out.noTexture = !m_props.textures;
    out.noMaterial = !m_props.materials;
    out.noCameraLights = !m_props.lighting;
    out.noSourceLights = !m_props.lighting;
    out.noSolarLight = !m_props.lighting;
    out.visEdges = m_props.visibleEdges;
    out.hidEdges = m_props.hiddenEdges;
    out.shadows = m_props.shadows;
    out.clipVol = m_props.clipVolume;
    out.monochrome = m_props.monochrome;
    out.backgroundMap = m_props.backgroundMap;
    out.ambientOcclusion = m_props.ambientOcclusion;
    out.thematicDisplay = m_props.thematicDisplay;
    out.wiremesh = m_props.wiremesh;
    out.forceSurfaceDiscard = m_props.forceSurfaceDiscard;
    out.noWhiteOnWhiteReversal = !m_props.whiteOnWhiteReversal;
    return out;
}

END_DQ_COMMON_NAMESPACE
