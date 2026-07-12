# 为 Atom Engine 贡献代码  

[English](../CONTRIBUTING.md) | [中文](CONTRIBUTING-CN.md)

---
首先，感谢您考虑为 Atom Engine 贡献代码！我们欢迎各种形式的贡献 —— Bug 报告、功能建议、文档改进和代码变更。

---

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发环境配置](#开发环境配置)
- [编码规范](#编码规范)
- [Pull Request 流程](#pull-request-流程)
- [提交信息规范](#提交信息规范)
- [报告问题](#报告问题)
- [许可证选择说明](#许可证选择说明)

---

## 行为准则

本项目致力于为所有人提供友好、包容的环境。参与本项目即表示您同意：

- 使用友善和包容的语言
- 尊重不同的观点和经验
- 优雅地接受建设性批评
- 以社区最佳利益为重

---

## 如何贡献

### 报告 Bug

1. 先检查 [issue 追踪器](https://github.com/RomiBrooks/Atom/issues) 避免重复
2. 提供清晰、描述性的标题
3. 包含以下详细信息：
   - 操作系统和编译器版本
   - CMake 配置参数
   - 复现步骤
   - 预期行为与实际行为
   - 相关日志或错误输出

### 建议功能

1. 描述该功能及其解决的问题
2. 说明它如何融入引擎架构
3. 如有可能，提供 API 或使用示例

### 文档改进

文档改进始终受欢迎 —— 拼写错误、表述不清、缺失翻译或新指南均可。

---

## 编码规范

所有代码**必须**符合项目的编码规范。要点如下：

- **后置返回类型**：`auto Func() -> ReturnType`（强制）
- **命名**：类/函数使用 PascalCase，成员变量使用 `snake_case_`，局部变量/参数使用 `camelCase`
- **缩进**：Tab，Allman 花括号风格
- **命名空间**：`snake_case`，顶层为 `atom`
- **Include Guard**：`#ifndef ATOM_<NAME>_HPP`
- **Include 顺序**：标准库 → 第三方 → 项目头文件（`<>`） → 自身头文件（`""`）

参见完整规范：
- [English](../CODING_STANDARD.md)
- [中文](CODING_STANDARD-CN.md)

### 提交前自查清单

- [ ] 代码编译无警告
- [ ] 使用后置返回类型
- [ ] 成员变量使用 `snake_case_` 尾下划线
- [ ] 没有 `m_` 前缀或首下划线
- [ ] Include Guard 使用 `ATOM_<NAME>_HPP` 格式
- [ ] Include 顺序正确
- [ ] 使用 Allman 花括号风格 + Tab 缩进
- [ ] `[[nodiscard]]` 用于适当的函数
- [ ] `#include` 中没有绝对路径

---

## Pull Request 流程

1. **Fork** 本仓库，从 `master` 创建一个新分支：

   ```bash
   git checkout -b <branch-name>
   ```

2. **进行修改**，遵循[编码规范](#编码规范)。

3. **保持提交聚焦**。每个提交应代表一个单一的逻辑变更。

4. **编写清晰的提交信息**（参见[提交信息规范](#提交信息规范)）。

5. **Rebase** 到最新的 `master`：

   ```bash
   git fetch origin
   git rebase origin/master
   ```

6. **提交 Pull Request**，包含：
   - 清晰的变更标题
   - 解释变更内容和原因
   - 关联相关 issue（如 `Closes #123`）

7. **处理审查反馈**。维护者可能要求修改。

---

## 提交信息规范

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 格式：

```
<type>: <简短描述>

<可选正文>
```

**类型：**

| 类型 | 用途 |
|---|---|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `refactor` | 代码重构 |
| `docs` | 仅文档 |
| `style` | 格式、缩进等 |
| `build` | CMake 或构建系统变更 |
| `chore` | 维护任务 |

**示例：**

```
feat: 添加 Vec2 数学类型及基本算术运算符

fix: 修正 RenderWindow.hpp 中的 Vec2 包含路径

docs: 添加 CONTRIBUTING.md 开发指南

build: 在 CMake 中添加 engine_math 库目标
```

---

## 报告问题

提交 issue 时，建议包含以下信息：

1. **环境**：操作系统、编译器、CMake 版本
2. **复现步骤**：最小化、完整、可验证
3. **预期行为**：您期望发生的事情
4. **实际行为**：实际发生的事情（包含日志）
5. **可能的修复**：如果您对问题有想法

---

*感谢您帮助 Atom Engine 变得更好！*
