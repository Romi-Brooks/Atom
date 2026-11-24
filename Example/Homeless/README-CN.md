# Homeless

Atom 的冬夜像素风散步示例，展示 Renderer2D、相机、逐帧动画、粒子、VFS 和音频。

```sh
cmake --build build --target Example_Homeless
cmake --build build --target Example_Homeless_Package
```

在 Atom 根目录配置构建。CLion 里选 `Example_Homeless` 运行，程序和资源位于
`build/Example/Homeless/Game/`。默认从 `Content/` 读取资源。加 `--package` 使用
`Packages/Homeless.apkg`。`--vulkan` 选择 Vulkan，`--frames=120` 渲染指定帧数后退出。
Windows 下普通 Release 使用图形子系统，启动时没有控制台窗口。启用
`-DATOM_ENABLE_DEBUGGER=ON` 的 Release 保留控制台。

| 操作 | 按键 |
| --- | --- |
| 移动 | A / D 或左右方向键 |
| 暂停、设置、退出 | Esc 或 Space |
| 环境音静音 | M |
| 全屏切换 | F11 |
| 调试面板 | F3，需启用 `ATOM_ENABLE_DEBUGGER` |

默认 1120 × 630 窗口、无限帧率、Immediate、3 帧在途。右下角播放器可点击播放或暂停。
本地音乐可通过 `HOMELESS_MUSIC_FILE` 指定，也可放在运行目录的
`Content/Audio/July - 바람에 쓰는 편지.mp3`。默认音量 70%，手动开始播放。

`Source/Homeless.cpp` 负责启动，`Source/Scene/` 负责场景、角色、天气和车辆，
`Source/UI/` 负责播放器和菜单，`Source/Content/` 挂载资源，`Source/Debug/` 装配调试面板。
`Content/` 存放运行资源，`SourceArt/` 与 `Tools/` 存放原图和生成脚本。
更换全景时保持水域遮罩同尺寸并逐像素对齐。车辆通过声来自
[Breviceps 的 CC0 录音](https://freesound.org/people/Breviceps/sounds/462862/)。
