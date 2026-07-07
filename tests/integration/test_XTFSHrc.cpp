#include "test_helpers.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

// Helper that runs the shell with a custom HOME directory so we don't
// touch the real ~/.XTFSHrc.  We create a temporary directory, write a
// .XTFSHrc into it, then invoke the shell with HOME overridden.
static ShellResult run_shell_with_home(const std::string &home_dir,
                                       const std::string &input) {
    char tmpfile[] = "/tmp/XTFSH_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (write(fd, input.c_str(), input.size())) {}
    close(fd);

    std::string cmd = "HOME=" + home_dir + " " + shell_binary + " < " + tmpfile + " 2>&1";
    FILE *pipe = popen(cmd.c_str(), "r");

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    int status = pclose(pipe);
    int exit_code = WEXITSTATUS(status);

    unlink(tmpfile);
    return {output, exit_code};
}

TEST(XTFSHrc, LoadsRcFileOnStartup) {
    // Create a temporary HOME directory
    std::string tmp_home = "/tmp/XTFSH_rc_test_" + std::to_string(getpid());
    std::filesystem::create_directories(tmp_home);

    // Write a .XTFSHrc that exports a variable
    {
        std::ofstream rc(tmp_home + "/.XTFSHrc");
        rc << "export XTFSH_RC_LOADED=hello_from_XTFSHrc" << std::endl;
        rc.close();
    }

    // Run the shell with custom HOME; echo the variable to verify it was set
    auto r = run_shell_with_home(tmp_home, "echo $XTFSH_RC_LOADED\nexit\n");
    EXPECT_NE(r.output.find("hello_from_XTFSHrc"), std::string::npos)
        << "Variable exported in .XTFSHrc should be available in session. Output: " << r.output;

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(tmp_home, ec);
}

TEST(XTFSHrc, NoRcFileIsOkay) {
    // Create a temporary HOME directory with no .XTFSHrc
    std::string tmp_home = "/tmp/XTFSH_rc_empty_" + std::to_string(getpid());
    std::filesystem::create_directories(tmp_home);

    // Shell should start normally without a .XTFSHrc
    auto r = run_shell_with_home(tmp_home, "echo works_fine\nexit\n");
    EXPECT_NE(r.output.find("works_fine"), std::string::npos)
        << "Shell should work normally without .XTFSHrc. Output: " << r.output;

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(tmp_home, ec);
}

TEST(XTFSHrc, MultipleCommandsInRc) {
    // Create a temporary HOME directory
    std::string tmp_home = "/tmp/XTFSH_rc_multi_" + std::to_string(getpid());
    std::filesystem::create_directories(tmp_home);

    // Write a .XTFSHrc with multiple export commands
    {
        std::ofstream rc(tmp_home + "/.XTFSHrc");
        rc << "export XTFSH_VAR_A=alpha" << std::endl;
        rc << "export XTFSH_VAR_B=bravo" << std::endl;
        rc.close();
    }

    // Both variables should be set
    auto r = run_shell_with_home(tmp_home, "echo $XTFSH_VAR_A $XTFSH_VAR_B\nexit\n");
    EXPECT_NE(r.output.find("alpha"), std::string::npos)
        << "First var from .XTFSHrc should be set. Output: " << r.output;
    EXPECT_NE(r.output.find("bravo"), std::string::npos)
        << "Second var from .XTFSHrc should be set. Output: " << r.output;

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(tmp_home, ec);
}
