# Lua Mod 资源加载设计方案

> 状态：设计文档（待实现）
> 关联：LuaHost / LuaContext / Vfs / AssetPath

## 一、问题

mod 开发者通常只能用 Lua 脚本，无法像游戏开发者那样在 C++ 侧
`NativeFileSystem::Create(...)` + `Vfs::Mount(...)` 挂载磁盘目录。那么 mod 如何
导入自己目录下的资源（音频、纹理、模型、配置）？

## 二、核心结论

**mod 脚本不应、也不能直接挂载目录。** 挂载是「宿主配置」，读取才是「mod
内容逻辑」。二者由不同角色完成：

| 层级 | 角色 | 机制 |
|------|------|------|
| 挂载目录到 VFS | 宿主 C++（游戏开发者） | 读 manifest → `NativeFileSystem::Create` → `Vfs::Mount` |
| 读取资源 | mod Lua 脚本 | 字符串 AssetPath 交给加载器（如 `Atom.Audio.Music.Load(id, "mymod://...")`） |

## 三、为什么不能让 mod 脚本挂载目录

1. **安全越权**：让任意 Lua 脚本 `Mount("/path/to/anything")` 等于给 mod 开
   一张读取宿主任意磁盘文件的通行证，是典型的路径遍历 / 越权漏洞。
2. **类型屏障**：`Vfs::Mount` 需要 `std::shared_ptr<IFileSystem>` 后端，而
   `NativeFileSystem` 是 C++ 类型、私有构造、带 symlink 逃逸防护，Lua 侧无法
   安全表达。
3. **职责错位**：目录挂哪个 mount、根在哪，是宿主配置，不是内容逻辑。

## 四、目标架构：声明式 manifest + 宿主代为挂载

mod 作者**声明自己需要什么**，宿主在加载 mod 时替它执行挂载。

```jsonc
// mod.json（mod 作者提供，放在 mod 根目录）
{
  "name": "my_mod",
  "mounts": [
    { "mount": "mymod", "path": "audio" },
    { "mount": "mymod", "path": "textures" }
  ]
}
```

宿主（游戏开发者的 C++ 侧）加载流程：

1. 定位 mod 根目录（sandboxed，通常是游戏自己的 mod 目录）。
2. 读取 `mod.json`。
3. 对每个 mount：`NativeFileSystem::Create(mount, mod_root + relative, fs)` →
   `Vfs::GetInstance().Mount(mount, priority, fs)`。
4. 再执行 mod 的 `main.lua`。

这样 mod 只能访问自己的目录（`NativeFileSystem` 的 canonicalization 已防 `../`
与 symlink 逃逸），脚本里用 `Atom.Audio.Music.Load("song", "mymod://audio/song.ogg")`
即可，字符串 AssetPath 就是唯一自然边界。

## 五、Lua 侧需要新增的绑定

当前 `Atom.Audio.*` 已支持 `Load(id, "res://...")` 字符串（走 `Vfs::GetInstance()`）。
mod 要读取任意字节流 / 枚举目录，需新增 **`Atom.Filesystem`** 绑定：

| API | 语义 |
|-----|------|
| `Atom.Filesystem.Load(mount_path) -> bytes\|nil` | 读取已挂载资源的原始字节 |
| `Atom.Filesystem.Exists(mount_path) -> bool` | 判断资源是否存在 |
| `Atom.Filesystem.List(mount_dir) -> {{name, type}, ...}` | 枚举目录（`IFileSystem::List` 转 Lua 表） |

### 安全边界（必须守住）

即使暴露 `Atom.Filesystem.Load`，它也只能访问**宿主已挂载的 mount**，mod 无法
通过它挂新目录，也无法越出已有 mount 的根（`NativeFileSystem` 已有 canonicalization
+ symlink 逃逸防护）。

## 六、可选：宿主侧 `LoadMod()` 帮助函数

为减少宿主样板代码，可提供 `LoadMod(vfs, mod_root)` 封装「读 manifest → 挂载 →
执行 main.lua」的完整流程。这是**引擎提供的便利函数**，不是 mod 的 API。

## 七、待办

- [ ] 新增 `Atom.Filesystem` 绑定（Load / Exists / List）
- [ ] 宿主侧 `LoadMod()` 帮助函数 + manifest 解析
- [ ] Example：一个最小 mod（mod.json + main.lua + 一张纹理/一段音频）演示完整链路
