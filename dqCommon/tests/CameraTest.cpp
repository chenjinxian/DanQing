// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/common/src/Camera.ts
// DanQing dqCommon — Camera, GlobeMode, ViewDefinitionProps unit tests
//
// Tests based on itwinjs-core core/common/src/Camera.ts API behavior
#include "dqCommon/Camera.h"
#include "dqCommon/GlobeMode.h"
#include "dqCommon/ViewDefinitionProps.h"

#include <gtest/gtest.h>

using namespace dqCommon;
using namespace dqGeom;

// Camera tests
// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, DefaultConstruction)
TEST(Camera, DefaultConstruction)
{
    const Camera cam;
    EXPECT_NEAR(cam.lensRadians, kPi / 2.0, 1e-10);
    EXPECT_DOUBLE_EQ(cam.focusDist, 1.0);
    EXPECT_TRUE(cam.eye.IsEqual(Point3d::FromZero()));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, FromProps)
TEST(Camera, FromProps)
{
    CameraProps props;
    props.lensRadians = kPi / 4.0;
    props.focusDist = 10.0;
    props.eyeX = 1.0;
    props.eyeY = 2.0;
    props.eyeZ = 3.0;

    const Camera cam(props);
    EXPECT_NEAR(cam.lensRadians, kPi / 4.0, 1e-10);
    EXPECT_DOUBLE_EQ(cam.focusDist, 10.0);
    EXPECT_TRUE(cam.eye.IsEqual(Point3d::From(1, 2, 3)));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, isValidLensAngle)
TEST(Camera, isValidLensAngle)
{
    // Valid: between PI/8 and PI
    EXPECT_TRUE(Camera::isValidLensAngle(kPi / 4.0));
    EXPECT_TRUE(Camera::isValidLensAngle(kPi / 2.0));
    EXPECT_TRUE(Camera::isValidLensAngle(3.0));

    // Invalid: too small or too large
    EXPECT_FALSE(Camera::isValidLensAngle(0.0));
    EXPECT_FALSE(Camera::isValidLensAngle(kPi / 16.0));
    EXPECT_FALSE(Camera::isValidLensAngle(kPi + 0.01));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, validateLensAngle)
TEST(Camera, validateLensAngle)
{
    // Valid angle returns same value
    EXPECT_NEAR(Camera::validateLensAngle(kPi / 4.0), kPi / 4.0, 1e-10);

    // Invalid angle returns PI/2
    EXPECT_NEAR(Camera::validateLensAngle(0.0), kPi / 2.0, 1e-10);
    EXPECT_NEAR(Camera::validateLensAngle(kPi + 1.0), kPi / 2.0, 1e-10);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, FocusValidation)
TEST(Camera, FocusValidation)
{
    Camera cam;
    EXPECT_TRUE(cam.isFocusValid());

    cam.invalidateFocus();
    EXPECT_FALSE(cam.isFocusValid());
    EXPECT_DOUBLE_EQ(cam.focusDist, 0.0);

    cam.setFocusDistance(100.0);
    EXPECT_TRUE(cam.isFocusValid());
    EXPECT_DOUBLE_EQ(cam.getFocusDistance(), 100.0);

    // Too large
    cam.setFocusDistance(1.0e15);
    EXPECT_FALSE(cam.isFocusValid());
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, LensValidation)
TEST(Camera, LensValidation)
{
    Camera cam;
    EXPECT_TRUE(cam.isLensValid());

    cam.lensRadians = 0.0;
    EXPECT_FALSE(cam.isLensValid());

    cam.validateLens();
    EXPECT_TRUE(cam.isLensValid());
    EXPECT_NEAR(cam.lensRadians, kPi / 2.0, 1e-10);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, EyePoint)
TEST(Camera, EyePoint)
{
    Camera cam;
    EXPECT_TRUE(cam.getEyePoint().IsEqual(Point3d::FromZero()));

    cam.setEyePoint(Point3d::From(10, 20, 30));
    EXPECT_TRUE(cam.getEyePoint().IsEqual(Point3d::From(10, 20, 30)));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, isValid)
TEST(Camera, isValid)
{
    Camera cam;
    EXPECT_TRUE(cam.isValid());

    cam.invalidateFocus();
    EXPECT_FALSE(cam.isValid());

    cam.setFocusDistance(1.0);
    EXPECT_TRUE(cam.isValid());

    cam.lensRadians = 0.0;
    EXPECT_FALSE(cam.isValid());
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, Equality)
TEST(Camera, Equality)
{
    const Camera a;
    const Camera b;
    EXPECT_TRUE(a.equals(b));

    Camera c;
    c.setFocusDistance(100.0);
    EXPECT_FALSE(a.equals(c));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, clone)
TEST(Camera, clone)
{
    Camera cam;
    cam.setFocusDistance(50.0);
    cam.setEyePoint(Point3d::From(1, 2, 3));

    const auto clone = cam.clone();
    EXPECT_TRUE(cam.equals(clone));
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(Camera, ToFromJSON)
TEST(Camera, ToFromJSON)
{
    Camera cam;
    cam.setFocusDistance(25.0);
    cam.setEyePoint(Point3d::From(5, 10, 15));
    cam.lensRadians = kPi / 3.0;

    const auto props = cam.toJSON();
    const auto cam2 = Camera::fromJSON(props);
    EXPECT_TRUE(cam.equals(cam2));
}

// GlobeMode tests
// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(GlobeMode, Values)
TEST(GlobeMode, Values)
{
    EXPECT_EQ(static_cast<int>(GlobeMode::Ellipsoid), 0);
    EXPECT_EQ(static_cast<int>(GlobeMode::Plane), 1);
}

// ViewDefinitionProps tests
// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, ModelSelectorProps)
TEST(ViewDefinitionProps, ModelSelectorProps)
{
    ModelSelectorProps props;
    EXPECT_TRUE(props.models.empty());
    props.models.push_back(dqBase::DqId(1));
    props.models.push_back(dqBase::DqId(2));
    EXPECT_EQ(props.models.size(), 2u);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, CategorySelectorProps)
TEST(ViewDefinitionProps, CategorySelectorProps)
{
    CategorySelectorProps props;
    EXPECT_TRUE(props.categories.empty());
    props.categories.push_back(dqBase::DqId(10));
    EXPECT_EQ(props.categories.size(), 1u);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, ViewDefinitionProps)
TEST(ViewDefinitionProps, ViewDefinitionProps)
{
    ViewDefinitionProps props;
    props.categorySelectorId = dqBase::DqId(1);
    props.displayStyleId = dqBase::DqId(2);
    props.description = "Test view";
    EXPECT_EQ(props.categorySelectorId, dqBase::DqId(1));
    EXPECT_EQ(props.displayStyleId, dqBase::DqId(2));
    EXPECT_EQ(props.description, "Test view");
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, ViewDefinition3dProps)
TEST(ViewDefinitionProps, ViewDefinition3dProps)
{
    ViewDefinition3dProps props;
    props.cameraOn = true;
    props.originX = 100.0;
    props.originY = 200.0;
    props.originZ = 300.0;
    props.extentsX = 50.0;
    props.extentsY = 50.0;
    props.extentsZ = 50.0;
    EXPECT_TRUE(props.cameraOn);
    EXPECT_DOUBLE_EQ(props.originX, 100.0);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, SpatialViewDefinitionProps)
TEST(ViewDefinitionProps, SpatialViewDefinitionProps)
{
    SpatialViewDefinitionProps props;
    props.modelSelectorId = dqBase::DqId(42);
    props.cameraOn = false;
    EXPECT_EQ(props.modelSelectorId, dqBase::DqId(42));
    EXPECT_FALSE(props.cameraOn);
}

// Ported from: itwinjs-core core/common/src/test/Camera.test.ts
//              TEST(ViewDefinitionProps, ViewDefinition2dProps)
TEST(ViewDefinitionProps, ViewDefinition2dProps)
{
    ViewDefinition2dProps props;
    props.baseModelId = dqBase::DqId(7);
    props.originX = 10.0;
    props.originY = 20.0;
    props.deltaX = 100.0;
    props.deltaY = 200.0;
    props.angleDegrees = 45.0;
    EXPECT_EQ(props.baseModelId, dqBase::DqId(7));
    EXPECT_DOUBLE_EQ(props.angleDegrees, 45.0);
}
