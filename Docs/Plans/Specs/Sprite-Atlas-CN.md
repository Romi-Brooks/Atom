# Atom Sprite Atlas v1（美术管线第一层）

> 状态：**已冻结**；实现见 `Sprite/`（`Atom_Sprite`）与场景 schema 的 `atlas` 形态。
> 更新日期：2026-10-29
> 关联：`Scene-Schema-CN.md` §4.2、`../TaskList-CN.md` 的 T2.4 / G0.3。

## 1. 这是什么，为什么单独一层

一张精灵表（spritesheet）被切成很多小图。没有元数据时，每个用它的实体都要手写三样东西：纹理里的矩形、
屏幕上的尺寸、轴心。这就是"美术给你一张 8×8 的图集，你手动数像素"。

这一层解决的就是这个：**让美术文件拥有这些数字，让实体只说"绿格子"**。

它不是动画（帧序列、状态机，属 T5.3），也不是导入器（PNG 解码已有），只做"命名 + 定位 + 采样方式"。

## 2. 文件格式

```jsonc
{
  "schema_version": 1,                         // 必填，必须等于 1
  "kind": "atlas",                             // 必填，必须是 "atlas"
  "id": "terrain",                             // 必填，[a-z0-9_-]*，≤ 64
  "texture": "res://textures/atlas.png",       // 必填，res:// URI
  "filter": "nearest",                         // 可选，默认 "linear"
  "frames": {                                  // 必填，非空对象
    "green":  { "rect": [8, 0, 8, 8], "size": [64.0, 64.0] },
    "yellow": { "rect": [8, 8, 8, 8], "size": [64.0, 64.0], "pivot": [0.5, 1.0] }
  }
}
```

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `frames.<name>.rect` | `[x, y, w, h]` | **必填** | 纹理内的 texel 矩形，`w`/`h` 必须 > 0 |
| `frames.<name>.size` | `[w, h]` | 等于 `rect` 的尺寸 | 屏幕像素尺寸；1 texel = 1 px 是默认，不是规则 |
| `frames.<name>.pivot` | `[nx, ny]` | `[0.5, 0.5]` | 归一化轴心 |
| `filter` | `"linear"` \| `"nearest"` | `"linear"` | 整张表共用一种采样 |

**帧名唯一**：`frames` 是对象，键重复会被 JSONC 读取器直接拒绝（`duplicate key "a" (first seen at 3:5)`），
所以不需要额外写一遍去重。

**`size` 与 `pivot` 属于帧，不属于实体**：这样同一个格子不会在关卡里被画成三种尺寸。想换个尺寸？改图集，
或者复制一个新帧。

## 3. 为什么 `filter` 在 atlas 上，而实现是逐次绘制

采样方式是**纹理属性**，不是单个精灵的偏好：一张像素风表被线性和最近邻混用，只会让其中一半难看。所以它写在
图集里，一个文件一个决定。

但渲染器的批处理是按 (texture, sampler, clip) 合并的，所以它最终还是逐次绘制的参数。链路是：

```text
atlas.filter --解析--> ecs::SpriteFilter --每帧--> SpriteDraw.filter --> Renderer2D::DrawTexture(..., filter)
                    （Scene 负责两边的映射）              （Scene2D 携带）      （Renderer2D 选 sampler）
```

三个类型是同一个概念的三次表示，各自在自己的层里：`sprite::AtlasFilter`（资产）、`ecs::SpriteFilter`
（组件，不能 include 渲染契约）、`render::Filter2D`（渲染契约）。唯一的映射点在 Scene loader 与 Scene2D，
各自一行。**不要为了消掉重复而让 ECS 依赖渲染契约**——那会破坏 A0.1 的边界。

## 4. 场景里怎么用

Sprite 块有两种**互斥**写法：

```jsonc
// (a) 直接指定纹理
{ "texture": "res://textures/player.png", "size": [32, 32], "pivot": [0.5, 0.5] }

// (b) 指定图集帧；rect / size / pivot / filter 都来自图集
{ "atlas": "res://atlases/terrain.atlas.json", "frame": "green", "layer": 0 }
```

两种形式下都能写的是**每次实例的艺术选择**：`tint`、`layer`、`sort_key`、`flip_x`、`flip_y`。
在 (b) 里再写 `texture`/`source`/`size`/`pivot` 是错误——那会让同一个数字有两个来源，正是图集要消灭的东西。

## 5. 错误与诊断

沿用 `Utilities/Json/Diagnostic` 的一行格式：

```text
res://scenes/x.scene.json:5:20: entity 5 ("atlas_cell"): sprite.size: "sprite.size" is defined by the atlas frame; remove it or drop "atlas"/"frame"
res://scenes/x.scene.json:5:20: entity 5: sprite.frame: atlas "res://atlases/terrain.atlas.json" has no frame "gren" (available: red, green, blue, yellow)
res://atlases/terrain.atlas.json:12:4: frames.a.rec: unknown field "rec" (did you mean "rect"?)
res://atlases/terrain.atlas.json:8:13: filter: unknown filter "point" (allowed: "linear", "nearest")
```

缺失帧会**列出可用的帧名**：`"gren"` → `available: red, green, blue, yellow`，这是一眼能改的反馈。

## 6. 与其他部分的关系

| 关注点 | 谁负责 |
|---|---|
| 一张表里有哪些帧、每帧矩形/尺寸/轴心/采样 | 本文（`Atom_Sprite`） |
| 纹理解码与 GPU 上传 | `Render/Resources`（已有） |
| 收集/排序/提交绘制 | `Render/Scene2D`（T1.2） |
| 场景实体怎么引用帧 | `Scene/SceneSchema`（T2.2/T2.4） |
| 帧序列、状态机、帧事件 | T5.3（动画），**不在本层** |
| 图集打包（把散图合成大图） | 工具，未排期 |

## 7. 已知限制

- **没有旋转精灵**：`Renderer2D` 没有旋转视图/旋转四边形，仍属 G0.3 的遗留项；
- **没有九宫格（9-slice）**：UI 面板需要它，属于 `UI/` 的未来需求，不塞进 atlas；
- **没有 atlas 内纹理合并/打包工具**：现在需要美术自己产出大图；
- **缓存是每次加载一份**：改图集要重新加载场景才生效（这是有意的，见 `SpriteAtlasLoader.hpp`）。

## 8. 测试

| 测试 | 覆盖 |
|---|---|
| `Atom_Sprite.Atlas` | 解析、默认值、帧名顺序、`filter` 词表、全部拒绝规则、**随仓库发布的图集**、缓存同一实例 |
| `Atom_Scene.SceneAtlas` | 帧解析进 SpriteDesc、两种形式互斥、缺帧列出可用名、缺 atlas/provider、prefab 里用帧 |
| `Atom_Scene2D.SpriteRenderer` | `SpriteFilter::Nearest` 一路传到 `SpriteDraw.filter`，其余精灵仍是 Linear |
