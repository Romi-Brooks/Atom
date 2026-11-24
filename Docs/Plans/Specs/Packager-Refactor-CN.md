# Packager 重构

本轮只读写 APKG v2。v1 已退役，RSA、压缩以及新的磁盘格式版本另行设计。
保留 `packager_tool pack <input> <output.apkg> [package_path]` 及其退出码，供 CMake 和脚本调用。

| 来源 | 代码复核 | 本轮处理 |
| --- | --- | --- |
| ARCH-107 | 文档仍混有 v1、`pack2`、单 chunk 的历史描述。v2 读取器已有目录表和多 chunk 读取，写入为 store-only | 同步文档、让打包后校验走运行时解析器，明确结构检查与内容校验 |
| ARCH-107 | 写入器共享 `.tmp` 文件，提交前先删目标，扫描失败也可能留下部分计划 | 独立暂存、同卷提交、扫描失败回滚，验证旧包和输入文件不受损 |
| ARCH-107 | 路径重新打开可能把新包内容与旧清单混合；挂载表写入与工作线程读快照没有同步 | 校验包身份，保护挂载表的快照，后端 I/O 在锁外执行 |
| ARCH-114 | 宽 argv 已覆盖 Windows CLI。当前没有交互菜单 | TUI 控制台读取宽字符，重定向标准输入按 UTF-8 处理 |
| CONCURRENCY-010 | store-only 先并行算哈希再拷贝会多读一次源文件 | GUI 先把整个构建交给 JobSystem。块级 fork-join 与压缩保留为待测优化，仍要求产物逐字节一致 |
| CORE-003 / CORE-005 | 本轮不新增 mount provider。GUI 依赖 Debugger，关闭后没有操作界面 | Filesystem 目录保持现状，GUI 走 Runtime 与 Renderer2D，CLI 和 TUI 可脱离图形后端 |

重构顺序为写入与读取契约、共享操作、终端入口和桌面 GUI。GUI 提供目录树、资源列表、类型筛选、
详情、校验和 Atom_Log 输出。工具可使用引擎的底层组件，工具工作流留在 `Tools/Packager`。
验收包括旧命令、Unicode 路径、覆盖失败、损坏包、Release（Debugger 关闭）和 Homeless 的现有打包流程。

## 实现与取舍

- 构建通过独立的同卷暂存目录提交。Windows 使用一次覆盖移动，POSIX 使用 rename 或无覆盖的硬链接发布。临时文件归构建实例清理。
- 写包后由运行时读取器检查包头、清单、目录关联及路径，随后按 64 KiB 流式核对各数据块和资源哈希。
  普通挂载校验结构。热读路径不重复全量哈希检查。磁盘包被替换时，已挂载实例拒绝为相异包头打开新句柄。
- VFS 挂载表按操作取得快照，后端 I/O 在锁外执行。跨多个调用的帧一致性仍由调用方约定。高低优先级目录
  与文件冲突时的跨后端遮蔽、热重载时的帧级切换，以及原生路径在外部并发更换链接时的句柄级隔离，仍需各自定义契约。
- TUI 与 GUI 共用打开、构建、校验。GUI 由 Atom Runtime、Renderer2D、UiTree 和 JobSystem 驱动。
  文件对话框经平台服务对接。表格、搜索框和目录树的工具交互留在 Packager，通用 UiTree 控件扩展单独评估。
- CONCURRENCY-010 的块级 fork-join 仍开放。GUI 当前在一个后台任务中流式写入和校验。待测量吞吐后决定是否拆并行阶段。

## Windows 验证

- CLion MinGW Release 全量构建和 80 项 CTest 通过。回归覆盖目标被锁定时保留旧包、内容损坏、替换后拒绝旧挂载的新读取，以及并行读与挂载切换。
- 中文路径经 CLI 与重定向 UTF-8 的 TUI 打包和验证，两种入口产物逐字节相同。损坏包由 CLI 返回失败退出码。
- 关闭 Debugger 的 Release GUI 打开 Homeless 包并校验 11 个资源。检查了浏览界面、构建表单和校验后的 Activity 日志。
- Homeless 的现有 CMake 展开与打包目标通过，Vulkan 下以新包运行 200 帧并完成 7 项加载任务。
