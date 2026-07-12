#include "test_helpers.h"

TEST(Explain, ShowsCommandHintAndFlags) {
    auto r = run_shell("explain tar -xzf archive.tar.gz\nexit\n");
    EXPECT_NE(r.output.find("创建或解压压缩归档"),
              std::string::npos);
    EXPECT_NE(r.output.find("解压归档文件"),
              std::string::npos);
    EXPECT_NE(r.output.find("通过 gzip 过滤"), std::string::npos);
}

TEST(Explain, UnknownCommandReportsError) {
    auto r = run_shell("explain totally_fake_command\nexit\n");
    EXPECT_NE(r.output.find("没有"), std::string::npos);
}

TEST(Explain, MissingArgumentPrintsUsage) {
    auto r = run_shell("explain\nexit\n");
    EXPECT_NE(r.output.find("用法："), std::string::npos);
}
