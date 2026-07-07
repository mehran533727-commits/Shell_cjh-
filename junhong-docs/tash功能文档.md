# tash（Shell_cjh）功能文档

> 项目：Shell_cjh-（本名 tash，Tavakkoli's Shell），C++17 Linux shell，v2.2.0
> 定位：操作系统课程设计——命令解释器（shell）
> 运行环境：Linux（本项目在 WSL2 Ubuntu 上构建运行）

---

## 一、系统概述

tash 是一个用 C++17 编写的现代 Unix shell（命令解释器）。它读取用户输入的一行命令，经过解析、展开、分发，最终通过 Linux 进程 API（`fork` / `execvp` / `waitpid`）执行外部程序或内建命令。除完整覆盖课程要求的 shell 基础功能外，还内置了 AI 自然语言助手、语法高亮、Tab 补全、主题、SQLite 历史等增强特性。

**一条命令的处理流程**（`src/repl.cpp` 主循环）：
```
读入一行
 → 历史展开 (!!、!n)
 → AI 拦截 (@ai 前缀，或 ? 结尾的自然语言问句)
 → 多行续行 / heredoc 收集
 → 解析操作符 (&&、||、;) 与管道 (|)  ── src/core/parser.cpp
 → 变量/命令替换、重定向、别名、glob 展开
 → 分发：内建命令 / 后台 / 管道 / fork+exec  ── src/core/executor.cpp、process.cpp
```

---

## 二、构建与运行

### 依赖
- CMake ≥ 3.17、g++（C++17）、make
- libcurl 开发头（AI 功能）、SQLite3 开发头（历史，可选）、nlohmann-json（可选，缺失时自动获取）
- 行编辑库 replxx（构建时通过 FetchContent 获取）

### 构建（离线方式，依赖包已置于 `vendor/`）
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
  -DFETCHCONTENT_SOURCE_DIR_REPLXX="$HOME/Shell_cjh-/vendor/replxx-release-0.0.4"
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
```
产物：`build/tash.out`（约 2.2 MB）

### 运行
```bash
./build/tash.out              # 交互模式（显示 TASH banner）
./build/tash.out demo.tash    # 脚本模式
./build/tash.out --version    # 版本与已编入特性
```

### 运行测试（902 个 GoogleTest 用例）
```bash
cmake -B build-test -DBUILD_TESTS=ON <上述 FetchContent 参数，另加 googletest 源目录>
cmake --build build-test -j N
ctest --test-dir build-test -j N
```

---

## 三、功能全景

### A. 课程要求的功能

| 功能 | 用法示例 | 说明 |
|---|---|---|
| 命令解释执行 | `ls -l /etc`、`cat 文件` | 外部命令经 `fork`+`execvp`+`waitpid` 执行；不存在命令返回 127 |
| 目录切换 cd | `cd /tmp`、`cd`（回 home）、`cd -`（回上一个） | 内建 |
| 当前目录 pwd | `pwd` | 内建 |
| 命令类型 type | `type ls`（→ 路径）、`type cd`（→ builtin） | 内建（=`which`） |
| 历史 history | `history`、`!!`、`!n` | 持久化（SQLite + 文件），跨会话累计 |
| 别名 alias | `alias ll='ls -l'`、`unalias ll` | 内建 |
| 命令补全 | 输入前缀按 `Tab` | 补 builtin/PATH/文件/变量，另带 fish(1056)/fig(715)/manpage 补全库 |
| 后台运行 | `bg sleep 10`、`bglist` | ⚠️ 语法是 `bg 命令`，**不支持 `命令 &`** |
| 管道 | `ls /etc \| grep conf \| wc -l` | 支持多级 |
| I/O 重定向 | `> >> < 2> 2>&1` | 如 `cmd > f`、`cmd >> f`、`cmd < f`、`cmd 2> err`、`cmd > f 2>&1` |

> 关于 ls/cat/grep/echo：这些是系统外部命令，任何 shell 都通过 `fork`+`exec` 调用它们，非 shell 自身实现。tash 的 `echo` 也走外部 `/usr/bin/echo`。

### B. 超纲增强功能

**Shell 语法**：逻辑连接 `&&` `||` `;` · 变量 `$VAR` · 命令替换 `$(...)` · glob `*` · heredoc `<<EOF` · 子 shell `( )` · auto-cd（直接输目录名进入）· 命令纠错 did-you-mean（交互模式）

**扩展内建命令**（共 33 个，`help` 可列全）：
- 目录栈：`pushd` `popd` `dirs` · 智能跳转：`z <关键词>`
- 环境变量：`export` `unset`
- 作业控制：`bglist` `bgkill` `bgstop` `bgstart` `fg`
- 会话/信号：`session`（保存/恢复）`trap`（信号陷阱）`source`/`.`（执行脚本）
- 信息：`explain`（AI 解释命令）`help` `config`
- 输出工具：`clear` `copy` `paste` `linkify`（URL→超链接）`block`（输出加框）`table`（渲染表格）`theme`（主题切换）

**交互体验**：语法高亮 · fish 风格灰色历史提示 · 快捷键（→ 接受提示 / Alt+→ 接受一词 / Alt+. 上条末参 / Ctrl-L 清屏）· 两行/starship prompt · TASH banner

**AI 功能**：见第四节

**历史与存储**：SQLite 跨会话历史 · frecency（频率+最近度）排序 · Atuin 桥接

**插件架构**：4 类 provider（补全 / prompt / 历史 / hook）· safety hook（危险命令拦截）

**主题**：`theme list` 可选 catppuccin-latte/mocha、dracula、nord、tokyo-night

---

## 四、AI 自然语言助手（已接入 DeepSeek）

### 支持的 provider
| provider | 说明 |
|---|---|
| Gemini | 原生默认 |
| OpenAI | 原生；**本项目已改造为指向 DeepSeek**（OpenAI 兼容协议） |
| Ollama | 本地模型 |

### 当前配置（DeepSeek）
- 端点：`https://api.deepseek.com/v1/chat/completions`
- 模型：`deepseek-chat`（非思考模式，快、省 token；旧别名，2026-07-24 后需改用 `deepseek-v4-flash`）
- 配置文件：`~/.config/tash/ai_provider`（=openai）、`~/.config/tash/openai_key`（DeepSeek key，0600）

### 用法
```bash
@ai 查看隐藏文件用什么命令        # → ls -la
@ai 统计当前目录文件数量的命令     # → ls -1 | wc -l
@ai 用一句话解释 pwd 命令          # → 文字解释
```
- `@ai <问题>`：**最可靠的触发方式**，始终生效。
- `<问题>?`（? 结尾）：也能触发，但当前对**中文连写、无空格**的句子不生效（见缺陷文档 D2），建议中文用户优先用 `@ai` 前缀。
- 返回类型：`command`（给命令）/ `steps`（多步）/ `script`（脚本）/ `answer`（解释）。

---

## 五、架构概览

| 目录/文件 | 职责 |
|---|---|
| `src/repl.cpp` | 交互主循环、replxx 配置、历史展开 |
| `src/core/parser.cpp` | 命令解析、操作符/管道切分、重定向解析 |
| `src/core/executor.cpp` | 分发：内建 / 后台 / 管道 / 外部命令 |
| `src/core/process.cpp` | `fork`/`execvp`/`waitpid`/`pipe`/`dup2`、后台 SIGCHLD 回收 |
| `src/builtins/` | 内建命令实现（按主题分文件） |
| `src/ui/` | prompt、补全、高亮、富输出 |
| `src/history/` | 历史与 frecency |
| `src/ai/` | AI provider、请求构造、上下文 |
| `src/plugins/` | 补全/prompt/历史/hook 插件 |
| `include/tash/shell.h` | 核心数据结构：`ShellState`、`CommandSegment`、`Redirection` 等 |

---

## 六、测试覆盖

- **黑盒功能测试**：两轮共约 50 组，覆盖课程要求全部功能 + 主要超纲功能，非交互管道验证。
- **项目自带测试**：902 个 GoogleTest 用例（24 单元 + 41 集成文件），覆盖 tokenizer、parser、重定向、管道、子 shell、信号、trap、session、Unicode 路径、补全、AI 等。
- **当前状态**：900 通过、2 失败（2 个失败为 AI structured 请求格式改造后的断言过时，详见缺陷文档 D3）。
- **未由自动化覆盖的**：真实终端的交互观感（banner、配色、Tab 补全弹窗、灰色提示），需在终端 `./build/tash.out` 亲验。

---

*本文档描述系统客观功能与用法；已知问题与修复策略见《tash系统测试缺陷文档.md》。*
