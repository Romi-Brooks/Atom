# Atom 物理系统设计（Physics Plan）

> 状态：物理系统的唯一架构与落地规划文档。M1~M6（形状/契约/刚体/宽相位/窄相位/接触求解/固定步+事件+休眠/revolute 关节）已实现，核心刚体物理模块完整，后续按需推进 M7（可选 Box2D 适配器）。
> 更新日期：2026-10-21

## 1. 决策总览

| 决策点 | 结论 |
|---|---|
| 自研 vs 引入库 | **契约先行 + 双后端**：基础自研（Internal 后端），高计算量/关节/CCD 引入 Box2D 作为可替换后端 |
| 与 ECS 关系 | **独立 world + 组件外壳**：物理世界自治，ECS 只放轻量 Collider/RigidBody 外壳组件，系统同步两边 transform |
| 维度 | **2D 先行，接口预留 3D**：契约层抽象维度，只实现 2D 后端 |
| 单位制 | **米制（MKS）+ y 向上**；与渲染（像素 + y 向下）由 PPM 换算 |
| 确定性 | fixed dt + 稳定遍历顺序；**预留**确定性能力，不实现定点数 |

### 为什么是"契约 + 双后端"而不是二选一

1. 引擎已有 Backend + Registry + 热切换的成熟骨架（Audio / Render），物理走同构路径成本最低、风格零违和。
2. ECS 已有「公开头不出现第三方类型」的边界规则——这几乎强制要求物理库只能藏在 backend 的 `.cpp` 内部，后端可替换是唯一能同时满足该规则 + 引库的架构。
3. 物理系统的难点在**约束求解器**与 **CCD**，不在碰撞检测。碰撞检测（凸形状 SAT/GJK）是可教学、可测试的纯函数，适合自研；求解器是数值深水区，适合在需要时引入成熟库。
4. Box2D（Erin Catto，MIT）是 2D 刚体事实标准，自研时参考其 GDC 论文而非闭门造车。

**换库 = 换一个 `IPhysicsBackend` 实现。** 游戏代码只依赖 `atom::physics::IPhysicsWorld` / `IPhysicsBody`，永远 include 不到任何第三方头。Box2D 只是契约的第一个可选实现，不是依赖。

## 2. 分层架构

```
PhysicsSystem（游戏侧唯一入口，尚未实现）
  ├─ Shapes/        形状（圆/凸多边形/线段）+ 材质 + 质量属性   ← M1 已实现
  ├─ Dynamics/      刚体（Static/Kinematic/Dynamic）+ 半隐式积分   ← M2
  ├─ Collision/     宽相位（网格→AABB Tree）+ 窄相位（SAT/GJK）    ← M2/M3
  ├─ Solver/        冲量求解 + 约束/关节 + 位置修正               ← M4/M6
  └─ PhysicsWorld/  固定步 + 查询/射线 + 碰撞事件 + 休眠           ← M5

Backend/（物理的后端层与引擎其余后端同树，对齐音频分层）
  ├─ Contracts/Physics/  IPhysics* 契约 + PhysicsTypes + PhysicsBackendId   ← M1
  └─ Physics/Internal/   自研 Internal 后端（Box2D 作为未来并列后端）
```

## 3. 单位制与坐标约定

- **物理内部**：米（MKS）+ y 向上（重力 `(0, -9.8)`），角度弧度。
- **渲染**：像素 + y 向下（Renderer2D 约定）。
- **换算**：游戏侧通过 `PPM`（pixels per meter，建议 32~100）做一次集中换算：

  ```text
  physics.x = render.x / PPM
  physics.y = -render.y / PPM
  ```

- 换算集中在游戏侧的同步层，物理模块**永不**出现像素或 y 翻转。这样数值健康（米制），且换渲染分辨率不影响物理行为。

## 4. 契约 API 摘要（已实现）

见 `Physics/Shapes/Shape.hpp` 与 `Backend/Contracts/Physics/`：

- `Shape::MakeCircle/MakePolygon/MakeSegment` → `Result<Shape>`（非法输入不静默截断）
- `ComputeArea/Centroid/Aabb/MassProperties`（纯函数）
- `IPhysicsBody`：类型、transform、速度、力/冲量/力矩
- `IPhysicsWorld`：`CreateBody` / `Step(dt, vel_iters, pos_iters)` / 重力 / `IContactListener`
- `IPhysicsBackend`：`CreateWorld()` / `Name()`

## 5. 里程碑

| 里程碑 | 内容 | 验收标准 | 状态 |
|---|---|---|---|
| M1 | Shapes + 质量属性 + 契约层 | 形状几何/惯性单测通过 | ✅ 已实现 |
| M2 | 刚体积分 + 宽相位（均匀网格，预留 AABB Tree） | 自由落体/抛体确定性回放一致 | ✅ 已实现 |
| M3 | 窄相位 SAT + 接触流形 | 各形状组合相交/分离用例全过 | ✅ 已实现 |
| M4 | Sequential Impulse 接触求解 + 位置修正 | 堆叠稳定、能量不爆炸、迭代数可调 | ✅ 已实现 |
| M5 | PhysicsWorld 固定步 + 事件 + sleeping | 静态堆叠休眠后零耗时 | ✅ 已实现 |
| M6 | revolute 关节 | 用约束架构实现，证明可拓展 | ✅ 已实现 |
| M7（可选） | Box2D 适配器 + 等价性回归 | 同一批测试两端跑通，验证契约够抽象 | |

## 6. 工业化稳定性要点

1. **确定性**：fixed dt + 稳定遍历顺序（ECS 已有此性质）+ 禁平台相关浮点假设。联机/回放都要它，现在定调最便宜。
2. **Sleeping 第一期就设计**：大量静态堆叠若每帧白算，性能崩且求解器"沸腾"。
3. **速度/位置迭代分离**：调稳定性的两个旋钮，`Step` 签名已预留。
4. **凸性/退化校验在工厂层**：`MakePolygon` 拒绝非凸/退化，而不是让求解器去撞 UB。

## 7. 扩展点与预留

- **复合形状（多 fixture）**：`BodyDef` 目前单形状，追加 `std::vector<Shape>` 即可，不改 `IPhysicsWorld`。
- **关节**：约束求解器架构下新增关节 = 加约束类型，不改框架（M6 验证）。
- **角色控制器（character controller）**：kinematic body + 特殊求解，后续里程碑。
- **确定性/定点**：fixed dt 已铺路，需要时在积分层替换数值类型。
- **新后端**：实现 `IPhysicsBackend` + 在 `PhysicsBackendId` 追加枚举。
