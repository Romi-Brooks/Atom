# Atom ECS 组件词汇准入规则（A0.1）

> 状态：**冻结**。A0.1 的交付物；T1.1 起所有新增 ECS 组件必须遵循本文。
> 更新日期：2026-10-25
> 关联：`Docs/Plans/TaskList-CN.md` 的 A0.1 与 G0.2。

## 1. 为什么需要一份准入规则

`EntityWorld` 的实例化由 `ECS/Components.hpp` 的单一宏列表驱动，而组件类型是**公开头文件的一部分**。因此往
列表里加一个类型，等于给 `Atom_ECS` 的每个消费者加一条编译期和链接期依赖。如果把 `SpriteRenderer` 写成
持有 `Renderer2D::Texture*` 的结构，`Atom_ECS` 就会反向依赖 Render，形成 ECS ↔ Render 的循环，随后任何一
个"很小的"游戏能力都会扩大耦合。

本文回答两个问题：

1. **哪些类型可以进入 `ATOM_ECS_COMPONENTS`**（第 2–4 节）；
2. **查询组合如何增长**（第 5 节，G0.2）。

## 2. 组件准入的五条硬规则

一个类型可以进入 `ATOM_ECS_COMPONENTS`，当且仅当同时满足：

| 规则 | 要求 | 反例（禁止） |
|---|---|---|
| R1 纯数据 | 无虚函数、无行为、无 world 反向指针。行为属于 System。 | `struct Mover { virtual void Tick() = 0; }` |
| R2 无资源句柄 | 只持有值、稳定 id、字符串或纯算法类型。 | `TextureHandle`、`Renderer2D::Texture*`、`GpuBuffer` |
| R3 无平台/第三方类型 | 不出现 SDL、SDL_GPU、EnTT、物理 backend 指针。 | `SDL_Texture*`、`b2Body*`、`InternalPhysicsBody*` |
| R4 无高层模块类型 | 不出现 `AssetPath`、`ResourceId`、`IFile`、Loader、Renderer 类型。 | `asset::ResourceHandle<Texture>` |
| R5 可默认构造且可移动 | 列表驱动的实例化要求默认构造 + 移动构造/赋值 + 析构。 | 只可拷贝的 RAII 类型 |

`algo::Vec2`、`algo::Rect`、`atom::color::Color` 这类纯值类型是允许的：它们来自 `Atom_Math` / `Atom_Color`
两个叶子 target，不引入任何子系统依赖。

**资源引用一律用稳定的字符串 key 或 URI**（例如 `texture_uri`），由 Scene2D/Scene loader 之类的系统在
渲染前解析成真实句柄。ECS 永远不知道句柄长什么样。

## 3. 依赖方向（可机械校验）

```text
Atom_Math / Atom_Color     纯值类型（叶子）
      ↑
Atom_Error / Atom_Log      错误与日志（Atom_Error 不依赖 SDL）
      ↑
Atom_ECS                   组件词汇 + 存储 + 查询
      ↑
Render/Scene2D、Physics/Integration、Scene      消费 ECS，ECS 永不反向依赖
```

验收判据（两条都要成立）：

1. `Atom_ECS` 的 CMake 依赖只有 `Atom_Error`、`Atom_Log`（PUBLIC）与 `Atom_Math`（PUBLIC，因为公开头文件
   include `Algorithm/Vector/Vec2.hpp`），以及 `Atom_3rd_EnTT`（PRIVATE，backend 不得出现在公开头文件）。
2. `ECS/` 下公开头文件的 `#include` 只允许标准库与上述基础模块；不得出现 `Render/`、`Asset/`、`Physics/`、
   `Media/`、`Runtime/`、`Backend/`。

机械校验方法：

```powershell
Get-ChildItem ECS -File | Select-String -Pattern '^#include <(Render|Asset|Physics|Media|Runtime|Backend)/'
# 期望：无输出
```

## 4. 现有组件与它们的存在理由

| 组件 | 类别 | 说明 |
|---|---|---|
| `Position` / `Velocity` / `Rotation` / `Scale` | 通用变换 | P0 遗留的分量表示；T1.1 的 `Transform2D` 与之并存，新代码优先用 `Transform2D` |
| `Name` | 身份 | 可选展示名；`std::string` 只在这里按需付出 |
| `Health` | **测试夹具** | 仅被 `Test/ECS/EntityWorldTest.cpp` 用作"任意值组件"样例。它不是引擎的游戏规则，玩法不得建立其上；T1.1 之后若要留一个样例组件，优先用它而不是再引入一个 |
| `Active` / `Static` / `PendingDestroy` | 标签 | 空类型，presence 即数据；禁止 `Patch()` |

**不得**为了某个玩法把 `Health`/`Inventory`/`Quest` 之类扩成引擎规则（见 `../Subsystem-Plans-CN.md`
"游戏项目决定规则"）。玩法组件属于游戏项目自己的组件层，等组件注册机制扩展后再谈。

## 5. 查询实例化策略（G0.2）

`EntityWorld` 的迭代按**组件集合**实例化，不是按单个组件：每一个新的 `With<A,B>()` / `ForEach<A,B>()`
调用点都需要 `ECS/EntityWorld.cpp` 里的一行显式实例化。这带来一笔持续的"税"，但换来三件事：公开头文件里
没有 backend 模板、编译期可知全部迭代组合、没有运行时注册。

P0 的决定：**接受逐组合显式实例化，不做类型擦除、不做动态注册。** 配套规则：

1. 全部实例化集中在 `ECS/EntityWorld.cpp` 的同一个区块（`ATOM_ECS_INSTANTIATE*` 宏之后），不得分散到其他
   `.cpp`。新增查询 = **在该区块增加一行**，不改任何头文件。
2. 每个组合必须真有系统在用。没有调用点的组合是纯编译期负担，评审时删掉。
3. 新增组合时同时补一条 headless 测试（`Test/ECS/`），断言该组合能取到预期实体集合——这是唯一能在 CI 里
   发现"忘了实例化"的地方。
4. 若组合数量超过约 20 个，再单独评估类型擦除式迭代；不得在 T1.1/T1.2/T3.1 期间顺手改掉实例化机制。

当前已实例化的组合（`EntityWorld.cpp`）：

| 组合 | 用途 |
|---|---|
| `Position` | 单组件遍历 |
| `Position, Velocity` | 运动积分 |
| `PendingDestroy`（仅收集，无 `ForEach`） | 延迟销毁清扫 |

## 6. 新增一个组件的完整步骤

1. 在 `ECS/Components.hpp` 里定义类型（只用允许的值类型）。
2. 把它加进 `ATOM_ECS_COMPONENTS` 宏列表。
3. 在 `ECS/EntityWorld.cpp` 里加一行 `ATOM_ECS_INSTANTIATE(Type)`（值组件）或
   `ATOM_ECS_INSTANTIATE_TAG(Type)`（空标签）。
4. 若有系统要遍历它，在同一区块加 `ATOM_ECS_INSTANTIATE_ITERATION(Type)`，并在 `Test/ECS/` 补测试。
5. 在本文第 4 节登记它的类别与存在理由。

跳过任何一步的后果分别是：编译失败（2/3）、链接失败（4）、或者一个没人校验的隐藏组合（4 的测试）。
