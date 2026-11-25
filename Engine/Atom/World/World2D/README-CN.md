# World2D：通用 2D 世界能力

`Atom_World2D` 不规定玩法类型。当前版本只实现正交网格；投影类型是显式枚举，后续投影需要分别实现坐标映射、裁剪和碰撞策略。

## Tilemap 与对象层

- `Tilemap` 按 `(layer, chunk_x, chunk_y)` 稀疏保存格子，支持负坐标、按视口枚举和 `RemoveChunk`。Tile ID 0 是空格。
- Tile 定义将图集源矩形、纹理 URI、过滤模式和 solid 标记与地图格子分离。同一 Tile 定义可被多个层和分块引用。
- `TilemapLoader` 从 VFS 读取 JSONC。首版格式见 [Tilemap 格式](../../../Docs/Plans/Specs/Tilemap-Schema-CN.md)。
- 对象层记录全地图唯一的 stable ID、prefab URI、位置与旋转。`InstantiateObjects` 接受游戏侧 prefab 工厂，将位置写入根实体，返回卸载句柄。它不会规定 RPG、平台或射击游戏的 prefab 内容。
- `BuildColliders` 在各分块内合并相邻 solid 格子。`CreateTileColliders` 将矩形转换成静态 ECS 物理壳；`DestroyChunkColliders` 支持按层和分块卸载。创建碰撞实体后，仍须由现有 `Physics2DWorld::Step` 同步到物理后端。

## 表现系统

- `AnimationLibrary2D` 管理 clip、状态名与触发转移表，`AnimationSystem2D::Step` 在固定步推进 `Animator2D`，写入 Sprite 源矩形并返回帧事件。游戏在同一步将事件加入 `GameEventQueue`。游戏规则通过 `Trigger` 或 `SetState` 切换状态，通用系统不内置玩法条件。
- `CameraSystem2D` 在 LateUpdate 处理目标跟随、平滑、死区、边界、震动与 `PanTo` 转场。视差因子位于 Tile 图层。
- `ParticleSystem2D` 使用 ECS `ParticleEmitter2D` 作为发射器配置，活粒子保存在有容量上限的连续数组。它支持一次性 Burst、固定步更新和场景代际变化时清空。
- `World2DRenderer` 按 layer、sort key 与类型稳定排序 Tile、Sprite、Particle，提交同一世界绘制 pass。Tile 枚举按相机视口裁剪。

## 当前边界

同步分块增删和碰撞卸载已具备底层接口；世界清单、跨场景 `WorldSession`、资源预算与异步流送尚未实现。动画 clip 与粒子 preset 当前由代码构造，尚无独立资源格式或编辑器。Tilemap 对象层由游戏的 prefab 工厂实例化；示例仅展示数据结构，尚未在关卡中放置对象层实体。
