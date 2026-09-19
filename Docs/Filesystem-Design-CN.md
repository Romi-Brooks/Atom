# ATOM 文件系统与资源包设计

> 状态：`Atom_FS` + `Atom_Assets` + C1/C2 消费端（Script、Texture/TextureCache）已完成。C3 Audio 与 APKG v2 未开始。完整进度见 `Docs/Resource-System-Plan-CN.md`。

## 目标与非目标

ATOM 需要拥有自己的文件系统语义，而不是把 `std::filesystem` 的类型传播到业务、渲染和资源模块。运行时资源必须能同时来自开发目录、内存、正式资源包和未来的网络/模组挂载点。

第一阶段的目标是稳定公共边界：UTF-8 虚拟资源路径、确定的错误码、随机读取、目录枚举与挂载后端接口。`std::filesystem` 可以暂时留在 Native 后端 `.cpp` 中；它不是公共 API，也不是资源身份。

这不是重写 STL 容器的项目。`std::vector`、`std::string` 等可以作为实现细节和普通值容器继续使用；ATOM 自己必须定义的是路径规则、挂载优先级、I/O 行为、安全边界和资源格式。

## 路径模型

运行时仅使用 `AssetPath`：

```text
res://textures/ui/button.png
engine://shaders/spirv/sprite.spv
```

- 全部 UTF-8，路径分隔符固定为 `/`；
- 格式为 `mount://relative/path`，mount 为小写 ASCII；
- 禁止反斜杠、空 segment、`.`、`..`、盘符和绝对宿主路径；
- `res://` 是一个合法挂载根，可用于目录枚举；
- 真实磁盘路径只能留在 Native backend 和工具链中，不能进入渲染、音频或资源加载器的公开接口。

这使同一个资源在开发时映射到目录、发布时映射到 APKG，而调用点保持不变。

## 模块边界

```text
Atom_UTF8             UTF-8 校验与平台编码转换
     │
Atom_FS               AssetPath, IFile, IFileSystem, Vfs
 ├── NativeFileSystem  本地目录，根目录/符号链接越界保护
 ├── MemoryFileSystem  测试与动态生成资源
 ├── OverlayFileSystem 按优先级覆盖多个挂载
 └── PackageFileSystem APKG v1（只读；v2 未实现）
     │
Atom_Assets           ResourceId, LoaderRegistry, ResourceHandle<T>, cache
     ├── DecodedImageLoader（VFS → DecodedImage，已接入）
     ├── ScriptSourceLoader（VFS → 脚本源码，已接入）
     ├── Audio loader（未接入，待 C3）
     ├── Shader loader（未接入）
     └── Model/video/font loader（未接入）
```

`IFile` 提供 `Size()`、随机 `ReadAt()` 和顺序游标（`Tell`/`Seek`/`ReadNext`）。纹理与小配置可以一次读取；音频、视频和大模型可以按块读取。每个 `IFile` 只允许单调用者并发模型，异步 I/O 将在此同步契约稳定后作为独立扩展实现。

Native 后端解析路径后必须 canonicalize，并验证目标仍在挂载根目录下。因此包内路径和通过符号链接间接到达的文件都不能越出受信根目录。

## VFS 挂载规则

`Vfs` 持有按优先级排序的挂载：

```text
res://  priority 200  DevelopmentDirectory   （开发覆盖）
res://  priority 100  game.apkg              （正式基础包）
engine:// priority 100 engine.apkg
```

`Open` 与 `Stat` 从最高优先级开始命中；`List` 合并目录条目，同名项由高优先级覆盖。挂载与卸载当前立即生效；帧边界/热重载安全点待 CORE-001 建立后再收紧。已打开的 `IFile` 保持有效直到调用者释放。

## APKG 演进

### v1

现有 APKG v1 是末尾文件表的顺序归档。它保留为只读兼容格式，但不再扩展：路径根依赖 CWD、类型只是扩展名、整个资源需要完整读入内存、没有内容校验和可用的压缩实现。

### v2

APKG v2 采用“固定头 + 数据 chunks + manifest 索引”的设计：

- Header：魔数、字节序、主/次版本、flags、包 UUID、manifest 偏移/大小与 header checksum；
- Chunk 表：每个块的逻辑/存储大小、压缩算法、内容 hash、数据偏移；
- Asset 表：规范路径、父目录、`AssetKind`、tags、逻辑大小、内容 hash、chunk 范围与 cook 元数据；
- DirectoryNode：父节点与 child 范围，支持高效 `List`；
- Kind/Tag/Dependency 索引：Texture、Config、Audio、Model、Shader、Font、Script、Scene、Video、Binary 及其分类查询；
- StringTable：所有路径、tag 与元数据键去重存储。

所有索引使用连续数组和 index，不在磁盘格式中存储指针。按规范路径 hash 查找后必须比较完整路径，避免 hash collision 被误解析。资源数据以 chunk 形式存放，允许局部解压和流式读取。

打包器先扫描并校验全部输入、构建确定的 `PackagePlan`，写入临时文件，校验 manifest 与内容 hash 后原子替换目标文件；不得删除旧包后再开始生成。重复 AssetPath 必须在任何数据写入前报错。

## 资源分类与树

目录树是人类组织维度，`AssetKind` 是运行时语义，tag/variant 是筛选维度。三者必须并存，不能仅凭扩展名推断类型。

```toml
[[asset]]
source = "Assets/UI/Button.png"
path = "res://textures/ui/button.png"
kind = "texture"
tags = ["ui"]

[[asset]]
source = "Assets/Config/default.toml"
path = "res://config/default.toml"
kind = "config"
```

目录规则可以批量产生条目，但最终每个资源必须拥有确定的 `AssetPath`、`AssetKind`、冲突策略和可选 cook 参数。源资源与平台 cook 产物分离；同一 AssetPath 可以有 `windows-dxil`、`linux-spirv` 或纹理质量等 variant。

## 迁移顺序与验收

1. [x] 完成 `Atom_FS`：AssetPath、Native/Memory/Overlay/Vfs/Package(v1)，并加入路径、Unicode、根目录逃逸、目录顺序、优先级覆盖和包内读取测试。
2. [x] 将 APKG v1 封装为 `PackageFileSystem`，使目录与旧包能透明挂载（`List` 由路径前缀合成目录）。
3. [ ] 设计并实现 v2 reader、manifest、chunk 校验与流式读取；v1 只读兼容。
4. [ ] 将 Packager 改为 manifest 驱动的 `PackageBuilder`，实现树、分类、重复检测、临时输出与原子替换。
5. [-] 迁移 loader：Script 已走 `IFile`/`AssetPath`；Texture 已有 `DecodedImageLoader` + `TextureCache`（MusicCard 已迁）；Audio/Shader/Font/Model 未迁。
6. [x] 资源身份与共享：`ResourceId`、`ResourceHandle<T>`、`LoaderRegistry`、去重缓存与 recycle 回调已落地（`Atom_Assets`）；异步加载、热重载、依赖图和内存预算仍后续。

验收标准：相同 manifest 在不同 CWD 和平台产生相同虚拟资源树；开发目录与 APKG 能以同一 URI 打开；非法路径和符号链接不能越过挂载根；损坏包、hash 不匹配、重复路径和不支持的 variant 都返回受控错误。
