// Ported from: Authored — test-only mock; no FreeCAD equivalent (FreeCAD tests use real view
// subclasses). Mirrors the onMsg/onHasMsg contract of Gui::MDIView for dispatch tests.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "Gui/MDIView.h"

namespace Gui {

// Test-only MDIView recording onMsg calls and returning a configurable onHasMsg result.
// No Q_OBJECT: adds no signals/slots of its own.
class MockMDIView : public MDIView
{
public:
    MockMDIView(Gui::Document* doc, QWidget* parent)
        : MDIView(doc, parent)
    {
    }

    std::vector<std::string> receivedMessages;   // messages received via onMsg, in order
    std::set<std::string> supportedMessages;     // messages this view "supports" (onHasMsg -> true)
    bool handlesMessages = true;                  // whether onMsg reports handled (true) or not

    bool onMsg(const char* pMsg) override
    {
        receivedMessages.push_back(pMsg);
        return handlesMessages;
    }

    bool onHasMsg(const char* pMsg) const override
    {
        return supportedMessages.count(pMsg) > 0;
    }
};

}  // namespace Gui
