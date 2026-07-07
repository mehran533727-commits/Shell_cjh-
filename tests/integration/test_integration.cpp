#include "test_helpers.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

std::string shell_binary;

// Integration tests spawn real XTFSH subprocesses that write to
// $HOME/.XTFSH/ (AI model override, usage counter, history DB,
// config.json, etc.). Without isolation that means running `ctest`
// on a dev machine overwrites the maintainer's real config — the
// test suite used to happily set the AI model override to "gpt-4o"
// against the user's live ~/.XTFSH/ai/ directory, which is how
// "clean install" could show a nonsense Provider=gemini/Model=gpt-4o
// mismatch.
//
// Redirect HOME, and the XDG/XTFSH overrides that the config resolver
// honors, to a per-process temp dir before any test runs. The
// spawned XTFSH inherits these via popen() so every file it touches
// lives under the sandbox.
namespace {
std::string g_tmp_home;

void isolate_home_once() {
    char tmpl[] = "/tmp/XTFSH-test-home-XXXXXX";
    const char *dir = mkdtemp(tmpl);
    if (!dir) {
        std::perror("mkdtemp");
        std::exit(2);
    }
    g_tmp_home = dir;
    setenv("HOME", dir, 1);
    // Also unset the *_HOME overrides that the config resolver checks
    // first — otherwise a developer shell with e.g. XDG_DATA_HOME set
    // would defeat the isolation.
    unsetenv("XTFSH_DATA_HOME");
    unsetenv("XTFSH_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_CACHE_HOME");
}

void cleanup_home() {
    if (g_tmp_home.empty()) return;
    std::error_code ec;
    std::filesystem::remove_all(g_tmp_home, ec);
    // Swallow cleanup errors — test already passed/failed, don't
    // mask that outcome on a stale temp dir.
}
} // namespace

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    isolate_home_once();
    std::atexit(cleanup_home);

    const char *bin = getenv("XTFSH_SHELL_BIN");
    if (bin) {
        shell_binary = bin;
    } else {
        shell_binary = "./XTFSH.out";
    }

    return RUN_ALL_TESTS();
}
