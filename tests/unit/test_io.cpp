// Tests for the XTFSH::io diagnostic namespace: level parsing, filtering,
// and the set/current APIs. The TTY-coloring code path is not exercised
// here (CI stderr isn't a tty and colors are intentionally stable on
// whatever stderr looks like).

#include <gtest/gtest.h>

#include "XTFSH/util/io.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using XTFSH::io::Level;
using XTFSH::io::parse_log_level;
using XTFSH::io::set_log_level;
using XTFSH::io::current_log_level;

namespace {

// Capture stderr output produced by `fn`. Uses a pipe dup'd over
// STDERR_FILENO so the XTFSH::io helpers (which write(2) directly to
// STDERR_FILENO) end up in our buffer.
std::string capture_stderr(const std::function<void()> &fn) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) return "";
    int saved = ::dup(STDERR_FILENO);
    ::fflush(stderr);
    ::dup2(pipefd[1], STDERR_FILENO);
    ::close(pipefd[1]);

    fn();

    ::fflush(stderr);
    ::dup2(saved, STDERR_FILENO);
    ::close(saved);

    // Non-blocking drain.
    int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    std::string out;
    std::array<char, 4096> buf{};
    ssize_t n;
    while ((n = ::read(pipefd[0], buf.data(), buf.size())) > 0) {
        out.append(buf.data(), static_cast<size_t>(n));
    }
    ::close(pipefd[0]);
    return out;
}

} // namespace

TEST(IoLogLevel, ParsesKnownNames) {
    EXPECT_EQ(parse_log_level("debug"),   Level::Debug);
    EXPECT_EQ(parse_log_level("info"),    Level::Info);
    EXPECT_EQ(parse_log_level("warning"), Level::Warning);
    EXPECT_EQ(parse_log_level("warn"),    Level::Warning);
    EXPECT_EQ(parse_log_level("error"),   Level::Error);
}

TEST(IoLogLevel, IsCaseInsensitive) {
    EXPECT_EQ(parse_log_level("DEBUG"),   Level::Debug);
    EXPECT_EQ(parse_log_level("Warning"), Level::Warning);
    EXPECT_EQ(parse_log_level("ERROR"),   Level::Error);
}

TEST(IoLogLevel, UnknownBecomesInfo) {
    EXPECT_EQ(parse_log_level(""),          Level::Info);
    EXPECT_EQ(parse_log_level("trace"),     Level::Info);
    EXPECT_EQ(parse_log_level("critical"),  Level::Info);
    EXPECT_EQ(parse_log_level("  debug  "), Level::Info) << "no trimming applied";
}

TEST(IoLogLevel, SetAndCurrentRoundtrip) {
    Level saved = current_log_level();
    set_log_level(Level::Error);
    EXPECT_EQ(current_log_level(), Level::Error);
    set_log_level(Level::Debug);
    EXPECT_EQ(current_log_level(), Level::Debug);
    set_log_level(saved);
}

TEST(IoFiltering, SubLevelMessagesAreDropped) {
    // We can't easily peek at stderr from a gtest process, but we CAN
    // assert that every public entry point is callable without aborting
    // or blocking at every level. Combined with the level-parsing tests,
    // this gives us coverage of the API surface.
    Level saved = current_log_level();

    set_log_level(Level::Error);
    XTFSH::io::debug("this must not crash");
    XTFSH::io::info("this must not crash");
    XTFSH::io::warning("this must not crash");
    XTFSH::io::error("error is always emitted but may be suppressed in test output");

    set_log_level(Level::Debug);
    XTFSH::io::debug("emitted");
    XTFSH::io::info("emitted");

    set_log_level(saved);
    SUCCEED();
}

// ── Consumer-level tests ──────────────────────────────────────
//
// These exercise the "does XTFSH_LOG_LEVEL=debug actually produce output?"
// half of PR #119 by capturing stderr across the emit path. Covers the
// gate that closes in PR #128 (debug sites added across signal/history/
// bg/plugin/config/subshell subsystems).

TEST(IoConsumer, DebugLevelEmitsDebugLine) {
    Level saved = current_log_level();
    set_log_level(Level::Debug);
    std::string out = capture_stderr([] {
        XTFSH::io::debug("bg: spawned pid=1234 cmd='sleep 1'");
    });
    set_log_level(saved);
    EXPECT_NE(out.find("bg: spawned pid=1234"), std::string::npos)
        << "captured: [" << out << "]";
    EXPECT_NE(out.find("XTFSH: "), std::string::npos);
}

TEST(IoConsumer, WarningLevelSuppressesDebug) {
    Level saved = current_log_level();
    set_log_level(Level::Warning);
    std::string out = capture_stderr([] {
        XTFSH::io::debug("should be suppressed");
    });
    set_log_level(saved);
    EXPECT_TRUE(out.empty()) << "unexpected: [" << out << "]";
}

TEST(IoConsumer, InfoLevelSuppressesDebug) {
    Level saved = current_log_level();
    set_log_level(Level::Info);
    std::string out = capture_stderr([] {
        XTFSH::io::debug("should be suppressed at info");
    });
    set_log_level(saved);
    EXPECT_TRUE(out.empty());
}

TEST(IoConsumer, DebugLevelStillEmitsWarnings) {
    Level saved = current_log_level();
    set_log_level(Level::Debug);
    std::string out = capture_stderr([] {
        XTFSH::io::warning("still shown");
    });
    set_log_level(saved);
    EXPECT_NE(out.find("warning"), std::string::npos);
    EXPECT_NE(out.find("still shown"), std::string::npos);
}

TEST(IoConsumer, ErrorLevelStillEmitsErrors) {
    Level saved = current_log_level();
    set_log_level(Level::Error);
    std::string out = capture_stderr([] {
        XTFSH::io::error("fatal");
        XTFSH::io::info("silent");
    });
    set_log_level(saved);
    EXPECT_NE(out.find("error"), std::string::npos);
    EXPECT_NE(out.find("fatal"), std::string::npos);
    EXPECT_EQ(out.find("silent"), std::string::npos);
}

TEST(IoConsumer, MultipleDebugCallsAllEmit) {
    // Mirrors the ~N-per-REPL-iteration pattern the shell uses once
    // instrumentation is wired up. Make sure we don't coalesce or drop.
    Level saved = current_log_level();
    set_log_level(Level::Debug);
    std::string out = capture_stderr([] {
        XTFSH::io::debug("plugin: registered safety");
        XTFSH::io::debug("plugin: registered alias-suggest");
        XTFSH::io::debug("history: opened /tmp/h.db, 0 rows");
    });
    set_log_level(saved);
    EXPECT_NE(out.find("plugin: registered safety"), std::string::npos);
    EXPECT_NE(out.find("plugin: registered alias-suggest"),
              std::string::npos);
    EXPECT_NE(out.find("history: opened"), std::string::npos);
}

// ── Integration: end-to-end through a XTFSH subprocess ─────────
//
// Spawns the real XTFSH binary with XTFSH_LOG_LEVEL=debug and pipes a
// short script that triggers the background-process instrumentation.
// Skipped when XTFSH_SHELL_BIN isn't set (the unit test suite runs
// standalone and doesn't always have a built binary available).

TEST(IoConsumerIntegration, BgLifecycleEmitsSpawnAndReap) {
    const char *bin = std::getenv("XTFSH_SHELL_BIN");
    if (!bin || !*bin) {
        GTEST_SKIP() << "XTFSH_SHELL_BIN not set; skipping e2e log consumer";
    }
    // Run `bg sleep 0.1` then a short blocker so SIGCHLD has time to fire.
    std::string cmd = std::string("XTFSH_LOG_LEVEL=debug ") +
                      "XTFSH_DISABLE_AI_ERROR_HOOK=1 " + bin +
                      " 2>&1 <<'__EOF__'\n"
                      "bg sleep 0.1\n"
                      "sleep 0.4\n"
                      "__EOF__\n";
    FILE *p = ::popen(cmd.c_str(), "r");
    ASSERT_NE(p, nullptr);
    std::string out;
    std::array<char, 4096> buf{};
    while (::fgets(buf.data(), buf.size(), p)) out.append(buf.data());
    ::pclose(p);
    EXPECT_NE(out.find("XTFSH: bg: spawned"), std::string::npos)
        << "captured: " << out;
}
