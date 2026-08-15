# Linux 编译指南

Atom 会从 `ThirdParty/` 中锁定版本的 Git submodule 编译 SDL3、TagLib、
Dear ImGui、Lua 和 utfcpp，并通过 minimp3 submodule 提供 MP3 解码。
Linux 不再需要单独安装这些库，也不能复用 Windows 或其他编译器生成的
二进制文件。

## 1. 环境要求

- Git
- CMake 3.20 或更高版本
- Ninja（推荐）或 Make
- 支持 C++23 的 GCC/Clang
- SDL3 对应的 Linux 窗口、音频和输入后端开发包

SDL3 和 TagLib 会与 Atom 使用同一套编译器从源码构建。首次干净构建耗时
较长，后续构建会复用已经生成的目标文件。

## 2. Ubuntu/Debian 构建环境

安装编译工具和 SDL3 常用后端的开发包：

```bash
sudo apt update
sudo apt install \
  build-essential git cmake ninja-build pkg-config \
  libasound2-dev libpulse-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxtst-dev \
  libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev \
  libudev-dev libusb-1.0-0-dev libwayland-dev \
  wayland-protocols libdecor-0-dev
```

不同 Ubuntu/Debian 版本的包名可能略有差异。某个可选包不存在时，可以先
移除该包继续安装；SDL3 会在 CMake 配置阶段报告实际启用的后端。

不再需要安装：

```text
libsdl3-dev
libtag-dev
libtag1-dev
```

Atom 会使用仓库锁定的源码版本。

## 3. Arch Linux 构建环境

```bash
sudo pacman -Syu
sudo pacman -S --needed \
  base-devel git cmake ninja pkgconf \
  alsa-lib libpulse libx11 libxext libxrandr libxcursor \
  libxfixes libxi libxss libxkbcommon libdrm mesa dbus \
  systemd-libs libusb wayland wayland-protocols libdecor
```

Arch 用户同样不需要安装系统的 `sdl3` 和 `taglib` 包。

## 4. Clone 和初始化依赖

```bash
git clone https://github.com/Romi-Brooks/Atom.git
cd Atom
git submodule update --init
```

检查 submodule 状态：

```bash
git submodule status
```

每行前面不应出现 `-`。Atom 只需要初始化顶层 submodule，不要求递归初始化
TagLib 自带的 utfcpp 副本，因为 Atom 已经锁定并共享自己的 utfcpp submodule。

如果已经 clone 了项目但缺少 `ThirdParty` 内容，执行：

```bash
git submodule sync --recursive
git submodule update --init
```

## 5. GCC Release 构建

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++

cmake --build build --parallel
```

构建内容包括：

1. utfcpp 接口目标。
2. minimp3 接口目标（header-only MP3 解码器）。
3. SDL3 静态库。
4. Lua 和 Dear ImGui 静态库。
5. TagLib 静态库。
6. Atom 引擎库、工具和示例程序。

CMake 会按照 target 依赖关系自动决定顺序，不需要手动先编译依赖。

## 6. GCC Debug 构建

```bash
cmake -S . -B build-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++

cmake --build build-debug --parallel
```

## 7. Clang 构建

```bash
cmake -S . -B build-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++

cmake --build build-clang --parallel
```

## 8. 更新到 Atom 锁定的依赖版本

切换分支或拉取更新后执行：

```bash
git pull
git submodule sync
git submodule update --init
```

不要在普通构建流程中执行：

```bash
git submodule update --remote
```

`--remote` 会尝试跟随上游分支，而不是使用 Atom 已验证的 commit。