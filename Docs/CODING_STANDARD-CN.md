

# Atom Engine — Coding Standard  

[English](../CODING_STANDARD.md) | [中文](CODING_STANDARD-CN.md)

---
> 这是 Atom 引擎在开发时所遵循的项目架构与代码风格的快速指引文档，当你想要为 Atom 引擎贡献代码时，请务必遵循此指引文档

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

按以下分组顺序排列，每组空一行：

```cpp
// Standard Library
#include <memory>
#include <string>
#include <unordered_map>

// Third Party Library
#include <SDL3/SDL.h>

// Engine Headers
#include <Media/Audio/Music.hpp>
#include <Log/LogSystem.hpp>

// Self Dependency
#include "VolumeManager.hpp"
```

**规则：**

- **标准库** → **第三方库** → **项目头文件（`<>`）** → **自身头文件（`""`）**
- 项目内头文件使用 `#include <ModuleName/FileName.hpp>` 语法（以项目根为基准）
- 自身头文件（对应的 `.hpp`）使用 `#include "FileName.hpp"`，置于最末尾
- 禁止使用 `../../` 相对路径

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
| **函数参数**        | `camelCase`          | `id`、`filePath`、`target`               |
| **局部变量**        | `camelCase`          | `it`、`result`、`music`                  |
| **宏**           | `UPPER_SNAKE_CASE`   | `LOG_INFO`、`LOG_ERROR`                 |

### 3.1.1 日志频道命名

日志频道常量遵循 `CATEGORY_SUBCATEGORY` 格式，显示名使用 `Category.Subcategory`：

| 常量名 | 显示字符串 | 说明 |
|--------|----------|------|
| `ATOM_AUDIO_MUSIC` | `Atom.Audio.Music ->` | 引擎音频-音乐域 |
| `SDL_BACKEND_AUDIO` | `SDL.Backend.Audio ->` | SDL 音频后端 |
| `ATOM_AUDIO_PLUG_MUSICFADE` | `Atom.Audio.Plug.MusicFade ->` | 插件子模块 |

- 顶层命名空间常量以 `ATOM_` 或 `SDL_` 开头
- 子模块间用 `_` 分隔，全部大写
- 显示字符串使用 `.` 分隔，每个单词 PascalCase

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

- **所有非平凡函数**使用**后置返回类型**：`auto FuncName() -> ReturnType`
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

```cpp
class Music {
    private:
        std::unordered_map<std::string, std::unique_ptr<sf::Music>> musics_;
        std::string current_playing_id_;
        static float music_volume_;
};
```

#### 参数与局部变量

- `camelCase`，首字母小写
- 单字母变量仅限于循环计数（`i`）或迭代器（`it`）
- 布尔参数使用动词或形容词：`isEnabled`、`shouldLoop`

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
              std::string fromId;
              std::string toId;
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

- **缩进：** Tab（1 Tab = 4 字符宽度）
- **花括号风格：** Allman（花括号独占一行）

```cpp
namespace atom {
    class Entity {
        private:
            float hp_;

        public:
            auto GetHP() const -> float
            {
                return hp_;
            }
    };
}
```

### 4.2 访问控制

```cpp
class ClassName {
    private:
        // 成员变量优先
        int member_;

    public:
        // 构造/析构
        ClassName() = default;
        ~ClassName() = default;

        // 删除拷贝
        ClassName(const ClassName&) = delete;
        auto operator=(const ClassName&) -> ClassName& = delete;

        // 静态方法
        static ClassName& GetInstance();

        // 公有方法
        auto DoSomething() -> void;

    private:
        // 私有方法
        auto Helper() -> void;
};
```

**规则：**

- `private:` → `public:` 顺序（成员变量在前，函数在后）
- 每个访问说明符缩进一级，其内容再缩进一级

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
| **后置返回类型**          | `auto Func() -> Type`（非构造/析构函数必须使用）  |
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

### 6.1 Doxygen 文件头

```cpp
/**
  * @file           : FileName.hpp
  * @author         : Author
  * @brief          : One-line description
  * @attention      : Any caveats or notes
  * @date           : YYYY/MM/DD
  Copyright (c) 2025 Author,  All rights reserved.
**/
```

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

