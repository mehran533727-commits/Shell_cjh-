# CLAUDE.md

本文件为在此仓库中编写代码时使用 Claude Code（claude.ai/code）提供指引。

## 构建、测试与运行

```sh
# Configure + build (tests ON by default)
cmake -B build && cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"

# Run the shell
./build/XTFSH.out

# Full test suite
ctest --test-dir build --output-on-failure -j "$(getconf _NPROCESSORS_ONLN)"

# Single test binary (gtest) — filter to one case
./build/test_tokenizer --gtest_filter='Tokenizer.HandlesQuotedStrings'
./build/test_integration --gtest_filter='integration/Basic.*'

# List everything ctest knows about (useful when hunting a prefixed name)
ctest --test-dir build -N | head
```

常用构建开关（均为缓存变量；通过 `-D` 传入）：

| 标志 | 作用 |
|---|---|
| `-DBUILD_TESTS=OFF` | 跳过全部测试树（发布构建） |
| `-DXTFSH_SANITIZERS=ON` | 启用 ASan + UBSan；应与 `-DCMAKE_BUILD_TYPE=RelWithDebInfo` 搭配 |
| `-DXTFSH_COVERAGE=ON` | 启用 `--coverage` 插桩（仅限 GCC/Clang） |
| `-DXTFSH_STATIC=ON` | 使用 `-static` 链接（实际上仅适用于 Linux/musl） |
| `-DXTFSH_ENABLE_FUZZER=ON` | 构建 `XTFSH_parser_fuzzer`（需要 clang；设置 `-DCMAKE_CXX_COMPILER=clang++`） |

模糊测试器：

```sh
cmake -B build-fuzz -DXTFSH_ENABLE_FUZZER=ON -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=OFF
cmake --build build-fuzz --target XTFSH_parser_fuzzer
./build-fuzz/XTFSH_parser_fuzzer fuzz/corpus -max_total_time=60
```

## 架构：修改前必须了解的内容

### 执行流水线（最重要的流程）

`src/repl.cpp` 驱动主循环。每一行输入会依次经过：

1. **Replxx**（高亮、提示与补全）——`src/ui/*.cpp`
2. **历史记录展开**（`!!`、`!n`）
3. **AI 拦截**——`@ai …` 或行尾 `?` 会被路由到 `src/ai/ai_handler.cpp`，而非普通执行器
4. **多行续行**——未闭合的引号、行尾 `|`/`&&` 或反斜杠续行
5. **解析运算符**——将 `&&`、`||`、`;` 解析为 `CommandSegment`，实现位于 `src/core/parser.cpp`
6. 对每个分段依次执行：变量展开 → 命令替换 → 重定向（含 heredoc）→ 分词 → 去除引号 → 别名展开 → glob → 自动 `cd` 检查 → 分派
7. **分派**——`src/core/executor.cpp` 中根据情况使用内建命令表、后台任务、管道，或经 `src/core/process.cpp` 的 `fork`/`exec` 执行
8. 退出码为 127 时显示 `"did you mean?"` 建议（基于 `PATH` 和内建命令的 Damerau-Levenshtein 距离）

中间表示位于 `include/XTFSH/shell.h`，包括 `CommandSegment`、`Command`、`PipelineSegment`、`Redirection`（内联 heredoc 字段），以及组合而成的 `ShellState { CoreState core; AiState ai; ExecutionState exec; }`。修改此头文件会触发全树重新编译；应优先向既有子结构添加字段，而非引入新的全局变量。

### ShellState（组合而非继承）

`ShellState` 是几乎所有调用点都会传递的唯一可变 Shell 上下文。最近的重构（参见提交 `bfd2a84`）已删除全部向后兼容层；**所有访问必须使用 `state.core.X` / `state.ai.X` / `state.exec.X`**。不要重新引入代理字段、包装函数或总括式访问器；修改 API 时须在同一拉取请求中迁移全部调用点。

重要字段：

- `exec.in_subshell`——在派生的子 Shell 中设定，使 `execute_command_line` 跳过历史记录；SQLite 不允许跨 `fork` 共享数据库，父进程会将整个子 Shell 命令作为一条记录写入。
- `exec.skip_execution`——安全钩子的“命令执行前”路径会将其设为真，从而跳过 `execve`。
- `exec.traps`——POSIX trap 表；信号编号 `0` 表示 `EXIT` 伪信号。

### 插件架构

`include/XTFSH/plugin.h` 定义了四个提供者接口及按优先级排序的 `PluginRegistry`：

- `ICompletionProvider`——fish（1,056 个命令）、fig（715 个命令）、manpage 的 `--help` 回退，以及内建命令
- `IPromptProvider`——starship 与默认的双行提示符
- `IHistoryProvider`——由 SQLite 支持，并提供 Atuin 桥接
- `IHookProvider`——生命周期钩子（`on_startup`、`on_exit`、`on_config_reload`）以及逐命令钩子（`on_before_command`、`on_after_command`）。全部五个方法特意设计为纯虚函数；实现者应使用 `{}` 显式选择不实现，而不是继承静默空操作。

注册表由 `src/startup.cpp` 中的 `register_default_plugins()` 填充。

### 添加插件（零接触注册表）

每个插件都在 `cmake/plugin_list.cmake` 中自行注册：**只需在文件底部追加一条 `XTFSH_register_plugin(...)` 调用**，无需其他操作。该函数在 `cmake/plugins.cmake` 中定义：它会追加 `SHELL_SOURCES`、记录测试规格，并在 `shell_lib` 存在后由 `XTFSH_finalize_plugin_tests()` 生成 `test_<NAME>` 二进制文件。此模式可避免各个拉取请求因修改中央源文件列表产生合并冲突。

`REQUIRES XTFSH_AI_ENABLED` / `REQUIRES XTFSH_SQLITE_ENABLED` 可按可选依赖为插件设置门控。`TEST_STANDALONE` 表示测试不链接 `shell_lib`；当测试自行编译部分源文件时应使用它。`TEST_AI_AWARE` 会在启用 AI 时增加 AI 宏定义与 libcurl 链接。

### CMake 布局

根目录的 `CMakeLists.txt` 很精简（约 100 行）；实际工作由 `cmake/` 承担：

- `features.cmake`——默认构建类型，并通过 `find_package()` 查找 libcurl（必需）、SQLite3（可选）和 nlohmann_json（可选；缺失时获取）。设置 `XTFSH_AI_ENABLED` / `XTFSH_SQLITE_ENABLED`。
- `fetch_deps.cmake`——为 replxx（始终）与 nlohmann_json 回退方案使用 `FetchContent`。
- `plugins.cmake` 与 `plugin_list.cmake`——上述插件注册表。
- `tests.cmake`——构建 `shell_lib`（与测试共用；使用与 `XTFSH.out` 相同的源文件，但不含 `src/main.cpp`），随后经 `XTFSH_finalize_plugin_tests()` 构建逐插件测试可执行文件，再构建单个 `test_integration`；它使用 `CONFIGURE_DEPENDS` 自动收集 `tests/integration/test_*.cpp`。
- `fuzzer.cmake`、`sanitizers.cmake`——可选开关。

`data/ai_models.json` 会通过 `cmake/ai_models_embedded.cpp.in` 在构建时嵌入二进制文件。环境变量 `XTFSH_AI_MODELS_JSON` 可在运行时覆盖它，适用于测试和运维。

### 测试

约有 779 项测试，分布在 24 个单元测试文件和 41 个集成测试文件中。集成测试使用 `tests/test_helpers.h` 中的 `run_shell(...)`：它将输入写入临时文件、通过管道传给 `$XTFSH_SHELL_BIN`（CTest 将其设为 `$<TARGET_FILE:XTFSH.out>`），并捕获标准输出和标准错误。存在 `count_occurrences()` 是因为 GNU readline 在 Linux 上从文件读取标准输入时会回显输入；检查“命令是否已执行”的测试必须区分回显与真实输出。

解析器同时拥有属性测试（`test_parser_properties.cpp`）和 libFuzzer 测试工具（`fuzz/parser_fuzz.cpp`）；后者只覆盖纯解析接口，即 `src/core/parser.cpp` 与 `src/util/io.cpp`。

## 约定：容易踩坑的事项

- **解析错误格式**：所有解析错误均输出 `XTFSH: error: L:C: <msg>`（参见提交 `55faeba`）。新增错误路径时必须严格匹配。
- **诊断输出**：子系统诊断请使用 `XTFSH::io::debug`（参见提交 `d3ea8c4`）。不要在信号、历史记录、后台任务、插件或配置代码中直接写入 `stderr`。
- **信号处理函数安全性**：`fg_child_pid` 是 `std::atomic<pid_t>`，并在头文件作用域通过 `static_assert` 断言为无锁类型；从信号处理函数读取的任何内容均须满足这一约束。
- **C++17、4 个空格缩进、`-Wall -Wextra`**。保持函数简洁且职责明确。
- **禁止向后兼容层**：修改 API 时，必须在同一拉取请求中迁移每个调用方。代码库明确拒绝代理字段、总括头文件和包装函数。
- **macOS 与 Linux**：`COLOR_FLAG` 不同（`-G` 与 `--color=auto`）。CI 覆盖 ubuntu-22.04/24.04、macos-14/15、Fedora、Alpine 与 manylinux_2_28。
- **manylinux 基线**：Linux 发布产物在 `quay.io/pypa/manylinux_2_28_x86_64` 中构建；libcurl 最低版本为 7.61+（AlmaLinux 8 / manylinux_2_28），不可依赖更新版本的 curl API。

## CI 任务线（`.github/workflows/build.yml`）

`test`（矩阵：ubuntu-22.04/24.04、macos-14/15）、`build-fedora`、`build-alpine`、`build-manylinux`（每个拉取请求都会对发布产物构建进行试运行）、`sanitizer`（ASan+UBSan）、`coverage`（lcov 下限为 10%，应逐步提高而非降低）和 `fuzz`（每个拉取请求运行 60 秒 libFuzzer）。在提议新的 CI 之前，先搜索这些任务线；常被请求的任务通常已经存在。

每个任务会使用带重试的 curl 预取 FetchContent 压缩包，以规避 codeload 的间歇性 504；`XTFSH_FC_ARGS` 会将 CMake 指向这些预取目录。

## 缺陷修复与功能拉取请求检查清单

1. **端到端连通性**：拉取请求的标题与说明是一项契约；在变基或合并前，差异中必须能验证每一项声明。
2. **优先采用成熟库**，而不是手写复杂功能。
3. **不设向后兼容垫层**：变更 API 时，应在同一拉取请求中迁移全部调用方。
4. **每项行为变更均须有测试或复现步骤**。集成测试应放在 `tests/integration/test_*.cpp`，系统会自动发现。
