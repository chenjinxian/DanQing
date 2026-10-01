// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — NotificationManager
//
// Ported from: itwinjs-core core/frontend/src/NotificationManager.ts
// Controls user interaction for prompts, messages, and alerts.
#pragma once

#include "Export.h"
#include "ToolAssistance.h"

#include <dqBase/DqEvent.h>

#include <cstdint>
#include <string>

namespace dqApp {

// Message type and behavior.
// Ported from: itwinjs-core OutputMessageType
enum class OutputMessageType : uint8_t {
    Toast = 0,       // Temporary, auto-dismiss
    Pointer = 1,     // Near cursor
    Sticky = 2,      // With close button
    InputField = 3,  // Near input field
    Alert = 4,       // Modal
};

// Message priority/severity.
// Ported from: itwinjs-core OutputMessagePriority
enum class OutputMessagePriority : uint8_t {
    None = 0,
    Success = 1,
    Error = 10,
    Warning = 11,
    Info = 12,
    Debug = 13,
    Fatal = 17,
};

// MessageBox button sets.
// Ported from: itwinjs-core MessageBoxType
enum class MessageBoxType : uint8_t {
    OkCancel = 0,
    Ok = 1,
    LargeOk = 2,
    MediumAlert = 3,
    YesNoCancel = 4,
    YesNo = 5,
};

// MessageBox icon types.
// Ported from: itwinjs-core MessageBoxIconType
enum class MessageBoxIconType : uint8_t {
    NoSymbol = 0,
    Information = 1,
    Question = 2,
    Warning = 3,
    Critical = 4,
    Success = 5,
};

// MessageBox return values.
// Ported from: itwinjs-core MessageBoxValue
enum class MessageBoxValue : uint8_t {
    Apply = 1,
    Reset = 2,
    Ok = 3,
    Cancel = 4,
    Default = 5,
    Yes = 6,
    No = 7,
    Retry = 8,
    Stop = 9,
    Help = 10,
    YesToAll = 11,
    NoToAll = 12,
};

// Activity message end reason.
// Ported from: itwinjs-core ActivityMessageEndReason
enum class ActivityMessageEndReason : uint8_t {
    Completed = 0,
    Cancelled = 1,
};

// Message details.
// Ported from: itwinjs-core NotifyMessageDetails
struct NotifyMessageDetails {
    OutputMessagePriority priority = OutputMessagePriority::None;
    std::string briefMessage;
    std::string detailedMessage;
    OutputMessageType msgType = OutputMessageType::Toast;

    NotifyMessageDetails() = default;
    NotifyMessageDetails(OutputMessagePriority p, const std::string& brief,
                         const std::string& detail = "", OutputMessageType type = OutputMessageType::Toast)
        : priority(p), briefMessage(brief), detailedMessage(detail), msgType(type)
    {
    }
};

// Activity message details.
// Ported from: itwinjs-core ActivityMessageDetails
struct ActivityMessageDetails {
    bool showProgressBar = false;
    bool showPercentInMessage = false;
    bool supportsCancellation = false;
    bool showDialogInitially = true;
    bool wasCancelled = false;

    void OnActivityCancelled() { wasCancelled = true; }
    void OnActivityCompleted() { wasCancelled = false; }
};

// The NotificationManager controls user interaction for prompts, messages, and alerts.
// Ported from: itwinjs-core NotificationManager
class DQ_APP_EXPORT NotificationManager {
public:
    virtual ~NotificationManager() = default;

    // Output a prompt to the user.
    // Ported from: itwinjs-core NotificationManager.outputPrompt()
    virtual void OutputPrompt(const std::string& prompt) { (void)prompt; }

    // Output a message/alert to the user.
    // Ported from: itwinjs-core NotificationManager.outputMessage() — the
    // reference base is a concrete no-op (NotificationManager.ts:204
    // `public outputMessage(_message) { }`，应用子类覆写——DTA Notifications.
    // ts:57 即覆写点)；the DanQing base implementation fans the message out
    // through OnMessageOutput (the application subscribes — main.cpp wires it
    // to the status bar). The base was a silent no-op before M-L(3): the event
    // had no raise site (ported-but-uncalled), so notifications never reached
    // the app surface.
    virtual void OutputMessage(const NotifyMessageDetails& message)
    {
        OnMessageOutput.Raise(message);
    }

    // Output a MessageBox and return the user's response.
    // Ported from: itwinjs-core NotificationManager.openMessageBox()
    virtual MessageBoxValue OpenMessageBox(MessageBoxType mbType, const std::string& message,
                                           MessageBoxIconType icon = MessageBoxIconType::NoSymbol)
    {
        (void)mbType; (void)message; (void)icon;
        return MessageBoxValue::Ok;
    }

    // Setup activity message.
    // Ported from: itwinjs-core NotificationManager.setupActivityMessage()
    virtual bool SetupActivityMessage(const ActivityMessageDetails& details) { (void)details; return true; }

    // Output activity message with progress.
    // Ported from: itwinjs-core NotificationManager.outputActivityMessage()
    virtual bool OutputActivityMessage(const std::string& messageText, int percentComplete)
    {
        (void)messageText; (void)percentComplete;
        return true;
    }

    // End activity message.
    // Ported from: itwinjs-core NotificationManager.endActivityMessage()
    virtual bool EndActivityMessage(ActivityMessageEndReason reason) { (void)reason; return true; }

    // Whether tooltips are supported.
    // Ported from: itwinjs-core NotificationManager.isToolTipSupported
    //（:246-248——base false，应用子类覆写 true[DTA Notifications.ts:94]）。
    // DanQing 无应用子类分层——宿主装配时 SetToolTipSupported(true)（M-O(1)：
    // DtaTools 的 QToolTip 订阅即覆写等价物）。
    virtual bool IsToolTipSupported() const noexcept { return m_toolTipSupported; }
    void SetToolTipSupported(bool supported) noexcept { m_toolTipSupported = supported; }

    // Whether tooltip is currently open.
    // Ported from: itwinjs-core NotificationManager.isToolTipOpen
    //（DTA Notifications.ts:95-97——`undefined !== this._tooltipDiv`）。
    virtual bool IsToolTipOpen() const noexcept { return m_toolTipOpen; }

    // Show a tooltip at a view point.
    // Ported from: itwinjs-core NotificationManager.showToolTip (NotificationManager
    // .ts:249-256——`_showToolTip(htmlElement, message, location)`；location 是
    // 视口内 hover 点，DTA 子类按 (x+15, y-20) 偏移定位 div :116-117)。
    // M-O(1) 前为 ported-but-uncalled 空面（§11.10）——现由 Viewport 的 hover
    // locate 链驱动（AccuSnap.displayToolTip → vp.openToolTip 的等价点），
    // base 记录开态并扇出 OnToolTip（宿主订阅渲染——参考 div 创建语义）。
    virtual void OpenToolTip(const std::string& message, double viewX = 0.0, double viewY = 0.0)
    {
        m_toolTipOpen = true;
        OnToolTip.Raise(message, viewX, viewY);
    }

    // Clear the tooltip.
    // Ported from: itwinjs-core NotificationManager.clearToolTip（DTA :99-104
    // ——div remove）。
    virtual void ClearToolTip()
    {
        if (m_toolTipOpen) {
            m_toolTipOpen = false;
            OnToolTipCleared.Raise();
        }
    }

    // Set the tool assistance instructions to be displayed.
    // Ported from: itwinjs-core NotificationManager.setToolAssistance
    //（NotificationManager.ts:204-207——参考基类是 no-op 具体方法，应用子类/
    // appui 消费）。DanQing 事件扇出等价（M-O(2) 3i）：OnToolAssistance 的
    // 宿主订阅（DisplayTestApp DtaTools → 状态栏 InputHints）即消费面——与
    // OutputMessage 的 OnMessageOutput 先例同构。
    // EQUIVALENCE: 参考源=NotificationManager.ts:204（no-op + 子类覆写）；
    // 发散=DanQing 无应用子类分层，以事件为宿主缝；验证法=ViewToolTest
    // ToolAssistance 族（payload 逐字段）+ DtaToolsWiring 显示面锁。
    virtual void setToolAssistance(ToolAssistanceInstructions const& instructions)
    {
        OnToolAssistance.Raise(instructions);
    }

    // Events
    dqBase::DqEvent<const NotifyMessageDetails&> OnMessageOutput;
    // tool assistance 面（M-O(2) 3i——provideToolAssistance 调用点经
    // setToolAssistance 扇出；mid-tool 跃迁如 WindowArea FirstPoint→NextPoint）。
    dqBase::DqEvent<const ToolAssistanceInstructions&> OnToolAssistance;
    // tooltip 面（M-O(1)——宿主渲染订阅：message + 视口内 hover 点）。
    dqBase::DqEvent<const std::string&, double, double> OnToolTip;
    dqBase::DqEvent<> OnToolTipCleared;

private:
    bool m_toolTipSupported = false;
    bool m_toolTipOpen = false;
};

}  // namespace dqApp
