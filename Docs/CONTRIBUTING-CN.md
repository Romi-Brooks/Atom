# 为 Atom Engine 贡献代码  

[English](../CONTRIBUTING.md) | [中文](CONTRIBUTING-CN.md)

---
首先，感谢您考虑为 Atom Engine 贡献代码！我们欢迎各种形式的贡献 —— Bug 报告、功能建议、文档改进和代码变更。

---

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [编码规范](#编码规范)
- [分支策略](#分支策略)
- [Pull Request 流程](#pull-request-流程)
- [提交信息规范](#提交信息规范)
- [报告问题](#报告问题)

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

1. 先检查 [issue 追踪器](https://github.com/Romi-Brooks/Atom/issues) 避免重复
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
- **命名**：类/函数使用 PascalCase，成员变量使用 `snake_case_`，局部变量/参数使用 `snake_case`
- **缩进**：4 个空格，K&R 花括号风格
- **命名空间**：`snake_case`，顶层为 `atom`
- **Include Guard**：`#ifndef ATOM_<NAME>_HPP`
- **Include 顺序**：自身头文件（`""`）→ 标准库 → 第三方 → 项目头文件（`<>`）

参见完整规范：
- [English](../CODING_STANDARD.md)
- [中文](CODING_STANDARD-CN.md)

### 提交前自查清单

- [ ] 代码编译无警告
- [ ] 使用后置返回类型（构造、析构、`main` 和必须匹配的 C 回调除外）
- [ ] 成员变量使用 `snake_case_` 尾下划线
- [ ] 没有 `m_` 前缀或首下划线
- [ ] 局部变量和参数使用 `snake_case`
- [ ] Include Guard 使用 `ATOM_<NAME>_HPP` 格式
- [ ] Include 顺序正确
- [ ] 使用 K&R 花括号风格 + 4 空格缩进
- [ ] `[[nodiscard]]` 用于适当的函数
- [ ] `#include` 中没有绝对路径

---

## 分支策略

- **`master`** — 稳定 Beta 版本。只从 `dev` 合并更新。
- **`dev`** — 日常开发分支。功能分支从这分出。
- **`feature/*`** — 新功能，从 `dev` 分出，完成后合并回 `dev`。

## Pull Request 流程

所有人通过 Fork 方式贡献：

```bash
# 1. 在 GitHub 上 Fork 本仓库

# 2. 连同依赖一起克隆 fork，并配置上游仓库
git clone --recurse-submodules https://github.com/<你的用户名>/Atom.git
cd Atom
git remote add upstream https://github.com/Romi-Brooks/Atom.git
git checkout dev

# 3. 从 dev 创建功能分支
git checkout -b feature/<功能名称>

# 4. 进行修改，遵循编码规范

# 5. 提交并推送到你的 fork
git add -A
git commit -m "feat: 简短的修改说明"
git push origin feature/<功能名称>

# 6. 在 GitHub 上打开 Pull Request
#    从你的 fork 的 feature/<功能名称> → 本仓库的 dev
```

### 提交前注意

- 保持提交聚焦，每个提交代表一个单一逻辑变更
- 推 PR 前 rebase 到最新 dev：
  ```bash
  git fetch upstream
  git rebase upstream/dev
  ```
- 切换分支后执行
  `git submodule sync --recursive && git submodule update --init`，更新到仓库锁定的依赖版本。
- 推送前在本地构建受影响的目标。当前仓库 CI 只在目标为 `master` 的 PR 上运行；
  目标为 `dev` 的功能 PR 暂时不会自动构建。
- PR 应包含清晰标题、变更内容与原因，以及 `Closes #123` 之类的关联 issue。
- 合并前处理维护者提出的审查意见。

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

build: 在 CMake 中添加 Atom_Math 库目标
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
