// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Key-in input field implementation
// Ported from: itwinjs-core core/frontend-devtools/src/widgets/KeyinField.ts
//              + display-test-app Surface.ts:61-73（挂载与 Escape/` 失焦）。
#include "KeyinField.h"

#include <QCompleter>
#include <QKeyEvent>
#include <QStringListModel>

#include <dqApp/Application.h>
#include <dqApp/NotificationManager.h>
#include <dqApp/ToolAdmin.h>

namespace Gui {

KeyinField::KeyinField(int historyLength, QWidget* parent)
    : QLineEdit(parent)
    , m_historyLength(historyLength)
{
    setObjectName(QStringLiteral("DTA.KeyinField"));
    setPlaceholderText(QStringLiteral("Type the key-in text here"));  // KeyinField.ts:81 tooltip
    setClearButtonEnabled(false);
    setMaximumWidth(340);

    // Auto-completion drop-down of all registered key-ins (KeyinField.ts:60-66 —
    // a <datalist>; browser filtering is substring → QCompleter MatchContains).
    m_completer = new QCompleter(this);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    refreshKeyinList();
    setCompleter(m_completer);
}

void KeyinField::refreshKeyinList()
{
    // Ported from: KeyinField.findKeyins (KeyinField.ts:192-210) — englishKeyin of
    // every registered tool (the reference's NonLocalized localization mode, the
    // default). Tools with no key-in (englishKeyin empty — e.g. all View.* tools)
    // are not key-in reachable and are omitted.
    QStringList keyins;
    auto const& tools
        = dqApp::Application::Get().GetToolAdmin().GetRegistry().getToolList();
    for (auto const& tool : tools) {
        if (!tool.keyin.empty())
            keyins << QString::fromStdString(tool.keyin);
    }
    if (m_completer) {
        if (auto* model = qobject_cast<QStringListModel*>(m_completer->model()))
            model->setStringList(keyins);
        else
            m_completer->setModel(new QStringListModel(keyins, m_completer));
    }
}

void KeyinField::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    // Ported from: respondToKeyinFocus (KeyinField.ts:174-190) — reset the history
    // index; new tools may have registered since the list was built.
    m_historyIndex = -2;
    refreshKeyinList();
}

void KeyinField::keyPressEvent(QKeyEvent* event)
{
    // Ported from: Surface.ts:64-70 — Escape or ` blurs the field (the global `
    // shortcut focuses it; inside the field ` closes it again).
    if (Qt::Key_Escape == event->key() || Qt::Key_QuoteLeft == event->key()) {
        loseFocus();
        event->accept();
        return;
    }

    if (Qt::Key_Return == event->key() || Qt::Key_Enter == event->key()) {
        // Ported from: handleKeyPress (KeyinField.ts:93-97) — Enter → submit.
        submitKeyin();
        event->accept();
        return;
    }

    if (m_historyLength > 0 && !m_history.isEmpty()
        && (Qt::Key_Up == event->key() || Qt::Key_Down == event->key())) {
        // Ported from: handleKeyDown (KeyinField.ts:100-127) — NB: the reference
        // maps BOTH ArrowDown and ArrowUp to direction +1 (backwards through the
        // most-recent-first history); ported 1:1.
        constexpr int direction = 1;
        event->accept();
        if (-2 == m_historyIndex)
            m_historyIndex = -1;
        int const newIndex = m_historyIndex + direction;
        if (newIndex >= 0 && newIndex < m_history.size()) {
            m_historyIndex = newIndex;
            setText(m_history.at(m_historyIndex));
        }
        return;
    }

    QLineEdit::keyPressEvent(event);
}

void KeyinField::pushHistory(QString const& keyin)
{
    // Ported from: pushHistory (KeyinField.ts:157-168) — clears the textbox,
    // resets the index, dedupes against the newest entry case-insensitively, and
    // caps the history at the configured length.
    clear();
    m_historyIndex = -2;
    if (m_history.isEmpty()
        || 0 != keyin.compare(m_history.first(), Qt::CaseInsensitive)) {
        m_history.prepend(keyin);
        while (m_history.size() > m_historyLength)
            m_history.removeLast();
    }
}

void KeyinField::submitKeyin()
{
    // Ported from: submitKeyin (KeyinField.ts:143-171).
    QString const input = text();
    if (m_historyLength > 0)
        pushHistory(input);

    auto const result = dqApp::Application::Get().GetToolAdmin().GetRegistry().parseAndRun(
        input.toStdString());

    // Failure reporting — the reference opens a MediumAlert warning message box
    // (KeyinField.ts:151-165); DanQing's NotificationManager has no message-box
    // UI, so the same strings go out as an OutputMessage warning (the established
    // notification surface in this app — main.cpp's listener → status bar).
    QString message;
    switch (result) {
        case dqApp::ParseAndRunResult::ToolNotFound:
            message = QStringLiteral("Cannot find a key-in that matches: %1").arg(input);
            break;
        case dqApp::ParseAndRunResult::BadArgumentCount:
            message = QStringLiteral("Incorrect number of arguments");
            break;
        case dqApp::ParseAndRunResult::FailedToRun:
            message = QStringLiteral("Key-in failed to run");
            break;
        case dqApp::ParseAndRunResult::Success:
        case dqApp::ParseAndRunResult::MismatchedQuotes:
            break;
    }
    if (!message.isEmpty()) {
        dqApp::Application::Get().GetNotificationManager().OutputMessage(
            dqApp::NotifyMessageDetails(dqApp::OutputMessagePriority::Warning,
                                        message.toStdString()));
    }
}

}  // namespace Gui
