# XTFSH 课程设计现场演示与答辩手册

> 项目：XTFSH Shell 命令解释器
>
> 课程：《操作系统实践》
>
> 最终核对日期：2026-07-12
> 当前验证口径：CTest 共注册 916 项，914 项通过、2 项因环境条件跳过、0 项失败。

## 一、演示目标与时间安排

现场演示的主线不是“展示很多命令”，而是说明 XTFSH 如何把一行文本转换为进程关系和文件描述符连接关系。建议控制在 10～12 分钟。

| 阶段 | 内容 | 建议时间 |
|---|---|---:|
| 1 | 项目定位、环境和版本 | 1 分钟 |
| 2 | 内建命令、外部命令和目录状态 | 2 分钟 |
| 3 | 别名、历史和 Tab 补全 | 1.5 分钟 |
| 4 | 重定向和多级管道 | 2 分钟 |
| 5 | 后台任务与信号 | 1.5 分钟 |
| 6 | 脚本模式、系统注册与 Shell 切换 | 1.5 分钟 |
| 7 | 自动化测试和可选 AI 功能 | 1.5 分钟 |

开场建议直接说：

> 老师好，我们实现的是一个运行在用户态的 Linux 命令解释器 XTFSH。它支持课程要求的命令执行、内建命令、历史、别名、补全、后台任务、管道和 I/O 重定向。核心执行路径使用 fork、execvp、waitpid、pipe 和 dup2，并通过 SIGCHLD 回收后台子进程。下面按“解析—执行—进程管理—测试”的顺序演示。

## 二、演示前准备

### 2.1 终端与目录

- 使用 WSL2 Ubuntu 22.04 终端，建议窗口最大化。
- 字体调到老师能看清的大小，确保 UTF-8 和 ANSI 颜色正常。
- 演示时区分 Bash 提示符与 XTFSH 提示符，不要把 Bash 命令和 XTFSH 内命令混在一起。
- AI 为可选增强功能；核心演示不依赖网络。

在 Bash 中执行：

```bash
cd /home/xue/Shell_cjh-updated
mkdir -p /tmp/xtfsh_demo
git branch --show-current
git rev-parse --short HEAD
cmake --build build -j "$(nproc)"
./build/XTFSH.out --version
```

预期看到：

```text
XTFSH 2.2.0
features: +ai +sqlite-history +fish-completion +fig-completion +manpage-completion ...
```

讲解话术：

> 版本命令同时打印编译进二进制的能力，可以确认 AI、SQLite 历史、补全、主题、trap、heredoc 和 subshell 等模块是否启用。课程要求的核心 Shell 功能不依赖 AI。

### 2.2 系统注册（演示前完成，现场只检查）

如果机器尚未注册 XTFSH，可提前在 Bash 中执行：

```bash
sudo install -m755 build/XTFSH.out /usr/local/bin/xtfsh
grep -qxF /usr/local/bin/xtfsh /etc/shells \
  || echo /usr/local/bin/xtfsh | sudo tee -a /etc/shells
```

现场只检查，避免等待 sudo：

```bash
command -v xtfsh
grep -n '/usr/local/bin/xtfsh' /etc/shells
```

预期：两条命令都能找到 `/usr/local/bin/xtfsh`。

## 三、完整现场演示流程

### 3.1 启动 XTFSH

在 Bash 中执行：

```bash
cd /home/xue/Shell_cjh-updated
./build/XTFSH.out
```

讲解话术：

> main.cpp 初始化主题、插件、ShellState 和信号处理器，然后进入 repl.cpp 的交互循环。提示符中的目录、Git 分支、后台任务数和上一次退出状态都来自 Shell 的运行状态。

### 3.2 内建命令、外部命令和目录状态

以下命令在 XTFSH 中执行：

```bash
help
help cd
pwd
type cd
type ls
which ls
cd /tmp/xtfsh_demo
pwd
cd -
cd /tmp/xtfsh_demo
echo hello-XTFSH
```

预期现象：

- 第一次 `pwd` 显示项目目录，`cd` 后的 `pwd` 显示 `/tmp/xtfsh_demo`。
- `help` 列出内建命令，`help cd` 显示 `usage: cd [dir|-]`。
- `cd -` 返回上一个目录，然后再次进入演示目录。
- `type cd` 表明 `cd` 是 Shell 内建命令。
- `type ls` 或 `which ls` 显示通过 PATH 找到的外部程序路径。
- `echo` 正常输出文本；本项目中的 `echo` 通常由系统外部程序执行。

讲解话术：

> cd 必须在父 Shell 进程中执行，否则只改变子进程目录，返回后父 Shell 的目录不会变化。ls 等外部命令则由 fork 创建子进程，再由 execvp 根据 PATH 装入程序。

### 3.3 退出状态与条件执行

```bash
false
echo $?
true && echo and-ok
false || echo or-ok
not_a_real_command
echo $?
```

预期现象：

- `false` 后 `$?` 为 1。
- `true && ...` 执行右侧，`false || ...` 执行右侧。
- 不存在的命令会报错，退出状态通常为 127。

讲解话术：

> XTFSH 把每条命令的退出状态保存在 ShellState 中。`&&` 在上一条为 0 时继续，`||` 在上一条非 0 时继续；127 是 Unix Shell 常用的“命令未找到”状态。

### 3.4 别名和历史记录

```bash
alias ll='ls -l'
ll
echo history-demo
!!
history
unalias ll
```

预期现象：

- `ll` 展开并执行 `ls -l`。
- `!!` 再次执行上一条 `echo history-demo`。
- 正式演示可把 `history` 改成 `history -n 8`，只显示最近八条记录。

讲解话术：

> 别名在执行前作用于命令首词；历史既用于交互回看，也可以由 SQLite provider 持久化。当前历史事件展开重点支持整行 `!!` 和 `!n`。

### 3.5 Tab 补全（必须在真实终端手动操作）

不要复制粘贴下面的动作，应逐键输入：

1. 输入 `ech`。
2. 按一次 `Tab`，应补成或提示 `echo`。
3. 输入空格和 `completion-ok`，回车执行。
4. 输入 `cd /tm`，按 `Tab`，应补全 `/tmp/`。

讲解话术：

> 补全由 replxx 的回调触发，候选来源包括内建命令、PATH、文件和目录、环境变量，以及 fish、fig、manpage 插件。普通管道测试可以验证补全算法，但只有真实 TTY 才能展示菜单和按键交互。

### 3.6 I/O 重定向

```bash
echo first-line > demo.txt
echo second-line >> demo.txt
cat < demo.txt
ls /path-not-exist 2> error.txt
cat error.txt
```

预期现象：

- `demo.txt` 依次包含 `first-line` 和 `second-line`。
- 不存在路径的错误没有直接打印到终端，而是写入 `error.txt`。

讲解话术：

> 0、1、2 分别是标准输入、标准输出和标准错误。Shell 在子进程 exec 前 open 文件，再用 dup2 替换相应标准描述符。`>` 使用截断方式，`>>` 使用追加方式，`2>` 修改标准错误。

可选追演示：

```bash
ls /path-not-exist > all-output.txt 2>&1
cat all-output.txt
```

> `2>&1` 的含义不是“把 2 写入 1”，而是让 fd 2 复制 fd 1 当前指向的打开文件，因此重定向顺序有意义。

### 3.7 多级管道

```bash
printf "apple\nbanana\napricot\n" | grep ap | wc -l
cat demo.txt | grep second
```

预期：第一条输出 `2`，第二条输出 `second-line`。

讲解话术：

> 三段命令需要两条内核管道。每个子进程用 dup2 把上一条管道读端接到标准输入，把下一条管道写端接到标准输出。父子进程都必须关闭不再使用的端点，否则读端可能永远等不到 EOF。

### 3.8 环境变量、子 Shell 与通配符（加分项）

```bash
export COURSE=OS
echo $COURSE
pwd
(cd /; pwd)
pwd
echo *.txt
```

预期：

- 子 Shell 内 `cd /` 的结果不会改变父 Shell 当前目录。
- `*.txt` 展开为当前目录下匹配的文件。

讲解话术：

> fork 后子进程的内存状态与父进程隔离，所以子 Shell 中的目录变化不会泄漏回父进程。glob 只对未被引号保护的通配符参数展开。

### 3.9 后台任务与信号

建议每次正式演示都新启动一个 XTFSH，以便第一个 job ID 为 1：

```bash
sleep 30 &
bglist
echo foreground-still-works
bgstop 1
bglist
bgstart 1
bgkill 1
sleep 1
bglist
```

如果输出的 job ID 不是 1，以 `bglist` 显示的实际编号替换。

预期现象：

- `sleep 30 &` 立即返回提示符，并打印 job ID 与 PID。
- 前台仍可执行 `echo`。
- `bgstop` 后任务显示 stopped，`bgstart` 恢复，`bgkill` 终止。
- 稍后 `bglist` 中任务被回收。

讲解话术：

> 后台命令创建子进程后父 Shell 不阻塞，而是记录 job ID、PID 和命令。子进程状态变化时内核发送 SIGCHLD，信号处理器只设置标志，主循环再用 waitpid 的 WNOHANG 模式循环回收，避免长期僵尸进程。

边界说明：

> 当前是 PID 驱动的课程级后台任务管理，不是完整 Bash/POSIX 作业控制。项目没有 setpgid 和 tcsetpgrp，因此不宣称支持完整进程组、Ctrl-Z 和 `%1` 语义。

### 3.10 脚本模式

先退出 XTFSH：

```bash
exit
```

回到 Bash 后执行：

```bash
cd /home/xue/Shell_cjh-updated
./build/XTFSH.out report_artifacts/demo_script.XTFSH
cat /tmp/XTFSH_script_report.txt
```

预期看到：

```text
=== script mode demo ===
<当前项目目录>
script-mode-ok
```

讲解话术：

> `XTFSH 文件名` 会启动新的 XTFSH 进程，逐行调用同一套解析和执行逻辑。`source 文件` 则在当前 ShellState 中执行，所以 source 中的 cd、export 和 alias 可以保留。

### 3.11 Shell 注册与切换

在 Bash 中检查：

```bash
command -v xtfsh
grep -n '/usr/local/bin/xtfsh' /etc/shells
xtfsh
```

进入 XTFSH 后演示嵌套 Bash：

```bash
bash
echo $0
exit
```

如果现场不想进入嵌套交互，可用更稳妥的替代命令：

```bash
bash -lc 'echo switched-to-bash-ok; echo $0'
sh -c 'echo switched-to-sh-ok; echo $0'
```

讲解话术：

> 安装到 `/usr/local/bin`、登记到 `/etc/shells` 和执行 `chsh` 是三件不同的事。在 XTFSH 中输入 bash 只是启动嵌套子 Shell，退出后仍返回 XTFSH。当前未实现 `-c` 参数，因此不建议把 XTFSH 设成默认登录 Shell，演示使用显式终端入口即可。

### 3.12 自动化测试

退出 XTFSH，回到 Bash：

```bash
cd /home/xue/Shell_cjh-updated
ctest --test-dir build-test --output-on-failure -j 4
```

最终核对结果：

```text
100% tests passed, 0 tests failed out of 916
914 passed, 2 skipped, 0 failed
```

两项跳过的准确解释：

- `IoConsumerIntegration.BgLifecycleEmitsSpawnAndReap`：该单元测试目标未设置真实二进制路径，因而条件跳过。
- `LiveProviders.SqliteHistoryPersistsCommands`：数据库已经创建，但环境没有 `sqlite3` 命令行工具读取内容，因而条件跳过。

建议话术：

> 跳过不是失败。解析器、内建命令、管道、重定向、后台任务、SIGCHLD 压力测试、历史、脚本和 AI 配置路径均有自动化覆盖。真实 TTY 的视觉补全和完整作业控制仍需手动验证，这也是我们主动说明的测试边界。

时间不足时只跑核心集成测试：

```bash
ctest --test-dir build-test \
  -R 'integration/(Basic|Cd|Redirect|Pipes|Background)' \
  -j "$(nproc)" --output-on-failure
```

Tab 菜单不便投屏时，用补全测试作为补充证据：

```bash
./build-test/test_tokenizer \
  --gtest_filter='CompletionCallback.PartialCommandMatchesPrefix:CompletionCallback.FileCompletionWithPrefix'
```

### 3.13 AI 辅助（可选加分，不作为主流程）

网络或 provider 未确认时，只演示本地状态和帮助：

```bash
./build/XTFSH.out
@ai
@ai status
```

网络已确认时可以追加：

```bash
@ai 查看隐藏文件用什么命令
```

讲解话术：

> AI 是交互增强模块，不参与基本命令解释器的正确性。它读取问题和必要上下文，返回命令建议或解释；网络不可用时，fork、管道、重定向和作业管理不受影响。

### 3.14 时间充足时的加分功能

以下功能不属于主演示必选项，每项只选一个，避免演示过长。

heredoc：

```bash
cat <<EOF
line-one
line-two
EOF
```

> Shell 先收集正文，再把正文作为命令的标准输入。

富表格输出：

```bash
printf 'NAME SCORE\nAlice 95\nBob 88\n' | table
```

离线命令解释：

```bash
explain tar -xzf archive.tar.gz
```

主题查看（只预览，不修改持久配置）：

```bash
theme list
theme current
theme preview dracula
```

命令纠错：

```bash
ech hello
```

> 预期在命令未找到后提示类似 `did you mean 'echo'?`。

退出 trap：

```bash
trap 'echo exit-trap-fired' EXIT
exit
```

启动性能分解需要回到 Bash：

```bash
./build/XTFSH.out --benchmark
```

## 四、5 分钟压缩版演示

时间不足时，只演示以下内容：

```bash
cd /home/xue/Shell_cjh-updated
./build/XTFSH.out --version
./build/XTFSH.out
```

进入 XTFSH：

```bash
pwd
type cd
type ls
cd /tmp/xtfsh_demo
alias ll='ls -l'
ll
echo first > a.txt
echo second >> a.txt
cat a.txt | grep second
sleep 30 &
bglist
bgkill 1
exit
```

回到 Bash：

```bash
./build/XTFSH.out report_artifacts/demo_script.XTFSH
ctest --test-dir build-test -N | tail -n 1
```

压缩版结尾话术：

> 以上演示覆盖了有状态内建命令、外部程序、重定向、管道、后台任务和脚本模式；完整测试集共注册 916 项，最终复核为 914 通过、2 条件跳过、0 失败。

## 五、常见现场故障与备用方案

| 现象 | 原因 | 立即处理 |
|---|---|---|
| `xtfsh: command not found` | 尚未安装到 PATH | 使用项目内的 `./build/XTFSH.out` |
| 首次启动询问 AI 配置 | 新 HOME 下尚未配置 | 输入 `n`，继续核心演示 |
| Tab 无补全界面 | 命令来自管道/非 TTY | 在真实终端直接启动 XTFSH，手动输入并按 Tab |
| `bgkill 1` 提示编号无效 | 当前 job ID 不是 1 或任务已结束 | 先执行 `bglist`，按实际编号操作；使用 `sleep 30 &` |
| 背景任务很快消失 | `sleep` 时间太短 | 改用 `sleep 30 &` |
| 进入 bash 后分不清层级 | 提示符相似 | 执行 `echo $0`，再用 `exit` 退回上一层 |
| AI 响应慢或失败 | 网络/provider 不稳定 | 说明 AI 为可选增强，跳过并继续核心功能 |
| 测试显示 2 skipped | 环境条件不足 | 使用上文两条准确解释，不说“测试失败” |
| 颜色或图标异常 | 终端字体/ANSI 支持不同 | 不影响命令执行；切换普通 UTF-8 字体或缩小主题展示 |
| 临时文件影响结果 | 上次演示遗留 | 重新创建 `/tmp/xtfsh_demo` 或更换文件名 |

## 六、操作系统理论答辩话术

答题建议采用四句结构：先下定义，再说 XTFSH 如何实现，再给演示证据，最后主动说明边界。

### 6.1 Shell 属于内核吗？

**建议回答：**

> Shell 不是内核，而是用户态程序。它读取和解析命令，再通过 fork、execvp、waitpid、pipe、dup2 等系统调用请求内核完成进程创建、程序装入、等待和通信。XTFSH 的作用是把用户文本翻译成进程关系和文件描述符连接关系。

**源码依据：** `src/repl.cpp`、`src/core/executor.cpp`、`src/core/process.cpp`。

### 6.2 输入 `ls` 后发生了什么？

**建议回答：**

> 解析器先得到 argv。如果不是内建命令，父进程 fork；子进程恢复默认信号状态、设置重定向，然后 execvp。execvp 根据 PATH 查找程序并替换子进程映像；父进程 waitpid 等待并获取退出状态，所以 Shell 自己不会被 ls 替换。

**追问：** execvp 成功后不会返回；失败时项目输出 errno 信息并以 127 退出。

### 6.3 为什么不能直接在 Shell 主进程里 exec？

> exec 会替换当前进程映像。如果 Shell 直接 exec，提示符、历史、别名和后台作业表都会消失。fork 保留父 Shell，让子进程专门运行外部程序。

### 6.4 fork 是否真的立刻复制全部内存？

> 语义上父子进程地址空间独立；现代 Linux 通常用写时复制，只有某一方写入页面时才真正复制，因此 fork 后立即 exec 的开销比完整复制小。

### 6.5 内建命令和外部命令有什么区别？

> 内建命令是 XTFSH 进程中的函数；外部命令是独立程序。cd、export、alias、exit 必须在父 Shell 中执行，否则状态只在子进程内改变。ls、cat、grep 通常由 execvp 启动。

可现场用 `type cd` 和 `type ls` 证明。

### 6.6 管道中的内建命令会改变父 Shell 吗？

> 不会。管道每一段需要独立执行单元，内建命令在管道子进程中运行，因此 `cd /tmp | cat` 不会改变父 Shell 的当前目录。这符合常见 Unix Shell 的管道隔离语义。

### 6.7 waitpid 的作用是什么？

> 前台命令需要等待退出并读取状态；后台命令则不能阻塞。waitpid 可以指定 PID 和选项，XTFSH 对前台阻塞等待，对后台使用 WNOHANG 非阻塞回收。

### 6.8 什么是僵尸进程？与孤儿进程有何区别？

> 僵尸进程已经退出，但父进程尚未 wait，内核仍保留退出状态；孤儿进程是父进程先退出、子进程仍在运行。XTFSH 用 SIGCHLD 通知和 waitpid 回收后台子进程，避免长期僵尸。

### 6.9 为什么 SIGCHLD 处理器只设置标志？

> 信号处理器处于异步上下文，大多数 C++ 容器、字符串和输出函数都不安全。XTFSH 在处理器中只修改简单标志，主循环到安全点后再 waitpid 和修改作业表。

**追问：** 普通信号可能合并，因此一次收到 SIGCHLD 后要循环 waitpid，不能只回收一个子进程。

### 6.10 Ctrl-C 如何处理？

> 父 Shell 记录前台 PID。存在前台子进程时，SIGINT 处理器向它转发 SIGINT；没有前台任务时只让 Shell 返回新提示符。子进程 exec 前恢复默认信号处置，避免继承 Shell 专用处理器。

### 6.11 文件描述符是什么？

> 文件描述符是每个进程描述符表中的整数索引。0、1、2 约定为标准输入、标准输出和标准错误；它们可以指向终端、普通文件或管道。

### 6.12 dup2 为什么能实现重定向？

> Shell 先 open 目标文件，再用 dup2 把目标描述符复制到 0、1 或 2。程序仍然读写标准描述符，但底层对象已经变成文件或管道。

### 6.13 `>`、`>>` 和 `2>&1` 的区别是什么？

> `>` 通常使用 O_TRUNC 覆盖文件，`>>` 使用 O_APPEND 追加。`2>&1` 让标准错误复制标准输出当前指向的对象，所以 `cmd >a 2>&1` 与 `cmd 2>&1 >a` 的结果可能不同。

### 6.14 N 个命令需要几条管道？

> N 个命令需要 N-1 条管道。第 i 个进程从前一条管道读、向后一条管道写；首段没有管道输入，末段没有管道输出。

### 6.15 为什么必须关闭无关管道端点？

> 管道读端只有在所有写端副本关闭后才会看到 EOF。如果父进程或无关子进程仍持有写端，grep、cat 等消费者可能一直等待；描述符泄漏也会耗尽进程资源。

### 6.16 前台与后台执行的本质差异是什么？

> 都会创建子进程。前台命令使父 Shell waitpid 等待；后台命令登记 job 后立即返回提示符，并在稍后通过 SIGCHLD 回收。

### 6.17 XTFSH 是否实现完整 POSIX 作业控制？

> 没有。当前实现的是 PID 级后台任务管理，包括 jobs/bglist、bgstop、bgstart、bgkill 和 fg。完整作业控制还需要 setpgid、tcsetpgrp、进程组信号和 Ctrl-Z 语义，本项目没有这些调用，因此不夸大为完整 Bash 作业控制。

### 6.18 解析器为什么不能简单按空格切分？

> 因为引号内的空格和操作符不是分隔符，还要识别单双引号、转义、变量、命令替换、管道、重定向、`&&`、`||`、`;`、`&` 和括号深度。XTFSH 使用状态机处理这些上下文。

### 6.19 变量、别名、glob 大致按什么顺序处理？

> 先进行变量和命令替换，再解析 argv 与重定向；随后对命令首词展开别名，对未引用参数执行 glob，最后分发内建或外部命令。当前实现是实用 Shell 子集，不宣称完全复制 Bash 的所有展开顺序。

### 6.20 单引号和双引号有什么区别？

> 单引号通常完全抑制变量展开；双引号保留字符串整体，但允许 `$VAR` 和命令替换。未引用的通配符才参与 glob。

### 6.21 history 如何持久化？

> 交互层使用 replxx 历史环；启用 SQLite provider 时记录命令、时间、目录和退出状态，否则有文本回退。`!!` 和 `!n` 主要读取交互历史进行整行展开。

### 6.22 脚本模式与 source 有何区别？

> `XTFSH 文件` 启动一个新 Shell 进程执行脚本，状态随进程结束消失；`source 文件` 在当前 ShellState 中执行，所以 cd、export、alias 等修改可以保留。

### 6.23 `/etc/shells`、PATH 和 chsh 分别是什么？

> PATH 决定命令能否按名称找到；`/etc/shells` 是系统认可登录 Shell 的列表；`chsh` 才真正修改账户默认登录 Shell。把 xtfsh 写入 `/etc/shells` 不等于已经设为默认 Shell。

### 6.24 为什么当前不建议用 chsh？

> 当前 main.cpp 只支持无参交互模式或一个脚本路径，没有实现常见的 `-c command`。WSL、VS Code Remote 和脚本工具经常依赖 `shell -c`，所以使用 Windows Terminal 显式入口更稳妥。

### 6.25 如何解释测试中的两条 skipped？

> 第一条缺少测试目标所需的二进制路径变量；第二条缺少用于读取数据库的 sqlite3 CLI。二者是环境前置条件不足，不是断言失败。最终口径是 916 注册、914 通过、2 条件跳过、0 失败。

### 6.26 自动测试能证明完全没有问题吗？

> 不能。单元和集成测试提高可信度，但真实 TTY 的按键、终端进程组、时序竞争和所有输入组合无法被有限测试完全覆盖。因此报告明确区分自动测试、手工终端验证和已知工程边界。

### 6.27 为什么用 C++17？

> 底层仍可直接调用 POSIX 系统接口，同时可以用 string、vector、RAII 和模块化结构管理解析结果、描述符和状态，比纯 C 更方便组织较大项目。

### 6.28 项目还有哪些明确不足？

建议诚实回答：

> 课程要求的核心闭环已经完成，但还不是完整 Bash。后续重点是进程组与控制终端、`-c` 参数、脚本最终退出码、完整 POSIX 语法、pipefail、PTY 自动化测试，以及 Shell 退出时对后台作业的统一处理。

### 6.29 程序和进程有什么区别？

> 程序是磁盘上的静态指令和数据，进程是程序运行后的动态实例，具有 PID、地址空间、文件描述符和调度状态。同一个程序可以同时对应多个进程。

### 6.30 为什么子进程 exec 失败时使用 `_exit`？

> fork 后子进程复制了父进程的用户态缓冲和运行库状态。exec 失败时使用 `_exit(127)` 可以直接结束子进程，避免再次刷新继承的缓冲区或执行父进程资源的析构逻辑。

### 6.31 项目如何减少文件描述符泄漏？

> 管道描述符使用 CLOEXEC，父进程和每个子进程都主动关闭不再使用的管道端点，内建重定向使用 RAII 描述符对象恢复和关闭临时 fd。heredoc 临时文件创建后立即 unlink，并限制权限。

### 6.32 管道的退出状态如何确定？

> 当前返回最后一个管道阶段的状态，这与常见 Shell 默认行为相近，但没有实现 Bash 的 `pipefail`。因此前段失败、末段成功时，整条管道仍可能显示成功，这是已知边界。

### 6.33 脚本执行结束时如何返回状态？

> 脚本内部每条命令都会更新 `$?`，但当前脚本文件成功读到结尾后整体返回 0，没有完整传播最后一条命令的失败状态。后续可把最后一次执行状态作为脚本进程退出码。

## 七、源码讲解索引

| 要讲的内容 | 主要文件 |
|---|---|
| 程序入口、版本和模式选择 | `src/main.cpp` |
| 交互循环、历史展开、AI 入口 | `src/repl.cpp` |
| 分词、变量、命令段、重定向 | `src/core/parser.cpp` |
| 内建分发、条件执行、管道入口 | `src/core/executor.cpp` |
| fork/exec/wait、管道、后台回收 | `src/core/process.cpp` |
| SIGINT、SIGCHLD、trap | `src/core/signals.cpp` |
| 内建命令注册表 | `src/core/builtins.cpp` |
| cd、目录栈 | `src/builtins/nav.cpp` |
| export、alias | `src/builtins/env.cpp` |
| jobs、fg、信号控制 | `src/builtins/bg.cpp` |
| history | `src/builtins/history.cpp` |
| 核心状态结构 | `include/XTFSH/shell.h` |
| 单元/集成测试注册 | `cmake/tests.cmake`、`tests/` |

## 八、分工被问到时的回答

- 薛腾飞：总体需求、Shell 与操作系统接口、进程执行、系统注册和切换验证。
- 陈俊宏：命令解析、内建命令、历史、别名与补全整理。
- 姚宇骁：管道、I/O 重定向、后台任务、脚本模式与测试整理。

统一话术：

> 三名成员都有共同联调、截图和报告审阅，只是在主负责模块上有所侧重。答辩时每个人应能说明完整的“读取—解析—分发—fork/exec—wait/reap”主流程。

## 九、最终上台检查清单

- [ ] `./build/XTFSH.out --version` 正常。
- [ ] `/tmp/xtfsh_demo` 已创建。
- [ ] `report_artifacts/demo_script.XTFSH` 存在。
- [ ] `command -v xtfsh` 和 `/etc/shells` 检查正常；若异常就使用项目内二进制。
- [ ] Tab 补全已在同一终端手动试过。
- [ ] `sleep 30 &`、`bglist`、`bgkill` 已试过一次。
- [ ] 测试口径统一为 916 注册、914 通过、2 跳过、0 失败。
- [ ] AI 若要展示，已提前确认网络；否则只展示 `@ai` 帮助或直接跳过。
- [ ] 清楚说明当前不是完整 POSIX 作业控制，也不支持 `-c`。
- [ ] 演示结束前退出所有嵌套 Shell，避免停留在 bash 子层。

## 十、一句话答辩主线

> XTFSH 是一个用户态命令解释器：解析器把文本转换为命令结构，父 Shell 直接执行有状态内建命令，外部程序通过 fork 和 execvp 启动，重定向与管道通过文件描述符和 dup2 连接，前台任务由 waitpid 等待，后台任务由 SIGCHLD 通知并非阻塞回收；最终由手工演示和 916 项 CTest 用例共同验证。
