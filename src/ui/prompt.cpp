#include "XTFSH/core/signals.h"
#include "XTFSH/plugin.h"
#include "XTFSH/ui.h"
#include "XTFSH/util/cwd.h"
#include "XTFSH/util/safe_exec.h"
#include "theme.h"

#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace std;

string get_git_branch() {
    // 500ms timeout is plenty for a local repo and short enough not to
    // stall the prompt when cwd sits on a network mount that hangs.
    // suppress_stderr=true: in a non-repo cwd git writes
    // "fatal: not a git repository" to stderr — we must not leak that
    // to the terminal on every prompt render.
    auto r = XTFSH::util::safe_exec(
        {"git", "rev-parse", "--abbrev-ref", "HEAD"}, 500,
        /*suppress_stderr=*/true);
    if (r.exit_code != 0) return "";
    string branch = r.stdout_text;
    while (!branch.empty() && (branch.back() == '\n' || branch.back() == '\r')) {
        branch.pop_back();
    }
    return branch;
}

string get_git_status_indicators() {
    auto r = XTFSH::util::safe_exec(
        {"git", "status", "--porcelain"}, 500,
        /*suppress_stderr=*/true);
    if (r.exit_code < 0) return "";
    bool has_staged = false, has_unstaged = false, has_untracked = false;
    const string &out = r.stdout_text;
    size_t pos = 0;
    while (pos < out.size()) {
        size_t nl = out.find('\n', pos);
        size_t len = (nl == string::npos ? out.size() : nl) - pos;
        if (len >= 1) {
            char c0 = out[pos];
            if (c0 == '?') has_untracked = true;
            else if (c0 != ' ' && c0 != '?') has_staged = true;
        }
        if (len >= 2) {
            char c1 = out[pos + 1];
            if (c1 == 'M' || c1 == 'D') has_unstaged = true;
        }
        if (nl == string::npos) break;
        pos = nl + 1;
    }

    string indicators;
    if (has_staged) indicators += "+";
    if (has_unstaged) indicators += "*";
    if (has_untracked) indicators += "?";
    return indicators;
}

void set_terminal_title(const string &title) {
    if (isatty(STDOUT_FILENO)) {
        string seq = "\033]0;" + title + "\007";
        if (write(STDOUT_FILENO, seq.c_str(), seq.size())) {}
    }
}

static string short_name(const string &full) {
    for (size_t i = 0; i < full.size(); i++) {
        if (full[i] == '.' || full[i] == '_' || full[i] == '-') {
            if (i > 0) return full.substr(0, i);
        }
    }
    if (full.size() > 12) return full.substr(0, 8);
    return full;
}

static string format_duration(double seconds) {
    if (seconds < 60) {
        stringstream ss;
        ss << fixed;
        ss.precision(1);
        ss << seconds << "s";
        return ss.str();
    } else if (seconds < 3600) {
        int mins = (int)(seconds / 60);
        int secs = (int)seconds % 60;
        return to_string(mins) + "m" + to_string(secs) + "s";
    } else {
        int hrs = (int)(seconds / 3600);
        int mins = ((int)seconds % 3600) / 60;
        return to_string(hrs) + "h" + to_string(mins) + "m";
    }
}

static size_t running_background_jobs(const ShellState &state) {
    size_t count = 0;
    for (const auto &entry : state.core.background_processes) {
        if (entry.second.running) ++count;
    }
    return count;
}

string write_shell_prefix(const ShellState &state) {
    // Let a registered prompt provider (e.g. Starship) override the builtin
    // prompt. Empty result means "fall through to builtin".
    {
        std::string custom = global_plugin_registry().render_prompt(state);
        if (!custom.empty()) {
            set_terminal_title("XTFSH");
            return custom;
        }
    }

    string cwd = XTFSH::util::current_working_directory();
    const char *env_user = getenv("USER");
    const char *login = getlogin();
    string user = env_user && *env_user ? string(env_user)
                                        : (login ? string(login) : "user");
    char host_buf[256] = {0};
    string host = (gethostname(host_buf, sizeof(host_buf) - 1) == 0 &&
                   host_buf[0] != '\0')
                      ? short_name(string(host_buf))
                      : "localhost";
    size_t running_jobs = running_background_jobs(state);
    string branch = get_git_branch();

    set_terminal_title("XTF'SH Shell: " + cwd);

    if (isatty(STDOUT_FILENO)) {
        // Catppuccin Mocha colored prompt — first line written to stdout
        string prefix;
        prefix += PROMPT_SEPARATOR + "\u250c\u2500" CAT_RESET;
        prefix += PROMPT_TEXT + "[XTF'SH Shell] " CAT_RESET;
        prefix += PROMPT_USER + user + "@" + host + CAT_RESET;
        if (!branch.empty()) {
            string git_indicators = get_git_status_indicators();
            prefix += PROMPT_TEXT + " git:" CAT_RESET;
            prefix += PROMPT_BRANCH + branch + CAT_RESET;
            if (!git_indicators.empty()) {
                prefix += PROMPT_GIT_DIRTY + "[" + git_indicators + "]" CAT_RESET;
            }
        }

        if (state.core.last_cmd_duration >= 0.5) {
            prefix += PROMPT_DURATION + " took " +
                      format_duration(state.core.last_cmd_duration) + CAT_RESET;
        }

        prefix += "\n";
        prefix += PROMPT_SEPARATOR + "\u251c\u2500 " CAT_RESET +
                  PROMPT_TEXT + "cwd: " CAT_RESET +
                  PROMPT_PATH + cwd + CAT_RESET + "\n";
        prefix += PROMPT_SEPARATOR + "\u251c\u2500 " CAT_RESET +
                  PROMPT_TEXT + "jobs: " CAT_RESET +
                  to_string(running_jobs) + " running | last: exit " +
                  to_string(state.core.last_exit_status) + "\n";
        write_stdout(prefix);

        // Second line — the actual prompt passed to replxx
        string arrow_color = (state.core.last_exit_status == 0)
                                 ? PROMPT_ARROW_OK
                                 : PROMPT_ARROW_ERR;
        return PROMPT_SEPARATOR + "\u2514\u2500" CAT_RESET
               + arrow_color + "\u27a4 " CAT_RESET;
    } else {
        stringstream ss;
        ss << "\u250c\u2500[XTF'SH Shell] " << user << "@" << host << "\n";
        ss << "\u251c\u2500 cwd: " << cwd << "\n";
        ss << "\u251c\u2500 jobs: " << running_jobs
           << " running | last: exit " << state.core.last_exit_status << "\n";
        ss << "\u2514\u2500\u27a4 ";
        return ss.str();
    }
}
