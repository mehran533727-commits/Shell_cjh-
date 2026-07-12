# 为 XTFSH 贡献代码

感谢你有兴趣参与 XTFSH！以下说明可帮助你快速开始。

## 开始参与

1. 派生仓库（Fork）并克隆到本地
2. 构建项目：

   ```sh
   cmake -B build && cmake --build build
   ```

3. 运行测试：

   ```sh
   cmake -B build -DBUILD_TESTS=ON && cmake --build build
   ctest --test-dir build --output-on-failure
   ```

4. 运行 Shell：

   ```sh
   ./build/XTFSH.out
   ```

## 提交修改

1. 从 `master` 创建分支：`git checkout -b my-feature`
2. 完成修改
3. 视情况补充测试
4. 确认所有测试均通过
5. 使用清晰的提交信息说明修改内容与原因
6. 推送分支并创建拉取请求（Pull Request）

## 可参与的方向

- 查看[开放议题](https://github.com/tavakkoliamirmohammad/XTFSH-shell/issues)，可优先寻找带有 `good first issue` 标签的议题
- 始终欢迎修复缺陷
- 新的内建命令、补全功能或 Shell 特性
- 文档改进
- 性能优化

## 代码风格

- 使用 C++17
- 使用 4 个空格缩进
- 保持函数简短且职责明确
- 遵循代码库已有的实现模式

## 报告缺陷

请创建议题，并提供：

- 你预期发生的情况
- 实际发生的情况
- 复现步骤
- 所用操作系统与系统架构

## 有疑问？

请创建[讨论](https://github.com/tavakkoliamirmohammad/XTFSH-shell/discussions)或议题，我们很乐意提供帮助。
