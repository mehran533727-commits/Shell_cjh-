#include "test_helpers.h"

TEST(Background, BgStartsProcess) {
    auto r = run_shell("bg sleep 1\nbglist\nexit\n");
    EXPECT_NE(r.output.find("[任务 1] 已启动"), std::string::npos);
    EXPECT_NE(r.output.find("sleep"), std::string::npos);
    EXPECT_NE(r.output.find("后台任务总数：1"), std::string::npos);
}

TEST(Background, AmpersandStartsProcess) {
    auto r = run_shell("sleep 1 &\nbglist\nexit\n");
    EXPECT_NE(r.output.find("[任务 1] 已启动"), std::string::npos);
    EXPECT_NE(r.output.find("sleep 1"), std::string::npos);
    EXPECT_NE(r.output.find("后台任务总数：1"), std::string::npos);
}

TEST(Background, AmpersandAllowsNextCommand) {
    auto r = run_shell("sleep 1 & echo done\nexit\n");
    EXPECT_NE(r.output.find("[任务 1] 已启动"), std::string::npos);
    EXPECT_NE(r.output.find("done"), std::string::npos);
}

TEST(Background, AmpersandStartsPipeline) {
    auto r = run_shell("sleep 1 | cat &\nbglist\nexit\n");
    EXPECT_NE(r.output.find("[任务 1] 已启动"), std::string::npos);
    EXPECT_NE(r.output.find("sleep 1 | cat"), std::string::npos);
}

TEST(Background, AmpersandReportsFinishedJob) {
    auto r = run_shell("sleep 0.1 &\nsleep 1\nexit\n");
    EXPECT_NE(r.output.find("[任务 1] 已完成"), std::string::npos);
    EXPECT_NE(r.output.find("退出码=0"), std::string::npos);
    EXPECT_NE(r.output.find("耗时="), std::string::npos);
}

TEST(Background, BgkillNoArgs) {
    auto r = run_shell("bgkill\nexit\n");
    EXPECT_NE(r.output.find("缺少任务编号"), std::string::npos);
}

TEST(Background, BgstopNoArgs) {
    auto r = run_shell("bgstop\nexit\n");
    EXPECT_NE(r.output.find("缺少任务编号"), std::string::npos);
}

TEST(Background, BgstartNoArgs) {
    auto r = run_shell("bgstart\nexit\n");
    EXPECT_NE(r.output.find("缺少任务编号"), std::string::npos);
}

TEST(Background, BgkillNonNumeric) {
    auto r = run_shell("bgkill abc\nexit\n");
    EXPECT_NE(r.output.find("任务编号无效"), std::string::npos);
}

TEST(Background, FgNoJobs) {
    auto r = run_shell("fg\nexit\n");
    EXPECT_NE(r.output.find("fg: 没有后台任务"), std::string::npos);
}

TEST(Background, FgWithNumberNoJobs) {
    auto r = run_shell("fg 1\nexit\n");
    EXPECT_NE(r.output.find("fg: 没有后台任务"), std::string::npos);
}

TEST(Background, FgInvalidJobNumber) {
    auto r = run_shell("bg sleep 8\nfg abc\nexit\n");
    EXPECT_NE(r.output.find("fg: 任务编号无效"), std::string::npos);
}
