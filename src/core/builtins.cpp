// Builtin registry: when adding builtins from new files, register them
// here. See src/builtins/*.cpp for implementations grouped by theme:
//
//   nav.cpp     — cd, pwd, pushd/popd/dirs, z
//   env.cpp     — export, unset, alias, unalias
//   bg.cpp      — bglist, bgkill, bgstop, bgstart, fg
//   history.cpp — history
//   ui.cpp      — clear, copy, paste, linkify, block, table
//   meta.cpp    — exit, which/type, source/., explain, help
//   config.cpp  — config, session, theme
//   trap.cpp    — trap
//
// Single source of truth is get_builtins_info() — the name→fn map
// returned by get_builtins() is derived from it at first call, so
// adding a builtin in exactly one place (the table below) is enough
// to register it and make it discoverable via `help`.

#include "XTFSH/builtins.h"
#include "XTFSH/core/builtins.h"
using namespace std;

const vector<BuiltinInfo>& get_builtins_info() {
    static const vector<BuiltinInfo> table = {
        // nav.cpp
        {"cd",       builtin_cd,       "cd [dir|-]",                  "切换当前工作目录"},
        {"pwd",      builtin_pwd,      "pwd",                         "显示当前工作目录"},
        {"pushd",    builtin_pushd,    "pushd <dir>",                 "将当前目录压入栈并切换到 <dir>"},
        {"popd",     builtin_popd,     "popd",                        "弹出目录栈顶并切换到该目录"},
        {"dirs",     builtin_dirs,     "dirs",                        "显示目录栈"},
        {"z",        builtin_z,        "z <pattern>",                 "跳转到匹配模式的高频目录"},

        // env.cpp
        {"export",   builtin_export,   "export [VAR=value]",          "设置环境变量，或列出全部变量"},
        {"unset",    builtin_unset,    "unset VAR",                   "删除环境变量"},
        {"alias",    builtin_alias,    "alias [name[='value']]",      "定义、显示或列出命令别名"},
        {"unalias",  builtin_unalias,  "unalias name",                "删除命令别名"},

        // bg.cpp
        {"bglist",   builtin_bglist,   "bglist",                      "列出后台任务"},
        {"jobs",     builtin_bglist,   "jobs",                        "列出后台任务"},
        {"bgkill",   builtin_bgkill,   "bgkill <n>",                  "向后台任务 #n 发送 SIGTERM"},
        {"bgstop",   builtin_bgstop,   "bgstop <n>",                  "向后台任务 #n 发送 SIGSTOP"},
        {"bgstart",  builtin_bgstart,  "bgstart <n>",                 "向后台任务 #n 发送 SIGCONT"},
        {"fg",       builtin_fg,       "fg [n]",                      "将后台任务切换到前台"},

        // history.cpp
        {"history",  builtin_history,  "history",                     "显示命令历史"},

        // ui.cpp
        {"clear",    builtin_clear,    "clear",                       "清空终端屏幕"},
        {"copy",     builtin_copy,     "copy [text...]",              "将参数或标准输入复制到剪贴板"},
        {"paste",    builtin_paste,    "paste",                       "将剪贴板内容输出到标准输出"},
        {"linkify",  builtin_linkify,  "linkify [text...]",           "将文本或标准输入中的 URL 转为 OSC-8 超链接"},
        {"block",    builtin_block,    "block <command> [args...]",   "以标题和页脚块包裹运行命令"},
        {"table",    builtin_table,    "table [--max-width N]",       "将列式标准输入渲染为整洁表格"},

        // meta.cpp
        {"exit",     builtin_exit,     "exit [code]",                 "退出 Shell（会执行 EXIT trap）"},
        {"which",    builtin_which,    "which <name>",                "通过别名、内建命令或 $PATH 查找命令"},
        {"type",     builtin_which,    "type <name>",                 "`which` 的别名：识别命令名称"},
        {"source",   builtin_source,   "source <file>",               "在当前 Shell 中读取并执行文件中的命令"},
        {".",        builtin_source,   ". <file>",                    "`source` 的 POSIX 同义命令"},
        {"explain",  builtin_explain,  "explain <cmd> [args...]",     "在行内说明命令及其选项"},
        {"help",     builtin_help,     "help [name]",                 "列出内建命令，或显示指定命令的用法"},

        // config.cpp
        {"config",   builtin_config,   "config <subcommand>",         "查看或修改 XTFSH 配置（同步）"},
        {"session",  builtin_session,  "session <list|save|load|rm>", "保存或恢复 Shell 会话状态"},
        {"theme",    builtin_theme,    "theme <list|current|set|preview>", "列出、切换或预览颜色主题"},

        // trap.cpp
        {"trap",     builtin_trap,     "trap [cmd] [signals...]",     "注册收到信号时执行的命令"},
    };
    return table;
}

const unordered_map<string, BuiltinFn>& get_builtins() {
    static const unordered_map<string, BuiltinFn> builtins = []() {
        unordered_map<string, BuiltinFn> m;
        for (const auto &b : get_builtins_info()) {
            m.emplace(b.name, b.fn);
        }
        return m;
    }();
    return builtins;
}

bool is_builtin(const string &name) {
    return get_builtins().count(name) > 0;
}
