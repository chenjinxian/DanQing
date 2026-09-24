// Ported from: FreeCAD src/Gui/View.cpp
// Minimal stub for DisplayTestApp UI shell

#include "View.h"
#include "MDIView.h"

using namespace Gui;

TYPESYSTEM_SOURCE_ABSTRACT(Gui::BaseView, Base::BaseClass)

BaseView::BaseView(Gui::Document* pcDocument)
    : _pcDocument(pcDocument)
{}

BaseView::~BaseView() = default;

void BaseView::setDocument(Gui::Document*) {}

void BaseView::onClose() {}

App::Document* BaseView::getAppDocument() const { return nullptr; }

void BaseView::deleteSelf() { delete this; }
