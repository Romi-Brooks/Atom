# Atom Packager

Packager 读写 APKG v2。它把文件或目录打成一个可由 Atom VFS 挂载的资源包，保留包内路径、目录树、
资源类别、大小及内容哈希。现行 v2 使用原始存储块。APKG v1 已退役。

## 命令行和交互终端

```text
packager_tool pack <input> <output.apkg> [package_path]
packager_tool list <package.apkg>
packager_tool verify <package.apkg>
packager_tool --tui
packager_tool --path <package.apkg>
```

打包目录时，`package_path` 是可选的包内前缀。打包单个文件时，需要指定该文件的完整包内路径。
包内路径使用 `/` 且遵循 VFS 路径规则。脚本式 `pack` 保留既有的覆盖行为和退出码。
TUI 提供打开、浏览、查找、验证和打包操作，默认保留已经存在的目标文件。

## 桌面界面

直接运行 `packager_tool` 打开 GUI，或用 `packager_tool --open example.apkg` 启动并打开资源包。
终端命令也使用同一个可执行文件。`--path` 在 TUI 中预先打开指定包。
在 Windows 11 24H2 及更新版本，从资源管理器启动普通 Release 时只打开图形界面。
Browse 页面提供目录树、按路径搜索、类型筛选、条目详情和完整性检查。
点击目录旁的箭头展开树。中间列表只显示当前目录的直属子目录和文件。点击子目录进入，点击 `..` 返回上一级，
搜索只匹配当前这一层。选中资源可在右侧预览图片、检查 JSON/JSONC 语法、
浏览 Lua 源码，或读取音频属性并试听。Lua 预览只读取文本。文本预览最多读取 128 KiB。
Build 页面支持文件和目录选择、包内前缀及覆盖选项。构建和校验在后台任务中进行，
Activity 面板订阅 Atom_Log 的 Packager 通道。关闭引擎 Debugger 也能使用该界面。

## 引擎接口

- 用 `PackageBuilder::AddFile` / `AddDirectory` 收集输入，再用 `Build` 写包。
- 用 `PackageFileSystem::Create` 读取清单，用 `EnumerateAssets` 和 `EnumerateDirectories` 访问资源和目录。
- `Create` 验证包头和清单结构。`VerifyContents` 流式检查资源和数据块的 FNV-1a 哈希。
- 运行时可以用 `Vfs::MountPackage` 挂载，随后通过 `res://path` 和 `IFile` 读取。
  已挂载路径上的包被替换后应重新挂载，读端会拒绝使用相异包头的文件。

格式与工具改造记录见 [Packager 重构](../../../../../Docs/Plans/Specs/Packager-Refactor-CN.md)。
