// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — AnalysisStyleExample implementation
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/AnalysisStyleExample.ts
#include "AnalysisStyleExample.h"

#include <QComboBox>

#include <dqApp/Application.h>
#include <dqApp/DecorateContext.h>
#include <dqApp/DisplayStyle.h>
#include <dqApp/IModelConnection.h>
#include <dqApp/StandardView.h>
#include <dqApp/ViewManager.h>
#include <dqApp/Viewport.h>
#include <dqApp/ViewState.h>

#include <dqBase/DqEvent.h>  // DqEventScope

#include <dqCommon/AnalysisStyle.h>
#include <dqCommon/ColorDef.h>
#include <dqCommon/ThematicDisplay.h>

#include <dqGeom/Angle.h>
#include <dqGeom/IModelJson.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/StrokeOptions.h>
#include <dqGeom/Transform.h>

#include <dqRender/GraphicBuilder.h>
#include <dqRender/RenderGraphic.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

#ifndef DANQING_DTA_ASSETS_DIR
#define DANQING_DTA_ASSETS_DIR "."
#endif

namespace dta {

namespace {

// Ported from: AnalysisStyleExample.ts:24 seaMountain 五色键（value, r, g, b, t）。
constexpr double kSeaMountain[6][5] = {
    {0.0, 0, 255, 0, 0x00}, {0.2, 72, 96, 160, 0x3f}, {0.4, 152, 96, 160, 0x7f},
    {0.6, 128, 32, 104, 0xbf}, {0.7, 148, 180, 128, 0xff}, {1.0, 240, 240, 240, 0x00},
};

// ---------------------------------------------------------------------------
// AnalysisDecorator（:187-240）——IDecorator 承载；样式变更 → disposeGraphic 重建。
// ---------------------------------------------------------------------------
class AnalysisDecorator : public dqApp::IDecorator {
public:
    AnalysisDecorator(dqApp::Viewport& viewport, AnalysisMesh const& mesh)
        : m_viewport(&viewport)
        , m_mesh(mesh)
    {
        // 参考 :200-203 addOnAnalysisStyleChangedListener（样式变→disposeGraphic 重建）。
        // DanQing：订阅当前 DisplayStyle 的 OnAnalysisStyleChanged（dqApp
        // DisplayStyle.h:60）。EQUIVALENCE：参考面随 view/style 替换重订阅
        // （Viewport.ts:3051-3073）；DanQing 装饰器在样式表切换的宿主侧销毁重建
        // （populateStylePicker/网格切换即重建 decorator——事件链不断）。
        // 订阅令牌自持（DqEventScope——M-P 的悬垂退订 saga 在案）。
        auto& style = viewport.GetView()->GetDisplayStyle();
        m_styleScope.add(style.OnAnalysisStyleChanged.AddListener([this]() {
            disposeGraphic();
            // 参考由 viewport 失效级联驱动重绘；DanQing 直推（InvalidateController →
            // 下一帧装饰收集重建）。
            m_viewport->InvalidateController();
        }));
        // 参考 :199 onDisposed → dispose；DanQing：QWidget destroyed 信号承载。
        // 连接句柄自持 + 析构断开（M-W 回修——无 context 的连接在 decorator
        // 先死、viewport 后毁时对悬垂 this 放 lambda → UAF/堆腐蚀；本仓既有
        // 模式是 connect(sender, destroyed, context, lambda) 经 context 自动
        // 断开[DtaToolBars.cpp:318 同族]，decorator 非 QObject → 显式断开户）。
        m_vpDestroyedConn = QObject::connect(&viewport, &QObject::destroyed, [this]() {
            disposeGraphic();
        });
        dqApp::Application::Get().GetViewManager().AddDecorator(this);
    }

    ~AnalysisDecorator() override
    {
        if (m_vpDestroyedConn)
            QObject::disconnect(m_vpDestroyedConn);
        dqApp::Application::Get().GetViewManager().DropDecorator(this);
    }

    // 参考 [Symbol.dispose]（:213-224）的显式形态（C++ 无 symbol-dispose 协议）。
    void disposeGraphic()
    {
        if (m_graphicOwner != nullptr) {
            m_graphicOwner->disposeGraphic();  // disposeGraphic 在 owner 面（RenderGraphic.h:80）
            m_graphicOwner = nullptr;
            m_graphic = nullptr;
        }
    }

    void Decorate(dqApp::DecorateContext& context) override
    {
        // 参考 :227-228：context.viewport !== this._viewport 门。
        if (&context.GetViewport() != m_viewport)
            return;

        if (m_graphic == nullptr) {
            // 参考 :231-236：createGraphicBuilder(Scene, undefined, id) +
            // setSymbology(darkSlateBlue) + addPolyface + createGraphicOwner。
            dqRender::GraphicBuilderOptions opts;
            opts.type = dqRender::GraphicType::Scene;
            // computeChordTolerance 闭包（PrimitiveBuilder LOD 门——AcsTriadDecorator
            // 先例：视口 pixel size 供给）。E4：pickable id 不接线（见 .h 登记）。
            double const worldPerPixel =
                m_viewport->GetViewingSpace().getPixelSizeAtPoint(nullptr);
            opts.computeChordTolerance = [worldPerPixel]() { return worldPerPixel; };
            auto builder = m_viewport->createGraphicBuilder(opts);
            if (!builder)
                return;
            // ColorByName.darkSlateBlue = 0x2F4F4F（web 具名色——参考 :232）。
            auto const darkSlateBlue = dqCommon::ColorDef::from(0x2F, 0x4F, 0x4F);
            builder->setSymbology(darkSlateBlue, darkSlateBlue, 1);
            builder->addPolyface(*m_mesh.polyface, false);
            auto* finished = builder->finish();
            if (finished == nullptr)
                return;
            m_graphic = finished;
            m_graphicOwner = m_viewport->createGraphicOwner(finished);
        }

        context.AddDecoration(dqRender::GraphicType::Scene, m_graphic);
    }

    bool TestDecorationHit(uint32_t) const override { return false; }  // E4：不拾取
    QString GetDecorationToolTip(uint32_t) const override { return {}; }

    AnalysisMesh const& mesh() const noexcept { return m_mesh; }

private:
    dqApp::Viewport* m_viewport;
    AnalysisMesh m_mesh;  // polyface RefPtr 共享——DanQing 值成员承载（参考 readonly 字段）
    dqRender::RenderGraphic* m_graphic = nullptr;
    dqRender::RenderGraphicOwner* m_graphicOwner = nullptr;
    dqBase::DqEventScope m_styleScope;  // OnAnalysisStyleChanged 订阅令牌（自持）
    QMetaObject::Connection m_vpDestroyedConn;  // viewport destroyed 订阅（析构断开——防悬垂 lambda UAF）
};

}  // namespace

// 测试缝（W-a6 E2E）——AnalysisDecorator 的生产出口（匿名命名空间类的工厂）。
dqApp::IDecorator* createAnalysisStyleDecorator(dqApp::Viewport& viewport,
                                                AnalysisMesh const& mesh)
{
    return new AnalysisDecorator(viewport, mesh);
}

// Ported from: AnalysisStyleExample.ts populateAnalysisStyles（:26-64）。
void populateAnalysisStyles(AnalysisMesh& mesh, double displacementScale)
{
    auto const& auxData = mesh.polyface->Data().auxData;
    if (!auxData.has_value())
        return;

    mesh.styles.clear();
    mesh.styles.emplace_back("None", std::nullopt);  // :31

    for (auto const& channel : auxData->channels) {
        if (!channel.name.has_value() || !channel.isScalar())
            continue;

        // :36——配套 Vector 通道（同 inputName + dataType==Vector）。
        dqGeom::AuxChannel const* displacementChannel = nullptr;
        for (auto const& x : auxData->channels) {
            if (x.inputName == channel.inputName
                && x.dataType == dqGeom::AuxChannelDataType::Vector) {
                displacementChannel = &x;
                break;
            }
        }

        dqCommon::ThematicGradientSettingsProps thematicSettings;
        if (channel.name->size() >= 6
            && channel.name->compare(channel.name->size() - 6, 6, "Height") == 0) {
            thematicSettings.mode = dqCommon::ThematicGradientMode::SteppedWithDelimiter;
            thematicSettings.colorScheme = dqCommon::ThematicGradientColorScheme::Custom;
            std::vector<dqCommon::GradientKeyColorProps> customKeys;
            for (auto const& x : kSeaMountain) {
                dqCommon::GradientKeyColorProps key;
                key.value = x[0];
                key.color = dqCommon::ColorDef::computeTbgrFromComponents(
                    static_cast<int>(x[1]), static_cast<int>(x[2]), static_cast<int>(x[3]),
                    static_cast<int>(x[4]));
                customKeys.push_back(key);
            }
            thematicSettings.customKeys = std::move(customKeys);
        }

        auto const scalarRange = channel.scalarRange();
        // 参考 :46 assert(undefined !== channel.scalarRange)——scalar 通道恒有。
        if (!scalarRange.has_value())
            continue;

        dqCommon::AnalysisStyleProps props;
        dqCommon::AnalysisStyleThematicProps scalar;
        scalar.channelName = *channel.name;
        scalar.range = *scalarRange;
        scalar.thematicSettings = std::move(thematicSettings);
        props.scalar = std::move(scalar);

        std::string name = *channel.name;
        if (displacementChannel != nullptr && displacementChannel->name.has_value()) {
            dqCommon::AnalysisStyleDisplacementProps disp;
            disp.channelName = *displacementChannel->name;
            disp.scale = displacementScale;
            props.displacement = disp;
            // :58 的 exaggeration 段（scale==1 时恒空串——参考疑似复制粘贴怪癖
            // `1 !== displacementScale ? "" : ...` 反向，原样注记：参考在 scale≠1
            // 时不附加——本仓 1:1 照抄该怪癖）。
            std::string const exaggeration =
                1.0 != displacementScale ? "" : (" X " + std::to_string(static_cast<int>(displacementScale)));
            name = name + " and " + *displacementChannel->name + exaggeration;
        }

        mesh.styles.emplace_back(std::move(name), dqCommon::AnalysisStyle::fromJSON(&props));
    }
}

// Ported from: AnalysisStyleExample.ts createCantilever（:66-77）。
dqBase::RefPtr<dqGeom::IndexedPolyface> createCantilever()
{
    std::string const path = std::string(DANQING_DTA_ASSETS_DIR) + "/cantilever.imjs";
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return nullptr;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    auto json = dqGeom::imodeljson::parseJsonDocument(oss.str());
    if (!json.has_value())
        return nullptr;
    auto parsed = dqGeom::IModelJson::Reader::parse(*json);
    if (!parsed.IsValid() || parsed->Category() != dqGeom::GeometryCategory::Polyface)
        return nullptr;
    auto polyface = dqBase::RefPtr<dqGeom::IndexedPolyface>(
        static_cast<dqGeom::IndexedPolyface*>(parsed.Get()));

    // :73-74——Transform.createScaleAboutPoint(new Point3d(), 30) + tryTransformInPlace。
    auto const transform = dqGeom::Transform::CreateScaleAboutPoint(
        dqGeom::Point3d::FromZero(), 30.0);
    polyface->TryTransformInPlace(transform);

    return polyface;
}

// Ported from: AnalysisStyleExample.ts createFlatMeshWithWaves（:85-177）。
dqBase::RefPtr<dqGeom::IndexedPolyface> createFlatMeshWithWaves()
{
    auto options = dqGeom::StrokeOptions::CreateForFacets();
    options.shouldTriangulate = true;
    auto builder = dqGeom::PolyfaceBuilder::create(options);
    int const nDimensions = 100;
    double const spacing = 1.0;

    // Create a simple flat mesh with 10,000 points (100x100)（:92-103）。
    for (int iRow = 0; iRow < nDimensions - 1; iRow++) {
        for (int iColumn = 0; iColumn < nDimensions - 1; iColumn++) {
            dqGeom::Point3d const quad[4] = {
                dqGeom::Point3d::From(iRow * spacing, iColumn * spacing, 0.0),
                dqGeom::Point3d::From((iRow + 1) * spacing, iColumn * spacing, 0.0),
                dqGeom::Point3d::From((iRow + 1) * spacing, (iColumn + 1) * spacing, 0.0),
                dqGeom::Point3d::From(iRow * spacing, (iColumn + 1) * spacing, 0.0),
            };
            builder->AddQuadFacet(quad);
        }
    }

    auto polyface = builder->ClaimPolyface();
    std::vector<double> zeroScalarData, zeroDisplacementData, radialHeightData, radialSlopeData,
        radialDisplacementData;
    double const radius = nDimensions * spacing / 2.0;
    auto const center = dqGeom::Point3d::From(radius, radius, 0.0);
    double const maxHeight = radius / 4.0;
    std::vector<dqGeom::AuxChannel> auxChannels;

    // Create a radial wave - start and return to zero（:112-128）。
    for (auto const& point : polyface->Data().points) {
        double const angle = dqGeom::Angle::k2Pi * point.Distance(center) / radius;
        double const height = maxHeight * std::sin(angle);
        double const slope = std::fabs(std::cos(angle));

        zeroScalarData.push_back(0.0);
        zeroDisplacementData.push_back(0.0);
        zeroDisplacementData.push_back(0.0);
        zeroDisplacementData.push_back(0.0);

        radialHeightData.push_back(height);
        radialSlopeData.push_back(slope);
        radialDisplacementData.push_back(0.0);
        radialDisplacementData.push_back(0.0);
        radialDisplacementData.push_back(height);
    }

    // Static Channels（:131-133）。
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, radialDisplacementData)},
        dqGeom::AuxChannelDataType::Vector, std::string("Static Radial Displacement"),
        std::string("Radial: Static"));
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(1.0, radialHeightData)},
        dqGeom::AuxChannelDataType::Distance, std::string("Static Radial Height"),
        std::string("Radial: Static"));
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(1.0, radialSlopeData)},
        dqGeom::AuxChannelDataType::Scalar, std::string("Static Radial Slope"),
        std::string("Radial: Static"));

    // Animated Channels（:136-142）。
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroDisplacementData),
                                            dqGeom::AuxChannelData(1.0, radialDisplacementData),
                                            dqGeom::AuxChannelData(2.0, zeroDisplacementData)},
        dqGeom::AuxChannelDataType::Vector, std::string("Animated Radial Displacement"),
        std::string("Radial: Time"));
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroScalarData),
                                            dqGeom::AuxChannelData(1.0, radialHeightData),
                                            dqGeom::AuxChannelData(2.0, zeroScalarData)},
        dqGeom::AuxChannelDataType::Distance, std::string("Animated Radial Height"),
        std::string("Radial: Time"));
    auxChannels.emplace_back(
        std::vector<dqGeom::AuxChannelData>{dqGeom::AuxChannelData(0.0, zeroScalarData),
                                            dqGeom::AuxChannelData(1.0, radialSlopeData),
                                            dqGeom::AuxChannelData(2.0, zeroScalarData)},
        dqGeom::AuxChannelDataType::Scalar, std::string("Animated Radial Slope"),
        std::string("Radial: Time"));

    // Create linear waves -- 10 separate frames（:144-173）。
    double const waveHeight = radius / 20.0;
    double const waveLength = radius / 2.0;
    int const frameCount = 10;
    std::vector<dqGeom::AuxChannelData> linearDisplacementDataVector, linearHeightDataVector,
        linearSlopeDataVector;

    for (int i = 0; i < frameCount; i++) {
        double const fraction = static_cast<double>(i) / (frameCount - 1);
        double const waveCenter = waveLength * fraction;
        std::vector<double> linearHeightData, linearSlopeData, linearDisplacementData;

        for (auto const& point : polyface->Data().points) {
            double const theta = dqGeom::Angle::k2Pi * (point.x - waveCenter) / waveLength;
            double const height = waveHeight * std::sin(theta);
            double const slope = std::fabs(std::cos(theta));

            linearHeightData.push_back(height);
            linearSlopeData.push_back(slope);
            linearDisplacementData.push_back(0.0);
            linearDisplacementData.push_back(0.0);
            linearDisplacementData.push_back(height);
        }
        linearDisplacementDataVector.emplace_back(static_cast<double>(i), linearDisplacementData);
        linearHeightDataVector.emplace_back(static_cast<double>(i), linearHeightData);
        linearSlopeDataVector.emplace_back(static_cast<double>(i), linearSlopeData);
    }
    auxChannels.emplace_back(std::move(linearDisplacementDataVector),
                             dqGeom::AuxChannelDataType::Vector, std::string("Linear Displacement"),
                             std::string("Linear: Time"));
    auxChannels.emplace_back(std::move(linearHeightDataVector),
                             dqGeom::AuxChannelDataType::Distance, std::string("Linear Height"),
                             std::string("Linear: Time"));
    auxChannels.emplace_back(std::move(linearSlopeDataVector),
                             dqGeom::AuxChannelDataType::Scalar, std::string("Linear Slope"),
                             std::string("Linear: Time"));

    polyface->Data().auxData =
        dqGeom::PolyfaceAuxData(std::move(auxChannels), polyface->Data().pointIndex);
    return polyface;
}

// Ported from: AnalysisStyleExample.ts openAnalysisStyleExample（:242-299）。
void openAnalysisStyleExample(dqApp::Viewport& viewport, QWidget* toolbarParent)
{
    // :243——双网格装载（Cantilever scale=100 / Flat with waves scale=1）。
    // meshes 堆持有（shared_ptr）——combo 的 Qt 连接 lambda 在函数返回后存活
    // （参考的模块级 `meshes` 闭包语义），值捕获免悬垂。
    auto meshes = std::make_shared<std::vector<AnalysisMesh>>();
    {
        AnalysisMesh cantilever;
        cantilever.type = "Cantilever";
        cantilever.polyface = createCantilever();
        if (cantilever.polyface.IsValid()) {
            populateAnalysisStyles(cantilever, 100.0);
            meshes->push_back(std::move(cantilever));
        }
    }
    {
        AnalysisMesh waves;
        waves.type = "Flat with waves";
        waves.polyface = createFlatMeshWithWaves();
        if (waves.polyface.IsValid()) {
            populateAnalysisStyles(waves, 1.0);
            meshes->push_back(std::move(waves));
        }
    }
    if (meshes->empty())
        return;

    // E2：双 DOM select → 双 QComboBox（toolbarParent 承载）。
    auto* meshPicker = new QComboBox(toolbarParent);
    auto* stylePicker = new QComboBox(toolbarParent);

    // decorator 的共享指针（双 combo 的 lambda 共用；参考的 `let decorator` 语义）。
    auto decoratorHolder = std::make_shared<AnalysisDecorator*>(nullptr);
    *decoratorHolder = new AnalysisDecorator(viewport, (*meshes)[0]);

    for (auto const& mesh : *meshes)
        meshPicker->addItem(QString::fromUtf8(mesh.type.c_str()));
    meshPicker->setCurrentIndex(0);

    auto populateStylePicker = [&viewport, meshes, decoratorHolder, stylePicker]() {
        stylePicker->clear();
        for (auto const& entry : (*decoratorHolder)->mesh().styles)
            stylePicker->addItem(QString::fromUtf8(entry.first.c_str()));
        // :283——初始/重建时 analysisStyle=undefined。
        viewport.GetView()->GetDisplayStyle().setAnalysisStyle(std::nullopt);
    };

    QObject::connect(meshPicker, &QComboBox::currentIndexChanged,
                     [&viewport, meshes, decoratorHolder, stylePicker, populateStylePicker](int index) {
        if (index < 0 || static_cast<size_t>(index) >= meshes->size())
            return;
        if ((*meshes)[static_cast<size_t>(index)].type == (*decoratorHolder)->mesh().type)
            return;
        // :260-263——dispose 旧 decorator + 新建 + 重填样式表。
        (*decoratorHolder)->disposeGraphic();
        delete *decoratorHolder;
        *decoratorHolder = new AnalysisDecorator(viewport, (*meshes)[static_cast<size_t>(index)]);
        populateStylePicker();
    });

    QObject::connect(stylePicker, &QComboBox::currentIndexChanged,
                     [&viewport, decoratorHolder](int index) {
        auto const& styles = (*decoratorHolder)->mesh().styles;
        if (index < 0 || static_cast<size_t>(index) >= styles.size())
            return;
        // :269-271——样式表值（"None" → nullopt）置位。
        viewport.GetView()->GetDisplayStyle().setAnalysisStyle(
            styles[static_cast<size_t>(index)].second);
    });

    populateStylePicker();

    // :288-298——Iso 旋转 + zoomToVolume(projectExtents) + SolidFill + 双色天空盒。
    auto* view3d = viewport.GetView()->AsViewState3d();
    if (view3d != nullptr) {
        view3d->SetRotation(dqApp::StandardView::GetStandardRotation(dqApp::StandardViewId::Iso));
        auto* imodel = viewport.GetIModel();
        if (imodel != nullptr)
            view3d->LookAtVolume(imodel->GetProjectExtents());

        auto& style = viewport.GetView()->GetDisplayStyle();
        auto vfProps = style.getViewFlags().Properties();
        vfProps.renderMode = dqCommon::RenderMode::SolidFill;
        style.setViewFlags(dqCommon::ViewFlags(vfProps));

        // 双色天空盒（:294-298——SkyBox.fromJSON({twoColor, nadirColor, zenithColor})）。
        dqCommon::SkyBoxProps skyProps;
        skyProps.twoColor = true;
        skyProps.nadirColor = 0xdfefff;
        skyProps.zenithColor = 0xffefdf;
        auto env = style.getSettings().getEnvironment().clone();
        env.sky = dqCommon::SkyBox::fromJSON(&skyProps);
        env.displaySky = true;
        style.getSettings().setEnvironment(env);
    }

    viewport.synchWithView(dqApp::ViewChangeOptions{/*noSaveInUndo=*/true});
}

}  // namespace dta
