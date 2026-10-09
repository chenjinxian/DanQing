// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — ThematicSensors GPU chain tests (M-S S-e)
//
// Authored: no reference test exists in itwinjs-core for the sensor texture
//           GPU path（行为锚定 ThematicSensors.ts create/_update/bindTexture +
//           BatchUniforms._setCurrentBatch :74-82 的逐 batch 分流 + Graphic.ts
//           :111-120 的 PerTargetBatchData 缓存——S-e 立案件）。
#include "render/ThematicSensors.h"

#include "NullDriver.h"
#include "NullTargetFixture.h"
#include "render/TextureHandle.h"
#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"

#include <dqGeom/Transform.h>

#include <gtest/gtest.h>

using namespace dqRender;
using namespace dqCommon;

namespace {

// CountingTextureDriver（ThematicUniformsTest 同型——createTexture 回有效
// 句柄 + 计数/维度记录）。
class SensorCountingDriver : public rhi::NullDriver {
public:
    rhi::TextureHandle createTexture(rhi::SamplerType, uint8_t, rhi::TextureFormat fmt,
                                     uint32_t w, uint32_t h, uint32_t,
                                     rhi::TextureUsage) noexcept override
    {
        ++createTextureCalls;
        lastFormat = fmt;
        lastTexWidth = w;
        lastTexHeight = h;
        return m_allocator.allocate<rhi::HwTexture>();
    }

    int createTextureCalls = 0;
    rhi::TextureFormat lastFormat = rhi::TextureFormat::RGBA8;
    uint32_t lastTexWidth = 0;
    uint32_t lastTexHeight = 0;

private:
    rhi::HandleAllocator m_allocator{1024 * 1024};
};

} // namespace

// 传感器纹理 GPU 上传：1×N RGBA32F 列向 + **视空间**位置打包
//（ThematicSensors.ts:82-93——viewMatrix.multiplyPoint3d 全点变换；S-e 订正：
// 原"世界位 E1"锁经 FRAGDBG=10 实测证伪[v_eyeSpace=视空间]，参考语义归位）。
TEST(ThematicSensorsGpuTest, UploadsOneByNFloatTextureWithViewSpacePositions)
{
    SensorCountingDriver driver;

    std::vector<ThematicDisplaySensor> sensors;
    {
        ThematicDisplaySensor s1;
        s1.position = dqGeom::Point3d::From(10.0, 20.0, 30.0);
        s1.value = 0.25;
        sensors.push_back(s1);
        ThematicDisplaySensor s2;
        s2.position = dqGeom::Point3d::From(-5.0, 0.0, 7.5);
        s2.value = 0.75;
        sensors.push_back(s2);
    }

    // 已知视矩阵：平移 (1,2,3) + 绕 X +90°（ŷ→ẑ, ẑ→−ŷ）——点变换
    // p' = R·p + t 可手算对拍。
    auto rot = dqGeom::Matrix3d::CreateRotationAroundAxis(
        dqGeom::Vector3d::From(1, 0, 0), dqGeom::Angle::kPi / 2.0);
    auto const view = dqGeom::Transform::CreateOriginAndMatrix(
        dqGeom::Point3d::From(1.0, 2.0, 3.0), rot);

    auto ts = ThematicSensors::create(sensors, view, &driver);

    // GPU 纹理已建且为 1×N RGBA32F（ThematicSensors.ts createForData 形态）。
    EXPECT_EQ(driver.createTextureCalls, 1);
    EXPECT_EQ(driver.lastTexWidth, 1u);
    EXPECT_EQ(driver.lastTexHeight, 2u);
    EXPECT_EQ(driver.lastFormat, rhi::TextureFormat::RGBA32F);
    EXPECT_TRUE(ts.getTexture() != rhi::TextureHandle{});

    // CPU 数据 = 视空间位：s1 (10,20,30) → R·p=(10,−30,20) → +t=(11,−28,23)。
    ASSERT_EQ(ts.byteSize(), 2u * 4 * sizeof(float));
    float const* d = ts.data();
    EXPECT_FLOAT_EQ(d[0], 11.0f);
    EXPECT_FLOAT_EQ(d[1], -28.0f);
    EXPECT_FLOAT_EQ(d[2], 23.0f);
    EXPECT_FLOAT_EQ(d[3], 0.25f);
    // s2 (−5,0,7.5) → R·p=(−5,−7.5,0) → +t=(−4,−5.5,3)。
    EXPECT_FLOAT_EQ(d[4], -4.0f);
    EXPECT_FLOAT_EQ(d[5], -5.5f);
    EXPECT_FLOAT_EQ(d[6], 3.0f);
    EXPECT_FLOAT_EQ(d[7], 0.75f);

    // update 惰性门（ThematicSensors.ts:95-99）：同视矩阵→不重传（无新增
    // 纹理创建/数据上传）；视矩阵变→replaceTextureData 重传且内容随动。
    ts.update(view, &driver);
    EXPECT_EQ(driver.createTextureCalls, 1);

    auto const view2 = dqGeom::Transform::CreateOriginAndMatrix(
        dqGeom::Point3d::From(0.0, 0.0, 0.0), rot);  // 去平移
    ts.update(view2, &driver);
    // s1 重打包=(10,−30,20)（无平移）。
    EXPECT_FLOAT_EQ(ts.data()[0], 10.0f);
    EXPECT_FLOAT_EQ(ts.data()[1], -30.0f);
    EXPECT_FLOAT_EQ(ts.data()[2], 20.0f);
}

// 逐 batch 传感器缓存（Graphic.ts:111-120——Batch.getThematicSensors 按
// batch.range × 栈顶 localToWorld 过滤 + 缓存）。
TEST(ThematicSensorsGpuTest, BatchFiltersSensorsByCutoffRange)
{
    NullTargetFixture fixture;

    // 传感器 3 枚：batch 域内 2 枚、域外 1 枚（cutoff 过滤）。
    std::vector<ThematicDisplaySensor> sensors;
    auto mk = [](double x, double y, double z, double v) {
        ThematicDisplaySensor s;
        s.position = dqGeom::Point3d::From(x, y, z);
        s.value = v;
        return s;
    };
    sensors.push_back(mk(0, 0, 0, 0.1));
    sensors.push_back(mk(4, 0, 0, 0.9));
    sensors.push_back(mk(1000, 0, 0, 0.5));  // 域外（cutoff=10）

    auto filtered = ThematicSensors::accumulateSensorsInRange(
        sensors, dqGeom::Range3d(-5, -5, -5, 5, 5, 5),
        dqGeom::Transform::CreateIdentity(), 10.0);
    EXPECT_EQ(filtered.size(), 2u);
    EXPECT_DOUBLE_EQ(filtered[0].value, 0.1);
    EXPECT_DOUBLE_EQ(filtered[1].value, 0.9);

    // cutoff<=0 不过滤（全局臂）。
    auto all = ThematicSensors::accumulateSensorsInRange(
        sensors, dqGeom::Range3d(-5, -5, -5, 5, 5, 5),
        dqGeom::Transform::CreateIdentity(), 0.0);
    EXPECT_EQ(all.size(), 3u);
}
