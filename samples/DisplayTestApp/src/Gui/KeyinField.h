// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Key-in input field
// Ported from: itwinjs-core core/frontend-devtools/src/widgets/KeyinField.ts
//              (textbox allowing input of key-ins combined with an auto-completion
//              drop-down of all registered key-ins; enter runs the key-in)
//              + display-test-app Surface.ts:61-73 (mounting: historyLength 50,
//              Escape / ` loses focus).
//
// Qt mapping (§3.4): DOM <input> + <datalist> → QLineEdit + QCompleter; the
// browser datalist filters by substring match → QCompleter filterMode MatchContains;
// ArrowUp/ArrowDown history → keyPressEvent (the reference handles both keys with
// the same +1 direction — KeyinField.ts handleKeyDown, ported 1:1).
#pragma once

#include <QLineEdit>
#include <QPointer>

class QCompleter;

namespace Gui {

class KeyinField : public QLineEdit
{
    Q_OBJECT
public:
    // historyLength = maximum number of submitted key-ins stored (0 disables
    // history navigation). Ported from: KeyinFieldProps.historyLength
    // (KeyinField.ts:38-43).
    explicit KeyinField(int historyLength = 0, QWidget* parent = nullptr);

public Q_SLOTS:
    // Ported from: KeyinField.focus / loseFocus (KeyinField.ts:91-92).
    void focusField() { setFocus(); }
    void loseFocus() { clearFocus(); }

protected:
    // Enter → submitKeyin; ArrowUp/ArrowDown → history navigation;
    // Escape / ` → loseFocus (Surface.ts:64-70).
    void keyPressEvent(QKeyEvent* event) override;
    // Ported from: respondToKeyinFocus (KeyinField.ts:174-190) — reset the history
    // index and pick up tools registered since the list was built.
    void focusInEvent(QFocusEvent* event) override;

private:
    // Parse the current text with ToolRegistry::parseAndRun and report failures
    // (ToolNotFound / BadArgumentCount / FailedToRun) as a warning message.
    // Ported from: KeyinField.submitKeyin (KeyinField.ts:143-171) — the reference
    // reports via notifications.openMessageBox; DanQing has no message-box UI on
    // NotificationManager, so the same strings go out as an OutputMessage warning
    // (main.cpp's NotificationManager → status-bar listener).
    void submitKeyin();

    // Push the submitted key-in (most-recent-first, case-insensitive dedupe
    // against the newest entry, capped at the history length).
    // Ported from: KeyinField.pushHistory (KeyinField.ts:157-168).
    void pushHistory(QString const& keyin);

    // Rebuild the auto-completion list from the tool registry.
    // Ported from: KeyinField.findKeyins (KeyinField.ts:192-210) — englishKeyin of
    // every registered tool.
    void refreshKeyinList();

    int m_historyLength = 0;
    QStringList m_history;             // most recent first (KeyinField.ts _history)
    int m_historyIndex = -2;           // undefined (TS) → -2 sentinel
    QCompleter* m_completer = nullptr;
};

}  // namespace Gui
