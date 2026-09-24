// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/frontend/src/common/internal/render/Polyface.ts
// DanQing dqRender — PolyfacePrimitive + PolyfacePrimitiveList (output of Geometry.getPolyfaces)
//
// 保真依据：逐类型移植 Polyface.ts。PolyfacePrimitive 持 displayParams + IndexedPolyface +
// displayEdges/isPlanar。clone()（参考 _polyface.clone()）用 CloneTransformed(identity).StaticCast；
// transform() 用 TryTransformInPlace。@internal；dqRender/src/render/Polyface.h（与 dqGeom/Polyface.h
// 不同模块/命名空间/路径，无冲突）。
#pragma once

#include "DisplayParams.h"

#include <dqBase/RefCounted.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/Transform.h>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// 1:1 PolyfacePrimitive (Polyface.ts:13-34).
class PolyfacePrimitive {
public:
    DisplayParams displayParams;
    const bool displayEdges;
    const bool isPlanar;

    const dqBase::RefPtr<dqGeom::IndexedPolyface>& indexedPolyface() const noexcept { return m_polyface; }

    static PolyfacePrimitive create(const DisplayParams& params, const dqBase::RefPtr<dqGeom::IndexedPolyface>& pf,
                                    bool displayEdges = true, bool isPlanar = false) {
        return PolyfacePrimitive(params, pf, displayEdges, isPlanar);
    }

    // 1:1 PolyfacePrimitive.clone — deep-copy the wrapped polyface (CloneTransformed(identity) ≡ clone).
    PolyfacePrimitive clone() const {
        auto pf = m_polyface->CloneTransformed(dqGeom::Transform::CreateIdentity())
                      .template StaticCast<dqGeom::IndexedPolyface>();
        return PolyfacePrimitive(displayParams, pf, displayEdges, isPlanar);
    }

    // 1:1 PolyfacePrimitive.transform — transform the wrapped polyface in place.
    bool transform(const dqGeom::Transform& trans) { return m_polyface->TryTransformInPlace(trans); }

private:
    PolyfacePrimitive(const DisplayParams& params, const dqBase::RefPtr<dqGeom::IndexedPolyface>& pf,
                      bool displayEdges, bool isPlanar)
        : displayParams(params), displayEdges(displayEdges), isPlanar(isPlanar), m_polyface(pf) {}

    dqBase::RefPtr<dqGeom::IndexedPolyface> m_polyface;
};

// 1:1 PolyfacePrimitiveList (Array<PolyfacePrimitive>).
using PolyfacePrimitiveList = std::vector<PolyfacePrimitive>;

END_DQ_RENDER_NAMESPACE
