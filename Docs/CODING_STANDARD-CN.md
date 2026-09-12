

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
- `#endif` 后必须添加注释宏名

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
- 禁止使用 `../../` 相对路径

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
  （如 `const auto& kMusicLogChannel = atom::log::audio::Music;`）——别名虽然让调用更短，
  但会给接手的人增加一层间接跳转，收益有限。

### 3.2 详细规则

#### 命名空间

- 使用 `snake_case`
- 顶层命名空间：`atom`
- 子命名空间：`atom::audio`、`atom::video`（若需细分）

#### 类

- PascalCase，首字母大写
- 缩写词全部大写：如：`SFXManager`
- 新类名应使用完整单词或公认缩写

#### 函数

- 引擎 C++ 函数使用**后置返回类型**：`auto FuncName() -> ReturnType`
- 构造函数、析构函数、`main` 和签名必须匹配 C API 的回调函数除外
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
| **`std::expected`**                      | 用于可能失败的返回值（若编译器支持）                                |
| **`std::print`** **/** **`std::format`** | 替代 `std::cout` / 字符串拼接                            |
| **`consteval`** **/** **`constexpr`**    | 编译期计算的函数                                          |
| **`std::span`**                          | 替代 `const std::vector<T>&` 参数                     |
| **`std::optional`**                      | 替代可能为空的返回值                                        |
| **`std::views`**                         | 使用 `std::views::filter`、`std::views::transform` 等 |

***

## 6. 注释规范

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

这是当前新建 Atom 源文件和示例使用的文头格式。现有文件暂不批量迁移；
SPDX/许可证文头迁移仍作为独立的机械任务记录在 `Docs/Remaining-Issues.md`。

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

