// XTFSH-specific config builtins: config (sync), session, theme.
//
// Split out of the old src/builtins/shell.cpp blob so POSIX meta
// builtins (meta.cpp) and signal/trap logic (trap.cpp) stand on their
// own.

#include "XTFSH/builtins.h"
#include "XTFSH/core/signals.h"
#include "XTFSH/core/config_sync.h"
#include "XTFSH/core/session.h"
#include "theme.h"

#include <cstdio>

using namespace std;

int builtin_config(const vector<string> &argv, ShellState &) {
    if (argv.size() < 2) {
        write_stderr(
            "config: 用法：\n"
            "  config sync init                   将 ~/.XTFSH 初始化为 git 仓库\n"
            "  config sync remote <url>           设置 git 远程 URL\n"
            "  config sync push                   提交并推送修改\n"
            "  config sync pull                   拉取最新修改\n"
            "  config sync diff                   显示待处理修改\n"
            "  config sync status                 查看仓库是否已初始化\n");
        return 1;
    }
    if (argv[1] != "sync") {
        write_stderr("config: 未知子命令：" + argv[1] + "\n");
        return 1;
    }
    if (argv.size() < 3) {
        write_stderr("config: sync 需要指定操作\n");
        return 1;
    }
    std::string dir = XTFSH::config_sync::get_XTFSH_config_dir();
    const string &action = argv[2];

    if (action == "init") {
        if (!XTFSH::config_sync::sync_init(dir)) {
            write_stderr("config: sync 初始化失败\n");
            return 1;
        }
        write_stdout("config: 已初始化 " + dir + "\n");
        return 0;
    }
    if (action == "remote") {
        if (argv.size() < 4) {
            write_stderr("config: remote 需要 URL\n");
            return 1;
        }
        if (!XTFSH::config_sync::sync_set_remote(dir, argv[3])) {
            write_stderr("config: 设置远程仓库失败\n");
            return 1;
        }
        write_stdout("config: 远程仓库已设为 " + argv[3] + "\n");
        return 0;
    }
    if (action == "push") {
        if (!XTFSH::config_sync::sync_push(dir)) {
            write_stderr("config: 推送失败\n");
            return 1;
        }
        write_stdout("config: 推送成功\n");
        return 0;
    }
    if (action == "pull") {
        if (!XTFSH::config_sync::sync_pull(dir)) {
            write_stderr("config: 拉取失败\n");
            return 1;
        }
        write_stdout("config: 拉取成功\n");
        return 0;
    }
    if (action == "diff") {
        write_stdout(XTFSH::config_sync::sync_diff(dir));
        return 0;
    }
    if (action == "status") {
        if (XTFSH::config_sync::sync_is_initialized(dir)) {
            write_stdout("config: 已在 " + dir + " 初始化\n");
            return 0;
        }
        write_stdout("config: 尚未初始化（请运行 'config sync init'）\n");
        return 1;
    }

    write_stderr("config: 未知 sync 操作：" + action + "\n");
    return 1;
}

int builtin_session(const vector<string> &argv, ShellState &state) {
    if (argv.size() < 2) {
        write_stderr(
            "session: 用法：\n"
            "  session list             列出已保存的会话\n"
            "  session save <name>      将当前状态保存为 <name>\n"
            "  session load <name>      从 <name> 恢复状态\n"
            "  session rm <name>        删除已保存的 <name>\n");
        return 1;
    }

    const string &sub = argv[1];

    if (sub == "list") {
        auto sessions = list_sessions();
        if (sessions.empty()) {
            write_stdout("（没有已保存的会话）\n");
            return 0;
        }
        for (const auto &s : sessions) {
            write_stdout("  " + s.name + "  " +
                         s.working_directory + "\n");
        }
        return 0;
    }

    if (sub == "save") {
        if (argv.size() < 3) {
            write_stderr("session: save 需要名称\n");
            return 1;
        }
        const string &name = argv[2];
        SessionInfo info = capture_current_state(name, state);
        string dir = get_sessions_dir();
        string path = dir + "/" + name + ".json";
        if (!save_session(path, info)) {
            write_stderr("session: 写入失败：" + path + "\n");
            return 1;
        }
        write_stdout("session: 已将 '" + name + "' 保存到 " + path + "\n");
        return 0;
    }

    if (sub == "load") {
        if (argv.size() < 3) {
            write_stderr("session: load 需要名称\n");
            return 1;
        }
        const string &name = argv[2];
        if (!session_exists(name)) {
            write_stderr("session: 没有此会话：" + name + "\n");
            return 1;
        }
        string path = get_sessions_dir() + "/" + name + ".json";
        SessionInfo info = load_session(path);
        restore_session(info, state);
        write_stdout("session: 已加载 '" + name + "'\n");
        return 0;
    }

    if (sub == "rm") {
        if (argv.size() < 3) {
            write_stderr("session: rm 需要名称\n");
            return 1;
        }
        const string &name = argv[2];
        if (!delete_session(name)) {
            write_stderr("session: 没有此会话：" + name + "\n");
            return 1;
        }
        write_stdout("session: 已删除 '" + name + "'\n");
        return 0;
    }

    write_stderr("session: 未知子命令：" + sub + "\n");
    return 1;
}

namespace {

string hex6(const RGB &c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

void theme_print_swatch(const string &label, const RGB &c) {
    write_stdout("  " + ansi_fg(c) + "██████" + CAT_RESET + "  " +
                 label + "  " + hex6(c) + "\n");
}

} // anonymous namespace

int builtin_theme(const vector<string> &argv, ShellState &) {
    if (argv.size() < 2) {
        write_stderr(
            "theme: 用法：\n"
            "  theme list          列出可用主题\n"
            "  theme current       显示当前主题名称\n"
            "  theme set <name>    切换主题（会保存）\n"
            "  theme preview [<name>]  显示颜色样本\n");
        return 1;
    }
    const string &sub = argv[1];

    if (sub == "list") {
        auto names = list_available_themes();
        if (names.empty()) {
            write_stdout("（未找到主题）\n");
            return 0;
        }
        for (const auto &n : names) {
            bool active = (n == g_current_theme_name);
            write_stdout((active ? string("* ") : string("  ")) + n + "\n");
        }
        return 0;
    }

    if (sub == "current") {
        write_stdout(g_current_theme_name + "\n");
        return 0;
    }

    if (sub == "set") {
        if (argv.size() < 3) {
            write_stderr("theme: set 需要主题名称\n");
            return 1;
        }
        string err;
        if (!set_active_theme(argv[2], err)) {
            write_stderr("theme: " + err + "\n");
            return 1;
        }
        write_stdout("theme: 已切换到 " + argv[2] + "\n");
        return 0;
    }

    if (sub == "preview") {
        Theme t = g_current_theme;
        if (argv.size() >= 3) {
            string path = find_theme_file(argv[2]);
            if (path.empty()) {
                write_stderr("theme: 未找到：" + argv[2] + "\n");
                return 1;
            }
            t = Theme::load_from_file(path);
        }
        write_stdout("\n" + string(CAT_BOLD) + t.name + CAT_RESET +
                     " (" + t.variant + ")\n\n");
        write_stdout(string(CAT_BOLD) + "语法" + CAT_RESET + "\n");
        theme_print_swatch("command_valid  ", t.command_valid);
        theme_print_swatch("command_builtin", t.command_builtin);
        theme_print_swatch("command_invalid", t.command_invalid);
        theme_print_swatch("string         ", t.string_color);
        theme_print_swatch("variable       ", t.variable);
        theme_print_swatch("operator       ", t.op);
        theme_print_swatch("redirect       ", t.redirect);
        theme_print_swatch("comment        ", t.comment);
        write_stdout("\n" + string(CAT_BOLD) + "提示符" + CAT_RESET + "\n");
        theme_print_swatch("success        ", t.prompt_success);
        theme_print_swatch("error          ", t.prompt_error);
        theme_print_swatch("path           ", t.prompt_path);
        theme_print_swatch("git            ", t.prompt_git);
        theme_print_swatch("duration       ", t.prompt_duration);
        theme_print_swatch("user           ", t.prompt_user);
        theme_print_swatch("separator      ", t.prompt_separator);
        write_stdout("\n" + string(CAT_BOLD) + "补全" + CAT_RESET + "\n");
        theme_print_swatch("builtin        ", t.comp_builtin);
        theme_print_swatch("command        ", t.comp_command);
        theme_print_swatch("file           ", t.comp_file);
        theme_print_swatch("directory      ", t.comp_directory);
        theme_print_swatch("option         ", t.comp_option);
        theme_print_swatch("description    ", t.comp_description);
        write_stdout("\n");
        return 0;
    }

    write_stderr("theme: 未知子命令：" + sub + "\n");
    return 1;
}
