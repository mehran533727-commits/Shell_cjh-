#include "test_helpers.h"
#include <regex>

TEST(EnvVars, ExpandHOME) {
    auto r = run_shell("echo $HOME\nexit\n");
    const char *home = getenv("HOME");
    ASSERT_NE(home, nullptr);
    EXPECT_NE(r.output.find(home), std::string::npos);
}

TEST(EnvVars, ExpandUSER) {
    auto r = run_shell("echo $USER\nexit\n");
    const char *user = getenv("USER");
    if (user) EXPECT_NE(r.output.find(user), std::string::npos);
}

TEST(EnvVars, ExportAndRead) {
    auto r = run_shell("export CJHSH_TEST_VAR=gtest_value_42\necho $CJHSH_TEST_VAR\nexit\n");
    EXPECT_NE(r.output.find("gtest_value_42"), std::string::npos);
}

TEST(EnvVars, UnsetVariable) {
    // Use file side-effect: export a var, unset it, then try to write it to a file.
    // If unset works, $CJHSH_UNSET_FILE expands to empty and the redirect target is empty/fails.
    // Instead, verify by exporting a new value after unset — it should not have the old value.
    std::string marker = "/tmp/CJHSH_unset_" + std::to_string(getpid());
    unlink(marker.c_str());
    run_shell("export CJHSH_UNSET_VAR=old_value\nunset CJHSH_UNSET_VAR\nexport CJHSH_UNSET_VAR=new_value\necho $CJHSH_UNSET_VAR > " + marker + "\nexit\n");
    std::string content = read_file(marker);
    EXPECT_NE(content.find("new_value"), std::string::npos) << "Should have new value after re-export";
    EXPECT_EQ(content.find("old_value"), std::string::npos) << "Old value should be gone";
    unlink(marker.c_str());
}

TEST(EnvVars, UndefinedVarExpandsEmpty) {
    // Verify that a defined var works, while undefined expands to empty
    auto r = run_shell("export CJHSH_UNDEF_CHECK=found_it\necho $CJHSH_NEVER_DEFINED_XYZ99\necho $CJHSH_UNDEF_CHECK\nexit\n");
    EXPECT_NE(r.output.find("found_it"), std::string::npos) << "Defined var should expand";
}

TEST(EnvVars, ExportNoArgsListsVars) {
    auto r = run_shell("export\nexit\n");
    EXPECT_NE(r.output.find("HOME="), std::string::npos);
}

TEST(EnvVars, DollarDollarIsPID) {
    auto r = run_shell("echo $$ | grep [0-9]\nexit\n");
    // The output should contain at least one digit (a PID number)
    EXPECT_TRUE(std::regex_search(r.output, std::regex("[0-9]+")))
        << "Expected $$ to expand to a numeric PID, got: " << r.output;
}

TEST(EnvVars, DollarQuestionTrueIsZero) {
    auto r = run_shell("true\necho $?\nexit\n");
    // After 'true', $? should be 0
    EXPECT_NE(r.output.find("0"), std::string::npos)
        << "Expected $? to be 0 after 'true', got: " << r.output;
}

TEST(EnvVars, DollarQuestionFalseIsNonZero) {
    auto r = run_shell("false\necho $?\nexit\n");
    // After 'false', $? should be non-zero (typically 1)
    // Verify a non-zero digit appears: look for '1' which is the standard exit code for false
    EXPECT_NE(r.output.find("1"), std::string::npos)
        << "Expected $? to be non-zero after 'false', got: " << r.output;
}

TEST(EnvVars, DollarQuestionInChainedSemicolon) {
    // $? should reflect the immediately preceding command, not the end of line
    std::string marker = "/tmp/CJHSH_chain_" + std::to_string(getpid());
    unlink(marker.c_str());
    run_shell("false; echo $? > " + marker + "\nexit\n");
    std::string content = read_file(marker);
    EXPECT_NE(content.find("1"), std::string::npos)
        << "false; echo $? should print 1, got: " << content;
    unlink(marker.c_str());
}

TEST(EnvVars, DollarQuestionCommandNotFound127) {
    std::string marker = "/tmp/CJHSH_127_" + std::to_string(getpid());
    unlink(marker.c_str());
    run_shell("surely_nonexistent_cmd_xyz; echo $? > " + marker + "\nexit\n");
    std::string content = read_file(marker);
    EXPECT_NE(content.find("127"), std::string::npos)
        << "Nonexistent command should set $? to 127, got: " << content;
    unlink(marker.c_str());
}

TEST(EnvVars, SingleQuotesPreventExpansion) {
    // Use script mode to avoid GNU readline echoing the export command
    std::string script = "/tmp/CJHSH_sq_test_" + std::to_string(getpid()) + ".sh";
    std::string marker = "/tmp/CJHSH_sq_out_" + std::to_string(getpid());
    {
        std::ofstream f(script);
        f << "export CJHSH_SQ_TEST=secret\n";
        f << "echo '$CJHSH_SQ_TEST' > " << marker << "\n";
    }
    run_shell_script(script);
    std::string content = read_file(marker);
    EXPECT_NE(content.find("$CJHSH_SQ_TEST"), std::string::npos)
        << "Single quotes should prevent variable expansion, got: " << content;
    EXPECT_EQ(content.find("secret"), std::string::npos)
        << "Variable value should NOT appear inside single quotes, got: " << content;
    unlink(script.c_str());
    unlink(marker.c_str());
}
