// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — View definition properties (JSON)
//
// Ported from: itwinjs-core core/common/src/ViewProps.ts
// JSON representation for view definitions, category/model selectors.
#pragma once

#include "Camera.h"
#include "Export.h"
#include "DqCommon.h"

#include <dqBase/DqId.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

BEGIN_DQ_COMMON_NAMESPACE

// Properties that define a ModelSelector.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT ModelSelectorProps {
    std::vector<dqBase::DqId> models;
};

// Properties that define a CategorySelector.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT CategorySelectorProps {
    std::vector<dqBase::DqId> categories;
};

// Parameters to construct a ViewDefinition.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT ViewDefinitionProps {
    dqBase::DqId categorySelectorId;
    dqBase::DqId displayStyleId;
    std::string description;
};

// Parameters to construct a ViewDefinition3d.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT ViewDefinition3dProps : ViewDefinitionProps {
    bool cameraOn = false;
    double originX = 0.0, originY = 0.0, originZ = 0.0;
    double extentsX = 0.0, extentsY = 0.0, extentsZ = 0.0;
    double yawDegrees = 0.0, pitchDegrees = 0.0, rollDegrees = 0.0;
    CameraProps camera;
};

// Parameters to construct a SpatialViewDefinition.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT SpatialViewDefinitionProps : ViewDefinition3dProps {
    dqBase::DqId modelSelectorId;
};

// Parameters to construct a ViewDefinition2d.
// Ported from: itwinjs-core ViewProps.ts
struct DQ_COMMON_EXPORT ViewDefinition2dProps : ViewDefinitionProps {
    dqBase::DqId baseModelId;
    double originX = 0.0, originY = 0.0;
    double deltaX = 0.0, deltaY = 0.0;
    double angleDegrees = 0.0;
};

END_DQ_COMMON_NAMESPACE
