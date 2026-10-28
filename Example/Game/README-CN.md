# Example/Game — 2D smoke scene (T1.3)

引擎里第一个**真正持有 `GameWorld` 的 Screen**：硬编码世界、真实 `res://` 贴图、键盘驱动角色、跟随相机。
它是 T2.2 的替换目标——`SeedSmokeScene` 就是将来被 Scene loader 取代的那个函数。

## 运行

```powershell
cmake --build cmake-build-debug --target Example_2D_Smoke
.\cmake-build-debug\Example_2D_Smoke.exe
```

- WASD / 方向键：移动
- Space：暂停世界（画面继续渲染，所以"暂停"是可见的）
- 关闭窗口：退出

![smoke scene](smoke-scene.png)

## 它验证了什么

| 环节 | 落点 |
|---|---|
| `GameScreen` 拥有 `GameWorld`，按正确顺序跑一帧 | `Runtime/GameScreen.*`（G0.1 决策写在头文件顶部） |
| 帧节奏：宿主 FixedUpdate 0..N + Update 1 → 世界 `Input → FixedUpdate×N → Update → LateUpdate` | `Atom_Runtime.GameScreenFrame` |
| 组件词汇：变换 / 显示副本 / 精灵 / 相机 / Lifetime / 场景 id | `ECS/Components.hpp`（T1.1） |
| `res://` → 纹理句柄 → 精灵排序 → 一次绘制 | `Render/Scene2D/`（T1.2） |
| 内容根与开发期挂载约定 | `Assets/` + `SmokeMain.cpp` |

## 内容根（G0.5）

`SmokeMain.cpp` 启动时把 `ATOM_SOURCE_ROOT/Assets` 以 priority 200 挂到 `res://`，所以
`SpriteRenderer::texture_uri` 里的 `res://textures/player.png` 是真实 VFS 查找，不是构造期路径。

美术是占位图，由 `Assets/generate_textures.py` 生成（Pillow）：

```powershell
python Assets\generate_textures.py
```

发布期同一批 URI 改由 APKG 提供，`SmokeScreen` 与 `Render/Scene2D` 都不用改。

## 刻意不做

- 没有 Scene loader（T2.2 才做），所以"贴图不显示"不可能是 schema 的问题；
- 没有物理与碰撞（T3.1），移动是直接写 `Transform2D`；
- 没有 HUD 与重开流程（T3.3），暂停只切换世界的推进；
- 不写 `Atom.cpp`：示例是独立可执行文件，引擎入口保持干净。
