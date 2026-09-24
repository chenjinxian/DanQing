// Authored: test-only stub for dqApp::ViewManager (no reference exists in FreeCAD)
// Minimal stub matching real ViewManager.h method signatures
#pragma once

#include <cstddef>

namespace dqApp {

class Viewport;

class ViewManager {
public:
    ViewManager() = default;
    ~ViewManager() = default;

    bool AddViewport(Viewport*) { return true; }
    void DropViewport(Viewport*) {}
    size_t GetViewportCount() const { return 0; }
    Viewport* GetSelectedViewport() const { return nullptr; }
    void SetSelectedViewport(Viewport*) {}
};

}  // namespace dqApp
