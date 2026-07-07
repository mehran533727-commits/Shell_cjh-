// One-time startup work: load theme, register the bundled providers,
// parse ~/.CJHSHrc, and (optionally) handle `--benchmark` mode. Kept out
// of main.cpp so the entry point stays a thin dispatcher.

#include "CJHSH/core/executor.h"
#include "CJHSH/core/parser.h"
#include "CJHSH/core/signals.h"
#include "CJHSH/history.h"
#include "CJHSH/plugin.h"
#include "CJHSH/ui.h"
#include "CJHSH/plugins/alias_suggest_provider.h"
#include "CJHSH/plugins/fig_completion_provider.h"
#include "CJHSH/plugins/fish_completion_provider.h"
#include "CJHSH/plugins/manpage_completion_provider.h"
#include "CJHSH/plugins/safety_hook_provider.h"
#include "CJHSH/plugins/starship_prompt_provider.h"
#include "CJHSH/util/benchmark.h"
#include "CJHSH/util/config_file.h"
#include "CJHSH/util/config_resolver.h"
#include "CJHSH/util/io.h"
#include "CJHSH/startup.h"
#include "theme.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>

#ifdef CJHSH_SQLITE_ENABLED
#include "CJHSH/plugins/sqlite_history_provider.h"
#endif

#include "CJHSH/ai.h"
#include "CJHSH/plugins/ai_error_hook_provider.h"

using std::string;

namespace CJHSH {

// ── Env-var gating ────────────────────────────────────────────
//
// Users can disable any bundled provider without rebuilding. Non-empty
// and not "0" means disabled.
static bool plugin_disabled(const char *env) {
    const char *v = std::getenv(env);
    return v && *v && string(v) != "0";
}

// ── Config-file gating ────────────────────────────────────────
//
// Bundled provider registrations go through this gate so both the
// env-var opt-out and the `plugins.disabled` list in config.json are
// honored. `enabled()` records which names it matched so we can warn
// the user about stale entries afterwards.
namespace {

struct ConfigGate {
    std::unordered_set<std::string> disabled;
    std::unordered_set<std::string> matched;
    bool enabled(const std::string &plugin_name) {
        auto it = disabled.find(plugin_name);
        if (it == disabled.end()) return true;
        matched.insert(plugin_name);
        return false;
    }
};

} // namespace

// ── Provider registration ─────────────────────────────────────

void register_default_plugins() {
    // Configure the diagnostic log level from the environment. There isn't
    // (yet) a config-file field for this, so CJHSH_LOG_LEVEL is the sole
    // knob; the io namespace treats unknown/missing as Info. Done here
    // rather than in main() because every early-startup code path
    // (benchmark, test harness) funnels through this function.
    if (const char *lvl = std::getenv("CJHSH_LOG_LEVEL")) {
        CJHSH::io::set_log_level(CJHSH::io::parse_log_level(lvl));
    }

    auto &reg = global_plugin_registry();

    CJHSH::config::UserConfig user_cfg = CJHSH::config::load();
    ConfigGate gate;
    gate.disabled.insert(user_cfg.disabled_plugins.begin(),
                         user_cfg.disabled_plugins.end());

    // Hook providers ------------------------------------------------
    if (!plugin_disabled("CJHSH_DISABLE_SAFETY_HOOK") &&
        gate.enabled("safety")) {
        reg.register_hook_provider(std::make_unique<SafetyHookProvider>());
        CJHSH::io::debug("plugin: registered safety");
    }
    if (!plugin_disabled("CJHSH_DISABLE_ALIAS_SUGGEST") &&
        gate.enabled("alias-suggest")) {
        reg.register_hook_provider(std::make_unique<AliasSuggestProvider>());
        CJHSH::io::debug("plugin: registered alias-suggest");
    }
    if (!plugin_disabled("CJHSH_DISABLE_AI_ERROR_HOOK") &&
        gate.enabled("ai-error-recovery")) {
        reg.register_hook_provider(std::make_unique<AiErrorHookProvider>(
            []() -> std::unique_ptr<LLMClient> { return ai_create_client(); }));
        CJHSH::io::debug("plugin: registered ai-error-recovery");
    }

    // Completion providers -----------------------------------------
    if (!plugin_disabled("CJHSH_DISABLE_MANPAGE_COMPLETION") &&
        gate.enabled("manpage")) {
        reg.register_completion_provider(
            std::make_unique<ManpageCompletionProvider>());
        CJHSH::io::debug("plugin: registered manpage");
    }
    if (!plugin_disabled("CJHSH_DISABLE_FISH_COMPLETION") &&
        gate.enabled("fish")) {
        reg.register_completion_provider(
            std::make_unique<FishCompletionProvider>());
        CJHSH::io::debug("plugin: registered fish");
    }
    if (!plugin_disabled("CJHSH_DISABLE_FIG_COMPLETION") &&
        gate.enabled("fig")) {
        reg.register_completion_provider(
            std::make_unique<FigCompletionProvider>());
        CJHSH::io::debug("plugin: registered fig");
    }

    // Prompt providers ---------------------------------------------
    if (!plugin_disabled("CJHSH_DISABLE_STARSHIP") &&
        gate.enabled("starship")) {
        reg.register_prompt_provider(
            std::make_unique<StarshipPromptProvider>());
        CJHSH::io::debug("plugin: registered starship");
    }

    // History providers --------------------------------------------
#ifdef CJHSH_SQLITE_ENABLED
    if (!plugin_disabled("CJHSH_DISABLE_SQLITE_HISTORY") &&
        gate.enabled("sqlite-history")) {
        try {
            reg.register_history_provider(
                std::make_unique<SqliteHistoryProvider>());
            CJHSH::io::debug("plugin: registered sqlite-history");
        } catch (const std::exception &e) {
            write_stderr(string("CJHSH: sqlite history disabled: ") +
                         e.what() + "\n");
        }
    }
#endif

    // Warn about disabled-list entries that matched no bundled
    // provider. Stale config is the usual cause (plugin renamed /
    // removed) and silently ignoring it hides typos.
    for (const auto &name : gate.disabled) {
        if (!gate.matched.count(name)) {
            std::cerr << "CJHSH: warning: config.json `plugins.disabled` "
                         "contains unknown plugin \"" << name
                      << "\" (ignored)\n";
        }
    }

    // Hand the config to the global slot so a future log-level
    // consumer (and tests) can retrieve it.
    CJHSH::config::set_loaded(std::move(user_cfg));
}

// ── CJHSHrc load ───────────────────────────────────────────────

void load_CJHSHrc(ShellState &state) {
    std::string path = CJHSH::config::get_CJHSHrc_path();
    std::ifstream CJHSHrc(path);
    if (!CJHSHrc.is_open()) return;
    std::string rc_line;
    while (getline(CJHSHrc, rc_line)) {
        if (rc_line.empty()) continue;
        std::vector<CommandSegment> segs = parse_command_line(rc_line);
        execute_command_line(segs, state);
    }
}

// ── Benchmark mode ────────────────────────────────────────────
//
// `CJHSH --benchmark` times every startup stage and exits with the
// breakdown. Useful for regression detection on cold start.
int run_benchmark_mode() {
    StartupBenchmark bench;

    bench.start("Theme load");
    load_user_theme();
    bench.end();

    bench.start("Plugin registration");
    register_default_plugins();
    bench.end();

    bench.start("Shell state init");
    ShellState state; (void)state;
    bench.end();

    bench.start("Command cache");
    build_command_cache();
    bench.end();

    bench.start("History load");
    (void)history_file_path();
    bench.end();

    write_stdout(bench.report());
    return 0;
}

} // namespace CJHSH
