// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — IModelJson implementation (indexedMesh+auxData 子集)
// Ported from: itwinjs-core core/geometry/src/serialization/IModelJsonSchema.ts
//              itwinjs-core core/geometry/src/serialization/SerializationHelpers.ts
#include "dqGeom/IModelJson.h"

#include <cmath>

BEGIN_DQ_GEOM_NAMESPACE

namespace {

using imodeljson::JsonValue;

// JSON 属性谓词（hasOwnProperty + 类型门——参考 parseBooleanProperty :726-733 /
// parseNumberProperty :666-673 / Geometry.isNumberArray 的 C++ 承载）。
JsonValue const* findProp(JsonValue const& json, char const* key)
{
    return json.type == JsonValue::Type::Object ? json.find(key) : nullptr;
}

bool isNumberArray(JsonValue const& v, size_t n)
{
    if (v.type != JsonValue::Type::Array || v.arr.size() != n)
        return false;
    for (auto const& e : v.arr)
        if (e.type != JsonValue::Type::Number)
            return false;
    return true;
}

// JSON int 数组抽取（announce* 的源——参考签名收 Int32Array；非数值元素在参考会
// 静默成 NaN→abs-1，这里按 DanQing 严格化：非 Number 元素 → false（EQUIVALENCE：
// 发散=畸形输入拒绝而非静默；消费资产全数值，验证=W-a2 锁全绿）。
bool readInt32Array(JsonValue const& v, std::vector<int32_t>& out)
{
    if (v.type != JsonValue::Type::Array)
        return false;
    out.clear();
    out.reserve(v.arr.size());
    for (auto const& e : v.arr) {
        if (e.type != JsonValue::Type::Number)
            return false;
        out.push_back(static_cast<int32_t>(e.number));
    }
    return true;
}

// validateAllIndices（Polyface.ts:509-541 子集——DanQing 1-based 有符号约定：
// abs(idx) ∈ [1, dataLength]；edgeMateIndex 面未移植）。
bool validateAllIndices(IndexedPolyface const& pf)
{
    auto const& data = pf.Data();
    size_t const numPointIndices = data.pointIndex.size();
    if (0 == numPointIndices)
        return false;
    if (!data.normalIndex.empty() && data.normalIndex.size() != numPointIndices)
        return false;
    if (!data.paramIndex.empty() && data.paramIndex.size() != numPointIndices)
        return false;
    if (!data.colorIndex.empty() && data.colorIndex.size() != numPointIndices)
        return false;
    if (data.edgeVisible.size() != numPointIndices)
        return false;
    auto valid = [](std::vector<int32_t> const& indices, size_t dataLength) {
        for (int32_t const idx : indices) {
            int64_t const a = idx < 0 ? -static_cast<int64_t>(idx) : static_cast<int64_t>(idx);
            if (a < 1 || static_cast<size_t>(a) > dataLength)
                return false;
        }
        return true;
    };
    if (!valid(data.pointIndex, data.points.size()))
        return false;
    if (!data.normalIndex.empty() && !valid(data.normalIndex, data.normals.size()))
        return false;
    if (!data.paramIndex.empty() && !valid(data.paramIndex, data.params.size()))
        return false;
    if (!data.colorIndex.empty() && !valid(data.colorIndex, data.colors.size()))
        return false;
    return true;
}

}  // namespace

namespace SerializationHelpers {

// Ported from: SerializationHelpers.ts:292-326。
void announceZeroBasedIndicesFromSignedOneBasedIndices(
    std::vector<int32_t> const& sourceIndices, int numPerBlock,
    AnnounceZeroBasedIndex const& announceZeroBasedIndex,
    std::function<void()> const& terminateBlock)
{
    size_t numIndices = sourceIndices.size();
    if (0 == numIndices)
        return;
    if (numPerBlock > 1) {
        numIndices -= sourceIndices.size() % static_cast<size_t>(numPerBlock);
        for (size_t i = 0; i < numIndices; i++) {
            int32_t const p = sourceIndices[i];
            if (p != 0)  // skip padding
                announceZeroBasedIndex(std::abs(p) - 1, p > 0);
            if (terminateBlock && ((i + 1) % static_cast<size_t>(numPerBlock)) == 0)
                terminateBlock();
        }
    } else {
        for (size_t i = 0; i < numIndices; i++) {
            int32_t const p = sourceIndices[i];
            if (p != 0)  // skip terminator
                announceZeroBasedIndex(std::abs(p) - 1, p > 0);
            if (terminateBlock) {
                if (p == 0) {
                    if (i + 1 == numIndices || sourceIndices[i + 1] != 0)  // skip extra terminators
                        terminateBlock();
                } else {
                    if (i + 1 == numIndices)  // missing last terminator
                        terminateBlock();
                }
            }
        }
    }
}

}  // namespace SerializationHelpers

namespace IModelJson {

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

// Ported from: IModelJsonSchema.ts:1274-1328 parse —— 子集分派（E3）：
// 数组 → parseArray 不属本函数签名（单列见 parseArray）；indexedMesh → parseIndexedMesh；
// 其余键 TODO（参考分派表 :1276-1327）。
dqBase::RefPtr<GeometryQuery> Reader::parse(JsonValue const& json)
{
    if (json.type == JsonValue::Type::Object) {
        if (findProp(json, "indexedMesh") != nullptr)
            return parseIndexedMesh(*findProp(json, "indexedMesh"));
        // TODO: 其余几何种类（lineSegment/lineString/arc/point/bcurve/path/loop/
        // parityRegion/unionRegion/bagOfCurves/bsurf/cone/cylinder/sphere/linearSweep/
        // box/rotationalSweep/ruledSweep/torusPipe/pointString/transitionSpiral）——
        // 参考 IModelJsonSchema.ts:1276-1327，消费到时补齐（E3）。
    }
    return nullptr;
}

// Ported from: IModelJsonSchema.ts:940-952 parseArray。
std::vector<dqBase::RefPtr<GeometryQuery>> Reader::parseArray(JsonValue const& json)
{
    std::vector<dqBase::RefPtr<GeometryQuery>> myArray;
    if (json.type == JsonValue::Type::Array) {
        for (auto const& c : json.arr) {
            auto g = parse(c);
            if (g.IsValid())
                myArray.push_back(g);
        }
    }
    return myArray;
}

// Ported from: IModelJsonSchema.ts:979-1051 parseIndexedMesh。
dqBase::RefPtr<IndexedPolyface> Reader::parseIndexedMesh(JsonValue const& data)
{
    if (data.type != JsonValue::Type::Object)
        return nullptr;
    JsonValue const* pointProp = findProp(data, "point");
    JsonValue const* pointIndexProp = findProp(data, "pointIndex");
    if (pointProp == nullptr || pointProp->type != JsonValue::Type::Array
        || pointIndexProp == nullptr || pointIndexProp->type != JsonValue::Type::Array)
        return nullptr;

    auto polyface = IndexedPolyface::create();
    int const numPerFace = [&data]() {
        JsonValue const* v = findProp(data, "numPerFace");
        return (v != nullptr && v->type == JsonValue::Type::Number) ? static_cast<int>(v->number) : 0;
    }();

    // default value is true!!（:988 parseBooleanProperty(data, "twoSided", true) ?? true）
    {
        JsonValue const* v = findProp(data, "twoSided");
        bool const twoSided = (v == nullptr || v->type != JsonValue::Type::Bool) ? true : v->boolean;
        polyface->SetTwoSided(twoSided);
    }
    if (JsonValue const* q = findProp(data, "expectedClosure")) {
        if (q->type == JsonValue::Type::Number && std::isfinite(q->number))
            polyface->SetExpectedClosure(static_cast<uint32_t>(q->number));
    }

    JsonValue const* normalProp = findProp(data, "normal");
    JsonValue const* normalIndexProp = findProp(data, "normalIndex");
    if (normalProp != nullptr && normalProp->type == JsonValue::Type::Array
        && normalIndexProp != nullptr) {
        // 参考 :997-1006：addNormalXYZ 恒新增（不用 addNormal 的去重热心）——DanQing
        // AddNormal 本就恒新增，同构。
        for (auto const& uvw : normalProp->arr) {
            if (isNumberArray(uvw, 3))
                polyface->AddNormal(Vector3d::From(uvw.arr[0].number, uvw.arr[1].number,
                                                   uvw.arr[2].number));
        }
        std::vector<int32_t> normalIndex;
        if (readInt32Array(*normalIndexProp, normalIndex)) {
            SerializationHelpers::announceZeroBasedIndicesFromSignedOneBasedIndices(
                normalIndex, numPerFace,
                [&polyface](int32_t x, bool) { polyface->AddNormalIndex(x + 1); });
        }
    }
    JsonValue const* paramProp = findProp(data, "param");
    JsonValue const* paramIndexProp = findProp(data, "paramIndex");
    if (paramProp != nullptr && paramProp->type == JsonValue::Type::Array
        && paramIndexProp != nullptr) {
        for (auto const& uv : paramProp->arr) {
            if (isNumberArray(uv, 2))
                polyface->AddParam(Point2d::From(uv.arr[0].number, uv.arr[1].number));
        }
        std::vector<int32_t> paramIndex;
        if (readInt32Array(*paramIndexProp, paramIndex)) {
            SerializationHelpers::announceZeroBasedIndicesFromSignedOneBasedIndices(
                paramIndex, numPerFace,
                [&polyface](int32_t x, bool) { polyface->AddParamIndex(x + 1); });
        }
    }
    JsonValue const* colorProp = findProp(data, "color");
    JsonValue const* colorIndexProp = findProp(data, "colorIndex");
    if (colorProp != nullptr && colorProp->type == JsonValue::Type::Array
        && colorIndexProp != nullptr) {
        for (auto const& c : colorProp->arr) {
            if (c.type == JsonValue::Type::Number)
                polyface->AddColor(static_cast<uint32_t>(c.number));
        }
        std::vector<int32_t> colorIndex;
        if (readInt32Array(*colorIndexProp, colorIndex)) {
            SerializationHelpers::announceZeroBasedIndicesFromSignedOneBasedIndices(
                colorIndex, numPerFace,
                [&polyface](int32_t x, bool) { polyface->AddColorIndex(x + 1); });
        }
    }

    for (auto const& p : pointProp->arr) {
        if (isNumberArray(p, 3))
            polyface->AddPoint(Point3d::From(p.arr[0].number, p.arr[1].number, p.arr[2].number));
    }
    std::vector<int32_t> pointIndex;
    if (readInt32Array(*pointIndexProp, pointIndex)) {
        SerializationHelpers::announceZeroBasedIndicesFromSignedOneBasedIndices(
            pointIndex, numPerFace,
            [&polyface](int32_t i, bool v) { polyface->AddPointIndex(i + 1, v); },
            [&polyface]() { polyface->TerminateFacet(); });
    }

    if (!validateAllIndices(*polyface))
        return nullptr;

    if (JsonValue const* auxDataProp = findProp(data, "auxData")) {
        auto auxData = parsePolyfaceAuxData(*auxDataProp, numPerFace);
        if (auxData.has_value())
            polyface->Data().auxData = std::move(*auxData);
    }

    // NOTE: faceData is ignored（参考 :1047）；edgeMateIndex/taggedNumericData 未移植（E3）。
    return polyface;
}

// Ported from: IModelJsonSchema.ts:955-976 parsePolyfaceAuxData。
std::optional<PolyfaceAuxData> Reader::parsePolyfaceAuxData(JsonValue const& data, int numPerFace)
{
    JsonValue const* channelsProp = findProp(data, "channels");
    JsonValue const* indicesProp = findProp(data, "indices");
    if (channelsProp == nullptr || channelsProp->type != JsonValue::Type::Array
        || indicesProp == nullptr || indicesProp->type != JsonValue::Type::Array)
        return std::nullopt;

    std::vector<AuxChannel> outChannels;
    for (auto const& inChannel : channelsProp->arr) {
        JsonValue const* dataProp = findProp(inChannel, "data");
        JsonValue const* dataTypeProp = findProp(inChannel, "dataType");
        if (dataProp != nullptr && dataProp->type == JsonValue::Type::Array
            && dataTypeProp != nullptr) {
            std::vector<AuxChannelData> outChannelData;
            for (auto const& inChannelData : dataProp->arr) {
                JsonValue const* inputProp = findProp(inChannelData, "input");
                JsonValue const* valuesProp = findProp(inChannelData, "values");
                if (inputProp != nullptr && inputProp->type == JsonValue::Type::Number
                    && valuesProp != nullptr && valuesProp->type == JsonValue::Type::Array) {
                    std::vector<double> values;
                    values.reserve(valuesProp->arr.size());
                    for (auto const& v : valuesProp->arr) {
                        if (v.type != JsonValue::Type::Number)
                            return std::nullopt;  // 严格化：非数值 → 拒绝（同 readInt32Array 注）
                        values.push_back(v.number);
                    }
                    outChannelData.emplace_back(inputProp->number, std::move(values));
                }
            }
            std::optional<std::string> name;
            if (JsonValue const* v = findProp(inChannel, "name"))
                if (v->type == JsonValue::Type::String)
                    name = v->str;
            std::optional<std::string> inputName;
            if (JsonValue const* v = findProp(inChannel, "inputName"))
                if (v->type == JsonValue::Type::String)
                    inputName = v->str;
            outChannels.emplace_back(std::move(outChannelData),
                                     static_cast<AuxChannelDataType>(dataTypeProp->number),
                                     std::move(name), std::move(inputName));
        }
    }

    PolyfaceAuxData auxData(std::move(outChannels), {});
    std::vector<int32_t> srcIndices;
    if (readInt32Array(*indicesProp, srcIndices)) {
        SerializationHelpers::announceZeroBasedIndicesFromSignedOneBasedIndices(
            srcIndices, numPerFace,
            [&auxData](int32_t x, bool flag) {
                // DanQing auxData.indices 约定=signed 1-based（AuxData.h 文件头 EQUIVALENCE）。
                auxData.indices.push_back(flag ? x + 1 : -(x + 1));
            });
    }

    return auxData;
}

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

// Ported from: IModelJsonSchema.ts:1991-2001 emit。
std::optional<JsonValue> Writer::emit(GeometryQuery const& data)
{
    if (data.Category() == GeometryCategory::Polyface)
        return handleIndexedPolyface(static_cast<IndexedPolyface const&>(data));
    // 其余 GeometryQuery 种类不移植（E3——参考经 dispatchToGeometryHandler 分派）。
    return std::nullopt;
}

// Ported from: IModelJsonSchema.ts:1731-1750 handlePolyfaceAuxData。
JsonValue Writer::handlePolyfaceAuxData(PolyfaceAuxData const& auxData, IndexedPolyface const& pf)
{
    JsonValue contents;
    contents.type = JsonValue::Type::Object;

    JsonValue indices;
    indices.type = JsonValue::Type::Array;
    // 参考 :1734-1740 经 visitor 逐 facet 摘 auxData.indices[i]+1 并补 0 终止符——
    // DanQing 的 auxData.indices 已是 signed 1-based（直写；facet 边界由 facetStart 切）。
    auto const& srcIndices = auxData.indices;
    for (size_t k = 0; k < pf.FacetCount(); k++) {
        size_t const i0 = pf.FacetIndex0(k);
        size_t const i1 = pf.FacetIndex1(k);
        for (size_t i = i0; i < i1 && i < srcIndices.size(); i++) {
            JsonValue v;
            v.type = JsonValue::Type::Number;
            v.number = static_cast<double>(srcIndices[i]);
            indices.arr.push_back(std::move(v));
        }
        JsonValue term;
        term.type = JsonValue::Type::Number;
        term.number = 0.0;
        indices.arr.push_back(std::move(term));  // facet terminator.
    }
    contents.obj.emplace_back("indices", std::move(indices));

    JsonValue channels;
    channels.type = JsonValue::Type::Array;
    for (auto const& inChannel : auxData.channels) {
        JsonValue outChannel;
        outChannel.type = JsonValue::Type::Object;
        JsonValue dataArr;
        dataArr.type = JsonValue::Type::Array;
        for (auto const& inData : inChannel.data) {
            JsonValue outData;
            outData.type = JsonValue::Type::Object;
            JsonValue input;
            input.type = JsonValue::Type::Number;
            input.number = inData.input;
            outData.obj.emplace_back("input", std::move(input));
            JsonValue values;
            values.type = JsonValue::Type::Array;
            for (double const d : inData.values) {
                JsonValue v;
                v.type = JsonValue::Type::Number;
                v.number = d;
                values.arr.push_back(std::move(v));
            }
            outData.obj.emplace_back("values", std::move(values));
            dataArr.arr.push_back(std::move(outData));
        }
        outChannel.obj.emplace_back("data", std::move(dataArr));
        JsonValue dataType;
        dataType.type = JsonValue::Type::Number;
        dataType.number = static_cast<double>(inChannel.dataType);
        outChannel.obj.emplace_back("dataType", std::move(dataType));
        if (inChannel.name.has_value()) {
            JsonValue name;
            name.type = JsonValue::Type::String;
            name.str = *inChannel.name;
            outChannel.obj.emplace_back("name", std::move(name));
        }
        if (inChannel.inputName.has_value()) {
            JsonValue inputName;
            inputName.type = JsonValue::Type::String;
            inputName.str = *inChannel.inputName;
            outChannel.obj.emplace_back("inputName", std::move(inputName));
        }
        channels.arr.push_back(std::move(outChannel));
    }
    contents.obj.emplace_back("channels", std::move(channels));
    return contents;
}

// Ported from: IModelJsonSchema.ts:1753-1868 handleIndexedPolyface。
JsonValue Writer::handleIndexedPolyface(IndexedPolyface const& pf)
{
    auto const& data = pf.Data();

    auto makeNumber = [](double d) {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        v.number = d;
        return v;
    };

    JsonValue contents;
    contents.type = JsonValue::Type::Object;

    // expectedClosure（:1834-1835——非 0 才写）。
    if (pf.ExpectedClosure() != 0)
        contents.obj.emplace_back("expectedClosure", makeNumber(static_cast<double>(pf.ExpectedClosure())));

    // twoSided（:1837——恒写）。
    {
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        v.boolean = pf.TwoSided();
        contents.obj.emplace_back("twoSided", std::move(v));
    }

    // auxData（:1839-1840）。
    if (data.auxData.has_value())
        contents.obj.emplace_back("auxData", handlePolyfaceAuxData(*data.auxData, pf));

    // color / colorIndex（:1842-1845）。
    if (!data.colors.empty()) {
        JsonValue colors;
        colors.type = JsonValue::Type::Array;
        for (uint32_t const value : data.colors)
            colors.arr.push_back(makeNumber(static_cast<double>(value)));
        contents.obj.emplace_back("color", std::move(colors));

        JsonValue colorIndex;
        colorIndex.type = JsonValue::Type::Array;
        for (size_t k = 0; k < pf.FacetCount(); k++) {
            for (size_t i = pf.FacetIndex0(k); i < pf.FacetIndex1(k) && i < data.colorIndex.size(); i++)
                colorIndex.arr.push_back(makeNumber(static_cast<double>(data.colorIndex[i])));
            colorIndex.arr.push_back(makeNumber(0.0));
        }
        contents.obj.emplace_back("colorIndex", std::move(colorIndex));
    }

    // normal / normalIndex（:1847-1850）。
    if (!data.normals.empty()) {
        JsonValue normals;
        normals.type = JsonValue::Type::Array;
        for (auto const& n : data.normals) {
            JsonValue xyz;
            xyz.type = JsonValue::Type::Array;
            xyz.arr.push_back(makeNumber(n.x));
            xyz.arr.push_back(makeNumber(n.y));
            xyz.arr.push_back(makeNumber(n.z));
            normals.arr.push_back(std::move(xyz));
        }
        contents.obj.emplace_back("normal", std::move(normals));

        JsonValue normalIndex;
        normalIndex.type = JsonValue::Type::Array;
        for (size_t k = 0; k < pf.FacetCount(); k++) {
            for (size_t i = pf.FacetIndex0(k); i < pf.FacetIndex1(k) && i < data.normalIndex.size(); i++)
                normalIndex.arr.push_back(makeNumber(static_cast<double>(data.normalIndex[i])));
            normalIndex.arr.push_back(makeNumber(0.0));
        }
        contents.obj.emplace_back("normalIndex", std::move(normalIndex));
    }

    // param / paramIndex（:1852-1855）。
    if (!data.params.empty()) {
        JsonValue params;
        params.type = JsonValue::Type::Array;
        for (auto const& uv : data.params) {
            JsonValue xy;
            xy.type = JsonValue::Type::Array;
            xy.arr.push_back(makeNumber(uv.x));
            xy.arr.push_back(makeNumber(uv.y));
            params.arr.push_back(std::move(xy));
        }
        contents.obj.emplace_back("param", std::move(params));

        JsonValue paramIndex;
        paramIndex.type = JsonValue::Type::Array;
        for (size_t k = 0; k < pf.FacetCount(); k++) {
            for (size_t i = pf.FacetIndex0(k); i < pf.FacetIndex1(k) && i < data.paramIndex.size(); i++)
                paramIndex.arr.push_back(makeNumber(static_cast<double>(data.paramIndex[i])));
            paramIndex.arr.push_back(makeNumber(0.0));
        }
        contents.obj.emplace_back("paramIndex", std::move(paramIndex));
    }

    // point / pointIndex（:1857-1858——逐 facet 带符号 1-based + 0 终止符；DanQing
    // pointIndex 已是 signed 1-based 直写，符号=edgeVisible 与参考 :1796 同信息）。
    {
        JsonValue points;
        points.type = JsonValue::Type::Array;
        for (auto const& p : data.points) {
            JsonValue xyz;
            xyz.type = JsonValue::Type::Array;
            xyz.arr.push_back(makeNumber(p.x));
            xyz.arr.push_back(makeNumber(p.y));
            xyz.arr.push_back(makeNumber(p.z));
            points.arr.push_back(std::move(xyz));
        }
        contents.obj.emplace_back("point", std::move(points));

        JsonValue pointIndex;
        pointIndex.type = JsonValue::Type::Array;
        for (size_t k = 0; k < pf.FacetCount(); k++) {
            for (size_t i = pf.FacetIndex0(k); i < pf.FacetIndex1(k); i++)
                pointIndex.arr.push_back(makeNumber(static_cast<double>(data.pointIndex[i])));
            pointIndex.arr.push_back(makeNumber(0.0));
        }
        contents.obj.emplace_back("pointIndex", std::move(pointIndex));
    }

    // NOTE: pf.data.face is not persistent（参考 :1866）；edgeMateIndex/tags 未移植（E3）。

    JsonValue out;
    out.type = JsonValue::Type::Object;
    out.obj.emplace_back("indexedMesh", std::move(contents));
    return out;
}

}  // namespace IModelJson

END_DQ_GEOM_NAMESPACE
