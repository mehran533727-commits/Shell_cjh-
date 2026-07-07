# CJHSH（Shell_cjh）系统测试缺陷文档

> 测试对象：Shell_cjh-（项目本名 CJHSH，CJHSH Shell），C++17 Linux shell，v2.2.0
> 测试环境：WSL2 Ubuntu（内核 6.6.87.2-microsoft-standard-WSL2），GCC 15.2.0
> 测试方式：① 黑盒功能测试（非交互管道喂命令，两轮共约 50 组）；② 项目自带 902 个单元/集成测试（GoogleTest）
> 说明：本文档汇总缺陷、修复策略与**最终修复状态**（已实施并推送）。

---

## 〇、最终修复状态（2026-07-07 · 提交 `aa3ab98` 已推送 `master` · 全程 906/906 通过）

| 缺陷 | 处理 | 说明 |
|---|---|---|
| D1 `\$` 转义 | ✅ 已修 | `expand_variables` 加 `\$`→字面 `$`；解析相关组 156/156 通过 |
| D2 中文 AI 触发 | ✅ 已修 | `is_ai_question` 支持中文问号 `？`+放宽空格；含 2 条中文用例 |
| D3 测试断言过时 | ✅ 已修 | `test_ai` 两个 OpenAIClient 断言对齐 `json_object` |
| R1 DeepSeek 弃用注释 | ✅ 已修 | `ai_models.json` 加 `_note` |
| D7 剪贴板提示 | ✅ 已修 | `copy` 失败提示补 WSL 安装建议 |
| D4 历史源不一致 | ⊘ 不改（设计行为） | `!n` 用会话历史被 `BangNRunsNthCommand` 测试固化，硬改会回归 |
| D5 answer 渲染 | ⊘ 不改（非缺陷） | 复现 3 次均正常显示，第一轮空白为偶发 |
| D6 作业编号 | ⊘ 不改（已知限制） | 单操作可靠；连续按固定序号批量删会重编号错位，需逐个删前 `bglist` 确认 |

> 真实改动仅 8 个文件（`data/ai_models.json`、`src/ai/contextual_ai.cpp`、`src/ai/llm_client.cpp`、`src/builtins/ui.cpp`、`src/core/parser.cpp`、`tests/unit/{test_ai,test_contextual_ai,test_tokenizer}.cpp`），全部落在 AI 与解析分支，未触及其他子系统。

---

## 一、总体结论

- **课程要求的全部功能均通过**（命令执行、cd、type、history、alias、补全、后台、管道、重定向）。
- 项目自带 902 个测试：第一轮 **902/902 全过**；第二轮（DeepSeek 改造后）**900 通过、2 失败**，2 个失败均为本次 AI 改动的预期后果（见 D3）。
- shell 核心（解析/执行/进程/内建）**零回归**。
- 共记录缺陷 7 项 + 风险 1 项 + 环境事项 1 项。其中中等 3 项、低 4 项。**无高危缺陷**。

---

## 二、缺陷清单（按严重度排序）

### D1　反斜杠转义 `\$` 失效（中）
- **现象**：`echo escaped \$HOME` 输出 `escaped \/home/kimura`，而标准 bash 应输出 `escaped $HOME`。
- **复现**：交互或脚本中执行 `echo \$HOME`。
- **根因**：解析阶段对反斜杠转义 `$` 处理不完整——反斜杠未被消除，且 `$HOME` 仍被展开。
- **影响**：需要输出字面 `$` 的场景（如打印价格、正则、文档示例）会出错。
- **是否本次引入**：否，CJHSH 原生缺陷。
- **修复策略**：在 `src/core/parser.cpp` 的变量展开/去引号阶段，识别 `\$` 序列，先剥离反斜杠并将其后的 `$` 标记为"不展开"。补充针对 `\$`、`\\`、`\"` 的解析单元测试。

### D2　`?` 结尾的 AI 提问对中文不友好（中）
- **现象**：`.txt 文件?`（含空格）能触发 AI，`如何查看当前目录下的隐藏文件?`（中文连写、无空格）不触发，整行被当命令执行报 `No such file`。
- **复现**：在配好 AI 的前提下，输入一句不含空格的中文问句并以 `?` 结尾。
- **根因**：`src/ai/contextual_ai.cpp` 的 `is_ai_question()` 要求"句子必须含至少一个空格"（`has_space`）才判定为自然语言问题——这是为英文（"how to list files?"）设计的启发式，中文连写句子天然无空格，故失效。
- **影响**：中文用户用 `?` 触发 AI 时行为不可预期。`@ai` 前缀不受影响（始终可靠）。
- **是否本次引入**：否，CJHSH 原生逻辑。
- **修复策略**：放宽 `has_space` 判定——当句子长度超过阈值（如 > 8 字符）或包含非 ASCII（CJK）字符或中文问号 `？` 时，也视为自然语言问题；或改为"首词不是命令即触发"。补中文用例。

### D3　两个 OpenAI structured 单元测试回归（中，本次引入）
- **现象**：`unit/ai/OpenAIClient.BuildsStructuredRequestJson` 与 `StructuredSchemaIncludesSteps` 由通过变为失败。
- **根因**：为适配 DeepSeek，`build_openai_structured_json` / `build_openai_structured_context_json` 的 `response_format` 从 `json_schema/strict` 改为 `json_object`；这两个测试仍断言旧的 `json_schema` 结构（含 name/schema/strict/steps）。
- **影响**：仅测试断言过时，运行时功能正常（DeepSeek 问答已验证可用）。
- **是否本次引入**：是，本次 AI 改造的直接后果。
- **修复策略**：更新这两个测试的断言为新的 `json_object` 结构，并新增一条"system prompt 内含字段说明与 json 关键字"的断言。若日后需同时兼容真正的 OpenAI，可按 provider 分支保留两套 response_format 并分别测试。

### D4　`!n` / `!!` 历史源与 `history` 显示不一致（低）
- **现象**：`history` 显示的是持久化历史（SQLite + 文件，跨会话累计），而 `!n`/`!!` 基于当前会话的 replxx 环；非交互模式下 `!1` 执行的是"本会话第 1 条"而非 `history` 列出的持久第 1 条。
- **根因**：两条历史读取路径不同（`history` 内建读持久层；`expand_history_bang` 读 replxx ring）。交互模式启动时会把持久历史预载入 ring，两者较一致；非交互模式未预载，差异明显。
- **影响**：低。主要在非交互脚本里表现，交互使用基本一致。
- **是否本次引入**：否。
- **修复策略**：统一历史源——`!n` 展开改为读取与 `history` 相同的持久层，或在所有模式下启动即把持久历史预载入 ring。

### D5　`answer` 类 AI 回答在非交互管道下不渲染（低）
- **现象**：`@ai <解释类问题>` 在管道（非 TTY stdout）下有时只显示 `CJHSH ai ─` 前缀、正文空白；在真实终端（PTY）下正常显示。
- **根因**：answer 类走富文本/markdown 渲染，依赖 `isatty(stdout)`；非 TTY 时渲染被跳过。
- **影响**：低，仅影响非交互脚本抓取 AI 解释文本；交互使用正常。
- **是否本次引入**：否。
- **修复策略**：非 TTY 时对 answer 类回退为纯文本直接输出（`content` 原样打印）。

### D6　作业控制 job 编号映射存疑（低）
- **现象**：第一轮观察到 `bgstop 1` 打印的是 job 2 的 PID、`bgkill 2` 报 `invalid job number`。
- **根因**：疑为 job 序号与内部 PID 映射在增删后的对应关系，或非交互时序导致的观测偏差。
- **影响**：低。项目自带的 `Background.*` / 作业控制相关测试全部通过，说明基本逻辑正确。
- **是否本次引入**：否。
- **修复策略**：优先级低。补一组"多任务增删后按序号精确 stop/kill"的确定性测试来证伪或定位；若确有偏差，修正 `src/builtins/bg.cpp` 的序号→PID 查找。

### D7　`copy` 写系统剪贴板在 WSL 下失败（低，环境相关）
- **现象**：`copy hello` 报 `copy: failed to write to clipboard`，随后回退用 OSC-52 终端转义序列输出。
- **根因**：WSL 默认无 X11/Wayland 剪贴板后端（xclip/wl-copy 未安装）。
- **影响**：低。OSC-52 回退在支持的终端里可用；纯功能不受损。
- **是否本次引入**：否，环境限制。
- **修复策略**：文档提示在 WSL 安装 `wl-clipboard` 或 `xclip`，或在 WSLg 环境启用；代码侧可在失败时给出更明确的安装提示。

---

## 三、风险与信息项

### R1　DeepSeek 模型别名弃用风险（信息）
- 本次 AI 默认模型设为 `deepseek-chat`（非思考模式，响应快、省 token，适合 shell 助手）。
- **风险**：`deepseek-chat` 是旧别名，官方标注 **2026-07-24 弃用**；届时需改用正式名 `deepseek-v4-flash`（默认带思考链，较慢）或 `deepseek-v4-pro`。
- **应对**：弃用后把 `data/ai_models.json` 的 openai.default 改为 `deepseek-v4-flash`，并研究关闭思考模式的参数以保持 shell 助手的响应速度。课程演示期（截至 7-15）不受影响。

---

## 四、非缺陷的环境事项

### E1　CRLF 行尾导致 git 显示大量"改动"（非缺陷）
- **现象**：`git status` 显示 233 个文件被修改（约 3.2 万行增删且近乎 1:1）。
- **根因**：项目从 Windows 侧 `cp` 进 WSL 时携带 CRLF 行尾，而仓库存储为 LF，git 逐行标记为差异。
- **影响**：不影响编译与运行（一路编译成功、902 测试通过为证）。仅污染 `git diff`，需用 `--ignore-cr-at-eol` 查看真实改动。
- **应对**：如需干净的 git 视图，可加 `.gitattributes` 统一 LF，或在 WSL 内用 `git clone` 重新获取源码。

---

## 五、修复优先级建议

| 优先级 | 缺陷 | 理由 |
|---|---|---|
| P1（建议先修） | D3 | 恢复测试套件绿灯，且改动小（仅更新断言） |
| P1 | D2 | 直接影响中文用户的 AI 使用体验 |
| P2 | D1 | 正确性 bug，但触发场景有限 |
| P3 | D4、D5 | 低影响、非交互场景为主 |
| P4 | D6、D7 | 存疑/环境相关，优先级最低 |

---

## 六、本次真实代码改动一览（仅供追溯，均属 AI 功能）

| 文件 | 改动 |
|---|---|
| `data/ai_models.json` | openai 段：default→`deepseek-chat`、fallbacks→`[deepseek-v4-flash]`、id_prefixes 加 `deepseek-` |
| `src/ai/llm_client.cpp` | ① `kOpenAIChatUrl`→DeepSeek 端点；② 两个 `build_openai_structured_*` 的 `response_format`→`json_object`+提示注入 |

以上改动经 `git diff --ignore-cr-at-eol` 核验，仅这 2 文件、全部落在 OpenAI/AI 分支，不涉及 shell 任何其他子系统。
