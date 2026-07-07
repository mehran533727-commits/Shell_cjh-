// Tests for expand_variables() refusing to balloon past
// XTFSH_MAX_EXPANSION_BYTES. Declares the helper from core.h to avoid
// pulling in the rest of the shell; the parser TU is added to the
// test's source list.

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "XTFSH/core/parser.h"
#include "XTFSH/util/limits.h"

TEST(ExpansionCaps, NormalExpansionUnaffected) {
    ::setenv("XTFSH_CAP_TEST_VAR", "hello", 1);
    std::string out = expand_variables("greet=$XTFSH_CAP_TEST_VAR", 0);
    EXPECT_EQ(out, "greet=hello");
    ::unsetenv("XTFSH_CAP_TEST_VAR");
}

TEST(ExpansionCaps, OversizeVariableBailsCleanly) {
    // Build a variable whose value is 2x the cap, then reference it.
    // The helper must refuse and return an empty string.
    std::string huge(XTFSH::util::XTFSH_MAX_EXPANSION_BYTES + 10, 'x');
    ::setenv("XTFSH_CAP_TEST_HUGE", huge.c_str(), 1);
    std::string out = expand_variables("before $XTFSH_CAP_TEST_HUGE after", 0);
    EXPECT_TRUE(out.empty())
        << "cap-exceeding expansion must return empty, got size="
        << out.size();
    ::unsetenv("XTFSH_CAP_TEST_HUGE");
}

TEST(ExpansionCaps, RepeatedReferenceEventuallyCaps) {
    // Each expansion of the 600 KiB variable is under the cap, but
    // three consecutive references blow past it. The cap check fires
    // cumulatively on the result, not per-reference, so the helper
    // aborts mid-expansion.
    std::string half(XTFSH::util::XTFSH_MAX_EXPANSION_BYTES - 1000, 'y');
    ::setenv("XTFSH_CAP_TEST_HALF", half.c_str(), 1);
    std::string out = expand_variables(
        "$XTFSH_CAP_TEST_HALF$XTFSH_CAP_TEST_HALF$XTFSH_CAP_TEST_HALF", 0);
    EXPECT_TRUE(out.empty());
    ::unsetenv("XTFSH_CAP_TEST_HALF");
}
