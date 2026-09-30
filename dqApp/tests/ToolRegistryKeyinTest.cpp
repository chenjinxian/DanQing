// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — ToolRegistry key-in parsing tests
//
// Ported from: itwinjs-core core/frontend/src/test/ToolRegistry.test.ts
//              (describe("ToolRegistry") — parseKeyin/parseAndRun/tokenize 的
//              行为契约；Keyin 输入框 M-L(3) 的引擎半边)。
//
// Port adaptation: the reference registers a generated 2544-entry command list
// (createTestTools, ToolRegistry.test.ts:247-255) plus TestImmediate; this port
// registers the subset those cases exercise, with the reference's keyin strings
// verbatim ("uccalc", "preprocessor", "preprocessor format",
// "preprocessor format double", "place", "place fence", "inputmanager training",
// "Localized TestImmediate Keyin"). The reference reads keyins from the
// localization namespace; DanQing passes them at registration (ToolAdmin.h
// Register) — same strings, no locale layer.
// findPartialMatches (fuzzy search) is not ported — the two "partial matches"
// cases are omitted (registered with the FuzzySearch port).
#include <gtest/gtest.h>

#include <QString>

#include <dqApp/ToolAdmin.h>

#include <cstdlib>
#include <string>
#include <vector>

using namespace dqApp;

namespace {

// Ported from: TestImmediate (ToolRegistry.test.ts:10-28) — minArgs/maxArgs 2;
// parseAndRun parses its two arguments and reports both to the test globals
// (TS file-scope testVal1/testVal2 — the registry deletes the instance it
// creates, so the values land in static storage).
class TestImmediateTool : public InteractiveTool {
public:
    const char* getToolId() const override { return "Test.Immediate"; }
    int minArgs() const override { return 2; }
    int maxArgs() const override { return 2; }
    std::string englishKeyin() const override { return "Localized TestImmediate Keyin"; }

    bool parseAndRun(std::vector<std::string> const& args) override
    {
        // TS: if (arguments.length !== 2) return false; — arity checked by the
        // registry (BadArgumentCount) before the call in DanQing; the direct-call
        // case keeps the reference's false return.
        if (args.size() != 2)
            return false;
        s_testVal1 = std::atoi(args[0].c_str());
        s_testVal2 = std::atoi(args[1].c_str());
        return true;
    }

    static inline int s_testVal1 = 0;
    static inline int s_testVal2 = 0;
};

// A no-arg generic tool (the generated test list's tools — update/place/uccalc/
// preprocessor family). Their parse behavior is registry-driven: the keyin lives
// in the registry entry (Register's third argument), not on the instance — the
// tool's run does nothing observable beyond being found and executed.
class TestGenericTool : public InteractiveTool {
public:
    const char* getToolId() const override { return "Test.Generic"; }
    std::string englishKeyin() const override { return {}; }  // unused by parsing
    bool parseAndRun(std::vector<std::string> const& args) override
    {
        lastArgs = args;
        return true;
    }

    static inline std::vector<std::string> lastArgs;
};

// The registry under test + the reference's tool set (see the file comment).
struct TestRegistry {
    ToolRegistry registry;
    TestRegistry()
    {
        registry.Register("uccalc", []() -> InteractiveTool* { return new TestGenericTool(); },
                          "uccalc");
        registry.Register("preprocessor", []() -> InteractiveTool* { return new TestGenericTool(); },
                          "preprocessor");
        registry.Register("preprocessor.format",
                          []() -> InteractiveTool* { return new TestGenericTool(); },
                          "preprocessor format");
        registry.Register("preprocessor.format.double",
                          []() -> InteractiveTool* { return new TestGenericTool(); },
                          "preprocessor format double");
        registry.Register("place", []() -> InteractiveTool* { return new TestGenericTool(); },
                          "place");
        registry.Register("place.fence", []() -> InteractiveTool* { return new TestGenericTool(); },
                          "place fence");
        registry.Register("update", []() -> InteractiveTool* { return new TestGenericTool(); },
                          "update");
        registry.Register("inputmanager.training",
                          []() -> InteractiveTool* { return new TestGenericTool(); },
                          "inputmanager training");
        registry.Register("Test.Immediate",
                          []() -> InteractiveTool* { return new TestImmediateTool(); },
                          "Localized TestImmediate Keyin");
    }
};

// Ported from: testKeyinArgs (ToolRegistry.test.ts:91-101).
void testKeyinArgs(ToolRegistry const& registry, std::string const& keyin,
                   std::vector<std::string> const& args)
{
    ToolRegistry::ParsedKeyin const result = registry.parseKeyin(keyin);
    ASSERT_TRUE(result.ok) << keyin;
    ASSERT_EQ(result.args.size(), args.size()) << keyin;
    for (std::size_t i = 0; i < args.size(); ++i)
        EXPECT_EQ(args[i], result.args[i]) << keyin << " arg " << i;
}

// Ported from: expectParseError (ToolRegistry.test.ts:103-108).
void expectParseError(ToolRegistry const& registry, std::string const& keyin,
                      ParseAndRunResult expectedError)
{
    ToolRegistry::ParsedKeyin const result = registry.parseKeyin(keyin);
    EXPECT_FALSE(result.ok) << keyin;
    if (!result.ok)
        EXPECT_EQ(expectedError, result.error) << keyin;
}

}  // namespace

// Ported from: ToolRegistry.test.ts "Should find Select tool" — findExactMatch is
// case-insensitive over the registered keyins (the reference asserts the
// registered Select tool is found by "Select Elements").
TEST(ToolRegistryKeyin, FindsSelectToolCaseInsensitively)
{
    ToolRegistry registry;
    registry.Register("Select", []() -> InteractiveTool* { return new TestGenericTool(); },
                      "select elements");
    bool found = false;
    for (auto const& tool : registry.getToolList())
        if (0 == QString::fromStdString(tool.keyin).compare(
                     QStringLiteral("Select Elements"), Qt::CaseInsensitive))
            found = true;
    EXPECT_TRUE(found);
}

// Ported from: ToolRegistry.test.ts "Should execute the TestImmediate command" —
// minArgs/maxArgs surface + parseAndRun delivers the parsed arguments.
TEST(ToolRegistryKeyin, ExecutesImmediateToolWithParsedArgs)
{
    TestRegistry fixture;
    TestImmediateTool tool;
    EXPECT_EQ(2, tool.minArgs());
    EXPECT_EQ(2, tool.maxArgs());

    EXPECT_TRUE(tool.parseAndRun({"5", "33"}));
    EXPECT_EQ(5, TestImmediateTool::s_testVal1);
    EXPECT_EQ(33, TestImmediateTool::s_testVal2);

    // TS: await new TestImmediate().parseAndRun("125") → false (wrong arity).
    EXPECT_FALSE(tool.parseAndRun({"125"}));
}

// Ported from: ToolRegistry.test.ts "Should parse command with quoted arguments".
TEST(ToolRegistryKeyin, ParsesQuotedArguments)
{
    TestRegistry fixture;
    testKeyinArgs(fixture.registry, "uccalc test args with \"a quoted string\" included",
                  {"test", "args", "with", "a quoted string", "included"});
    testKeyinArgs(fixture.registry, "uccalc \"a quoted string\"", {"a quoted string"});
    testKeyinArgs(fixture.registry, "uccalc this has \"a quoted string\"",
                  {"this", "has", "a quoted string"});
    testKeyinArgs(fixture.registry, "uccalc \"a quoted string\" is before me",
                  {"a quoted string", "is", "before", "me"});
    testKeyinArgs(fixture.registry, "uccalc \"my arg\"", {"my arg"});
}

// Ported from: ToolRegistry.test.ts "Should parse quoted arguments with embedded
// quotes" — a literal " is embedded as "".
TEST(ToolRegistryKeyin, ParsesEmbeddedQuotes)
{
    TestRegistry fixture;
    testKeyinArgs(fixture.registry, "uccalc \"a single \"\" inside\"", {"a single \" inside"});
    testKeyinArgs(fixture.registry, "uccalc \"\"\" is first\"", {"\" is first"});
    testKeyinArgs(fixture.registry, "uccalc \"trailing \"\"\"", {"trailing \""});
    testKeyinArgs(fixture.registry, "uccalc \"double \"\"\"\" quotes\"", {"double \"\" quotes"});
    testKeyinArgs(fixture.registry, "uccalc \"\" \"\"\"\" \"\"\"\"\"\"", {"", "\"", "\"\""});
    testKeyinArgs(fixture.registry,
                  "uccalc no \"yes \"\"\" no \"\"\" yes\" no \"yes \"\" yes\"",
                  {"no", "yes \"", "no", "\" yes", "no", "yes \" yes"});
}

// Ported from: ToolRegistry.test.ts "Should parse command with mismatched quotes".
TEST(ToolRegistryKeyin, ReportsMismatchedQuotes)
{
    TestRegistry fixture;
    expectParseError(fixture.registry, "uccalc \"test", ParseAndRunResult::MismatchedQuotes);
    expectParseError(fixture.registry, "uccalc abc \"xyz", ParseAndRunResult::MismatchedQuotes);
    expectParseError(fixture.registry, "uccalc abc \"x \"\" y \"\" z",
                     ParseAndRunResult::MismatchedQuotes);
}

// Ported from: ToolRegistry.test.ts "Should not consider quoted tokens as part of
// tool keyin" — the tool keyin match consumes only unquoted leading tokens.
TEST(ToolRegistryKeyin, QuotedTokensAreNotPartOfToolKeyin)
{
    TestRegistry fixture;
    testKeyinArgs(fixture.registry, "preprocessor format double", {});
    testKeyinArgs(fixture.registry, "preprocessor format double abc \"d e f\"",
                  {"abc", "d e f"});
    testKeyinArgs(fixture.registry, "preprocessor format \"double\"", {"double"});
    testKeyinArgs(fixture.registry, "preprocessor format \"double\" abc \"d e f\"",
                  {"double", "abc", "d e f"});
    testKeyinArgs(fixture.registry, "preprocessor \"format\" double", {"format", "double"});

    // A quoted first token leaves no unquoted leading substring → ToolNotFound.
    expectParseError(fixture.registry, "\"preprocessor\" format double",
                     ParseAndRunResult::ToolNotFound);
}

// Ported from: ToolRegistry.test.ts "Should parse whitespace" — a quoted argument
// must be preceded by whitespace; tabs/newlines separate tokens.
TEST(ToolRegistryKeyin, ParsesWhitespaceForms)
{
    TestRegistry fixture;
    testKeyinArgs(fixture.registry, "uccalc abc xyz\"", {"abc", "xyz\""});
    testKeyinArgs(fixture.registry,
                  "  uccalc   one two  three   \"four\"     \"five six\" seven",
                  {"one", "two", "three", "four", "five six", "seven"});
    testKeyinArgs(fixture.registry, "uccalc one\"two\"three four\"\"five\"",
                  {"one\"two\"three", "four\"\"five\""});
    testKeyinArgs(fixture.registry, "\tuccalc\none\t \ttwo \n three", {"one", "two", "three"});
}

// Ported from: ToolRegistry.test.ts "Should find the MicroStation inputmanager
// training command" — parseAndRun executes the located tool and reports Success.
TEST(ToolRegistryKeyin, ParseAndRunExecutesLocatedTool)
{
    TestRegistry fixture;
    EXPECT_EQ(ParseAndRunResult::Success, fixture.registry.parseAndRun("inputmanager training"));
    EXPECT_TRUE(TestGenericTool::lastArgs.empty());

    // ToolNotFound for an unregistered key-in (single token and multi-token forms).
    EXPECT_EQ(ParseAndRunResult::ToolNotFound, fixture.registry.parseAndRun("fjt"));
    EXPECT_EQ(ParseAndRunResult::ToolNotFound,
              fixture.registry.parseAndRun("no such keyin here"));
}

// Ported from: ToolRegistry.parseAndRun's BadArgumentCount branch (Tool.ts:1198-
// 1200) + Tool.minArgs/maxArgs doc contract (Tool.ts:375-385).
TEST(ToolRegistryKeyin, EnforcesArgumentCountConstraints)
{
    TestRegistry fixture;
    // TestImmediate: minArgs == maxArgs == 2.
    EXPECT_EQ(ParseAndRunResult::BadArgumentCount,
              fixture.registry.parseAndRun("Localized TestImmediate Keyin"));
    EXPECT_EQ(ParseAndRunResult::BadArgumentCount,
              fixture.registry.parseAndRun("Localized TestImmediate Keyin 1 2 3"));
    EXPECT_EQ(ParseAndRunResult::Success,
              fixture.registry.parseAndRun("Localized TestImmediate Keyin 4 22"));
    EXPECT_EQ(4, TestImmediateTool::s_testVal1);
    EXPECT_EQ(22, TestImmediateTool::s_testVal2);
}

// View.* tools have no key-in in the reference locale (CoreTools.json has no
// tools.View.Fit.keyin) — they are not key-in reachable. Authored: no direct
// reference case (the reference registry never registers view tools in this test
// file); the empty-keyin-nonmatchable contract follows Tool.ts:411-418's
// "no translation" default.
TEST(ToolRegistryKeyin, EmptyKeyinIsNotReachable)
{
    ToolRegistry registry;
    registry.Register("View.Fit", []() -> InteractiveTool* { return new TestGenericTool(); });
    EXPECT_EQ(ParseAndRunResult::ToolNotFound, registry.parseAndRun("View.Fit"));
    EXPECT_TRUE(registry.getToolList().size() == 1
                && registry.getToolList()[0].keyin.empty());
}
