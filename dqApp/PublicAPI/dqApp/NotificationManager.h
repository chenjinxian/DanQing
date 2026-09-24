// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — NotificationManager
//
// Ported from: itwinjs-core core/frontend/src/NotificationManager.ts
// Controls user interaction for prompts, messages, and alerts.
#pragma once

#include "Export.h"

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
    // Ported from: itwinjs-core NotificationManager.outputMessage()
    virtual void OutputMessage(const NotifyMessageDetails& message) { (void)message; }

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
    virtual bool IsToolTipSupported() const noexcept { return false; }

    // Whether tooltip is currently open.
    virtual bool IsToolTipOpen() const noexcept { return false; }

    // Show a tooltip.
    virtual void OpenToolTip(const std::string& message) { (void)message; }

    // Clear the tooltip.
    virtual void ClearToolTip() {}

    // Events
    dqBase::DqEvent<const NotifyMessageDetails&> OnMessageOutput;
};

}  // namespace dqApp
