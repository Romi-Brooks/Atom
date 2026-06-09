# Atom Engine

[English](../README.md) | [中文](README-CN.md)

***

**Atom** 是一个使用 **C++23** 编写的模块化 **2D 游戏引擎**，旨在提供现代，清晰，轻量化的开发体验。

> **目前正在积极开发中** — Atom 仍处于早期开发阶段。API 和架构可能会发生变化。欢迎反馈和贡献。

***

## 快速开始

### 前置依赖

- CMake >= 3.20
- 支持 C++23 的编译器
- 第三方依赖（SFML 等）**不随仓库分发** — 需自行从官网下载（参见下方 [引擎依赖](#引擎依赖)）。

***

## 示例

可直接运行的示例位于 [`Example/`](../Example/) 目录：

***

### 编码规范

请参阅完整的编码规范文档：

- [CODING\_STANDARD](../CODING_STANDARD.md)

***

## 资源打包工具

Atom 提供了资源打包工具，用于将游戏资源打包/解包为 HPKG 存档格式。

- [打包工具文档](../Utilities/Packager/Doc/README-CN.md) — CLI 使用、API 参考和代码示例

***

## 引擎依赖

Atom 引擎目前使用 **SFML 3.0.0** 作为其多媒体库。

### SFML 部署

本项目遵循官方 SFML 下载指南：[SFML 3.0.0 下载](https://www.sfml-dev.org/download/sfml/3.0.0/)

各平台工具链的安装说明请参考上方官方指南。

#### Windows 构建支持

**支持的编译器：**

| 平台     | 编译器                           |
| ------ | ----------------------------- |
| 32-bit | GCC 14.2.0 MinGW (DW2) (UCRT) |
| 64-bit | GCC 14.2.0 MinGW (SEH) (UCRT) |

**重要提示：** 编译器版本必须 100% 匹配！使用 MinGW 包时，仅版本号大致相符是不够的。必须使用以下匹配的编译器之一：

- WinLibs UCRT 14.2.0 (32-bit)
- WinLibs UCRT 14.2.0 (64-bit)

#### Linux 构建支持

在 Linux 上，若使用 64 位系统，默认安装 64 位工具链。

**支持的编译器：**

| 平台     | 编译器          |
| ------ | ------------ |
| 64-bit | GCC - 64-bit |

**注意：**

- 编译 32 位版本需要安装特定包和/或使用特定编译器选项
- 如果需要 SFML 的 32 位版本，需自行编译
- **推荐：** 使用包管理器中的 SFML 版本（若足够新）或从源码构建，以避免不兼容问题

#### macOS 构建支持

**通过 Homebrew 安装：**

```bash
brew install sfml@3.0.0
```

**支持的编译器：**

| 平台     | 编译器            |
| ------ | -------------- |
| 64-bit | Clang - 64-bit |
| ARM64  | Clang - ARM64  |

***

## 未来规划

Atom 计划进行以下重要演进：

- **后端抽象** — 从 SFML 解耦，迁移到**SDL3**：从而更底层的提升性能、支持**Vulkan**、**OpenGL** 和 **DirectX** 等多后端渲染器。
- **渲染器插件系统** — 用户可以选择或编写自己的渲染后端。
- **ECS 框架** — 将当前简单的实体系统替换为正式的 Entity-Component-System 架构。
- **工具链** — 编辑器、资源管线和分析工具。

***

### 致谢

Atom 引擎使用了以下开源库，衷心感谢这些项目的开发者：

- [SFML](https://www.sfml-dev.org/) — 窗口管理、渲染、音频和输入
- [ImGui](https://github.com/ocornut/imgui) — 调试覆盖层 UI
- [ImGui-SFML](https://github.com/SFML/imgui-sfml) — ImGui 与 SFML 的集成桥接
- [Lua](https://www.lua.org/) — 脚本引擎
- [TagLib](https://taglib.org/) — 音频元数据读取
- [utfcpp](https://github.com/nemtrif/utfcpp) — UTF-8 验证和编码转换
- [FFmpeg](https://ffmpeg.org/) — 视频/音频解码

***

## 许可证
本项目使用[MIT License](../LICENSE)
