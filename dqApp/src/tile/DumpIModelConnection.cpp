// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — DumpIModelConnection implementation（imodel.json → 打开连接）
// Authored: 见 DumpIModelConnection.h 文件头（§8.2 零网络回放缝）。
#include "dqApp/tile/DumpIModelConnection.h"

#include "dqApp/tile/DumpTileTreeProps.h"  // dumpjson 解析器（dqApp 共享）
#include "dqApp/ViewState.h"               // ViewStateProps 完整消费（views.load 链）

#include <cstdint>
#include <cstdio>
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

// styles.hline JSON → HiddenLineSettingsProps（键名 1:1——HiddenLine.ts
// StyleProps/SettingsProps 持久化字段名：ovrColor/color/pattern/width +
// transThreshold；语义门——color 仅 ovrColor≠false 时生效、pattern≠Invalid、
// width≠0 clamp[1,32]——在 dqCommon HiddenLineStyle::fromJSON 侧，本层只搬运
// 原始值。dump 实态（joeshouse-v1 / instances60-imodel-v1 同）：
//   visible={color:0,ovrColor:true,pattern:0,width:1}（黑/实线/1px 覆盖），
//   hidden={color:0,ovrColor:true,pattern:3435973836(0xCCCCCCCC),width:1}，
//   transThreshold:0.3。
dqCommon::HiddenLineStyleProps parseHiddenLineStyleProps(dumpjson::JsonValue const& json)
{
    dqCommon::HiddenLineStyleProps out;
    if (dumpjson::JsonValue const* v = json.find("ovrColor"))
        if (v->type == dumpjson::JsonValue::Type::Bool)
            out.ovrColor = v->boolean;
    if (dumpjson::JsonValue const* v = json.find("color"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.color = static_cast<dqCommon::ColorDefProps>(
                static_cast<uint32_t>(static_cast<int64_t>(v->number)));
    if (dumpjson::JsonValue const* v = json.find("pattern"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.pattern = static_cast<dqCommon::LinePixels>(
                static_cast<uint32_t>(static_cast<int64_t>(v->number)));
    if (dumpjson::JsonValue const* v = json.find("width"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.width = static_cast<int>(v->number);
    return out;
}

dqCommon::HiddenLineSettingsProps parseHiddenLineSettingsProps(dumpjson::JsonValue const& json)
{
    dqCommon::HiddenLineSettingsProps out;
    if (dumpjson::JsonValue const* v = json.find("visible"))
        if (v->type == dumpjson::JsonValue::Type::Object)
            out.visible = parseHiddenLineStyleProps(*v);
    if (dumpjson::JsonValue const* v = json.find("hidden"))
        if (v->type == dumpjson::JsonValue::Type::Object)
            out.hidden = parseHiddenLineStyleProps(*v);
    if (dumpjson::JsonValue const* v = json.find("transThreshold"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.transThreshold = v->number;
    return out;
}

// styles.lights JSON → LightSettingsProps（M-M(1)：saved display style 的自定义
// 灯光此前静默丢弃，渲染恒用 LightSettings{} 默认值——参考 DisplayStyleSettings
// ctor `this._json3d.lights = LightSettings.fromJSON(this._json3d.lights)`。
// 键名 1:1 LightSettingsProps 线格式：solar{intensity,direction{x,y,z},
// alwaysEnabled,timePoint} / ambient{color{r,g,b},intensity} / hemisphere{
// upperColor,lowerColor,intensity} / portrait{intensity} / specularIntensity /
// numCels / fresnel{intensity,invert}——缺席键留 nullopt，fromJSON 按参考缺省）。
dqCommon::LightSettingsProps parseLightSettingsProps(dumpjson::JsonValue const& json)
{
    dqCommon::LightSettingsProps out;
    auto numField = [&](dumpjson::JsonValue const& obj, char const* key,
                        std::optional<double>& field) {
        if (dumpjson::JsonValue const* v = obj.find(key))
            if (v->type == dumpjson::JsonValue::Type::Number)
                field = v->number;
    };
    auto boolField = [&](dumpjson::JsonValue const& obj, char const* key,
                         std::optional<bool>& field) {
        if (dumpjson::JsonValue const* v = obj.find(key))
            if (v->type == dumpjson::JsonValue::Type::Bool)
                field = v->boolean;
    };
    auto rgbField = [&](dumpjson::JsonValue const& obj, char const* key,
                        std::optional<dqCommon::RgbColorProps>& field) {
        if (dumpjson::JsonValue const* v = obj.find(key)) {
            if (v->type != dumpjson::JsonValue::Type::Object)
                return;
            dqCommon::RgbColorProps rgb;
            if (dumpjson::JsonValue const* c = v->find("r"))
                if (c->type == dumpjson::JsonValue::Type::Number)
                    rgb.r = static_cast<int>(c->number);
            if (dumpjson::JsonValue const* c = v->find("g"))
                if (c->type == dumpjson::JsonValue::Type::Number)
                    rgb.g = static_cast<int>(c->number);
            if (dumpjson::JsonValue const* c = v->find("b"))
                if (c->type == dumpjson::JsonValue::Type::Number)
                    rgb.b = static_cast<int>(c->number);
            field = rgb;
        }
    };

    if (dumpjson::JsonValue const* solar = json.find("solar")) {
        if (solar->type == dumpjson::JsonValue::Type::Object) {
            dqCommon::SolarLightProps p;
            numField(*solar, "intensity", p.intensity);
            boolField(*solar, "alwaysEnabled", p.alwaysEnabled);
            numField(*solar, "timePoint", p.timePoint);
            if (dumpjson::JsonValue const* d = solar->find("direction")) {
                // XYZProps：{x,y,z} 对象（或 [x,y,z] 数组形态同取）。
                if (d->type == dumpjson::JsonValue::Type::Object) {
                    if (dumpjson::JsonValue const* c = d->find("x"))
                        if (c->type == dumpjson::JsonValue::Type::Number)
                            p.dirX = c->number;
                    if (dumpjson::JsonValue const* c = d->find("y"))
                        if (c->type == dumpjson::JsonValue::Type::Number)
                            p.dirY = c->number;
                    if (dumpjson::JsonValue const* c = d->find("z"))
                        if (c->type == dumpjson::JsonValue::Type::Number)
                            p.dirZ = c->number;
                } else if (d->type == dumpjson::JsonValue::Type::Array
                           && d->arr.size() >= 3) {
                    p.dirX = d->arr[0].number;
                    p.dirY = d->arr[1].number;
                    p.dirZ = d->arr[2].number;
                }
            }
            out.solar = p;
        }
    }
    if (dumpjson::JsonValue const* amb = json.find("ambient")) {
        if (amb->type == dumpjson::JsonValue::Type::Object) {
            dqCommon::AmbientLightProps p;
            rgbField(*amb, "color", p.color);
            numField(*amb, "intensity", p.intensity);
            out.ambient = p;
        }
    }
    if (dumpjson::JsonValue const* hemi = json.find("hemisphere")) {
        if (hemi->type == dumpjson::JsonValue::Type::Object) {
            dqCommon::HemisphereLightsProps p;
            rgbField(*hemi, "upperColor", p.upperColor);
            rgbField(*hemi, "lowerColor", p.lowerColor);
            numField(*hemi, "intensity", p.intensity);
            out.hemisphere = p;
        }
    }
    if (dumpjson::JsonValue const* portrait = json.find("portrait")) {
        if (portrait->type == dumpjson::JsonValue::Type::Object) {
            std::optional<double> intensity;
            numField(*portrait, "intensity", intensity);
            if (intensity)
                out.portraitIntensity = *intensity;
        }
    }
    numField(json, "specularIntensity", out.specularIntensity);
    if (dumpjson::JsonValue const* v = json.find("numCels"))
        if (v->type == dumpjson::JsonValue::Type::Number)
            out.numCels = static_cast<int>(v->number);
    if (dumpjson::JsonValue const* fresnel = json.find("fresnel")) {
        if (fresnel->type == dumpjson::JsonValue::Type::Object) {
            dqCommon::FresnelSettingsProps p;
            numField(*fresnel, "intensity", p.intensity);
            boolField(*fresnel, "invert", p.invert);
            out.fresnel = p;
        }
    }
    return out;
}

// viewDetails.clip JSON → ClipVectorProps（M-P P-B——线格式 1:1：
// [{shape:{points,trans?,zlow?,zhigh?,mask?,invisible?}} |
//   {planes:{clips?:[[{normal:[x,y,z],dist,invisible?,interior?}]],invisible?}}]，
// ClipPrimitive.ts:62-96 / ClipPlane.ts:213-223）。
dqGeom::ClipVectorProps parseClipVectorProps(dumpjson::JsonValue const& json)
{
    dqGeom::ClipVectorProps out;
    if (json.type != dumpjson::JsonValue::Type::Array)
        return out;
    for (dumpjson::JsonValue const& primJson : json.arr) {
        if (primJson.type != dumpjson::JsonValue::Type::Object)
            continue;
        dqGeom::ClipPrimitiveProps prim;
        if (dumpjson::JsonValue const* shape = primJson.find("shape")) {
            if (shape->type == dumpjson::JsonValue::Type::Object) {
                dqGeom::ClipPrimitiveShapePart part;
                if (dumpjson::JsonValue const* points = shape->find("points")) {
                    if (points->type == dumpjson::JsonValue::Type::Array) {
                        for (dumpjson::JsonValue const& pt : points->arr) {
                            if (pt.type == dumpjson::JsonValue::Type::Array && pt.arr.size() >= 3)
                                part.points.push_back(dqGeom::Point3d::From(
                                    pt.arr[0].number, pt.arr[1].number, pt.arr[2].number));
                        }
                    }
                }
                if (dumpjson::JsonValue const* trans = shape->find("trans")) {
                    if (trans->type == dumpjson::JsonValue::Type::Array && trans->arr.size() >= 3) {
                        dqGeom::ClipShapeTransformProps rows;
                        for (int r = 0; r < 3; ++r) {
                            dumpjson::JsonValue const& row = trans->arr[r];
                            if (row.type == dumpjson::JsonValue::Type::Array && row.arr.size() >= 4)
                                for (int c = 0; c < 4; ++c)
                                    rows.rows[r][c] = row.arr[c].number;
                        }
                        part.trans = rows;
                    }
                }
                if (dumpjson::JsonValue const* v = shape->find("zlow"))
                    if (v->type == dumpjson::JsonValue::Type::Number)
                        part.zlow = v->number;
                if (dumpjson::JsonValue const* v = shape->find("zhigh"))
                    if (v->type == dumpjson::JsonValue::Type::Number)
                        part.zhigh = v->number;
                if (dumpjson::JsonValue const* v = shape->find("mask"))
                    if (v->type == dumpjson::JsonValue::Type::Bool)
                        part.mask = v->boolean;
                if (dumpjson::JsonValue const* v = shape->find("invisible"))
                    if (v->type == dumpjson::JsonValue::Type::Bool)
                        part.invisible = v->boolean;
                prim.shape = part;
            }
        } else if (dumpjson::JsonValue const* planes = primJson.find("planes")) {
            if (planes->type == dumpjson::JsonValue::Type::Object) {
                dqGeom::ClipPrimitivePlanesPart part;
                if (dumpjson::JsonValue const* clips = planes->find("clips")) {
                    // UnionOfConvexClipPlaneSetsProps = [ConvexClipPlaneSetProps]
                    if (clips->type == dumpjson::JsonValue::Type::Array) {
                        dqGeom::UnionOfConvexClipPlaneSetsProps unionProps;
                        for (dumpjson::JsonValue const& setJson : clips->arr) {
                            dqGeom::ConvexClipPlaneSetProps setProps;
                            if (setJson.type == dumpjson::JsonValue::Type::Array) {
                                for (dumpjson::JsonValue const& planeJson : setJson.arr) {
                                    if (planeJson.type != dumpjson::JsonValue::Type::Object)
                                        continue;
                                    dqGeom::ClipPlaneProps plane;
                                    if (dumpjson::JsonValue const* n = planeJson.find("normal")) {
                                        if (n->type == dumpjson::JsonValue::Type::Array && n->arr.size() >= 3)
                                            plane.normal = dqGeom::Vector3d::From(
                                                n->arr[0].number, n->arr[1].number, n->arr[2].number);
                                    }
                                    if (dumpjson::JsonValue const* d = planeJson.find("dist"))
                                        if (d->type == dumpjson::JsonValue::Type::Number)
                                            plane.dist = d->number;
                                    if (dumpjson::JsonValue const* v = planeJson.find("invisible"))
                                        if (v->type == dumpjson::JsonValue::Type::Bool)
                                            plane.invisible = v->boolean;
                                    if (dumpjson::JsonValue const* v = planeJson.find("interior"))
                                        if (v->type == dumpjson::JsonValue::Type::Bool)
                                            plane.interior = v->boolean;
                                    setProps.push_back(plane);
                                }
                            }
                            unionProps.push_back(setProps);
                        }
                        part.clips = unionProps;
                    }
                }
                if (dumpjson::JsonValue const* v = planes->find("invisible"))
                    if (v->type == dumpjson::JsonValue::Type::Bool)
                        part.invisible = v->boolean;
                prim.planes = part;
            }
        }
        out.push_back(prim);
    }
    return out;
}

// parseClipVectorProps 的序列化对偶——viewDetails.clip 线格式
// （2026-10-07 一致性审计修复：SavedViews 保存面原先不写 jsonProperties，
// clip 在 round-trip 中丢失——参考 EntityState.toJSON :57-64 保留该段）。
void appendJsonNumber(std::string& out, double v);   // 定义于本文件后部

std::string jsonClipVectorProps(dqGeom::ClipVectorProps const& props)
{
    std::string out = "[";
    bool firstPrim = true;
    for (auto const& prim : props) {
        if (!firstPrim)
            out.push_back(',');
        firstPrim = false;
        out.push_back('{');
        if (prim.shape.has_value()) {
            auto const& s = *prim.shape;
            out += "\"shape\":{\"points\":[";
            bool first = true;
            for (auto const& p : s.points) {
                if (!first)
                    out.push_back(',');
                first = false;
                out.push_back('[');
                appendJsonNumber(out, p.x);
                out.push_back(',');
                appendJsonNumber(out, p.y);
                out.push_back(',');
                appendJsonNumber(out, p.z);
                out.push_back(']');
            }
            out.push_back(']');
            if (s.trans.has_value()) {
                out += ",\"trans\":[";
                for (int r = 0; r < 3; ++r) {
                    if (r)
                        out.push_back(',');
                    out.push_back('[');
                    for (int c = 0; c < 4; ++c) {
                        if (c)
                            out.push_back(',');
                        appendJsonNumber(out, s.trans->rows[r][c]);
                    }
                    out.push_back(']');
                }
                out.push_back(']');
            }
            if (s.zlow.has_value()) {
                out += ",\"zlow\":";
                appendJsonNumber(out, *s.zlow);
            }
            if (s.zhigh.has_value()) {
                out += ",\"zhigh\":";
                appendJsonNumber(out, *s.zhigh);
            }
            if (s.mask.has_value())
                out += *s.mask ? ",\"mask\":true" : ",\"mask\":false";
            if (s.invisible.has_value())
                out += *s.invisible ? ",\"invisible\":true" : ",\"invisible\":false";
            out.push_back('}');
        } else if (prim.planes.has_value()) {
            auto const& pl = *prim.planes;
            out += "\"planes\":{";
            if (pl.clips.has_value()) {
                out += "\"clips\":[";
                bool firstSet = true;
                for (auto const& set : *pl.clips) {
                    if (!firstSet)
                        out.push_back(',');
                    firstSet = false;
                    out.push_back('[');
                    bool first = true;
                    for (auto const& plane : set) {
                        if (!first)
                            out.push_back(',');
                        first = false;
                        out += "{\"normal\":[";
                        appendJsonNumber(out, plane.normal.x);
                        out.push_back(',');
                        appendJsonNumber(out, plane.normal.y);
                        out.push_back(',');
                        appendJsonNumber(out, plane.normal.z);
                        out += "],\"dist\":";
                        appendJsonNumber(out, plane.dist);
                        if (plane.invisible.has_value())
                            out += *plane.invisible ? ",\"invisible\":true" : ",\"invisible\":false";
                        if (plane.interior.has_value())
                            out += *plane.interior ? ",\"interior\":true" : ",\"interior\":false";
                        out.push_back('}');
                    }
                    out.push_back(']');
                }
                out.push_back(']');
            }
            if (pl.invisible.has_value())
                out += *pl.invisible ? ",\"invisible\":true" : ",\"invisible\":false";
            out.push_back('}');
        }
        out.push_back('}');
    }
    out.push_back(']');
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

    // jsonProperties.viewDetails.clip（M-P P-B——ViewDetails.ts:137-146 惰性
    // getter 的线形态）。
    if (dumpjson::JsonValue const* jp = vdp->find("jsonProperties")) {
        if (dumpjson::JsonValue const* vd = jp->find("viewDetails")) {
            if (dumpjson::JsonValue const* clip = vd->find("clip"))
                out.viewDetailsProps.clip = parseClipVectorProps(*clip);
            // disable3dManipulations（ViewDetails.ts:198——反转存储，缺省 false）。
            if (dumpjson::JsonValue const* d3d = vd->find("disable3dManipulations"))
                if (d3d->type == dumpjson::JsonValue::Type::Bool)
                    out.viewDetailsProps.disable3dManipulations = d3d->boolean;
        }
    }

    // --- categorySelectorProps ---
    if (dumpjson::JsonValue const* v = csp->find("id"))
        out.categorySelectorProps.id = parseId(*v);
    if (dumpjson::JsonValue const* v = csp->find("categories")) {
        if (v->type == dumpjson::JsonValue::Type::Array)
            for (auto const& entry : v->arr)
                out.categorySelectorProps.categories.push_back(parseId(entry));
    }

    // --- displayStyleProps（消费面 = jsonProperties.styles.viewflags +
    //     styles.hline[M-I(4)]） ---
    if (dumpjson::JsonValue const* v = dsp->find("id"))
        out.displayStyleProps.id = parseId(*v);
    if (dumpjson::JsonValue const* jp = dsp->find("jsonProperties")) {
        if (dumpjson::JsonValue const* styles = jp->find("styles")) {
            if (dumpjson::JsonValue const* vf = styles->find("viewflags")) {
                if (vf->type == dumpjson::JsonValue::Type::Object)
                    out.displayStyleProps.viewflags = parseViewFlagProps(*vf);
            }
            // styles.hline（DisplayStyleSettings.ts:1104 ctor 的
            // `this._json3d.hline` 段——HiddenLine.Settings.fromJSON 输入）。
            if (dumpjson::JsonValue const* hl = styles->find("hline")) {
                if (hl->type == dumpjson::JsonValue::Type::Object)
                    out.displayStyleProps.hline = parseHiddenLineSettingsProps(*hl);
            }
            // styles.lights（DisplayStyleSettings ctor 的 `this._json3d.lights`
            // 段——LightSettings.fromJSON 输入；M-M(1) 接线）。
            if (dumpjson::JsonValue const* ls = styles->find("lights")) {
                if (ls->type == dumpjson::JsonValue::Type::Object)
                    out.displayStyleProps.lights = parseLightSettingsProps(*ls);
            } else if (dumpjson::JsonValue const* sl = styles->find("sceneLights")) {
                // 旧格式回退（DisplayStyleSettings.ts:1109-1116）：MicroStation 遗留
                // 灯光设置只保留 sunDir——`LightSettings.fromJSON(sunDir ?
                // { solar: { direction: sunDir } } : undefined)`（强度/ambient 全部
                // 忽略，用参考默认 rig）。dump 实态：五模型 saved style 均携带
                // sceneLights（如 instances60 sunDir=[0.19, 0.78, -0.60]）。
                if (dumpjson::JsonValue const* sd = sl->find("sunDir")) {
                    dqCommon::LightSettingsProps props;
                    dqCommon::SolarLightProps solar;
                    if (sd->type == dumpjson::JsonValue::Type::Array && sd->arr.size() >= 3) {
                        solar.dirX = sd->arr[0].number;
                        solar.dirY = sd->arr[1].number;
                        solar.dirZ = sd->arr[2].number;
                    } else if (sd->type == dumpjson::JsonValue::Type::Object) {
                        if (dumpjson::JsonValue const* c = sd->find("x"))
                            if (c->type == dumpjson::JsonValue::Type::Number)
                                solar.dirX = c->number;
                        if (dumpjson::JsonValue const* c = sd->find("y"))
                            if (c->type == dumpjson::JsonValue::Type::Number)
                                solar.dirY = c->number;
                        if (dumpjson::JsonValue const* c = sd->find("z"))
                            if (c->type == dumpjson::JsonValue::Type::Number)
                                solar.dirZ = c->number;
                    } else {
                        solar.dirX = 0.0;
                    }
                    if (solar.dirX || solar.dirY || solar.dirZ) {
                        props.solar = solar;
                        out.displayStyleProps.lights = props;
                    }
                }
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

// --- M-O(2) I9：序列化缝的写出半边 ---
// 字段面与 parseViewStateProps 的消费面互为闭偶；optional 字段缺席不写出
//（回读 nullopt = fromJSON 缺省语义）。数值 %.17g = double 的 round-trip
// 精度（parse 侧 strtod 同域）。
namespace {

void appendJsonEscaped(std::string& out, std::string const& s)
{
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void appendJsonNumber(std::string& out, double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

// 紧凑 JSON 对象写出器（参考 JSON.stringify(props) 的无缩进形态）。
struct JsonObjWriter {
    std::string out = "{";
    bool first = true;

    void separator(char const* key)
    {
        if (!first)
            out.push_back(',');
        first = false;
        appendJsonEscaped(out, key);
        out.push_back(':');
    }
    void str(char const* key, std::string const& v)
    {
        separator(key);
        appendJsonEscaped(out, v);
    }
    void num(char const* key, double v)
    {
        separator(key);
        appendJsonNumber(out, v);
    }
    void numOpt(char const* key, std::optional<double> const& v)
    {
        if (v)
            num(key, *v);
    }
    void intOpt(char const* key, std::optional<int> const& v)
    {
        if (v)
            num(key, static_cast<double>(*v));
    }
    void boolean(char const* key, bool v)
    {
        separator(key);
        out += v ? "true" : "false";
    }
    void boolOpt(char const* key, std::optional<bool> const& v)
    {
        if (v)
            boolean(key, *v);
    }
    void raw(char const* key, std::string const& v)  // 已构造的子对象/数组
    {
        separator(key);
        out += v;
    }
};

std::string jsonIdArray(std::vector<dqBase::DqId> const& ids)
{
    std::string out = "[";
    bool first = true;
    for (auto const& id : ids) {
        if (!first)
            out.push_back(',');
        first = false;
        appendJsonEscaped(out, id.ToString());
    }
    out.push_back(']');
    return out;
}

std::string jsonPoint3(dqGeom::Point3d const& p)
{
    std::string out = "[";
    appendJsonNumber(out, p.x);
    out.push_back(',');
    appendJsonNumber(out, p.y);
    out.push_back(',');
    appendJsonNumber(out, p.z);
    out.push_back(']');
    return out;
}

std::string jsonViewFlagProps(dqCommon::ViewFlagProps const& vf)
{
    JsonObjWriter w;
    w.boolOpt("noConstruct", vf.noConstruct);
    w.boolOpt("noDim", vf.noDim);
    w.boolOpt("noPattern", vf.noPattern);
    w.boolOpt("noWeight", vf.noWeight);
    w.boolOpt("noStyle", vf.noStyle);
    w.boolOpt("noTransp", vf.noTransp);
    w.boolOpt("noFill", vf.noFill);
    w.boolOpt("grid", vf.grid);
    w.boolOpt("acs", vf.acs);
    w.boolOpt("noTexture", vf.noTexture);
    w.boolOpt("noMaterial", vf.noMaterial);
    w.boolOpt("noCameraLights", vf.noCameraLights);
    w.boolOpt("noSourceLights", vf.noSourceLights);
    w.boolOpt("noSolarLight", vf.noSolarLight);
    w.boolOpt("visEdges", vf.visEdges);
    w.boolOpt("hidEdges", vf.hidEdges);
    w.boolOpt("shadows", vf.shadows);
    w.boolOpt("clipVol", vf.clipVol);
    w.boolOpt("monochrome", vf.monochrome);
    w.boolOpt("backgroundMap", vf.backgroundMap);
    w.boolOpt("ambientOcclusion", vf.ambientOcclusion);
    w.boolOpt("thematicDisplay", vf.thematicDisplay);
    w.boolOpt("wiremesh", vf.wiremesh);
    w.boolOpt("forceSurfaceDiscard", vf.forceSurfaceDiscard);
    w.boolOpt("noWhiteOnWhiteReversal", vf.noWhiteOnWhiteReversal);
    if (vf.renderMode.has_value())
        w.num("renderMode", static_cast<double>(static_cast<int>(*vf.renderMode)));
    w.out.push_back('}');
    return std::move(w.out);
}

std::string jsonHiddenLineStyleProps(dqCommon::HiddenLineStyleProps const& s)
{
    JsonObjWriter w;
    w.boolOpt("ovrColor", s.ovrColor);
    if (s.color.has_value())
        w.num("color", static_cast<double>(*s.color));
    if (s.pattern.has_value())
        w.num("pattern", static_cast<double>(static_cast<uint32_t>(*s.pattern)));
    w.intOpt("width", s.width);
    w.out.push_back('}');
    return std::move(w.out);
}

std::string jsonHiddenLineSettingsProps(dqCommon::HiddenLineSettingsProps const& s)
{
    JsonObjWriter w;
    if (s.visible.has_value())
        w.raw("visible", jsonHiddenLineStyleProps(*s.visible));
    if (s.hidden.has_value())
        w.raw("hidden", jsonHiddenLineStyleProps(*s.hidden));
    w.numOpt("transThreshold", s.transThreshold);
    w.out.push_back('}');
    return std::move(w.out);
}

std::string jsonRgb(dqCommon::RgbColorProps const& rgb)
{
    JsonObjWriter w;
    w.num("r", static_cast<double>(rgb.r));
    w.num("g", static_cast<double>(rgb.g));
    w.num("b", static_cast<double>(rgb.b));
    w.out.push_back('}');
    return std::move(w.out);
}

std::string jsonLightSettingsProps(dqCommon::LightSettingsProps const& ls)
{
    JsonObjWriter w;
    if (ls.solar.has_value()) {
        JsonObjWriter solar;
        solar.numOpt("intensity", ls.solar->intensity);
        solar.boolOpt("alwaysEnabled", ls.solar->alwaysEnabled);
        solar.numOpt("timePoint", ls.solar->timePoint);
        if (ls.solar->dirX.has_value() || ls.solar->dirY.has_value()
            || ls.solar->dirZ.has_value()) {
            JsonObjWriter dir;
            dir.numOpt("x", ls.solar->dirX);
            dir.numOpt("y", ls.solar->dirY);
            dir.numOpt("z", ls.solar->dirZ);
            dir.out.push_back('}');
            solar.raw("direction", std::move(dir.out));
        }
        solar.out.push_back('}');
        w.raw("solar", std::move(solar.out));
    }
    if (ls.ambient.has_value()) {
        JsonObjWriter amb;
        if (ls.ambient->color.has_value())
            amb.raw("color", jsonRgb(*ls.ambient->color));
        amb.numOpt("intensity", ls.ambient->intensity);
        amb.out.push_back('}');
        w.raw("ambient", std::move(amb.out));
    }
    if (ls.hemisphere.has_value()) {
        JsonObjWriter hemi;
        if (ls.hemisphere->upperColor.has_value())
            hemi.raw("upperColor", jsonRgb(*ls.hemisphere->upperColor));
        if (ls.hemisphere->lowerColor.has_value())
            hemi.raw("lowerColor", jsonRgb(*ls.hemisphere->lowerColor));
        hemi.numOpt("intensity", ls.hemisphere->intensity);
        hemi.out.push_back('}');
        w.raw("hemisphere", std::move(hemi.out));
    }
    // portrait 段：parse 侧从 {"portrait":{"intensity":..}} 对象读取
    //（采集面线格式——LightSettingsProps.portraitIntensity 的承载形态）。
    if (ls.portraitIntensity.has_value()) {
        JsonObjWriter portrait;
        portrait.numOpt("intensity", ls.portraitIntensity);
        portrait.out.push_back('}');
        w.raw("portrait", std::move(portrait.out));
    }
    w.numOpt("specularIntensity", ls.specularIntensity);
    w.intOpt("numCels", ls.numCels);
    if (ls.fresnel.has_value()) {
        JsonObjWriter fresnel;
        fresnel.numOpt("intensity", ls.fresnel->intensity);
        fresnel.boolOpt("invert", ls.fresnel->invert);
        fresnel.out.push_back('}');
        w.raw("fresnel", std::move(fresnel.out));
    }
    w.out.push_back('}');
    return std::move(w.out);
}

}  // namespace

// Ported from: itwinjs-core frontend-devtools serializeViewState（SavedViews
// 保存面——props → JSON 字串；NamedVSPSProps._viewStatePropsString 载体）。
std::string serializeViewStatePropsJson(ViewStateProps const& props)
{
    auto const& vd = props.viewDefinitionProps;

    JsonObjWriter vdp;
    vdp.str("classFullName", vd.classFullName);
    vdp.str("id", vd.id.ToString());
    {
        JsonObjWriter code;
        code.str("value", vd.codeValue);
        code.out.push_back('}');
        vdp.raw("code", std::move(code.out));
    }
    vdp.str("description", vd.description);
    vdp.boolean("isPrivate", vd.isPrivate);
    vdp.boolean("cameraOn", vd.cameraOn);
    vdp.raw("origin", jsonPoint3(vd.origin));
    {
        std::string extents = "[";
        appendJsonNumber(extents, vd.extents.x);
        extents.push_back(',');
        appendJsonNumber(extents, vd.extents.y);
        extents.push_back(',');
        appendJsonNumber(extents, vd.extents.z);
        extents.push_back(']');
        vdp.raw("extents", std::move(extents));
    }
    if (vd.hasAngles) {
        JsonObjWriter angles;
        angles.num("yaw", vd.yawDegrees);
        angles.num("pitch", vd.pitchDegrees);
        angles.num("roll", vd.rollDegrees);
        angles.out.push_back('}');
        vdp.raw("angles", std::move(angles.out));
    }
    {
        JsonObjWriter camera;
        camera.raw("eye", jsonPoint3(vd.camera.eye));
        camera.num("focusDist", vd.camera.focusDist);
        camera.num("lens", vd.camera.lensDegrees);
        camera.out.push_back('}');
        vdp.raw("camera", std::move(camera.out));
    }
    // jsonProperties.viewDetails（2026-10-07 一致性审计修复：原先保存面不写
    // jsonProperties——clip 在 SavedViews round-trip 中丢失；参考 EntityState.
    // toJSON :57-64 保留该段。clip 在场写线格式；disable3dManipulations 仅
    // true 写出 = 参考 setter allow 分支置 undefined 的语义）。
    if (props.viewDetailsProps.clip.has_value()
        || props.viewDetailsProps.disable3dManipulations.value_or(false)) {
        JsonObjWriter viewDetails;
        if (props.viewDetailsProps.clip.has_value())
            viewDetails.raw("clip", jsonClipVectorProps(*props.viewDetailsProps.clip));
        if (props.viewDetailsProps.disable3dManipulations.value_or(false))
            viewDetails.boolean("disable3dManipulations", true);
        viewDetails.out.push_back('}');
        JsonObjWriter jp;
        jp.raw("viewDetails", std::move(viewDetails.out));
        jp.out.push_back('}');
        vdp.raw("jsonProperties", std::move(jp.out));
    }
    vdp.out.push_back('}');

    JsonObjWriter csp;
    csp.raw("categories", jsonIdArray(props.categorySelectorProps.categories));
    csp.out.push_back('}');

    JsonObjWriter styles;
    auto const& dsp = props.displayStyleProps;
    if (dsp.viewflags.has_value())
        styles.raw("viewflags", jsonViewFlagProps(*dsp.viewflags));
    if (dsp.hline.has_value())
        styles.raw("hline", jsonHiddenLineSettingsProps(*dsp.hline));
    if (dsp.lights.has_value())
        styles.raw("lights", jsonLightSettingsProps(*dsp.lights));
    styles.out.push_back('}');
    JsonObjWriter jsonProperties;
    jsonProperties.raw("styles", std::move(styles.out));
    jsonProperties.out.push_back('}');
    JsonObjWriter displayStyle;
    displayStyle.raw("jsonProperties", std::move(jsonProperties.out));
    displayStyle.out.push_back('}');

    JsonObjWriter out;
    out.raw("viewDefinitionProps", std::move(vdp.out));
    out.raw("categorySelectorProps", std::move(csp.out));
    out.raw("displayStyleProps", std::move(displayStyle.out));
    if (props.modelSelectorProps.has_value()) {
        JsonObjWriter msp;
        msp.raw("models", jsonIdArray(props.modelSelectorProps->models));
        msp.out.push_back('}');
        out.raw("modelSelectorProps", std::move(msp.out));
    }
    out.out.push_back('}');
    return std::move(out.out);
}

// Ported from: itwinjs-core frontend-devtools deserializeViewState（SavedViews
// 恢复面——JSON 字串 → props；deserializeViewState(vsp, iModel) 的 props 半边）。
std::optional<ViewStateProps> deserializeViewStatePropsJson(std::string_view json)
{
    auto doc = dumpjson::parseJsonDocument(json);
    if (!doc)
        return std::nullopt;
    return parseViewStateProps(*doc);
}

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
    // M-O(2) I11：ecefLocation 段（数据面缺席恒 false——见头文件注释）。
    if (dumpjson::JsonValue const* ecef = conn->find("ecefLocation"))
        out->m_hasEcefLocation = ecef->type == dumpjson::JsonValue::Type::Object;

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

    // --- M-N(2)：placements.json（imodel.json 同目录，可缺席）---
    // getPlacements 的 ECSQL 行回放（ZoomToSelectedElements 数据源）。
    {
        std::string placementsPath = imodelJsonPath;
        auto const slash = placementsPath.find_last_of("/\\");
        std::string const base =
            slash == std::string::npos ? std::string() : placementsPath.substr(0, slash + 1);
        bool plOk = false;
        auto const plBytes = readFileBytesLocal(base + "placements.json", &plOk);
        if (plOk) {
            auto plDoc = dumpjson::parseJsonDocument(
                std::string(plBytes.begin(), plBytes.end()));
            if (plDoc) {
                auto const* arr = plDoc->find("placements");
                if (arr && arr->type == dumpjson::JsonValue::Type::Array) {
                    for (auto const& row : arr->arr) {
                        PlacementInfo info;
                        auto const* id = row.find("id");
                        auto const* org = row.find("origin");
                        auto const* low = row.find("bboxLow");
                        auto const* high = row.find("bboxHigh");
                        auto const* ang = row.find("angles");
                        if (!id || !org || !low || !high || !ang)
                            continue;
                        info.elementId = dqBase::DqId::FromString(id->str);
                        for (int i = 0; i < 3 && i < static_cast<int>(org->arr.size()); ++i)
                            info.origin[i] = org->arr[i].number;
                        for (int i = 0; i < 3 && i < static_cast<int>(low->arr.size()); ++i)
                            info.bboxLow[i] = low->arr[i].number;
                        for (int i = 0; i < 3 && i < static_cast<int>(high->arr.size()); ++i)
                            info.bboxHigh[i] = high->arr[i].number;
                        for (int i = 0; i < 3 && i < static_cast<int>(ang->arr.size()); ++i)
                            info.angles[i] = ang->arr[i].number;
                        out->m_placements.push_back(std::move(info));
                    }
                }
            }
        }
    }

    // ← IModelConnection.onOpen.raiseEvent(connection)（:796/:845/:863 同款）。
    OnOpen.Raise(out.Get());
    return out;
}

// elementId → placement 查找（zoomToElements 的 ids→placements 步——
// 线性扫描即可：表量级 = 元素数，zoomToElements 每次 O(n·m) 参考同量级）。
DumpIModelConnection::PlacementInfo const* DumpIModelConnection::findPlacement(
    dqBase::DqId elementId) const noexcept
{
    for (auto const& p : m_placements)
        if (p.elementId == elementId)
            return &p;
    return nullptr;
}

END_DQ_APP_NAMESPACE
