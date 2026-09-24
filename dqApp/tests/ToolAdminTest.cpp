// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ToolAdmin and NotificationManager tests
//
// Ported from: itwinjs-core core/frontend/src/test/tools/ToolAdmin.test.ts
#include <gtest/gtest.h>

#include <dqApp/NotificationManager.h>
#include <dqApp/ToolAdmin.h>
#include <dqApp/ViewTool.h>  // ViewTool complete type (Task 9 expanded it from the marker).

using namespace dqApp;

// Concrete test tool
class TestTool : public InteractiveTool {
public:
    const char* getToolId() const override { return "TestTool"; }

    // Lifecycle counters — names follow the faithful itwinjs Tool.ts methods
    // (Task 2 migrated this fixture from legacy OnStart/OnStop/OnSuspend/OnResume to
    // onPostInstall/onCleanup/onSuspend/onUnsuspend alongside the SetActiveTool rewrite).
    int postInstallCount = 0;
    int cleanupCount = 0;
    int suspendCount = 0;
    int unsuspendCount = 0;

    void onPostInstall() override { postInstallCount++; setActive(true); }
    void onCleanup() override { cleanupCount++; setActive(false); }
    void onSuspend() override { suspendCount++; }
    void onUnsuspend() override { unsuspendCount++; setActive(true); }
};

// NotificationManager tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(NotificationManager, DefaultBehavior)
{
    NotificationManager nm;
    EXPECT_FALSE(nm.IsToolTipSupported());
    EXPECT_FALSE(nm.IsToolTipOpen());
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(NotificationManager, OutputPrompt)
{
    NotificationManager nm;
    // Should not crash
    nm.OutputPrompt("Test prompt");
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(NotificationManager, OutputMessage)
{
    NotificationManager nm;
    NotifyMessageDetails msg(OutputMessagePriority::Info, "Test message");
    nm.OutputMessage(msg);
    EXPECT_EQ(msg.priority, OutputMessagePriority::Info);
    EXPECT_EQ(msg.briefMessage, "Test message");
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(NotificationManager, OpenMessageBox)
{
    NotificationManager nm;
    auto result = nm.OpenMessageBox(MessageBoxType::Ok, "Test", MessageBoxIconType::Information);
    EXPECT_EQ(result, MessageBoxValue::Ok);
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(NotificationManager, ActivityMessage)
{
    NotificationManager nm;
    ActivityMessageDetails details;
    details.showProgressBar = true;
    details.supportsCancellation = true;

    EXPECT_TRUE(nm.SetupActivityMessage(details));
    EXPECT_TRUE(nm.OutputActivityMessage("Processing...", 50));
    EXPECT_TRUE(nm.EndActivityMessage(ActivityMessageEndReason::Completed));
}

// OutputMessageType tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(OutputMessageType, Values)
{
    EXPECT_EQ(static_cast<int>(OutputMessageType::Toast), 0);
    EXPECT_EQ(static_cast<int>(OutputMessageType::Pointer), 1);
    EXPECT_EQ(static_cast<int>(OutputMessageType::Sticky), 2);
    EXPECT_EQ(static_cast<int>(OutputMessageType::InputField), 3);
    EXPECT_EQ(static_cast<int>(OutputMessageType::Alert), 4);
}

// OutputMessagePriority tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(OutputMessagePriority, Values)
{
    EXPECT_EQ(static_cast<int>(OutputMessagePriority::None), 0);
    EXPECT_EQ(static_cast<int>(OutputMessagePriority::Success), 1);
    EXPECT_EQ(static_cast<int>(OutputMessagePriority::Error), 10);
    EXPECT_EQ(static_cast<int>(OutputMessagePriority::Warning), 11);
    EXPECT_EQ(static_cast<int>(OutputMessagePriority::Info), 12);
}

// MessageBoxType tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(MessageBoxType, Values)
{
    EXPECT_EQ(static_cast<int>(MessageBoxType::OkCancel), 0);
    EXPECT_EQ(static_cast<int>(MessageBoxType::Ok), 1);
    EXPECT_EQ(static_cast<int>(MessageBoxType::YesNo), 5);
}

// ToolAdmin tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, DefaultState)
{
    ToolAdmin admin;
    EXPECT_EQ(admin.GetActiveTool(), nullptr);
    EXPECT_FALSE(admin.IsToolActive());
    EXPECT_EQ(admin.GetViewTool(), nullptr);
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, SetActiveTool)
{
    ToolAdmin admin;
    TestTool tool;

    EXPECT_TRUE(admin.SetActiveTool(&tool));
    EXPECT_EQ(admin.GetActiveTool(), &tool);
    EXPECT_TRUE(admin.IsToolActive());
    EXPECT_EQ(tool.postInstallCount, 1);
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, SwitchTool)
{
    ToolAdmin admin;
    TestTool tool1;
    TestTool tool2;

    admin.SetActiveTool(&tool1);
    EXPECT_EQ(tool1.postInstallCount, 1);
    EXPECT_EQ(tool1.cleanupCount, 0);

    admin.SetActiveTool(&tool2);
    EXPECT_EQ(tool1.cleanupCount, 1);
    EXPECT_EQ(tool2.postInstallCount, 1);
    EXPECT_EQ(admin.GetActiveTool(), &tool2);
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, SetNullTool)
{
    ToolAdmin admin;
    TestTool tool;

    admin.SetActiveTool(&tool);
    EXPECT_TRUE(admin.IsToolActive());

    admin.SetActiveTool(nullptr);
    EXPECT_EQ(admin.GetActiveTool(), nullptr);
    EXPECT_FALSE(admin.IsToolActive());
    EXPECT_EQ(tool.cleanupCount, 1);
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, SameToolNoOp)
{
    ToolAdmin admin;
    TestTool tool;

    admin.SetActiveTool(&tool);
    admin.SetActiveTool(&tool);
    EXPECT_EQ(tool.postInstallCount, 1);  // Should not restart
}

// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ToolAdmin, ViewTool)
{
    ToolAdmin admin;
    EXPECT_EQ(admin.GetViewTool(), nullptr);

    ViewTool viewTool;
    admin.SetViewTool(&viewTool);
    EXPECT_EQ(admin.GetViewTool(), &viewTool);
}

// Tool tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(Tool, DefaultState)
{
    TestTool tool;
    EXPECT_FALSE(tool.isActive());
    EXPECT_STREQ(tool.getToolId(), "TestTool");
}

// ButtonEvent tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(ButtonEvent, DefaultState)
{
    ButtonEvent event;
    EXPECT_DOUBLE_EQ(event.x, 0.0);
    EXPECT_DOUBLE_EQ(event.y, 0.0);
    EXPECT_DOUBLE_EQ(event.z, 0.0);
    EXPECT_EQ(event.button, BeButton::Data);
    EXPECT_FALSE(event.isDown);
    EXPECT_FALSE(event.isDoubleClick);
}

// StartOrResume tests
// Ported from: itwinjs-core core/frontend/src/test/ToolAdmin.test.ts
TEST(StartOrResume, Values)
{
    EXPECT_EQ(static_cast<int>(StartOrResume::Start), 1);
    EXPECT_EQ(static_cast<int>(StartOrResume::Resume), 2);
}
