#include "XTFSH/ui/inline_docs.h"

#include <algorithm>

// ── Built-in flag database ───────────────────────────────────
//
// Maps command name -> (flag -> description).
// Covers the most common POSIX/GNU utilities and git subcommands.

static const std::unordered_map<std::string,
    std::unordered_map<std::string, std::string>> flag_db = {
    {"tar", {
        {"-x", "解压归档文件"},
        {"-z", "通过 gzip 过滤"},
        {"-f", "使用归档文件（下一个参数）"},
        {"-c", "创建新归档文件"},
        {"-v", "详细显示已处理的文件"},
        {"-t", "列出归档内容"},
        {"--extract", "解压归档文件"},
        {"--gzip", "通过 gzip 过滤"},
        {"--file", "使用归档文件"},
        {"--create", "创建新归档文件"},
        {"--verbose", "显示详细输出"},
        {"--list", "列出内容"},
    }},
    {"grep", {
        {"-i", "忽略大小写"},
        {"-r", "递归搜索"},
        {"-n", "显示行号"},
        {"-v", "反向匹配（显示未匹配项）"},
        {"-c", "统计匹配行数"},
        {"-l", "仅列出文件名"},
        {"-E", "使用扩展正则表达式"},
        {"--color", "为匹配项着色"},
        {"--include", "仅搜索匹配的文件"},
        {"--exclude", "跳过匹配的文件"},
    }},
    {"find", {
        {"-name", "匹配文件名模式"},
        {"-type", "匹配文件类型（f=文件，d=目录）"},
        {"-size", "匹配文件大小"},
        {"-mtime", "匹配修改时间（天）"},
        {"-exec", "对结果执行命令"},
        {"-delete", "删除匹配的文件"},
        {"-maxdepth", "限制目录深度"},
    }},
    {"chmod", {
        {"-R", "递归应用"},
        {"--recursive", "递归应用"},
    }},
    {"ls", {
        {"-l", "长格式列表"},
        {"-a", "显示隐藏文件"},
        {"-h", "以易读单位显示大小"},
        {"-R", "递归列出"},
        {"-t", "按修改时间排序"},
        {"-S", "按文件大小排序"},
    }},
    {"git", {
        {"add", "暂存修改以便提交"},
        {"commit", "将修改记录到仓库"},
        {"push", "将本地提交上传到远程仓库"},
        {"pull", "下载并整合远程修改"},
        {"status", "显示工作树状态"},
        {"diff", "显示提交之间的差异"},
        {"log", "显示提交历史"},
        {"branch", "列出、创建或删除分支"},
        {"checkout", "切换分支或还原文件"},
        {"merge", "合并两段开发历史"},
        {"rebase", "将提交重新应用到另一基线之上"},
        {"stash", "暂存工作目录中的未提交修改"},
        {"clone", "克隆仓库"},
        {"fetch", "从远程下载对象"},
        {"reset", "将当前 HEAD 重置到指定状态"},
    }},
};

// ── Built-in command hints ───────────────────────────────────
//
// One-line descriptions for common commands.

static const std::unordered_map<std::string, std::string> cmd_hints = {
    {"tar", "创建或解压压缩归档"},
    {"grep", "在文件中搜索模式"},
    {"find", "在目录层级中搜索文件"},
    {"chmod", "修改文件权限"},
    {"chown", "修改文件所有者和组"},
    {"ls", "列出目录内容"},
    {"cp", "复制文件和目录"},
    {"mv", "移动或重命名文件"},
    {"rm", "删除文件或目录"},
    {"mkdir", "创建目录"},
    {"cat", "连接并显示文件内容"},
    {"head", "显示文件开头的若干行"},
    {"tail", "显示文件结尾的若干行"},
    {"sed", "用于文本转换的流编辑器"},
    {"awk", "模式扫描与处理工具"},
    {"curl", "通过 URL 传输数据"},
    {"wget", "从网络下载文件"},
    {"ssh", "安全 Shell 远程登录"},
    {"scp", "通过 SSH 安全复制"},
    {"rsync", "快速增量文件传输"},
    {"docker", "容器平台"},
    {"git", "分布式版本控制系统"},
    {"make", "构建自动化工具"},
    {"cmake", "跨平台构建系统生成器"},
    {"python", "Python 解释器"},
    {"python3", "Python 3 解释器"},
    {"node", "Node.js JavaScript 运行时"},
    {"npm", "Node.js 包管理器"},
    {"pip", "Python 包安装器"},
    {"cargo", "Rust 包管理与构建工具"},
    {"go", "Go 编程语言工具"},
};

// ── Database access ──────────────────────────────────────────

const std::unordered_map<std::string,
    std::unordered_map<std::string, std::string>> &get_flag_db() {
    return flag_db;
}

const std::unordered_map<std::string, std::string> &get_cmd_hints() {
    return cmd_hints;
}

// ── Helper: try to split combined short flags ────────────────
//
// e.g., "-la" -> {"-l", "-a"}.  Only splits when every character
// after the leading '-' maps to a known single-char flag for the
// given command.  Returns a vector with the original flag if
// splitting is not possible.

static std::vector<std::string> try_split_combined_flags(
    const std::string &arg,
    const std::unordered_map<std::string, std::string> &flags)
{
    // Must start with '-' but not '--', and have 2+ chars after '-'
    if (arg.size() < 3 || arg[0] != '-' || arg[1] == '-') {
        return {arg};
    }

    // Check that every character after '-' is a known single-char flag
    std::vector<std::string> parts;
    for (size_t i = 1; i < arg.size(); ++i) {
        std::string single_flag = std::string("-") + arg[i];
        if (flags.find(single_flag) == flags.end()) {
            // Cannot split -- return original
            return {arg};
        }
        parts.push_back(single_flag);
    }
    return parts;
}

// ── explain_command ──────────────────────────────────────────

std::vector<FlagExplanation> explain_command(
    const std::string &command,
    const std::vector<std::string> &args)
{
    if (args.empty()) {
        return {};
    }

    auto cmd_it = flag_db.find(command);
    if (cmd_it == flag_db.end()) {
        return {};
    }

    const auto &flags = cmd_it->second;
    std::vector<FlagExplanation> result;

    for (const auto &arg : args) {
        // Skip bare positional arguments (not starting with '-' and
        // not a known subcommand like git's)
        auto direct_it = flags.find(arg);
        if (direct_it != flags.end()) {
            FlagExplanation fe;
            fe.flag = arg;
            fe.description = direct_it->second;
            result.push_back(fe);
            continue;
        }

        // Handle long options with = (e.g., --color=auto)
        if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
            std::string::size_type eq_pos = arg.find('=');
            if (eq_pos != std::string::npos) {
                std::string base_flag = arg.substr(0, eq_pos);
                auto base_it = flags.find(base_flag);
                if (base_it != flags.end()) {
                    FlagExplanation fe;
                    fe.flag = arg;
                    fe.description = base_it->second;
                    result.push_back(fe);
                    continue;
                }
            }
        }

        // Try splitting combined short flags (e.g., -xzf)
        if (arg.size() >= 3 && arg[0] == '-' && arg[1] != '-') {
            auto parts = try_split_combined_flags(arg, flags);
            if (parts.size() > 1) {
                for (const auto &p : parts) {
                    FlagExplanation fe;
                    fe.flag = p;
                    fe.description = flags.at(p);
                    result.push_back(fe);
                }
                continue;
            }
        }

        // If arg starts with '-', it is an unrecognized flag
        if (!arg.empty() && arg[0] == '-') {
            FlagExplanation fe;
            fe.flag = arg;
            fe.description = "";
            result.push_back(fe);
        }
        // else: plain positional argument, skip silently
    }

    return result;
}

// ── get_command_hint ─────────────────────────────────────────

std::string get_command_hint(const std::string &command) {
    auto it = cmd_hints.find(command);
    if (it != cmd_hints.end()) {
        return it->second;
    }
    return "";
}
