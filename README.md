# Shell_cjh

## 🚀 本组快速开始（组员必读）

拉下来后**一条脚本**即可得到与作者一致的环境（自动安装依赖、解压离线依赖、配置 DeepSeek、编译）：

```bash
git clone https://github.com/mehran533727-commits/Shell_cjh-
cd Shell_cjh-
bash cjh-setup.sh        # 装系统依赖 + 解压离线依赖 + 配置 DeepSeek + 离线编译
./build/XTFSH.out         # 运行；进入后输入   @ai 查看隐藏文件用什么命令   可试 AI
```

- 运行环境：Linux / WSL（建议 Ubuntu 22.04+）
- 课设文档（缺陷清单 / 功能说明 / 修复策略）见 [`junhong-docs/`](junhong-docs/)
- 依赖已内置于 [`vendor/`](vendor/)，无需联网 GitHub 即可编译

## 🖥️ 把 XTFSH 当系统 shell 用（注册 + 终端入口 + 随意切换）

上面 `./build/XTFSH.out` 只是"从 bash 里启动一个程序"。想让它像 bash / PowerShell 那样成为**系统认可、可直接选择**的 shell，按下面做（编译完成后执行一次即可）：

### 1. 注册到系统（Linux / WSL 通用）

```bash
sudo install -m755 build/XTFSH.out /usr/local/bin/xtfsh              # 装到稳定路径
grep -qxF /usr/local/bin/xtfsh /etc/shells \
  || echo /usr/local/bin/xtfsh | sudo tee -a /etc/shells            # 登记为合法登录 shell
```

做完后 `cat /etc/shells` 里就有它、和 bash 并列；任何 bash 里直接敲 `xtfsh` 即可进入。

### 2. 随意切换 shell（不用 exit）

- 在 **bash** 里敲 `xtfsh` → 进入 XTFSH；
- 在 **XTFSH** 里敲 `bash` / `sh` / `zsh` / `fish` / `dash` → 进入对应 shell（都是**正常交互 shell**：有提示符、加载 `~/.bashrc`）。
- 只要敲名字就切过去，**无需 `exit`**。它们是**嵌套**关系，想往上退一层时再用 `exit`。

### 3.（WSL 用户）在 Windows Terminal 里加一个 XTFSH 入口

让 Windows Terminal 顶部下拉里出现 “XTFSH”，与 PowerShell / CMD / Ubuntu 并排，点一下即进：

**图形方式**：Windows Terminal → 设置 → 左下角“新建配置文件” → 填：

- 名称：`XTFSH`
- 命令行：`wsl.exe -d Ubuntu --cd ~ -- /usr/local/bin/xtfsh`
- 图标（可选）：`🐱`

**或**直接编辑 `settings.json`（路径 `%LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json`），在 `profiles.list` 数组里加一段（`guid` 换成你自己生成的；命令行里的 `Ubuntu` 换成你的发行版名，用 `wsl -l -q` 查）：

```json
{
  "name": "XTFSH",
  "commandline": "wsl.exe -d Ubuntu --cd ~ -- /usr/local/bin/xtfsh",
  "icon": "🐱",
  "guid": "{在此填一个新生成的 GUID}"
}
```

> ⚠️ 不建议用 `chsh` 把 XTFSH 设成默认登录 shell——它目前不认 `-c`，会破坏 `wsl <命令>`、VS Code Remote-WSL、git hook 等非交互调用。想“打开就进 XTFSH”，用上面的终端入口来**选择**更安全（也可在 Windows Terminal 设置里把默认配置文件设为 XTFSH）。

---
Shell_cjh 是一个使用 C++17 实现的 Linux Shell 项目，面向操作系统课程设计整理。项目用于演示 Shell 的核心机制，包括命令解析、进程创建、前台/后台执行、输入输出重定向、管道、历史记录、别名、自动补全和信号处理等。

本项目基于一个现代 Unix Shell 代码库改造，推荐在 Ubuntu 或其他 Linux 发行版中运行。课程演示时可以重点说明 Shell 的基本流程：读取用户命令、解析命令结构、分发内建命令，或通过 Linux 进程相关 API 启动外部程序。

## 功能特性

- 基于 `fork`、`execvp`、`waitpid` 实现命令解释与执行
- 支持 `cd`、`pwd`、`exit`、`history`、`alias`、`unalias`、`type`、`which` 等内建命令
- 支持通过 `PATH` 查找并执行外部命令，例如 `ls`、`cat`、`grep`、`echo`
- 支持内建命令、PATH 命令、变量、文件和目录的 Tab 自动补全
- 支持命令历史记录和历史展开
- 支持别名定义与展开
- 支持后台任务管理，例如 `bg`、`fg`、`bglist`
- 支持 Bash 风格的 `&` 后台执行，例如 `sleep 3 &`
- 支持输入输出重定向：`<`、`>`、`>>`、`2>`、`2>&1`
- 支持管道执行：`|`
- 支持脚本模式：`XTFSH <script-file>`

## 环境要求

推荐环境：

- Ubuntu 22.04 或更高版本
- CMake 3.17 或更高版本
- 支持 C++17 的编译器，例如 `g++`
- Make 或 Ninja
- libcurl 开发头文件
- SQLite 开发头文件，推荐安装，用于增强历史记录功能
- nlohmann-json 开发包，可选；缺失时 CMake 也可以自动获取

在 Ubuntu 中安装常用依赖：

```bash
sudo apt update
sudo apt install -y cmake g++ make git \
  libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev
```

## 编译

配置并编译项目：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
```

生成的 Shell 可执行文件位于：

```bash
./build/XTFSH.out
```

如果需要同时编译测试：

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
```

## 运行

启动交互式 Shell：

```bash
./build/XTFSH.out
```

如果首次启动时出现 AI 功能配置提示：

```text
XTFSH ai - AI features available! Set up now? [y/n]
```

课程实验演示时可以直接输入 `n` 并回车，跳过 AI 配置。

常用示例命令：

```bash
pwd
ls
cd /tmp
echo hello > hello.txt
cat hello.txt
cat hello.txt | grep hello
alias ll='ls -l'
ll
history
sleep 3 &
bglist
bg sleep 5
type ls
exit
```

运行脚本文件：

```bash
./build/XTFSH.out demo.XTFSH
```

## 实验功能验证

进入项目目录并启动 Shell：

```bash
cd Shell_cjh-
./build/XTFSH.out
```

进入 `XTFSH` 后可以按下面顺序验证：

```bash
pwd
cd /tmp
pwd
echo hello
echo hello > /tmp/XTFSH_demo.txt
cat /tmp/XTFSH_demo.txt
cat /tmp/XTFSH_demo.txt | grep hello
sleep 10 &
bglist
sleep 5 & echo done
exit
```

预期现象：

- `pwd` 可以显示当前工作目录
- `cd /tmp` 后再次执行 `pwd` 应显示 `/tmp`
- `echo hello` 可以正常输出文本
- `echo hello > /tmp/XTFSH_demo.txt` 可以把输出写入文件
- `cat /tmp/XTFSH_demo.txt | grep hello` 可以通过管道筛选出 `hello`
- `sleep 10 &` 会立即返回提示符，不会阻塞 Shell
- `bglist` 可以查看仍在运行的后台任务
- `sleep 5 & echo done` 会立即输出 `done`，说明 `sleep 5` 已在后台运行

## 测试

编译并运行完整测试集：

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
ctest --test-dir build --output-on-failure
```

运行单个 CTest 用例，例如第 360 个 AI error hook 限流测试：

```bash
cd build
ctest -R "AiErrorHookTest.RateLimiterBlocks" --output-on-failure
```

也可以直接运行对应 GoogleTest 二进制：

```bash
cd build
./test_ai_error_hook --gtest_filter=AiErrorHookTest.RateLimiterBlocks
```

## 可选安装

安装到用户本地目录：

```bash
mkdir -p "$HOME/.local/bin"
install -m 755 build/XTFSH.out "$HOME/.local/bin/XTFSH"
```

确认 `~/.local/bin` 已加入 `PATH`：

```bash
export PATH="$HOME/.local/bin:$PATH"
```

安装到 Ubuntu 系统目录：

```bash
sudo install -m 755 build/XTFSH.out /usr/local/bin/XTFSH
```

注册为可用登录 Shell：

```bash
command -v XTFSH
echo /usr/local/bin/XTFSH | sudo tee -a /etc/shells
chsh -s /usr/local/bin/XTFSH
```

切回 Bash：

```bash
chsh -s /bin/bash
```

## 目录结构

```text
.
|-- CMakeLists.txt          # CMake 项目入口
|-- README.md               # 项目说明文档
|-- AGENTS.md               # 本项目 Git/GitHub 协作规则
|-- install.sh              # 上游安装辅助脚本
|-- XTFSH.1.in               # man 手册模板
|-- cmake/                  # CMake 功能、插件、测试和 sanitizer 模块
|-- data/                   # 运行时数据，例如主题和模型元数据
|-- docs/                   # 图片和文档资源
|-- include/XTFSH/           # 项目公共头文件
|-- src/                    # Shell 主要实现
|   |-- core/               # 解析器、执行器、进程、信号和会话逻辑
|   |-- builtins/           # Shell 内建命令
|   |-- ui/                 # 提示符、补全、高亮和 UI 辅助逻辑
|   |-- history/            # 历史记录和 frecency 支持
|   |-- plugins/            # 补全、历史、提示符和 hook provider
|   |-- util/               # 通用工具代码
|   `-- ai/                 # 上游项目保留的可选 AI 支持
|-- tests/                  # 单元测试和集成测试
|-- fuzz/                   # 解析器 fuzz 测试语料和入口
`-- Formula/                # 上游项目保留的 Homebrew formula
```

## 课程阅读重点

操作系统课程设计中，建议重点阅读以下文件：

- `src/repl.cpp`：交互式读取、执行循环和历史记录处理
- `src/core/parser.cpp`：命令解析、重定向解析和管道拆分
- `src/core/executor.cpp`：内建命令分发和命令执行流程
- `src/core/process.cpp`：`fork`、`execvp`、`waitpid`、`pipe`、`dup2` 的使用
- `src/builtins/`：各类内建命令实现
- `src/ui/completion.cpp`：命令和文件自动补全

## 许可证

本项目派生自使用 MIT License 的 XTF'SH Shell 项目。重新分发或提交派生作品时，请保留原始许可证和来源说明。
