// Ported from: Authored — no reference test exists in FreeCAD for InputHintWidget showHints
// Tests for InputHintWidget HTML rendering of input hints.
#include <gtest/gtest.h>

#include <QApplication>
#include <QPalette>

#include "QtTestFixtures.h"
#include "Gui/InputHint.h"
#include "Gui/InputHintWidget.h"

// Application singleton — needed by InputHintWidget palette
#include <App/Application.h>

// Per-test helper ensureAppReady() is defined in CommandTest.cpp and declared in
// QtTestFixtures.h.

using namespace Gui;

// =====================================================================
// showHints: non-empty HTML produced for a single hint
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for showHints HTML rendering
TEST(InputHintWidgetTest, ShowHintsProducesNonEmptyText)
{
    ensureAppReady();

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 select point"), .sequences = {MouseLeft}}
    };

    widget.showHints(hints);
    qApp->processEvents();

    QString text = widget.text();
    EXPECT_FALSE(text.isEmpty()) << "showHints should produce non-empty HTML text";
    EXPECT_TRUE(text.contains(QStringLiteral("<table")))
        << "Output should contain an HTML table element";
    EXPECT_TRUE(text.contains(QStringLiteral("<img")))
        << "Output should contain at least one <img> tag for the key icon";
}

// =====================================================================
// showHints: message text is embedded in the HTML
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for showHints message content
TEST(InputHintWidgetTest, ShowHintsContainsMessageText)
{
    ensureAppReady();

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 select point"), .sequences = {MouseLeft}}
    };

    widget.showHints(hints);
    qApp->processEvents();

    QString text = widget.text();
    EXPECT_TRUE(text.contains(QStringLiteral("select point")))
        << "Output should contain the hint message text";
}

// =====================================================================
// showHints: multiple hints produce separator between them
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for multi-hint rendering
TEST(InputHintWidgetTest, ShowHintsMultipleHintsSeparate)
{
    ensureAppReady();

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 select point"), .sequences = {MouseLeft}},
        {.message = QStringLiteral("%1 cancel"), .sequences = {KeyEscape}},
    };

    widget.showHints(hints);
    qApp->processEvents();

    QString text = widget.text();
    EXPECT_TRUE(text.contains(QStringLiteral("select point")))
        << "Output should contain first hint message";
    EXPECT_TRUE(text.contains(QStringLiteral("cancel")))
        << "Output should contain second hint message";
    // Multiple hints separated by spacer td
    EXPECT_TRUE(text.contains(QStringLiteral("width=10")))
        << "Output should contain a spacer td between hints";
}

// =====================================================================
// showHints: empty hints clears the widget
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for showHints empty
TEST(InputHintWidgetTest, ShowHintsEmptyClears)
{
    ensureAppReady();

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    // First show something
    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 test"), .sequences = {KeyA}}
    };
    widget.showHints(hints);
    qApp->processEvents();
    EXPECT_FALSE(widget.text().isEmpty());

    // Then clear by passing empty list
    widget.showHints({});
    qApp->processEvents();
    EXPECT_TRUE(widget.text().isEmpty()) << "Passing empty hints should clear the widget";
}

// =====================================================================
// clearHints: clears the text
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for clearHints
TEST(InputHintWidgetTest, ClearHintsClearsText)
{
    ensureAppReady();

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 test"), .sequences = {KeyB}}
    };
    widget.showHints(hints);
    qApp->processEvents();
    EXPECT_FALSE(widget.text().isEmpty());

    widget.clearHints();
    qApp->processEvents();
    EXPECT_TRUE(widget.text().isEmpty()) << "clearHints should clear the widget text";
}

// =====================================================================
// inputRepresentation: key names are correct
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for inputRepresentation unit test
TEST(InputHintWidgetTest, InputRepresentationKeyNames)
{
    ensureAppReady();

    // Verify inputRepresentation is accessible via a hint with the key in the message
    // We can't call it directly (private), but we can verify through showHints that
    // the icon is generated without crash for common keys.

    InputHintWidget widget(nullptr);
    widget.show();
    qApp->processEvents();

    using enum InputHint::UserInput;

    // Test a range of key types: letter, number, function, modifier
    std::list<InputHint> hints = {
        {.message = QStringLiteral("%1 letter A"), .sequences = {KeyA}},
        {.message = QStringLiteral("%1 number 0"), .sequences = {Key0}},
        {.message = QStringLiteral("%1 function F1"), .sequences = {KeyF1}},
        {.message = QStringLiteral("%1 escape"), .sequences = {KeyEscape}},
        {.message = QStringLiteral("%1 left arrow"), .sequences = {KeyLeft}},
    };

    // Should not crash or produce empty output
    widget.showHints(hints);
    qApp->processEvents();

    QString text = widget.text();
    EXPECT_FALSE(text.isEmpty()) << "showHints should handle all key types without crash";
    EXPECT_TRUE(text.contains(QStringLiteral("<table")))
        << "Output should contain HTML table";
}

// =====================================================================
// UserInput enum: values match Qt key constants
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for UserInput enum value check
TEST(InputHintWidgetTest, UserInputEnumValuesMatchQtKeys)
{
    using enum InputHint::UserInput;

    // Verify enum values match Qt::Key constants
    EXPECT_EQ(static_cast<int>(KeyA), Qt::Key_A);
    EXPECT_EQ(static_cast<int>(KeyZ), Qt::Key_Z);
    EXPECT_EQ(static_cast<int>(Key0), Qt::Key_0);
    EXPECT_EQ(static_cast<int>(Key9), Qt::Key_9);
    EXPECT_EQ(static_cast<int>(KeyF1), Qt::Key_F1);
    EXPECT_EQ(static_cast<int>(KeyF12), Qt::Key_F12);
    EXPECT_EQ(static_cast<int>(KeyEscape), Qt::Key_Escape);
    EXPECT_EQ(static_cast<int>(KeyReturn), Qt::Key_Return);
    EXPECT_EQ(static_cast<int>(KeySpace), Qt::Key_Space);
    EXPECT_EQ(static_cast<int>(KeyLeft), Qt::Key_Left);
    EXPECT_EQ(static_cast<int>(KeyUp), Qt::Key_Up);
    EXPECT_EQ(static_cast<int>(KeyRight), Qt::Key_Right);
    EXPECT_EQ(static_cast<int>(KeyDown), Qt::Key_Down);

    // Mouse keys use bit-shifted values, not Qt keys
    EXPECT_NE(static_cast<int>(MouseLeft), 0);
    EXPECT_NE(static_cast<int>(MouseRight), 0);
    EXPECT_NE(static_cast<int>(MouseMiddle), 0);
}

// =====================================================================
// InputSequence: implicit conversion from single UserInput
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for InputSequence conversion
TEST(InputHintWidgetTest, InputSequenceImplicitConversion)
{
    using enum InputHint::UserInput;

    // Single UserInput should implicitly convert to InputSequence
    InputHint::InputSequence seq = KeyA;
    EXPECT_EQ(seq.keys.size(), 1u);
    EXPECT_EQ(seq.keys.front(), KeyA);

    // Initializer list should work
    InputHint::InputSequence combo = {KeyControl, KeyA};
    EXPECT_EQ(combo.keys.size(), 2u);
}

// =====================================================================
// StateHints + lookupHints: template works correctly
// =====================================================================

// Ported from: Authored — no reference test exists in FreeCAD for StateHints lookupHints
TEST(InputHintWidgetTest, StateHintsLookupHints)
{
    using enum InputHint::UserInput;

    enum class ToolState { Idle, Drawing, Selecting };

    HintTable<ToolState> table = {
        {ToolState::Idle, {{.message = QStringLiteral("%1 start"), .sequences = {MouseLeft}}}},
        {ToolState::Drawing, {{.message = QStringLiteral("%1 place point"), .sequences = {MouseLeft}}}},
    };

    auto idleHints = lookupHints(ToolState::Idle, table);
    EXPECT_EQ(idleHints.size(), 1u);
    EXPECT_TRUE(idleHints.front().message.contains(QStringLiteral("start")));

    auto drawingHints = lookupHints(ToolState::Drawing, table);
    EXPECT_EQ(drawingHints.size(), 1u);
    EXPECT_TRUE(drawingHints.front().message.contains(QStringLiteral("place point")));

    // Unknown state returns fallback
    auto fallback = std::list<InputHint>{
        {.message = QStringLiteral("fallback"), .sequences = {KeyEscape}}
    };
    auto unknownHints = lookupHints(ToolState::Selecting, table, fallback);
    EXPECT_EQ(unknownHints.size(), 1u);
    EXPECT_TRUE(unknownHints.front().message.contains(QStringLiteral("fallback")));

    // Unknown state without fallback returns empty
    auto emptyHints = lookupHints(ToolState::Selecting, table);
    EXPECT_TRUE(emptyHints.empty());
}
