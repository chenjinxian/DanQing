// SPDX-License-Identifier: Apache-2.0
// DanQing dqGeom — PolyfaceAuxData tests
// Ported from: itwinjs-core core/geometry/src/test/polyface/PolyfaceAuxData.test.ts
#include <gtest/gtest.h>

#include <dqGeom/Angle.h>
#include <dqGeom/AuxData.h>
#include <dqGeom/Arc3d.h>
#include <dqGeom/IndexedPolyface.h>
#include <dqGeom/LineString3d.h>
#include <dqGeom/Path.h>
#include <dqGeom/PolyfaceBuilder.h>
#include <dqGeom/StrokeOptions.h>
#include <dqGeom/Transform.h>

#include <cmath>
#include <vector>

using namespace dqGeom;

namespace {

// Ported from: PolyfaceAuxData.test.ts createCantileverBeamPolyface (:27-53).
// 1:1 场景：环形截面 arc → maxEdgeLength 打线 → Path 单例包装（触碰 else 分支）→
// 逐段 addBetweenTransformedLineStrings → 高度（z*zScale）Distance 通道 + auxData
// indices = pointIndex 直拷贝。
dqBase::RefPtr<IndexedPolyface> createCantileverBeamPolyface(double beamRadius = 10.0,
                                                             double beamLength = 100.0,
                                                             double facetSize = 1.0,
                                                             double zScale = 1.0)
{
    auto builder = PolyfaceBuilder::create();
    // 参考 Arc3d.create(center, vector0, vector90) 无 sweep → 全圆（Arc3d.ts:426-434
    // + set() 的 sweep 缺席 → create360）；DanQing 等价面 = FromVectors + FullCircle。
    auto crossSectionArc = Arc3d::FromVectors(
        Point3d::From(0.0, 0.0, 0.0), Vector3d::From(0.0, beamRadius, 0.0),
        Vector3d::From(0.0, 0.0, beamRadius), AngleSweep::FullCircle());
    auto strokedCrossSection = LineString3d::create(std::vector<Point3d>{});
    StrokeOptions strokeOptions = StrokeOptions::CreateForCurves();
    strokeOptions.maxEdgeLength = facetSize;
    crossSectionArc->EmitStrokes(*strokedCrossSection, strokeOptions);
    // pack as a singleton path to touch "else"（参考 :36 注释原样）
    auto path = Path::Create(std::vector<CurvePrimitivePtr>{strokedCrossSection});
    for (double x = 0.0; x < beamLength; x += facetSize)
        builder->AddBetweenTransformedLineStrings(*path,
                                                  Transform::CreateTranslation(x, 0.0, 0.0),
                                                  Transform::CreateTranslation(x + facetSize, 0.0, 0.0),
                                                  true);

    auto polyface = builder->ClaimPolyface();
    std::vector<double> heightData;
    for (auto const& point : polyface->Data().points)
        heightData.push_back(point.z * zScale);
    AuxChannel heightChannel({AuxChannelData(0.0, heightData)}, AuxChannelDataType::Distance,
                             std::string("Height"), std::string(""));

    polyface->Data().auxData = PolyfaceAuxData({heightChannel}, polyface->Data().pointIndex);
    return polyface;
}

}  // namespace

// Ported from: PolyfaceAuxData.test.ts it("CreateAuxChannelData") (:70-83).
TEST(PolyfaceAuxDataTest, CreateAuxChannelData)
{
    AuxChannelData auxData0(1.0, {10.0, 11.0, 12.0});
    AuxChannelData auxData1 = auxData0.clone();
    AuxChannelData auxData10(2.0, {10.0, 11.0, 12.0});
    AuxChannelData auxData11(2.0, {10.0, 11.0, 12.0, 13.0});
    AuxChannelData auxData12(2.0, {10.0, 21.0, 12.0});
    EXPECT_TRUE(auxData0.isAlmostEqual(auxData1, 1.0e-5));
    EXPECT_FALSE(auxData0.isAlmostEqual(auxData10));
    EXPECT_FALSE(auxData0.isAlmostEqual(auxData11));
    EXPECT_FALSE(auxData0.isAlmostEqual(auxData12));
}

// Ported from: PolyfaceAuxData.test.ts it("CreateAuxChannel") (:84-136).
TEST(PolyfaceAuxDataTest, CreateAuxChannel)
{
    AuxChannelData data00(0.0, {1.0, 2.0, 3.0});
    AuxChannelData data10(10.0, {11.0, 12.0, 13.0});
    AuxChannelData data20(20.0, {21.0, 22.0, 25.0});
    AuxChannelData data20B(20.0, {21.0, 22.0, 26.0});
    AuxChannel channel0({data00, data10, data20}, AuxChannelDataType::Distance,
                        std::string("MyDistances"), std::string("MicrometerA"));
    AuxChannel channelA({data00, data10}, AuxChannelDataType::Distance,
                        std::string("MyDistancesA"), std::string("MicrometerA"));
    AuxChannel channelB({data00, data20, data10}, AuxChannelDataType::Distance,
                        std::string("MyDistancesB"), std::string("MicrometerA"));
    AuxChannel channelC({data00, data20, data10}, AuxChannelDataType::Distance,
                        std::string("MyDistancesC"), std::string("MicrometerA"));
    AuxChannel channelD({data00, data20B, data10}, AuxChannelDataType::Distance,
                        std::string("MyDistancesC"), std::string("MicrometerA"));

    AuxChannel channel1 = channel0.clone();
    EXPECT_TRUE(channel0.isAlmostEqual(channel0));
    EXPECT_TRUE(channel0.isAlmostEqual(channel1));

    EXPECT_FALSE(channel0.isAlmostEqual(channelA));
    EXPECT_FALSE(channel0.isAlmostEqual(channelB));
    EXPECT_FALSE(channel0.isAlmostEqual(channelC));
    EXPECT_FALSE(channelD.isAlmostEqual(channelC));

    EXPECT_EQ(data00.values.size(), channel0.valueCount());
    auto range00 = channel0.scalarRange();
    ASSERT_TRUE(range00.has_value());
    EXPECT_TRUE(range00->Contains(12.0)) << "channel range";
    EXPECT_FALSE(range00->Contains(100.0)) << "channel range";

    for (AuxChannelDataType channelType : {AuxChannelDataType::Normal, AuxChannelDataType::Vector}) {
        AuxChannelData vectorData(1.0, {10.0, 11.0, 12.0, 20.0, 21.0, 22.0, 30.0, 31.0, 32.0});
        AuxChannel vectorChannel({vectorData}, channelType, std::string("vector or normal data"),
                                 std::string("inputB"));
        EXPECT_FALSE(vectorChannel.isScalar()) << "vector type";
        EXPECT_FALSE(vectorChannel.scalarRange().has_value()) << "no scalarRange on vector channel";
        EXPECT_EQ(vectorChannel.entriesPerValue(), 3u) << "3 members in vector data";
    }
}

// Ported from: PolyfaceAuxData.test.ts it("CreatePolyfaceAuxData") (:138-153).
TEST(PolyfaceAuxDataTest, CreatePolyfaceAuxData)
{
    auto beam = createCantileverBeamPolyface(10.0, 100.0, 1.0);
    auto beam1 = createCantileverBeamPolyface(20.0, 25.0, 0.5);
    auto beam2 = createCantileverBeamPolyface(20.0, 25.0, 0.5, 4.0);
    ASSERT_TRUE(beam->Data().auxData.has_value());
    // 静态双 undefined 捕获（AuxData.ts:224-230）→ C++ nullptr 双态。
    EXPECT_FALSE(PolyfaceAuxData::isAlmostEqual(&*beam->Data().auxData, nullptr));
    EXPECT_FALSE(PolyfaceAuxData::isAlmostEqual(nullptr, &*beam->Data().auxData));
    EXPECT_TRUE(PolyfaceAuxData::isAlmostEqual(nullptr, nullptr));
    {
        PolyfaceAuxData auxDataB = beam->Data().auxData->clone();
        EXPECT_TRUE(beam->Data().auxData->isAlmostEqual(auxDataB)) << "cloned PolyfaceAuxData";
        ASSERT_TRUE(beam1->Data().auxData.has_value());
        EXPECT_FALSE(beam->Data().auxData->isAlmostEqual(*beam1->Data().auxData, 1.0e-9));
    }
    // beam1/beam2 仅 auxData 的 zScale 不同（几何全同）——false 只能由 auxData 比较腿产出
    // （PolyfaceData.isAlmostEqual 的 :201 auxData 段）。
    EXPECT_FALSE(beam1->IsAlmostEqual(*beam2, 1.0e-8));
}

// Ported from: PolyfaceAuxData.test.ts it("FlatMeshWithWaves") (:218-327) 的构造面——
// 参考断言面仅 testDefined(mesh/bytes)+FB/IMJS 文件落盘（GeometryCoreTestIO 设施，
// DanQing 无 BentleyGeometryFlatBuffer——该两条腿省略，见 W-a1 报告）；这里以结构断言
// 锚定构造产物（网格 4×4 点/9 通道/Static 1 帧 Animated 3 帧 Linear 10 帧形态）。
TEST(PolyfaceAuxDataTest, FlatMeshWithWaves)
{
    StrokeOptions options = StrokeOptions::CreateForFacets();
    options.shouldTriangulate = true;
    auto builder = PolyfaceBuilder::create(options);
    int const nDimensions = 4;
    double const spacing = 1.0;

    for (int iRow = 0; iRow < nDimensions - 1; iRow++) {
        for (int iColumn = 0; iColumn < nDimensions - 1; iColumn++) {
            Point3d const quad[4] = {
                Point3d::From(iRow * spacing, iColumn * spacing, 0.0),
                Point3d::From((iRow + 1) * spacing, iColumn * spacing, 0.0),
                Point3d::From((iRow + 1) * spacing, (iColumn + 1) * spacing, 0.0),
                Point3d::From(iRow * spacing, (iColumn + 1) * spacing, 0.0),
            };
            builder->AddQuadFacet(quad);
        }
    }

    auto polyface = builder->ClaimPolyface();
    size_t const numPoint = polyface->Data().points.size();
    ASSERT_EQ(numPoint, 16u);  // 4×4 网格点（无共享边重复——builder 逐 quad 加点，compress 去重后 16）

    std::vector<double> zeroScalarData, zeroDisplacementData, radialHeightData, radialSlopeData,
        radialDisplacementData;
    double const radius = nDimensions * spacing / 2.0;
    Point3d const center = Point3d::From(radius, radius, 0.0);
    double const maxHeight = radius / 4.0;
    std::vector<AuxChannel> auxChannels;

    // Create a radial wave - start and return to zero（参考 :248-264）
    for (auto const& pt : polyface->Data().points) {
        double const angle = Angle::k2Pi * pt.Distance(center) / radius;  // Angle.pi2Radians
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

    // Static Channels（参考 :267-269）
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(0.0, radialDisplacementData)},
                             AuxChannelDataType::Vector, std::string("Static Radial Displacement"),
                             std::string("Radial: Static"));
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(1.0, radialHeightData)},
                             AuxChannelDataType::Distance, std::string("Static Radial Height"),
                             std::string("Radial: Static"));
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(1.0, radialSlopeData)},
                             AuxChannelDataType::Scalar, std::string("Static Radial Slope"),
                             std::string("Radial: Static"));

    // Animated Channels（参考 :272-278）
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(0.0, zeroDisplacementData),
                                                         AuxChannelData(1.0, radialDisplacementData),
                                                         AuxChannelData(2.0, zeroDisplacementData)},
                             AuxChannelDataType::Vector, std::string("Animated Radial Displacement"),
                             std::string("Radial: Time"));
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(0.0, zeroScalarData),
                                                         AuxChannelData(1.0, radialHeightData),
                                                         AuxChannelData(2.0, zeroScalarData)},
                             AuxChannelDataType::Distance, std::string("Animated Radial Height"),
                             std::string("Radial: Time"));
    auxChannels.emplace_back(std::vector<AuxChannelData>{AuxChannelData(0.0, zeroScalarData),
                                                         AuxChannelData(1.0, radialSlopeData),
                                                         AuxChannelData(2.0, zeroScalarData)},
                             AuxChannelDataType::Scalar, std::string("Animated Radial Slope"),
                             std::string("Radial: Time"));

    // Linear waves -- 10 separate frames（参考 :281-309）
    double const waveHeight = radius / 20.0;
    double const waveLength = radius / 2.0;
    int const frameCount = 10;
    std::vector<AuxChannelData> linearDisplacementDataVector, linearHeightDataVector, linearSlopeDataVector;

    for (int i = 0; i < frameCount; i++) {
        double const fraction = static_cast<double>(i) / (frameCount - 1);
        double const waveCenter = waveLength * fraction;
        std::vector<double> linearHeightData, linearSlopeData, linearDisplacementData;

        for (auto const& point : polyface->Data().points) {
            double const theta = Angle::k2Pi * (point.x - waveCenter) / waveLength;
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
    auxChannels.emplace_back(linearDisplacementDataVector, AuxChannelDataType::Vector,
                             std::string("Linear Displacement"), std::string("Linear: Time"));
    auxChannels.emplace_back(linearHeightDataVector, AuxChannelDataType::Distance,
                             std::string("Linear Height"), std::string("Linear: Time"));
    auxChannels.emplace_back(linearSlopeDataVector, AuxChannelDataType::Scalar,
                             std::string("Linear Slope"), std::string("Linear: Time"));

    polyface->Data().auxData = PolyfaceAuxData(auxChannels, polyface->Data().pointIndex);

    // 结构断言（替代参考的文件落盘面）：
    ASSERT_TRUE(polyface->Data().auxData.has_value());
    EXPECT_EQ(polyface->Data().auxData->channels.size(), 9u);
    EXPECT_EQ(polyface->Data().auxData->indices.size(), polyface->Data().pointIndex.size());
    // Static 1 帧 / Animated Radial 3 帧 / Linear 10 帧：
    EXPECT_EQ(polyface->Data().auxData->channels[0].data.size(), 1u);
    EXPECT_EQ(polyface->Data().auxData->channels[3].data.size(), 3u);
    EXPECT_EQ(polyface->Data().auxData->channels[6].data.size(), 10u);
    // 每帧 valueCount = 顶点数（scalar 1 维/vector 3 维）：
    EXPECT_EQ(polyface->Data().auxData->channels[0].valueCount(), numPoint);
    EXPECT_EQ(polyface->Data().auxData->channels[1].valueCount(), numPoint);
}

// Authored: no reference test exists in itwinjs-core for PolyfaceAuxData.tryTransformInPlace
// （参考 PolyfaceAuxData.test.ts 全读确认——该文件不覆盖变换腿；变换语义锚定
// AuxData.ts:252-292 的逐类型分支与 :21-38 的枚举文档）。
// 场景锚定 AnalysisStyleExample.ts:73-74 的 createCantilever（polyface.tryTransformInPlace
// scale 30 消费链——PolyfaceData.ts:584-585 的 auxData 联动 + AuxData.ts 逐型语义）。
TEST(PolyfaceAuxDataTest, TryTransformInPlacePerDataType)
{
    // 四通道各一帧两顶点值：Scalar(不变) / Distance(×scale) / Vector(旋转+缩放) / Normal(逆转置+重规范化)
    AuxChannel scalarChannel({AuxChannelData(0.0, {1.0, 2.0})}, AuxChannelDataType::Scalar,
                             std::string("s"), std::string("i"));
    AuxChannel distanceChannel({AuxChannelData(0.0, {1.0, -2.0})}, AuxChannelDataType::Distance,
                               std::string("d"), std::string("i"));
    AuxChannel vectorChannel({AuxChannelData(0.0, {1.0, 0.0, 0.0, 0.0, 1.0, 0.0})},
                             AuxChannelDataType::Vector, std::string("v"), std::string("i"));
    AuxChannel normalChannel({AuxChannelData(0.0, {1.0, 0.0, 0.0, 0.0, 1.0, 0.0})},
                             AuxChannelDataType::Normal, std::string("n"), std::string("i"));
    PolyfaceAuxData auxData({scalarChannel, distanceChannel, vectorChannel, normalChannel}, {1, 2});

    // 变换 = 绕 Z 90° 旋转 × 均匀缩放 2（det = 8 > 0 → scale = 2）。
    Transform rot = Transform::CreateRotationAboutAxis(Point3d::From(0.0, 0.0, 0.0),
                                                       Vector3d::From(0.0, 0.0, 1.0), M_PI / 2.0);
    Matrix3d const scale2 = Matrix3d::CreateScale(2.0, 2.0, 2.0);
    Transform const transform = Transform::CreateOriginAndMatrix(
        Point3d::From(0.0, 0.0, 0.0), rot.GetMatrix().MultiplyMatrix(scale2));
    double const det = transform.GetMatrix().Determinant();
    ASSERT_NEAR(det, 8.0, 1.0e-12);
    double const scale = std::pow(std::fabs(det), 1.0 / 3.0);  // AuxData.ts:256 原样 pow

    EXPECT_TRUE(auxData.tryTransformInPlace(transform));

    // Scalar 不变：
    EXPECT_DOUBLE_EQ(auxData.channels[0].data[0].values[0], 1.0);
    EXPECT_DOUBLE_EQ(auxData.channels[0].data[0].values[1], 2.0);
    // Distance × scale：
    EXPECT_NEAR(auxData.channels[1].data[0].values[0], scale, 1.0e-12);
    EXPECT_NEAR(auxData.channels[1].data[0].values[1], -2.0 * scale, 1.0e-12);
    // Vector 旋转+缩放：(1,0,0)→(0,scale,0)、(0,1,0)→(-scale,0,0)：
    EXPECT_NEAR(auxData.channels[2].data[0].values[0], 0.0, 1.0e-12);
    EXPECT_NEAR(auxData.channels[2].data[0].values[1], scale, 1.0e-12);
    EXPECT_NEAR(auxData.channels[2].data[0].values[3], -scale, 1.0e-12);
    EXPECT_NEAR(auxData.channels[2].data[0].values[4], 0.0, 1.0e-12);
    // Normal 逆转置+重规范化（均匀缩放下=旋转，单位长保持）：(1,0,0)→(0,1,0)、(0,1,0)→(-1,0,0)：
    EXPECT_NEAR(auxData.channels[3].data[0].values[0], 0.0, 1.0e-12);
    EXPECT_NEAR(auxData.channels[3].data[0].values[1], 1.0, 1.0e-12);
    EXPECT_NEAR(auxData.channels[3].data[0].values[3], -1.0, 1.0e-12);
    EXPECT_NEAR(auxData.channels[3].data[0].values[4], 0.0, 1.0e-12);
}

// Authored: no reference test exists in itwinjs-core for the IndexedPolyface.tryTransformInPlace →
// auxData.tryTransformInPlace 联动（PolyfaceData.ts:584-585 的调用腿无测试覆盖——实证见 W-a1 报告）。
// 场景锚定 AnalysisStyleExample.ts:73-74（createCantilever 对 polyface 调 tryTransformInPlace(scale 30)）。
TEST(PolyfaceAuxDataTest, PolyfaceTransformTransformsAuxData)
{
    auto beam = createCantileverBeamPolyface(10.0, 100.0, 1.0);
    ASSERT_TRUE(beam->Data().auxData.has_value());
    double const zBefore = beam->Data().auxData->channels[0].data[0].values[0];

    Transform const scale30 = Transform::CreateFixedPointAndMatrix(
        Point3d::From(0.0, 0.0, 0.0), Matrix3d::CreateScale(30.0, 30.0, 30.0));
    EXPECT_TRUE(beam->TryTransformInPlace(scale30));

    // Distance 通道 ×30（scale=|det|^(1/3)=30）；点坐标同样 ×30（点变换原有语义不动）。
    ASSERT_TRUE(beam->Data().auxData.has_value());
    EXPECT_NEAR(beam->Data().auxData->channels[0].data[0].values[0], zBefore * 30.0, 1.0e-8);
    EXPECT_NEAR(beam->Data().points[0].Magnitude(), 30.0 * 10.0, 1.0e-6);  // 环半径 10 → 300
}

// ---------------------------------------------------------------------------
// W-a2：IModelJson Reader/Writer 子集（indexedMesh+auxData）消费锁
// ---------------------------------------------------------------------------

#include <dqGeom/Geometry.h>
#include <dqGeom/IModelJson.h>

#include <fstream>
#include <sstream>

#ifndef DANQING_GEOM_TEST_DATA_DIR
#define DANQING_GEOM_TEST_DATA_DIR "."
#endif
#ifndef DANQING_DTA_ASSETS_DIR
#define DANQING_DTA_ASSETS_DIR "."
#endif

namespace {

// 装载 JSON 文本（1:1 参考 GeometryCoreTestIO.jsonFileToIndexedPolyface 的读文件腿）。
std::string loadTextFile(std::string const& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

}  // namespace

// Ported from: PolyfaceAuxData.test.ts it("testRoundTrip") (:57-68)。
// 参考经 IModelJson.Writer.emit + Reader.parse 对象级 round-trip（非文本序列化——
// DanQing 同构：Writer 产 JsonValue DOM、Reader 直消费）。
TEST(PolyfaceAuxDataTest, TestRoundTrip)
{
    auto beam = createCantileverBeamPolyface();

    IModelJson::Writer writer;
    auto json = writer.emit(*beam);
    ASSERT_TRUE(json.has_value());
    auto roundTrippedBeam = IModelJson::Reader::parse(*json);
    ASSERT_TRUE(roundTrippedBeam.IsValid());

    EXPECT_TRUE(beam->IsAlmostEqual(*roundTrippedBeam, 1.0e-8));
}

// Ported from: PolyfaceAuxData.test.ts it("Compress") (:155-215) ——
// 网格源 rhombicosidodecahedron.imjs（ImportedSample.createPolyhedron62 的同文件装载
// 等价——GeometryCoreTestIO.jsonFileToIndexedPolyface → IModelJson::Reader）。
// FB 序列化 sanity 腿（BentleyGeometryFlatBuffer）未移植省略；JSON round-trip 腿保留
// （DanQing IModelJson Writer/Reader 在位）。
TEST(PolyfaceAuxDataTest, Compress)
{
    std::string const path =
        std::string(DANQING_GEOM_TEST_DATA_DIR) + "/polyface/rhombicosidodecahedron.imjs";
    std::string const text = loadTextFile(path);
    ASSERT_FALSE(text.empty()) << "load " << path;
    auto json = imodeljson::parseJsonDocument(text);
    ASSERT_TRUE(json.has_value());
    auto meshQ = IModelJson::Reader::parse(*json);
    // 顶层为数组包装（jsonFileToIndexedPolyface 语义——取第一个 IndexedPolyface）。
    if (!meshQ.IsValid() && json->type == imodeljson::JsonValue::Type::Array) {
        for (auto& g : IModelJson::Reader::parseArray(*json)) {
            if (g.IsValid() && g->Category() == GeometryCategory::Polyface) {
                meshQ = g;
                break;
            }
        }
    }
    ASSERT_TRUE(meshQ.IsValid());
    ASSERT_EQ(meshQ->Category(), GeometryCategory::Polyface);
    auto& mesh = static_cast<IndexedPolyface&>(*meshQ);

    // latitude 通道（z 值——Distance "Latitude"/"Time"）+ octant 通道（split3Way01 —— Vector "Octant"/"Time"）。
    std::vector<double> latitude;
    for (auto const& pt : mesh.Data().points)
        latitude.push_back(pt.z);
    AuxChannel latitudeChannel({AuxChannelData(0.0, latitude)}, AuxChannelDataType::Distance,
                               std::string("Latitude"), std::string("Time"));

    std::vector<double> octant;
    for (auto const& pt : mesh.Data().points) {
        octant.push_back(static_cast<double>(split3Way01(pt.x)));
        octant.push_back(static_cast<double>(split3Way01(pt.y)));
        octant.push_back(static_cast<double>(split3Way01(pt.z)));
    }
    AuxChannel octantChannel({AuxChannelData(0.0, octant)}, AuxChannelDataType::Vector,
                             std::string("Octant"), std::string("Time"));

    // mesh0：单 1D 通道 → auxData 被压缩（13 值）。
    auto mesh0Q = mesh.clone();
    auto& mesh0 = static_cast<IndexedPolyface&>(*mesh0Q);
    mesh0.Data().auxData = PolyfaceAuxData({latitudeChannel.clone()}, mesh0.Data().pointIndex);
    mesh0.Data().Compress();
    EXPECT_NE(mesh.Data().pointIndex, mesh0.Data().auxData->indices)
        << "Single 1D channel AuxData was compressed";
    EXPECT_EQ(mesh0.Data().auxData->channels[0].valueCount(), 13u)
        << "Compress leaves expected number of AuxData 1D values";

    // mesh1：单 3D 通道 → auxData 被压缩（20 值）。
    auto mesh1Q = mesh.clone();
    auto& mesh1 = static_cast<IndexedPolyface&>(*mesh1Q);
    mesh1.Data().auxData = PolyfaceAuxData({octantChannel.clone()}, mesh1.Data().pointIndex);
    mesh1.Data().Compress();
    EXPECT_NE(mesh.Data().pointIndex, mesh1.Data().auxData->indices)
        << "Single 3D channel AuxData was compressed";
    EXPECT_EQ(mesh1.Data().auxData->channels[0].valueCount(), 20u)
        << "Compress leaves expected number of AuxData 3D values";

    // mesh2：双通道 → auxData 不压缩（60 值、indices 保持原 pointIndex）。
    auto mesh2Q = mesh.clone();
    auto& mesh2 = static_cast<IndexedPolyface&>(*mesh2Q);
    mesh2.Data().auxData = PolyfaceAuxData({latitudeChannel.clone(), octantChannel.clone()},
                                           mesh2.Data().pointIndex);
    mesh2.Data().Compress();
    EXPECT_EQ(mesh.Data().pointIndex, mesh2.Data().auxData->indices)
        << "Multichannel AuxData indices was untouched by compress";
    EXPECT_EQ(mesh2.Data().auxData->channels[0].valueCount(), 60u)
        << "Compress leaves multichannel AuxData 1D values untouched";
    EXPECT_EQ(mesh2.Data().auxData->channels[1].valueCount(), 60u)
        << "Compress leaves multichannel AuxData 3D values untouched";

    // sanity check roundtrips（JSON 腿保留——参考 :187-194 的 IModelJson 半）。
    for (IndexedPolyface* meshWithAuxData : {&mesh0, &mesh1, &mesh2}) {
        IModelJson::Writer writer;
        auto jsonOut = writer.emit(*meshWithAuxData);
        ASSERT_TRUE(jsonOut.has_value());
        auto meshJson = IModelJson::Reader::parse(*jsonOut);
        ASSERT_TRUE(meshJson.IsValid());
        ASSERT_EQ(meshJson->Category(), GeometryCategory::Polyface);
        EXPECT_TRUE(meshWithAuxData->IsAlmostEqual(static_cast<IndexedPolyface&>(*meshJson), 1.0e-8))
            << "roundtrip through JSON";
    }
}

// Authored: no reference test exists in itwinjs-core for IModelJson.Reader 对
// Cantilever.ts 资产的消费（参考的 Cantilever 消费面在 display-test-app
// AnalysisStyleExample.ts:66-77，非 geometry 测试）——截段断言锚定资产键面
// （W-a2 计划锁：顶点数/索引数/通道计数/输入序列）。
TEST(PolyfaceAuxDataTest, CantileverAssetLoadsWithAuxData)
{
    std::string const path = std::string(DANQING_DTA_ASSETS_DIR) + "/cantilever.imjs";
    std::string const text = loadTextFile(path);
    ASSERT_FALSE(text.empty()) << "load " << path;
    auto json = imodeljson::parseJsonDocument(text);
    ASSERT_TRUE(json.has_value());
    auto parsed = IModelJson::Reader::parse(*json);
    ASSERT_TRUE(parsed.IsValid());
    ASSERT_EQ(parsed->Category(), GeometryCategory::Polyface);
    auto& pf = static_cast<IndexedPolyface&>(*parsed);

    // 资产钉值（截段读取断言——值取自资产 JSON 实测）：
    // 顶点 1424 / facet 1422（pointIndex 无终止符 5688=7110-1422 终止符）/ auxData 2 通道
    // （Vector "Displacement" 3 帧 input 0/1/2 + Scalar "Stress" 3 帧 input 0/1/2）。
    EXPECT_EQ(pf.Data().points.size(), 1424u);
    EXPECT_EQ(pf.Data().pointIndex.size(), 5688u);
    EXPECT_EQ(pf.FacetCount(), 1422u);
    ASSERT_TRUE(pf.Data().auxData.has_value());
    ASSERT_EQ(pf.Data().auxData->channels.size(), 2u);
    auto const& ch0 = pf.Data().auxData->channels[0];
    auto const& ch1 = pf.Data().auxData->channels[1];
    EXPECT_TRUE(ch0.dataType == AuxChannelDataType::Vector);
    EXPECT_EQ(ch0.name.value_or(""), "Displacement");
    EXPECT_TRUE(ch1.dataType == AuxChannelDataType::Scalar);
    EXPECT_EQ(ch1.name.value_or(""), "Stress");
    ASSERT_EQ(ch0.data.size(), 3u);
    EXPECT_DOUBLE_EQ(ch0.data[0].input, 0.0);
    EXPECT_DOUBLE_EQ(ch0.data[1].input, 1.0);
    EXPECT_DOUBLE_EQ(ch0.data[2].input, 2.0);
    ASSERT_EQ(ch1.data.size(), 3u);
    EXPECT_DOUBLE_EQ(ch1.data[0].input, 0.0);
    EXPECT_DOUBLE_EQ(ch1.data[1].input, 1.0);
    EXPECT_DOUBLE_EQ(ch1.data[2].input, 2.0);
    // 通道值数 = 顶点数 × entriesPerValue：
    EXPECT_EQ(ch0.data[0].values.size(), 1424u * 3u);
    EXPECT_EQ(ch1.data[0].values.size(), 1424u);
    // auxData.indices parallel to pointIndex（无终止符）：
    EXPECT_EQ(pf.Data().auxData->indices.size(), pf.Data().pointIndex.size());

    // zero-based 转换对账（round-trip 语义腿）：writer 写回 → reader 重读 → 全等。
    IModelJson::Writer writer;
    auto jsonOut = writer.emit(pf);
    ASSERT_TRUE(jsonOut.has_value());
    auto reparsed = IModelJson::Reader::parse(*jsonOut);
    ASSERT_TRUE(reparsed.IsValid());
    EXPECT_TRUE(pf.IsAlmostEqual(*reparsed, 1.0e-8));
}
