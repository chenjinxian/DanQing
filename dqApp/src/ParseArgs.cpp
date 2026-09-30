// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — key-in argument parsing implementation
// Ported from: itwinjs-core core/frontend-devtools/src/tools/ParseArgs.ts
#include "dqApp/ParseArgs.h"

#include <cctype>
#include <cstdlib>

namespace dqApp {

namespace {
// TS `name.toLowerCase()` / `arg.split("=")` helpers.
std::string toLowerCase(std::string const& s)
{
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool startsWith(std::string const& s, std::string const& prefix)
{
    return s.size() >= prefix.size() && 0 == s.compare(0, prefix.size(), prefix);
}
}  // namespace

// Ported from: itwinjs-core parseArgs (ParseArgs.ts:37-45).
ToolArgs::ToolArgs(std::vector<std::string> const& args)
{
    for (std::string const& arg : args) {
        std::size_t const eq = arg.find('=');
        if (eq != std::string::npos && eq + 1 <= arg.size()) {
            // TS: parts.length === 2 → exactly one '='; a second '=' lands in the
            // value (TS split would make 3 parts and drop the arg — replicate by
            // only accepting args whose value contains no '='... actually TS
            // "a=b=c" splits into 3 → dropped. Replicate that faithfully.)
            if (arg.find('=', eq + 1) != std::string::npos)
                continue;
            m_entries.emplace_back(toLowerCase(arg.substr(0, eq)), arg.substr(eq + 1));
        }
    }
}

// Ported from: itwinjs-core findArgValue (ParseArgs.ts:47-55).
std::optional<std::string> ToolArgs::get(std::string const& namePrefix) const
{
    std::string const prefix = toLowerCase(namePrefix);
    for (auto const& entry : m_entries)
        if (startsWith(entry.first, prefix))
            return entry.second;
    return std::nullopt;
}

// Ported from: itwinjs-core ToolArgs.getBoolean (ParseArgs.ts:52-58).
std::optional<bool> ToolArgs::getBoolean(std::string const& namePrefix) const
{
    auto const val = get(namePrefix);
    if (val.has_value() && (*val == "0" || *val == "1"))
        return *val == "1";
    return std::nullopt;
}

// Ported from: itwinjs-core ToolArgs.getInteger (ParseArgs.ts:60-66).
std::optional<int> ToolArgs::getInteger(std::string const& namePrefix) const
{
    auto const val = get(namePrefix);
    if (!val.has_value())
        return std::nullopt;

    // TS: Number.parseInt(val, 10) — leading numeric prefix parses, trailing junk
    // ignored; Number.isNaN → undefined. strtol replicates both.
    char* end = nullptr;
    long const num = std::strtol(val->c_str(), &end, 10);
    if (end == val->c_str())
        return std::nullopt;  // no digits parsed → NaN
    return static_cast<int>(num);
}

// Ported from: itwinjs-core ToolArgs.getFloat (ParseArgs.ts:68-74).
std::optional<double> ToolArgs::getFloat(std::string const& namePrefix) const
{
    auto const val = get(namePrefix);
    if (!val.has_value())
        return std::nullopt;

    // TS: Number.parseFloat + Number.isNaN check — strtod replicates.
    char* end = nullptr;
    double const num = std::strtod(val->c_str(), &end);
    if (end == val->c_str())
        return std::nullopt;
    return num;
}

}  // namespace dqApp
