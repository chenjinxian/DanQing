// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RenderMaterial and RenderTexture abstract types.
//
// Ported from: itwinjs-core core/common/src/RenderMaterial.ts
//              core/common/src/RenderTexture.ts
//
// The CreateRenderMaterialArgs and CreateTextureArgs structs now live in their
// own dedicated headers (CreateRenderMaterialArgs.h / CreateTextureArgs.h),
// matching the itwinjs-core filename mapping 1:1. They are pulled in below for
// backward compatibility so existing `#include "RenderMaterial.h"` keeps working.
#pragma once

#include "Export.h"

#include <dqCommon/ColorDef.h>
#include <dqCommon/Image.h>
#include <dqCommon/RgbColor.h>
#include <dqCommon/RenderMaterial.h>
#include <dqCommon/RenderTexture.h>
#include <dqCommon/TextureMapping.h>
#include <dqCommon/TextureProps.h>

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

// RenderTexture is the authoritative dqCommon::RenderTexture (RefCounted base; Type
// enum Normal/Glyph/TileSection/SkyBox/FilteredTileSection/ThematicGradient).
// itwinjs RenderTexture.ts lives in core/common/src/ → dqCommon is the faithful home.
// (Previously this header defined a divergent abstract RenderTexture with an invented
// Type enum {normal,SkyBox,TileSection,Cube,Unknown}: Cube/Unknown do not exist in the
// reference and Glyph/FilteredTileSection/ThematicGradient were missing; getType()/
// isValid() were also un-invented-from-reference §7 violations. Deleted to unify on
// dqCommon::RenderTexture per §0/§7 — mirrors F9 GeometryClass unification.)
using dqCommon::RenderTexture;

// TextureTransparency is the authoritative dqCommon::TextureTransparency
// (Opaque/Translucent/Mixed). itwinjs TextureTransparency lives in core/common/src/
// TextureProps.ts → dqCommon is the faithful home. (Previously this header defined a
// duplicate enum here, and src/render/TextureHandle.h defined yet another divergent
// copy with swapped Mixed/Translucent values — both deleted to unify on dqCommon; see
// TextureProps.h. Mirrors F9 / RenderTexture unification.)
using dqCommon::TextureTransparency;

// RenderMaterial is the authoritative dqCommon::RenderMaterial (RefCounted base).
// itwinjs RenderMaterial.ts lives in core/common/src/ → dqCommon is the faithful home.
// (Previously this header defined a divergent abstract RenderMaterial with invented
// virtual key()/hasTexture()/textureMapping() accessors — no §7 reference, and the
// dqCommon::RenderMaterial value base already provides key/textureMapping fields +
// hasTexture(). The divergent class had zero qualified usages; its only holder was
// MeshArgs::material (RenderMaterial* placeholder, always null, never dereferenced),
// which transparently resolves through this alias. Deleted per §0/§7 — mirrors F9 /
// RenderTexture unification.)
using dqCommon::RenderMaterial;

END_DQ_RENDER_NAMESPACE

// Backward compatibility — args structs are declared in their own 1:1 headers.
#include "CreateRenderMaterialArgs.h"
#include "CreateTextureArgs.h"
