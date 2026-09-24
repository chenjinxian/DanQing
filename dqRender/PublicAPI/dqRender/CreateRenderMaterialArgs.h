// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — arguments for creating a RenderMaterial.
//
// Ported from: itwinjs-core core/frontend/src/render/CreateRenderMaterialArgs.ts
//              (extends MaterialParams — core/frontend/src/common/render/MaterialParams.ts)
//
// Dedicated header matching the itwinjs-core filename 1:1. The args struct was
// previously inlined in RenderMaterial.h; it is now declared here and pulled
// back into RenderMaterial.h for backward compatibility.
//
// Type-only port: CreateRenderMaterialArgs struct (extends MaterialParams in
// itwinjs — fields are inlined here). The RenderMaterialSource (internal) is
// faithfully modeled with an opaque iModel handle (§6 — no invention).
#pragma once

#include "Export.h"

#include <dqBase/DqId.h>
#include <dqCommon/RgbColor.h>

#include <cstdint>
#include <optional>
#include <string>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#endif
#ifndef END_DQ_RENDER_NAMESPACE
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Faithful placeholder for itwinjs internal RenderMaterialSource. It carries
// the iModel + element Id on which a material is cached. Marked internal in
// itwinjs; DanQing surfaces the data but defers the caching machinery.
// Ported from: itwinjs-core internal RenderMaterialSource
//
// TODO: IModelConnection not yet ported — handle is opaque (§6: no half-impl).
struct DQ_RENDER_EXPORT RenderMaterialSource {
    // The iModel on which the material is cached.
    void* iModel = nullptr;  // TODO: IModelConnection* once ported

    // The element Id used to cache the material for reuse.
    dqBase::DqId id{};
};

// Arguments for creating a RenderMaterial.
// Ported from: itwinjs-core CreateRenderMaterialArgs (extends MaterialParams)
struct DQ_RENDER_EXPORT CreateRenderMaterialArgs {
    // MaterialParams fields (itwinjs MaterialParams inlined).
    std::optional<dqCommon::RgbColorProps> diffuseColor;
    std::optional<dqCommon::RgbColorProps> specularColor;
    std::optional<double> finish;       // specular exponent [0..128]
    std::optional<double> diffuse;      // diffuse reflectivity
    std::optional<double> specular;     // specular reflectivity
    std::optional<double> reflect;      // environmental reflectivity
    std::optional<double> transmit;     // transparency
    std::optional<double> pbrNormal;    // normal map scale
    std::string key;                    // element Id for caching

    // If supplied, the material will be cached on the iModel by its element Id.
    // Ported from: itwinjs-core CreateRenderMaterialArgs.source (internal)
    std::optional<RenderMaterialSource> source;
};

END_DQ_RENDER_NAMESPACE
