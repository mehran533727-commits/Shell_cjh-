#include "test_helpers.h"

TEST(AliasSuggest, RemindsOnExactMatch) {
    auto r = run_shell("alias gst=\"ls\"\nls\nexit\n");
    EXPECT_NE(r.output.find("此命令可使用别名：gst"),
              std::string::npos);
}

TEST(AliasSuggest, SilentWhenAliasNotUsed) {
    auto r = run_shell("pwd\nexit\n");
    EXPECT_EQ(r.output.find("此命令可使用别名："),
              std::string::npos);
}

TEST(AliasSuggest, OnlyRemindsOncePerSession) {
    auto r = run_shell(
        "alias gst=\"ls\"\n"
        "ls\n"
        "ls\n"
        "exit\n");
    // Only the first `ls` should produce the reminder.
    size_t first = r.output.find("此命令可使用别名：gst");
    ASSERT_NE(first, std::string::npos);
    size_t second = r.output.find("此命令可使用别名：gst",
                                   first + 1);
    EXPECT_EQ(second, std::string::npos);
}
