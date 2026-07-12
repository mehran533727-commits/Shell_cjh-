#include <gtest/gtest.h>

#include "XTFSH/util/config_file.h"
#include "XTFSH/util/config_resolver.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

// Scoped stdout/stderr redirect — malformed-JSON path writes a warning,
// and we don't want it cluttering test output. Also lets us inspect it.
class StderrCapture {
public:
    StderrCapture() {
        char tmpl[] = "/tmp/XTFSH_config_stderr_XXXXXX";
        fd_ = ::mkstemp(tmpl);
        path_ = tmpl;
        saved_fd_ = ::dup(STDERR_FILENO);
        ::dup2(fd_, STDERR_FILENO);
    }
    ~StderrCapture() {
        ::dup2(saved_fd_, STDERR_FILENO);
        ::close(saved_fd_);
        ::close(fd_);
        ::unlink(path_.c_str());
    }
    std::string str() const {
        ::fsync(fd_);
        ::lseek(fd_, 0, SEEK_SET);
        std::string output;
        char buffer[4096];
        ssize_t count = 0;
        while ((count = ::read(fd_, buffer, sizeof(buffer))) > 0) {
            output.append(buffer, static_cast<size_t>(count));
        }
        return output;
    }
private:
    int fd_ = -1;
    int saved_fd_ = -1;
    std::string path_;
};

// Each test gets its own temporary $HOME so `get_data_dir()` resolves
// under it (with XDG/XTFSH_DATA_HOME cleared). That means writing a
// config at `<home>/.XTFSH/config.json` is the file the loader reads.
class ConfigFileTest : public ::testing::Test {
protected:
    std::string tmp_home;
    std::string saved_home, saved_xdg_data, saved_XTFSH_data, saved_log_level;

    void SetUp() override {
        saved_home       = save("HOME");
        saved_xdg_data   = save("XDG_DATA_HOME");
        saved_XTFSH_data  = save("XTFSH_DATA_HOME");
        saved_log_level  = save("XTFSH_LOG_LEVEL");

        unsetenv("XDG_DATA_HOME");
        unsetenv("XTFSH_DATA_HOME");
        unsetenv("XTFSH_LOG_LEVEL");

        // PID + nanosecond suffix to avoid collisions between
        // concurrent tests.
        tmp_home = "/tmp/XTFSH_config_file_test_" +
                   std::to_string(::getpid()) + "_" +
                   std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch().count());
        fs::remove_all(tmp_home);
        fs::create_directories(tmp_home + "/.XTFSH");
        setenv("HOME", tmp_home.c_str(), 1);
    }

    void TearDown() override {
        fs::remove_all(tmp_home);
        restore("HOME",           saved_home);
        restore("XDG_DATA_HOME",  saved_xdg_data);
        restore("XTFSH_DATA_HOME", saved_XTFSH_data);
        restore("XTFSH_LOG_LEVEL", saved_log_level);
    }

    std::string config_path() const { return tmp_home + "/.XTFSH/config.json"; }

    void write_config(const std::string &body) const {
        std::ofstream out(config_path());
        out << body;
    }

    static std::string save(const char *k) {
        const char *v = std::getenv(k);
        return v ? std::string(v) : std::string("\x01");  // sentinel=unset
    }
    static void restore(const char *k, const std::string &v) {
        if (v == "\x01") unsetenv(k);
        else setenv(k, v.c_str(), 1);
    }
};

// ── Missing file → defaults ────────────────────────────────────

TEST_F(ConfigFileTest, MissingFileYieldsDefaults) {
    // No config.json written.
    auto cfg = XTFSH::config::load();
    EXPECT_TRUE(cfg.disabled_plugins.empty());
    EXPECT_EQ(cfg.log_level, "info");
}

// ── Valid file with both fields ────────────────────────────────

TEST_F(ConfigFileTest, ValidConfigIsParsed) {
    write_config(R"({
        "plugins": { "disabled": ["safety", "fish"] },
        "log_level": "warn"
    })");

    auto cfg = XTFSH::config::load();
    ASSERT_EQ(cfg.disabled_plugins.size(), 2u);
    EXPECT_EQ(cfg.disabled_plugins[0], "safety");
    EXPECT_EQ(cfg.disabled_plugins[1], "fish");
    EXPECT_EQ(cfg.log_level, "warn");
}

// ── Unknown fields are ignored (forward compat) ────────────────

TEST_F(ConfigFileTest, UnknownFieldsAreIgnored) {
    write_config(R"({
        "log_level": "debug",
        "totally_new_setting": { "nested": true }
    })");

    auto cfg = XTFSH::config::load();
    EXPECT_EQ(cfg.log_level, "debug");
    EXPECT_TRUE(cfg.disabled_plugins.empty());
}

// ── Malformed JSON → warning + defaults, no throw ──────────────

TEST_F(ConfigFileTest, MalformedJsonYieldsDefaultsWithWarning) {
    write_config("{ this is not json");

    XTFSH::config::UserConfig cfg;
    std::string stderr_text;
    {
        StderrCapture cap;
        EXPECT_NO_THROW(cfg = XTFSH::config::load());
        stderr_text = cap.str();
    }

    EXPECT_TRUE(cfg.disabled_plugins.empty());
    EXPECT_EQ(cfg.log_level, "info");
    EXPECT_NE(stderr_text.find("XTFSH: 警告"), std::string::npos);
    EXPECT_NE(stderr_text.find("config.json"), std::string::npos);
}

// ── XTFSH_LOG_LEVEL env override wins ───────────────────────────

TEST_F(ConfigFileTest, EnvLogLevelOverridesFile) {
    write_config(R"({ "log_level": "warn" })");
    setenv("XTFSH_LOG_LEVEL", "debug", 1);

    auto cfg = XTFSH::config::load();
    EXPECT_EQ(cfg.log_level, "debug");
}

// ── XTFSH_LOG_LEVEL applies even when no file exists ────────────

TEST_F(ConfigFileTest, EnvLogLevelAppliesWithoutFile) {
    setenv("XTFSH_LOG_LEVEL", "error", 1);

    auto cfg = XTFSH::config::load();
    EXPECT_EQ(cfg.log_level, "error");
    EXPECT_TRUE(cfg.disabled_plugins.empty());
}

// ── Non-string entries inside `plugins.disabled` are skipped ───

TEST_F(ConfigFileTest, NonStringDisabledEntriesAreSkipped) {
    write_config(R"({
        "plugins": { "disabled": ["fig", 42, null, "starship"] }
    })");

    auto cfg = XTFSH::config::load();
    ASSERT_EQ(cfg.disabled_plugins.size(), 2u);
    EXPECT_EQ(cfg.disabled_plugins[0], "fig");
    EXPECT_EQ(cfg.disabled_plugins[1], "starship");
}

// ── loaded() reflects set_loaded() ─────────────────────────────

TEST_F(ConfigFileTest, LoadedAccessorMirrorsSetLoaded) {
    XTFSH::config::UserConfig cfg;
    cfg.disabled_plugins = {"fish"};
    cfg.log_level = "debug";
    XTFSH::config::set_loaded(cfg);

    EXPECT_EQ(XTFSH::config::loaded().log_level, "debug");
    ASSERT_EQ(XTFSH::config::loaded().disabled_plugins.size(), 1u);
    EXPECT_EQ(XTFSH::config::loaded().disabled_plugins[0], "fish");

    // Reset so other tests start from a clean slate.
    XTFSH::config::set_loaded(XTFSH::config::UserConfig{});
}
