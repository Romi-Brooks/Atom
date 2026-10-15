

# Atom Engine — Coding Standard  

[English](../CODING_STANDARD.md) | [中文](CODING_STANDARD-CN.md)

---
> 这是 Atom 引擎在开发时所遵循的项目架构与代码风格的快速指引文档，当你想要为 Atom 引擎贡献代码时，请务必遵循此指引文档

仓库根目录的 `.clang-format` 是格式化规则的唯一来源。提交前应对修改过的 C/C++
文件执行 `clang-format -i <files>`。

***

## 1. 目录与文件命名

### 1.1 目录结构

```
ModuleName/            # 模块名，PascalCase，单数
├── Manager/           # 该模块的管理器子模块
├── Plug/              # 该模块的插件/扩展子模块
├── SubModule/         # 按功能拆分的子目录
│   ├── Foo.cpp
│   └── Foo.hpp
├── ModuleName.cpp     # 模块主源文件
├── ModuleName.hpp     # 模块主头文件
└── CMakeLists.txt     # （可选）子 CMake 配置
```

**规则：**

- 目录名与模块名一致，使用 **PascalCase**
- 子目录按功能/职责分组，而非按文件类型（不要出现 `src/`、`include/`、`headers/` 这样的结构）
- `Manager/`、`Plug/` 等命名反映职责

### 1.2 文件命名

| 文件类型   | 命名规则               | 示例                               |
| ------ | ------------------ | -------------------------------- |
| 类定义头文件 | 与类名完全一致，PascalCase | `Entity.hpp`、`VolumeManager.hpp` |
| 类实现源文件 | 与头文件同名             | `Entity.cpp`、`VolumeManager.cpp` |
| 非类工具文件 | 描述性 PascalCase     | `LogSystem.hpp`、`MusicFade.hpp`  |
| 测试文件   | `*Test.cpp`        | `AudioPlaybackTest.cpp`          |

***

## 2. 头文件规范

### 2.1 Include Guard

```cpp
#ifndef ATOM_MODULENAME_HPP
#define ATOM_MODULENAME_HPP
// ...
#endif // ATOM_MODULENAME_HPP
```

**规则：**

- 格式：`ATOM_<NAME>_HPP`，`<NAME>` 为全大写 PascalCase
- `<NAME>` 取**文件名**（`ScreenManager.hpp` → `ATOM_SCREENMANAGER_HPP`）。可以在前面保留目录分量
  （`Backend/Contracts/Window/IWindow.hpp` → `ATOM_BACKEND_CONTRACTS_WINDOW_IWINDOW_HPP`）；接口名去掉
  开头的 `I` 也可以接受（`IAudioEffect.hpp` → `ATOM_AUDIO_EFFECT_HPP`）。禁止使用改名前的旧名字——
  这个宏是读者判断代码块归属的唯一依据。
- `#ifndef` 与 `#define` 必须相邻，中间不得插入其他内容
- `#endif` 后必须添加注释宏名
- 本仓库任何位置都不使用 `#pragma once`

### 2.2 Include 顺序

实现文件首先包含对应的自身头文件，以便尽早发现头文件不能独立包含的问题。其余
include 按以下分组顺序排列，每组空一行：

```cpp
// Self Dependency
#include "VolumeManager.hpp"

// Standard Library
#include <memory>
#include <string>
#include <unordered_map>

// Third Party Library
#include <SDL3/SDL.h>

// Engine Headers
#include <Media/Audio/Music.hpp>
#include <Log/LogSystem.hpp>
```

**规则：**

- `.cpp` 文件：**自身头文件（`""`）** → **标准库** → **第三方库** → **项目头文件（`<>`）**
- 头文件没有自身依赖，从标准库 include 开始
- 项目内头文件使用 `#include <ModuleName/FileName.hpp>` 语法（以项目根为基准）
- 自身头文件（对应的 `.hpp`）使用 `#include "FileName.hpp"`，置于最前
- **禁止**相对 include 路径（`../`、`../../`）：引擎头文件一律以项目根为基准书写
- 按需包含（include what you use）：用到的每个名字都要包含声明它的头文件，且同一文件不得重复包含同一头文件
- 上述分组的唯一例外是前置声明块，它归入「项目头文件」组

### 2.3 `using` 与类型别名

- 公共头文件**禁止** `using namespace xxx;`；它会污染所有包含者的查找范围。
- `using Name = Type;` 是在当前命名空间声明一个受限的别名，不会把名字导入包含者的全局命名空间；允许用于明确且有意的 API 门面、资源句柄或类内 callback 类型。
- 命名空间级别别名必须表示**完全相同的类型与单位语义**，并放在拥有该 API 的命名空间中。例如 `render::Color = color::Color`；如果坐标系、单位、所有权或不变量不同，必须使用独立 struct/wrapper，不能用别名。
- 别名没有独立的前置声明。若公共接口按值存储或构造该类型，头文件必须包含底层类型的完整定义；若仅使用指针/引用，可先前置声明底层 tag 后再声明别名。
- 不要为缩短局部代码随意新增命名空间级别别名；名字冲突应由限定命名空间或更清晰的类型名解决。新增公共别名须在代码审查中说明其稳定 API 意图。

***

## 3. 命名规范

### 3.1 命名风格速查表

| 类别              | 风格                   | 示例                                     |
| --------------- | -------------------- | -------------------------------------- |
| **命名空间**        | `snake_case`         | `atom`、`atom::audio`                   |
| **类 / 结构体**     | `PascalCase`         | `VolumeManager`、`MusicFade`、`SFX`      |
| **枚举类型**        | `PascalCase`         | `LogLevel`、`FadeState`、`NPCType`       |
| **枚举值**         | `PascalCase`         | `Idle`、`FadingOut`                     |
| **常量**（`const` / `constexpr`，任意作用域） | `snake_case` | `max_volume`、`default_recent_capacity`、`invalid_texture_2d` |
| **公有 / 私有成员函数** | `PascalCase`         | `GetInstance()`、`Load()`、`SetVolume()` |
| **静态成员函数**      | `PascalCase`         | `GetInstance()`、`SetSfxVolume()`       |
| **成员变量**        | `snake_case_` + 尾下划线 | `music_volume_`、`current_playing_id_`  |
| **静态成员变量**      | `snake_case_` + 尾下划线 | `static float music_volume_`           |
| **公开聚合/配置字段**   | `snake_case`          | `sample_rate`、`output_dir`             |
| **函数参数**        | `snake_case`         | `id`、`file_path`、`target`              |
| **局部变量**        | `snake_case`         | `it`、`load_result`、`music`             |
| **宏**           | `UPPER_SNAKE_CASE`   | `LOG_INFO`、`LOG_ERROR`                 |

### 3.1.1 日志通道命名

通道是按日志命名空间分组的**枚举支持值**——每个域是一个
`ATOM_DEFINE_CHANNELS` 块（引擎通道在 `Log/AtomLogChannels.hpp`，游戏域在游戏项目里）。
一个域拥有命名空间、内部 `Channel` 枚举和显示前缀：

| 域 | 前缀 | 示例 |
|---|---|---|
| `atom::log::core` | `Atom.` | `atom::log::core::Main` |
| `atom::log::audio` | `Atom.Audio.` | `atom::log::audio::Music` |
| `atom::log::render` | `Atom.Render.` | `atom::log::render::Renderer2D` |
| `atom::log::image` | `Atom.Image.` | `atom::log::image::Decoder` |
| `atom::log::backend::Audio` | `Atom.Backend.Audio.` | `atom::log::backend::Audio::sdl3` |
| `game::log` | `Game.` | `game::log::Npc` |

- 通道值使用 `PascalCase`（`ScreenManager`、`PlugMusicFade`）。后端实现标识是例外：
  使用其规范的注册 ID（`sdl3`、`sdl3_mixer`），以保证日志通道和运行时后端 ID 一致。
- 显示名使用 `.` 分隔的 PascalCase（`Atom.Entity.NPC ->`）
- 调用处一律通过日志域直接写通道值（如 `atom::log::audio::Music`），不要定义局部别名
  （如 `const auto& music_log_channel = atom::log::audio::Music;`）——别名虽然让调用更短，
  但会给接手的人增加一层间接跳转，收益有限。

### 3.2 详细规则

#### 命名空间

- 使用 `snake_case`
- 顶层命名空间：`atom`
- 子命名空间：`atom::audio`、`atom::video`（若需细分）
- 头文件中**禁止** `using namespace`；`.cpp` 中也只允许用于自身模块的命名空间（或测试文件），
  不得用它引入第三方命名空间

#### 类

- PascalCase，首字母大写
- 缩写词全部大写：如：`SFXManager`
- 新类名应使用完整单词或公认缩写

#### 函数

- 引擎 C++ 函数使用**后置返回类型**：`auto FuncName() -> ReturnType`
- 构造函数、析构函数、`main` 和签名必须匹配 C API 的回调函数除外
  （仓库内唯一例外是 `Lua/LuaBinding/` 里的 `int LuaXxx(lua_State* L)` 绑定函数）
- 该规则同样覆盖类内的小工具函数：写 `auto Reset() -> void`，不写 `void Reset()`
- PascalCase，动词开头：`GetInstance()`、`Load()`、`SetVolume()`
- getter 以 `Get` 开头，setter 以 `Set` 开头
- 布尔查询以 `Is` / `Has` 开头：`IsLoaded()`、`HasSFX()`
- 构造函数/析构函数使用传统语法

```cpp
// 正确
auto Play(const std::string& id) -> void;
auto GetMusicVolume() const -> float;
[[nodiscard]] auto IsLoaded(const std::string& id) const -> bool;

// 避免
void Play(const std::string& id);
```

#### 成员变量

- `snake_case_`，**尾下划线**是必须的
- 不要使用 `m_` 前缀或首下划线
- 简单聚合或配置结构体中的公开字段使用 `snake_case`

```cpp
class Music {
    private:
        std::unordered_map<std::string, std::unique_ptr<IAudioSource>> tracks_;
        std::string current_playing_id_;
        static float music_volume_;
};
```

#### 常量

- 任意作用域（命名空间、类、文件、函数）统一 `snake_case`
- **不使用 `k` 前缀**，**也不加尾下划线**：尾下划线标记的是对象状态，而常量不是对象状态；
  类作用域常量在使用处总是带限定名（`Log::default_recent_capacity`、`LayoutTree::invalid_node`），
  已经足以与局部变量区分
- `UPPER_SNAKE_CASE` 保留给宏，不要用它命名 C++ 常量
- 由宏生成、必须经预处理器拼接的常量无法被转成小写，因此宏改为输出固定的 `snake_case` 名字，
  并依赖上面「一个域拥有一个命名空间」的约束（`LogChannelMacros.hpp`：`channel_names`、`channel_registered`）

```cpp
namespace atom::numeric {
inline constexpr float max_volume = 100.0f;  // 命名空间作用域
}

class Log {
    public:
        static constexpr std::size_t default_recent_capacity = 512;  // 类作用域
};

namespace {
constexpr float two_pi = 6.28318530717958647692f;  // 文件作用域
}
```

#### 参数与局部变量

- `snake_case`，首字母小写
- 单字母变量仅限于循环计数（`i`）或迭代器（`it`）
- 布尔参数使用动词或形容词：`is_enabled`、`should_loop`

#### 类内私有类型

- 枚举类型名：PascalCase
- 枚举值：PascalCase
- 若枚举或者结构体仅在类内部使用，定义在类内 `private:` 区域：
  ```cpp
  class MusicFade {
      private:
          enum class FadeState {           // 类内私有
              Idle,
              FadingOut,
              FadingIn,
              Completed
          };

          struct FadeContext {             // 类内私有
              std::string from_id;
              std::string to_id;
              float duration{0.0f};
              FadeState state{FadeState::Idle};
          } context_;
  };
  ```

#### 文件命名与类名一致

- 一个文件一个类，文件名与主类名一致

***

## 4. 代码格式

### 4.1 缩进与花括号

- **缩进：** 4 个空格，不使用 Tab 缩进
- **花括号风格：** K&R（左花括号与声明或控制语句同行）

```cpp
namespace atom {
class Entity {
    public:
        auto GetHP() const -> float {
            return hp_;
        }

    private:
        float hp_;
};
}
```

### 4.2 访问控制

```cpp
class ClassName {
    public:
        ClassName() = default;
        ~ClassName() = default;

        ClassName(const ClassName&) = delete;
        auto operator=(const ClassName&) -> ClassName& = delete;

        static auto GetInstance() -> ClassName&;
        auto DoSomething() -> void;

    private:
        auto Helper() -> void;

        int member_;
};
```

**规则：**

- 优先使用 `public:` → `protected:` → `private:`，先展示公共 API
- 访问说明符在类内缩进一级，声明再额外缩进一级
- 移动成员声明时必须考虑初始化顺序；纯风格修改不得改变行为

### 4.3 成员初始化

- 使用统一初始化（brace-init）优先

```cpp
float moveSpeed_ {};           // 值初始化
float music_volume_ = 100.0f;  // 也可用 = 
unsigned int fps_ = 60;
```

### 4.4 指针与引用

```cpp
auto DoSomething(const std::string& str) -> void;   // const 引用
auto DoSomething(std::string&& str) -> void;         // 右值引用
auto GetPointer() const -> SomeType*;                // 指针
```

- `*` 和 `&` 贴在类型侧（左引用），而非变量名侧

### 4.5 `const` 位置

- 成员函数 `const` 使用**尾随**形式：

```cpp
auto GetValue() const -> float;       // trailing const
auto GetValue()const -> float;        // 缺少空格
```

***

## 5. C++23 特性使用规范

### 5.1 强制使用

| 特性                  | 用法                                   |
| ------------------- | ------------------------------------ |
| **后置返回类型**          | 引擎 C++ 函数使用 `auto Func() -> Type`；C 回调和 `main` 除外 |
| **`auto`** **占位符**  | `auto it = map.find(id);`            |
| **`[[nodiscard]]`** | 所有 getter、查询函数、不应忽略返回值的函数            |
| **`enum class`**    | 所有枚举必须使用 `enum class`，禁止裸 `enum`     |
| **范围 for + 结构化绑定**  | `for (const auto& [key, val] : map)` |

### 5.2 建议使用（后期支持）

| 特性                                       | 用法                                                |
| ---------------------------------------- | ------------------------------------------------- |
| **`std::expected`**                      | 用于可能失败的返回值（若编译器支持；引擎统一用 `atom::Result<T>`，见 §6）      |
| **`std::print`** **/** **`std::format`** | 替代 `std::cout` / 字符串拼接                            |
| **`consteval`** **/** **`constexpr`**    | 编译期计算的函数                                          |
| **`std::span`**                          | 替代 `const std::vector<T>&` 参数                     |
| **`std::optional`**                      | 替代可能为空的返回值                                        |
| **`std::views`**                         | 使用 `std::views::filter`、`std::views::transform` 等 |

***

## 6. 统一错误规范（CORE-004）

引擎只有**一套**失败表示：`atom::Result<T>`（`Error/Error.hpp`）。新增或修改可失败 API 时必须遵守本节。

### 6.1 返回什么

| 情形 | 返回类型 |
| --- | --- |
| 可能失败并需要携带值 | `atom::Result<T>` |
| 可能失败但不携带值 | `atom::Result<void>` |
| 只有成/败、且失败无需上报（内部分支判断） | `bool`，且函数名以 `Try`/`Is`/`Has` 开头 |
| 真的不会失败 | `void`，并在注释里写明"设计上不返回失败" |

- **禁止**新增模块专属的结果枚举。已有枚举（`fs::Result`、`AssetResult`、`DecoderOpenStatus`、
  `ScreenOpResult`、`PackageBuilder::Result` 等）只作为**后端边界的小整数码**保留：热路径
  （`ReadAt`/`ReadNext`）返回它，由上层用 `ToError()` 转换一次；不让 `std::string` 穿过读路径。
- **禁止**用异常表达可预期的失败（文件不存在、格式不支持、设备打开失败）。异常只留给编程错误。
- **禁止**"只写日志然后返回 `void`/`bool`"。调用方必须能拿到原因。
- 返回 `Result` 的函数仍需标 `[[nodiscard]]`：`[[nodiscard]]` 不会从返回类型传播到对象。

### 6.2 错误码

- 错误码在**拥有该决策的模块**的块内追加（`ErrorCode` 中每模块 256 个号段）。
  跨模块翻译只用于下层原因向上传递（如 asset 透出 `FsNotFound`），这合法且可见。
- **只追加、不改号、不复用**退役的值。
- 代码名用 `ErrorCodeName()`，模块名用 `ModuleName()`；不要在别处再写一套 switch。

### 6.3 上下文与底层原因

```cpp
// 失败点：把原因一次性补齐
const auto opened = filesystem.OpenRead(path, file);
if (opened != fs::Result::Success)
    return std::unexpected<Error>{fs::ToError(opened, "OpenRead", path.String())};
```

- `operation` 用**最内层知道自己在做什么的那个**操作名；`WithOperation()` 不覆盖已有值，
  所以"OpenRead failed"不会被上层的"AcquireRecord failed"盖掉。
- `resource` 填 `AssetPath` 字符串（`Error` 不依赖 Filesystem，所以是字符串）。
- **平台错误状态必须在失败点立即捕获**：`SDL_GetError`/`GetLastError`/`errno` 都是会被下一次调用覆盖的
  单槽。用 `CaptureSdlError()`（读后即清）、`CaptureLastSystemError()`、`CaptureErrnoMessage()`。

### 6.4 谁负责记录日志

- `Error` **不写日志**，也**不提供**"记录并返回"的函数：`Log` 依赖 `Error`，反过来会破坏叶子模块分层。
- 在**最接近失败、且知道 operation 与 resource 的那一层**记录一次，其余层只传递：

```cpp
LOG_ERROR(atom::log::asset::Resource, "ResourceManager: " + error.Message());
```

- 一次失败只应产生**一条**诊断。上层转发 `Error` 时不要重复记录。
- 模块自己有日志频道就用它；不要借用 `core::Screen` 之类不相干的频道。

### 6.5 `Error` 的边界

- `Error` 是纯值类型：不抛异常、不依赖 Log/后端、成功路径不分配。
- 默认构造的 `Error` 表示 `Internal`，**不表示成功**——"有没有失败"由 `Result` 承载。
- 需要新增跨模块的错误分类时，先加到 `ErrorCode` 的模块号段，再在使用处转换，不要新建枚举。
- **引擎叶子层（`Error/`）不使用 `std::format`**：它要求 MSVC 17.10+ / GCC 13+ / 较新的 libc++，
  而 CI 还会增加 MinGW 与 Clang（QUALITY-002），为一个诊断辅助函数给整个构建加门槛不划算。
  叶子层用字符串拼接构造诊断文本。这是唯一严格执行该限制的地方，将来可与 CI 矩阵一起重新评估。
- **实时路径不做格式化**：音频回调与解码线程不得为一次运行期格式串解析付出开销，那里要么拼接、要么不记。

***

## 7. 注释规范

注释默认使用英文。仅在必要时才使用中文等其他语言，例如引用本地化文案，
或补充英文无法准确表达的语境说明。

### 6.1 Doxygen 文件头

```cpp
/**
  * @file           : FileName.hpp
  * @author         : Author
  * @brief          : 一行文件说明
  * @attention      : 可选的注意事项
  * @date           : YYYY/M/D
  Copyright (c) YYYY Author, All rights reserved.
**/
```

这是当前新建 Atom 源文件和示例使用的文头格式。**此后新增的文件必须带此文件头**。现有文件暂不批量迁移；
SPDX/许可证文头迁移作为 **CORE-007** 独立记录在 `Docs/Remaining-Issues.md`（当前审计：Atom 自有 C/C++
文件 161 个，其中 45 个仍是旧文头，9 个已有 SPDX）。不要因为改别的问题而顺手给文件加第二种文头风格——
迁移统一交给 CORE-007。

### 6.2 行内注释

```cpp
// 获取当前的 SFX 音量
auto GetSfxVolume() const -> float;
```

### 6.3 复杂方法/属性注释

```cpp
/**
  * @name           : Method/Property
  * @author         : Author
  * @brief          : Description for the method/property
  * @attention      : Any caveats or notes
**/
```

### 6.4 `TODO` / `FIXME`

```cpp
// TODO(Author): Implement retry logic
// FIXME: Race condition on shutdown
```

