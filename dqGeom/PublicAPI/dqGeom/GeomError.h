// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/GeomLibs/PublicAPI/Geom/GeomApi.h (error codes)
// DanQing dqGeom — 错误类型
// 遵循 dqBase DqError 模式（code + message）；可失败操作返回 Result<T, GeomError>。
#pragma once

#include "DqGeom.h"

#include <string>

BEGIN_DQ_GEOM_NAMESPACE

// 几何操作结果码
enum class GeomResult : int {
    Success = 0,
    NullInput,           // 入参空指针/空对象
    InvalidParameter,    // 参数越界/非法（负半径、退化变换等）
    DegenerateGeometry,  // 退化几何（零长、共点、法线为零）
    UnsupportedType,     // 曲线/实体类型未实现或不可用于此操作
    NumericalFailure,    // 数值发散（求根失败、矩阵奇异）
    TopologyError,       // B-Rep/图拓扑不一致
    OutOfRange,          // fraction/参数超出 [0,1] 等
};

// 几何错误（带诊断信息）
struct DQ_GEOM_EXPORT GeomError {
    GeomResult code = GeomResult::Success;
    std::string message;
};

END_DQ_GEOM_NAMESPACE
