# Linux 编译指南

本文说明如何在 Linux 上使用 CMake 和 Ninja 编译 Atom Engine。

## 1. 环境要求

- CMake 3.20 或更高版本
- Ninja
- 支持 C++23 的 GCC 或 Clang
- SDL 3.x 开发包
- TagLib 2.x 开发包

仓库内 `ThirdParty/SDL3/x86_64-w64-mingw32` 和现有的
`ThirdParty/taglib/lib/libtag.a` 是 MinGW/Windows 构建产物，不能用于
Linux。Linux 必须使用本机编译的 ELF 库。

## 2. 安装依赖

### 2.1 Ubuntu 26.04 或更高版本

Ubuntu 26.04 提供 SDL3 和 TagLib 2 开发包，可以直接安装：

```bash
sudo apt update
sudo apt install \
  build-essential cmake ninja-build pkg-config \
  libsdl3-dev libtag-dev
```

然后直接跳到“配置与编译”一节。

> `libtag1-dev` 是旧版本或兼容过渡包，本项目需要的是提供 TagLib 2 的
> `libtag-dev`。

### 2.2 Ubuntu 24.04 LTS

Ubuntu 24.04 的官方仓库提供 TagLib 1.13.1，不能满足本项目的 TagLib 2
要求，并且没有合适的 SDL3 开发包。因此建议将两者从源码安装到用户目录，
无需污染 `/usr`。

先安装编译工具和 SDL3 常用的 Linux 后端依赖：

```bash
sudo apt update
sudo apt install \
  build-essential git cmake ninja-build pkg-config \
  libasound2-dev libpulse-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libudev-dev libusb-1.0-0-dev libwayland-dev \
  wayland-protocols libdecor-0-dev zlib1g-dev
```

创建统一的本地安装前缀：

```bash
export ATOM_DEPS="$HOME/.local/atom-deps"
mkdir -p "$ATOM_DEPS/src"
```

编译并安装 TagLib 2：

```bash
cd "$ATOM_DEPS/src"
git clone --depth 1 --branch v2.3.1 --recurse-submodules \
  https://github.com/taglib/taglib.git

cmake -S taglib -B taglib-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ATOM_DEPS" \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF

cmake --build taglib-build --parallel
cmake --install taglib-build
```

编译并安装 SDL3：

```bash
cd "$ATOM_DEPS/src"
git clone --depth 1 --branch release-3.4.10 \
  https://github.com/libsdl-org/SDL.git SDL3

cmake -S SDL3 -B SDL3-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ATOM_DEPS" \
  -DBUILD_SHARED_LIBS=ON \
  -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF

cmake --build SDL3-build --parallel
cmake --install SDL3-build
```

最后配置 Atom 时传入这个前缀：

```bash
cd /path/to/Atom

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$ATOM_DEPS"

cmake --build build --parallel
```

运行使用本地动态库的程序前执行：

```bash
export LD_LIBRARY_PATH="$ATOM_DEPS/lib:$LD_LIBRARY_PATH"
./build/Atom
```

### 2.3 Arch Linux

Arch 官方 `extra` 仓库直接提供 SDL3 和 TagLib 2，安装命令最简单：

```bash
sudo pacman -Syu
sudo pacman -S --needed base-devel cmake ninja pkgconf sdl3 taglib
```

Arch 的开发头文件、共享库和 CMake 配置都包含在主包中，不需要额外的
`-dev` 包。安装后可以直接配置 Atom，不需要设置 `CMAKE_PREFIX_PATH`。

### 2.4 验证安装

本项目需要 CMake 能找到以下配置文件：

```text
SDL3Config.cmake
taglib-config.cmake
```

可以使用以下命令查找它们：

```bash
find /usr /usr/local -type f \
  \( -name 'SDL3Config.cmake' -o -name 'taglib-config.cmake' \) \
  2>/dev/null
```

如果按 Ubuntu 24.04 的步骤安装在用户目录，还需要搜索该目录：

```bash
find "$ATOM_DEPS" -type f \
  \( -name 'SDL3Config.cmake' -o -name 'taglib-config.cmake' \)
```

## 3. 配置与编译

在项目根目录执行：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

Debug 构建：

```bash
cmake -S . -B build-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build-debug --parallel
```

构建成功后，可执行文件位于对应的构建目录中，例如：

```bash
./build/Atom
./build/example_media_metadata
./build/example_simple_window
```

在没有图形桌面或显示服务器的环境中，窗口示例可能无法运行，但这不影响
编译。

## 4. 使用 ThirdParty 中的 Linux 原生依赖

如果不想把 SDL3 和 TagLib 安装到系统，可以将它们的 Linux 原生安装产物
放在项目内。例如：

```text
ThirdParty/
├── SDL3-linux/
│   ├── include/
│   └── lib/cmake/SDL3/SDL3Config.cmake
└── taglib-linux/
    ├── include/
    └── lib/cmake/taglib/taglib-config.cmake
```

配置时通过 `CMAKE_PREFIX_PATH` 指定两个前缀：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/ThirdParty/SDL3-linux;$PWD/ThirdParty/taglib-linux"

cmake --build build --parallel
```

这里的库必须在 Linux 上为当前 CPU 架构编译，不能直接复制 Windows 的
`.dll`、`.lib` 或 MinGW `.a` 文件。

## 5. 检查依赖发现结果

若 CMake 找不到依赖，可以启用包查找调试输出：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_FIND_DEBUG_MODE=ON
```

也可以检查生成的缓存：

```bash
grep -Ei 'SDL3|TagLib' build/CMakeCache.txt
```

修改依赖位置后，建议使用新的构建目录，或只删除旧构建目录后重新配置：

```bash
cmake -S . -B build-new -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/native/dependencies"
```

## 6. TagLib 未定义符号排查

如果链接阶段出现大量 `TagLib::... undefined reference`，依次检查：

1. 确认没有使用仓库内的 MinGW `libtag.a`。
2. 确认安装的是 TagLib 2.x，并且头文件和库来自同一个安装前缀。
3. 确认 CMake 链接的是 `TagLib::TagLib` 目标，而不是手写 `-ltag` 路径。
4. 检查库文件格式和架构：

   ```bash
   file /path/to/libtag.so
   file /path/to/libtag.a
   uname -m
   ```

5. 查看最终链接命令：

   ```bash
   ninja -C build -v example_media_metadata
   ```

Linux 动态库通常显示为 ELF：

```text
ELF 64-bit LSB shared object, x86-64
```

如果输出包含 `PE32`、`MS Windows` 或 `MinGW`，说明错误使用了 Windows
版本的库。

## 7. 常见问题

### CMake 找不到 SDL3

```text
Could not find a package configuration file provided by "SDL3"
```

安装 SDL3 开发包，或把 SDL3 的安装前缀加入 `CMAKE_PREFIX_PATH`。传入的
路径应是包含 `include`、`lib` 等目录的安装前缀，而不是
`SDL3Config.cmake` 文件本身。

### CMake 找不到 TagLib 2

```text
Could not find a configuration file for package "TagLib"
```

确认安装的是 TagLib 2.x。部分较旧的 Linux 发行版只提供 TagLib 1.x，
这种情况下需要自行构建 TagLib 2，安装到自定义前缀后通过
`CMAKE_PREFIX_PATH` 使用。

### 运行时找不到共享库

如果依赖安装在非标准位置，可以临时设置：

```bash
export LD_LIBRARY_PATH=/path/to/dependencies/lib:$LD_LIBRARY_PATH
./build/Atom
```

更推荐将依赖安装到系统可识别的位置，或在打包阶段为程序配置合适的
RPATH。
