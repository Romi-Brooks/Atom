# Homeless

Atom 的冬夜像素风散步示例，展示 Renderer2D、相机、逐帧动画、粒子、VFS 和音频。

```sh
cmake --build build --target Example_Homeless
cmake --build build --target Example_Homeless_Package
```

在 Atom 根目录配置构建。CLion 里选 `Example_Homeless` 运行，程序和资源位于
`build/AtomExample/Homeless/`。默认从 `Content/` 读取资源。加 `--package` 使用
`Packages/Homeless.apkg`。`--vulkan` 选择 Vulkan，`--frames=120` 渲染指定帧数后退出。
Windows 下普通 Release 使用图形子系统，启动时没有控制台窗口。启用
`-DATOM_ENABLE_DEBUGGER=ON` 的 Release 保留控制台。

| 操作 | 按键 |
| --- | --- |
| 移动 | A / D 或左右方向键 |
| 查看街道入口 | 靠近小吃摊、小卖部或网吧时按 E |
| 暂停、设置、退出 | Esc 或 Space |
| 环境音静音 | M |
| 全屏切换 | F11 |
| 调试面板 | F3，需启用 `ATOM_ENABLE_DEBUGGER` |

默认 1120 × 630 窗口、无限帧率、Immediate、3 帧在途。
将歌曲放入 `Homeless.exe` 旁的 `Music/` 文件夹，启动时按文件名排序扫描这一层，
识别已注册解码器支持的格式（默认包括 MP3 和 WAV）。右下角播放器支持播放、暂停、
上一首和下一首，播完会继续下一首。重新启动可刷新目录。默认音量 70%，手动开始播放。

从大桥一路向右，走到尽头会进入桥头街。沿街向左走到头即可回到大桥。
小吃摊、小卖部和网吧目前只提供门口提示。大桥和桥头街都可遇见左右行走的路人，
路人的衣着和配色会变化，也会遇见并肩散步的两人。

Job System 在后台处理音乐目录扫描、流式音频打开与控制、标签和封面处理，
以及场景音效的解码和播放控制。渲染线程接收已完成的结果并上传纹理。
小规模的行人、相机、动画和粒子共用固定步长更新。场景纹理、水域遮罩和字体
在启动时准备，进入游戏后切歌和换场无需等待资源工作。

`Source/Homeless.cpp` 负责启动，`Source/Scene/` 负责场景、角色、天气和车辆，
`Source/UI/` 负责播放器和菜单，`Source/Content/` 挂载资源，`Source/Debug/` 装配调试面板。
`Content/` 存放运行资源，`SourceArt/` 与 `Tools/` 存放原图和生成脚本。
大桥与桥头街全景共用道路的高度与车道位置。更换大桥全景时保持水域遮罩同尺寸并逐像素对齐。
桥头街的生成原图位于 `SourceArt/BridgeStreet.png`，运行纹理由 `Tools/BuildScrollingAssets.py` 生成。
车辆通过声来自
[Breviceps 的 CC0 录音](https://freesound.org/people/Breviceps/sounds/462862/)。

<details>
<summary>桥头街 ImageGen 提示词</summary>

> Use case: stylized-concept. Create the actual game environment texture for Homeless, an original circa-2001 Chinese winter-night pixel-art side-scroller. The supplied WaterfrontPanorama.png is a style reference only. Continue immediately to its right at the landward end of the bridge, on a local neighborhood high street. Same strict level, eye-height, front-on side-view elevation, same deep violet sky, cold blue snow, brickwork, cyan CRT/fluorescent lighting, and amber tungsten windows. Relaxed and lived-in. Draw chunky carefully clustered pixel art with hard edges and matching asset scale. Ultra-wide single continuous panorama, approximately 3:1 aspect ratio. Keep the bottom 25% an uninterrupted flat straight asphalt roadway with dashed lane markings and an exactly horizontal curb, and a continuous walkable sidewalk above. Ground line where storefronts meet the sidewalk at 65% image height. Keep roofs and skyline in the top 65%. Nothing across the sidewalk in the lower central section. On the far left 12%, a short stretch of bridge railing and a warm iron lamp with the cold dark river city in background, leading gently into low-rise street facades. Broad compositional beats across the width: at about x=30% a small yellow-lit street food stall under an awning with an open customer counter; at about x=53% an amber-green corner shop with a glass entrance, visible shelves, and a modest sign that reads “小卖部”; at about x=77% a cyan and muted magenta internet café, with a clearly framed flat-front entrance and a modest exact sign “网吧”, faint warm CRT monitors through its windows. Use modest Chinese turn-of-the-millennium visual details like tile-front buildings, exposed wiring, wall AC units and enamel signs, weathered materials. Screen fills edge to edge. No people, cars, gameplay UI, lettering beyond the period shop signs, readable Latin text, brand logos, snow particles, vanishing point, street receding into depth, or camera tilt.

</details>

<details>
<summary>行人 ImageGen 提示词</summary>

> Use case: stylized-concept. Production asset for Homeless, a low-resolution original Chinese winter-night sidewalk game set in 2001. The attached Walker.png is a style and anatomy reference only, not a character to copy. Generate a transparent sprite sheet, 4 columns by 4 rows with exactly 16 evenly spaced equal square cells, no gridlines. Exactly one whole character per cell, all strict side profiles facing RIGHT, at identical visual scale and fixed foot baseline within each cell, clear surrounding transparent gutter. Each row is one distinct character repeated through a four-frame walk loop, keep the head, hair, jacket, and palette visually identical across each row, alter the boots and legs. Column 1 rear foot back and front extended, column 2 passing feet close together, column 3 other foot leading and rear extended, column 4 opposite passing feet close together. Row 1 short-haired man in a teal blue short puffer and charcoal knit hat. Row 2 man in a muted ochre padded work jacket, side-parted dark hair, dark trousers. Row 3 woman with a dark ponytail, plum mid-length winter coat and scarf, boots. Row 4 woman with short dark hair, mint sage padded jacket and dark trousers. Natural neighborhood pedestrians, age adult. Same compact blocky proportions as reference, game-scale palette-clustered true pixel art: each sprite is intended to read at roughly 24 by 42 active pixels. One crisp pixel edge, highly simplified faces, cool navy outlines, warmer reflected light on the face. No shadows under feet, captions, numbers, guides, scenery, white backing, gradients, illustration-style anti-aliasing, or decorative outlines. Canvas exactly square, all transparent outside the 16 silhouettes.

</details>
