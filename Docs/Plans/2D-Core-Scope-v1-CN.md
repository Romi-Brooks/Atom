# Atom 2D Core Scope v1

> 状态：已冻结 P0 范围；以 Windows/Linux 桌面端的可测试游戏世界底座为目标。
>
> 本文不定义任何具体游戏玩法。游戏项目决定经营、RPG、横版、动作或解谜等规则；Atom 提供可组合的 2D 世界能力。

## 目标与非目标

P0 支持 Windows 与 Linux、键盘鼠标、SDL3/SDL_GPU 后端，以及由一个 `GameWorld` 驱动的小型可玩场景。资源使用 VFS URI（例如 `res://`），游戏逻辑不得依赖平台路径或固定像素尺寸。

P0 不实现手柄、联网、编辑器、无缝大世界流送、动画、粒子、Tilemap、导航、存档或任意脚本对象序列化。它们必须建立在已验证的世界生命周期上。

## 职责边界

```text
Screen       菜单、暂停、加载等呈现导航
GameWorld    一个运行中的 ECS world、固定顺序系统、命令和事件
Scene        一个可序列化、可加载的世界片段（下一阶段）
Prefab       可复用实体模板（Scene 之后）
WorldSession 多个 Scene/WorldChunk 的生命周期与流送（大世界阶段）
```

`Screen` 不保存可序列化游戏实体；游戏 `Screen` 可以拥有或展示一个 `GameWorld`。`Scene` 也不等于开放世界：它既可表示单个小关卡，也可在后续作为 `WorldChunk` 的内容。

## P0 帧与提交顺序

```text
Input -> commit -> FixedUpdate × N -> commit -> Update -> commit -> LateUpdate -> commit
```

系统按注册顺序执行。任何创建、销毁和组件结构变化都通过 `EntityCommandBuffer` 在阶段边界提交；阶段内不得改变 ECS 结构。游戏事件以 FIFO 方式收集，由消费该事件的系统在其所属阶段显式 drain，Runtime 不隐式调用游戏回调。

## 后续地图路线

1. `Scene`：版本化 schema、`res://` 资源 URI、临时 world 加载后原子切换。
2. Tilemap：正交 tile 层、对象层、图块碰撞；对象层保留自由摆放实体能力。
3. `WorldManifest`、`WorldSession` 与 `WorldChunk`：先同步切块，再增加异步流送、预算和跨 chunk 持久状态。

这条路线优先覆盖俯视角的大世界表达，同时不把横版游戏排除在外。单向平台、coyote time、种田、背包、NPC 日程等属于具体游戏扩展，而非 P0 引擎规则。
