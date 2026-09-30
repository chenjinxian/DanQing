// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — key-in argument parsing
// Ported from: itwinjs-core core/frontend-devtools/src/tools/ParseArgs.ts
//
// Consumed by key-in tools (SaveImageTool.parseAndRun etc. — the display-test-app
// tools take "name=value" flags). Lives beside ToolRegistry::parseAndRun, which
// produces the argument token list this module parses.
#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dqApp {

// Represents parsed arguments as name-value pairs.
// Ported from: itwinjs-core ToolArgs (ParseArgs.ts:16-30).
class DQ_APP_EXPORT ToolArgs {
public:
    // Ported from: itwinjs-core parseArgs (ParseArgs.ts:37-45) — each input string
    // is expected to be of the format "name=value"; names are lower-cased, values
    // are left untouched.
    explicit ToolArgs(std::vector<std::string> const& args);

    // Find the value associated with the first argument that begins with the
    // specified prefix, case-insensitively; or nullopt if no such argument exists.
    // Ported from: itwinjs-core ToolArgs.get / findArgValue (ParseArgs.ts:47-55).
    std::optional<std::string> get(std::string const& namePrefix) const;

    // Convert the value associated with the first argument beginning with the
    // specified prefix to an integer; nullopt if not found or not an integer.
    // Ported from: itwinjs-core ToolArgs.getInteger (ParseArgs.ts:52-61).
    std::optional<int> getInteger(std::string const& namePrefix) const;

    // Convert the value associated with the first argument beginning with the
    // specified prefix to a boolean, where "1" indicates true and "0" indicates
    // false. Ported from: itwinjs-core ToolArgs.getBoolean (ParseArgs.ts:52-58).
    std::optional<bool> getBoolean(std::string const& namePrefix) const;

    // Convert the value associated with the first argument beginning with the
    // specified prefix to a float; nullopt if not found or not a float.
    // Ported from: itwinjs-core ToolArgs.getFloat (ParseArgs.ts:63-72).
    std::optional<double> getFloat(std::string const& namePrefix) const;

private:
    // Insertion-ordered pairs — the reference's findArgValue returns the FIRST
    // (insertion-order) entry whose key starts with the prefix (TS Map iteration).
    std::vector<std::pair<std::string, std::string>> m_entries;
};

// Given a list of arguments, parse the arguments into name-value pairs.
// Ported from: itwinjs-core parseArgs (ParseArgs.ts:37-45).
inline ToolArgs parseArgs(std::vector<std::string> const& args) { return ToolArgs(args); }

}  // namespace dqApp
