// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DumpIModelConnection implementation（imodel.json → 打开连接）
// Authored: 见 DumpIModelConnection.h 文件头（§8.2 零网络回放缝）。
#include "dqApp/tile/DumpIModelConnection.h"

#include "dqApp/tile/DumpTileTreeProps.h"  // dumpjson 解析器（dqApp 共享）
#include "dqApp/ViewState.h"               // ViewStateProps 完整消费（views.load 链）

#include <cstdint>
#include <fstream>

BEGIN_DQ_APP_NAMESPACE

namespace {

std::vector<uint8_t> readFileBytesLocal(std::string const& path, bool* ok)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        *ok = false;
        return {};
    }
    ifs.seekg(0, std::ios::end);
    auto const size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0)
        ifs.read(reinterpret_cast<char*>(bytes.data()), size);
    *ok = ifs.good() || ifs.eof();
    return bytes;
}

// Range3dProps {low:[x,y,z], high:[x,y,z]} → dqGeom::Range3d——与
// DumpTileTreeProps.cpp:43 parseRange3d 同源同构（Range3d.fromJSON 逐轴赋值
// 不重排语义；该 helper 在匿名命名空间不可跨 TU 复用，此处持副本）。
bool parseRange3dLocal(dumpjson::JsonValue const& json, dqGeom::Range3d& out)
{
    if (json.type != dumpjson::JsonValue::Type::Object)
        return false;
    dumpjson::JsonValue const* low = json.find("low");
    dumpjson::JsonValue const* high = json.find("high");
    if (!low || !high || low->type != dumpjson::JsonValue::Type::Array
        || high->type != dumpjson::JsonValue::Type::Array
        || low->arr.size() < 3 || high->arr.size() < 3)
        return false;
    out = dqGeom::Range3d(
        dqGeom::Point3d::From(low->arr[0].number, low->arr[1].number,
                              low->arr[2].number),
        dqGeom::Point3d::From(high->arr[0].number, high->arr[1].number,
                              high->arr[2].number));
    return true;
}

dqBase::DqId parseId(dumpjson::JsonValue const& json)
{
    if (json.type != dumpjson::JsonValue::Type::String)
        return dqBase::DqId();
    return dqBase::DqId::FromString(json.str);
}

// XYZProps [x,y,z] → Point3d/Vector3d。
dqGeom::Point3d parsePoint3d(dumpjson::JsonValue const& json)
{
    dqGeom::Point3d out = dqGeom::Point3d::From(0.0, 0.0, 0.0);
    if (json.type == dumpjson::JsonValue::Type::Array && json.arr.size() >= 3)
        out = dqGeom::Point3d::From(json.arr[0].number, json.arr[1].number,
                                    json.arr[2].number);
    return out;
}

// styles.viewflags JSON → ViewFlagProps（键名 1:1——ViewFlagProps 持久化
// 字段名；只置出现的键，缺席留 nullopt = ViewFlags.fromJSON 的 asBool(false)
// 缺省语义，ViewFlags.ts:471-511）。
dqCommon::ViewFlagProps parseViewFlagProps(dumpjson::JsonValue const& json)
{
    dqCommon::ViewFlagProps out;
    auto boolField = [&](char const* key, std::optional<bool>& field) {
        if (dumpjson::JsonValue const* v = json.find(key))
            if (v->type == dumpjson::JsonValue::Type::Bool)
                field = v->boolean;
    };
    boolField("noConstruct", out.noConstruct);
    boolField("noDim", out.noDim);
    boolField("noPattern", out.noPattern);
    boolField("noWeight", out.noWeight);
    boolField("noStyle", out.noStyle);
    boolField("noTransp", out.noTransp);
    boolField("noFill", out.noFill);
    boolField("grid", out.grid);
    boolField("acs", out.acs);
    boolField("noTexture", out.noTexture);
    boolField("noMaterial", out.noMaterial);
    boolField("noCameraLights", out.noCameraLights);
    boolField("noSourceLights", out.noSourceLights);
    boolField("noSolarLight", out.noSolarLight);
    boolField("visEdges", out.visEdges);
    boolField("hidEdges", out.hidEdges);
    boolField("shadows", out.shadows);
    boolField("clipVol", out.clipVol);
    boolField("monochrome", out.monochrome);
    boolField("backgroundMap", out.backgroundMap);
    boolField("ambientOcclusion", out.ambientOcclusion);
    boolField("thematicDisplay", out.thematicDisplay);
    boolField("wiremesh", out.wiremesh);
    boolField("forceSurfaceDiscard", out.forceSurfaceDiscard);
    boolField("noWhiteOnWhiteReversal", out.noWhiteOnWhiteReversal);
    if (dumpjson::JsonValue const* v = json.find("renderMode"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.renderMode = static_cast<dqCommon::RenderMode>(
                static_cast<int>(v->number));
    return out;
}

// views.defaultViewState JSON（getViewStateData RPC 载荷原样）→ ViewStateProps。
// 参考锚 = convertViewStatePropsToViewState 的 props 形态（IModelConnection.ts
// :1548-1561）+ ViewState3d ctor 的消费面（ViewState.ts:1497-1515）。
std::optional<ViewStateProps> parseViewStateProps(dumpjson::JsonValue const& json)
{
    if (json.type != dumpjson::JsonValue::Type::Object)
        return std::nullopt;
    dumpjson::JsonValue const* vdp = json.find("viewDefinitionProps");
    dumpjson::JsonValue const* csp = json.find("categorySelectorProps");
    dumpjson::JsonValue const* dsp = json.find("displayStyleProps");
    if (!vdp || !csp || !dsp
        || vdp->type != dumpjson::JsonValue::Type::Object
        || csp->type != dumpjson::JsonValue::Type::Object
        || dsp->type != dumpjson::JsonValue::Type::Object)
        return std::nullopt;

    ViewStateProps out;

    // --- viewDefinitionProps（3d 消费面） ---
    if (dumpjson::JsonValue const* v = vdp->find("classFullName"))
        out.viewDefinitionProps.classFullName = v->str;
    if (dumpjson::JsonValue const* v = vdp->find("id"))
        out.viewDefinitionProps.id = parseId(*v);
    if (dumpjson::JsonValue const* v = vdp->find("code")) {
        if (dumpjson::JsonValue const* cv = v->find("value"))
            out.viewDefinitionProps.codeValue = cv->str;
    }
    if (dumpjson::JsonValue const* v = vdp->find("description"))
        out.viewDefinitionProps.description = v->str;
    if (dumpjson::JsonValue const* v = vdp->find("isPrivate"))
        out.viewDefinitionProps.isPrivate =
            v->type == dumpjson::JsonValue::Type::Bool && v->boolean;
    if (dumpjson::JsonValue const* v = vdp->find("cameraOn"))
        out.viewDefinitionProps.cameraOn =
            v->type == dumpjson::JsonValue::Type::Bool && v->boolean;
    if (dumpjson::JsonValue const* v = vdp->find("origin"))
        out.viewDefinitionProps.origin = parsePoint3d(*v);
    if (dumpjson::JsonValue const* v = vdp->find("extents")) {
        auto const p = parsePoint3d(*v);
        out.viewDefinitionProps.extents = dqGeom::Vector3d::From(p.x, p.y, p.z);
    }
    // angles（YawPitchRollProps number=degrees 形态——ViewState.ts:1502 经
    // YawPitchRollAngles.fromJSON 消费）。
    if (dumpjson::JsonValue const* v = vdp->find("angles")) {
        if (v->type == dumpjson::JsonValue::Type::Object) {
            out.viewDefinitionProps.hasAngles = true;
            if (dumpjson::JsonValue const* a = v->find("yaw"))
                out.viewDefinitionProps.yawDegrees = a->number;
            if (dumpjson::JsonValue const* a = v->find("pitch"))
                out.viewDefinitionProps.pitchDegrees = a->number;
            if (dumpjson::JsonValue const* a = v->find("roll"))
                out.viewDefinitionProps.rollDegrees = a->number;
        }
    }
    // camera（CameraProps——Camera.ts:66-76 消费面）。
    if (dumpjson::JsonValue const* v = vdp->find("camera")) {
        if (dumpjson::JsonValue const* e = v->find("eye"))
            out.viewDefinitionProps.camera.eye = parsePoint3d(*e);
        if (dumpjson::JsonValue const* f = v->find("focusDist"))
            out.viewDefinitionProps.camera.focusDist = f->number;
        if (dumpjson::JsonValue const* l = v->find("lens"))
            out.viewDefinitionProps.camera.lensDegrees = l->number;
    }

    // --- categorySelectorProps ---
    if (dumpjson::JsonValue const* v = csp->find("id"))
        out.categorySelectorProps.id = parseId(*v);
    if (dumpjson::JsonValue const* v = csp->find("categories")) {
        if (v->type == dumpjson::JsonValue::Type::Array)
            for (auto const& entry : v->arr)
                out.categorySelectorProps.categories.push_back(parseId(entry));
    }

    // --- displayStyleProps（消费面 = jsonProperties.styles.viewflags） ---
    if (dumpjson::JsonValue const* v = dsp->find("id"))
        out.displayStyleProps.id = parseId(*v);
    if (dumpjson::JsonValue const* jp = dsp->find("jsonProperties")) {
        if (dumpjson::JsonValue const* styles = jp->find("styles")) {
            if (dumpjson::JsonValue const* vf = styles->find("viewflags")) {
                if (vf->type == dumpjson::JsonValue::Type::Object)
                    out.displayStyleProps.viewflags = parseViewFlagProps(*vf);
            }
        }
    }

    // --- modelSelectorProps（optional——空间视图必携） ---
    if (dumpjson::JsonValue const* msp = json.find("modelSelectorProps")) {
        if (msp->type == dumpjson::JsonValue::Type::Object) {
            ModelSelectorProps ms;
            if (dumpjson::JsonValue const* v = msp->find("id"))
                ms.id = parseId(*v);
            if (dumpjson::JsonValue const* v = msp->find("models")) {
                if (v->type == dumpjson::JsonValue::Type::Array)
                    for (auto const& entry : v->arr)
                        ms.models.push_back(parseId(entry));
            }
            out.modelSelectorProps = std::move(ms);
        }
    }

    return out;
}

}  // namespace

dqBase::RefPtr<DumpIModelConnection> DumpIModelConnection::open(
    std::string const& imodelJsonPath)
{
    bool ok = false;
    std::vector<uint8_t> const bytes = readFileBytesLocal(imodelJsonPath, &ok);
    if (!ok)
        return nullptr;
    auto doc = dumpjson::parseJsonDocument(std::string(bytes.begin(), bytes.end()));
    if (!doc || doc->type != dumpjson::JsonValue::Type::Object)
        return nullptr;

    dumpjson::JsonValue const* conn = doc->find("connection");
    dumpjson::JsonValue const* views = doc->find("views");
    dumpjson::JsonValue const* models = doc->find("models");
    if (!conn || !views || !models
        || conn->type != dumpjson::JsonValue::Type::Object
        || views->type != dumpjson::JsonValue::Type::Object
        || models->type != dumpjson::JsonValue::Type::Array)
        return nullptr;

    dqBase::RefPtr<DumpIModelConnection> out(new DumpIModelConnection());

    // --- connection 段（IModelConnection open 数据面） ---
    if (dumpjson::JsonValue const* v = conn->find("name"))
        out->m_name = v->str;
    if (dumpjson::JsonValue const* v = conn->find("guid"))
        out->m_guid = v->str;
    if (dumpjson::JsonValue const* v = conn->find("projectExtents")) {
        if (!parseRange3dLocal(*v, out->m_projectExtents))
            return nullptr;
    }
    if (dumpjson::JsonValue const* rpc = conn->find("rpcProps")) {
        if (dumpjson::JsonValue const* v = rpc->find("key"))
            out->m_key = v->str;
        if (dumpjson::JsonValue const* v = rpc->find("iTwinId"))
            out->m_iTwinId = v->str;
    }

    // --- views 段 ---
    dumpjson::JsonValue const* list = views->find("list");
    if (!list || list->type != dumpjson::JsonValue::Type::Array)
        return nullptr;
    for (auto const& entry : list->arr) {
        if (entry.type != dumpjson::JsonValue::Type::Object)
            return nullptr;
        ViewInfo info;
        if (dumpjson::JsonValue const* v = entry.find("id"))
            info.id = parseId(*v);
        if (dumpjson::JsonValue const* v = entry.find("name"))
            info.name = v->str;
        if (dumpjson::JsonValue const* v = entry.find("class"))
            info.className = v->str;
        if (dumpjson::JsonValue const* v = entry.find("isPrivate"))
            info.isPrivate =
                v->type == dumpjson::JsonValue::Type::Bool && v->boolean;
        out->m_capturedViews.push_back(std::move(info));
    }
    if (dumpjson::JsonValue const* v = views->find("defaultViewId"))
        out->m_defaultViewId = parseId(*v);
    if (dumpjson::JsonValue const* v = views->find("defaultViewState")) {
        out->m_defaultViewState = parseViewStateProps(*v);
        if (!out->m_defaultViewState.has_value())
            return nullptr;  // 采集契约破口（defaultViewState 段在但不可解析）
    }

    // --- models 段（ModelProps 消费面子集） ---
    for (auto const& entry : models->arr) {
        if (entry.type != dumpjson::JsonValue::Type::Object)
            return nullptr;
        ModelInfo info;
        if (dumpjson::JsonValue const* v = entry.find("id"))
            info.id = parseId(*v);
        if (dumpjson::JsonValue const* v = entry.find("name"))
            info.name = v->str;
        if (dumpjson::JsonValue const* v = entry.find("classFullName"))
            info.classFullName = v->str;
        out->m_models.push_back(std::move(info));
    }

    // --- Views::RpcHooks 安装（IModelReadRpcInterface 回放——
    //     IModelConnection.ts:1502/:1537/:1548 三 RPC 的等价数据面） ---
    {
        Views::RpcHooks hooks;
        // queryElementProps → getViewList（:1499-1501 的 wantPrivate 语义：
        // wantPrivate=false → WHERE IsPrivate=FALSE；true → 全集）。
        hooks.getViewList = [captured = &out->m_capturedViews](bool wantPrivate) {
            std::vector<ViewSpec> specs;
            for (auto const& info : *captured) {
                if (!wantPrivate && info.isPrivate)
                    continue;
                ViewSpec spec;
                spec.id = info.id;
                spec.name = info.name;        // :1522 code.value → name（采集侧已映射）
                spec.className = info.className;
                specs.push_back(std::move(spec));
            }
            return specs;
        };
        hooks.getDefaultViewId = [id = out->m_defaultViewId]() { return id; };
        // getViewStateData——采集面只落默认视图一份（imodel.json
        // views.defaultViewState）；其他 id → nullopt（RPC 失败语义——
        // ViewPicker.ts:39-44 catch → manufactureSpatialView 的忠实对应）。
        hooks.getViewStateData =
            [state = &out->m_defaultViewState, defaultId = out->m_defaultViewId](
                dqBase::DqId viewDefinitionId) -> std::optional<ViewStateProps> {
            if (viewDefinitionId == defaultId && state->has_value())
                return *state;
            return std::nullopt;
        };
        out->GetViews().SetRpcHooks(std::move(hooks));
    }

    // ← IModelConnection.onOpen.raiseEvent(connection)（:796/:845/:863 同款）。
    OnOpen.Raise(out.Get());
    return out;
}

END_DQ_APP_NAMESPACE
