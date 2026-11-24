# Render/Scene2D

把 ECS 里的 2D 精灵与相机变成一个 `Renderer2D` 绘制帧。**不含**世界、主循环、帧节奏，也不含物理。
它依赖 `Atom_ECS`、`Atom_Renderer2D`、`Atom_Render_Resources`，且**不依赖 `Atom_Runtime`**——工具与测试可以
在没有帧循环的情况下用它。

## 四个步骤，一条分工线

```text
SnapshotTransforms   Transform2D      -> RenderTransform2D    （每个固定步一次，在步进之前）
FindActiveCamera     Transform2D+Camera2D (+alpha) -> CameraView
Collect              RenderTransform2D+SpriteRenderer (+alpha) -> SpriteDraw[]
Submit               SpriteDraw[] -> Renderer2D                （唯一触碰 GPU 的一步，不做决策）
```

`SnapshotTransforms` 必须注册成 **FixedUpdate 阶段的第一个系统**，不能每帧调一次。这样
`RenderTransform2D` 天然就是"上一个固定步的状态"，而 `Transform2D` 是"这一步的状态"，`Collect(alpha)` 才能在
两者之间插值。每帧调一次只会把最新状态复制过去，等于没插值。

### 插值（为什么需要，怎么算）

逻辑固定 60 步/秒，屏幕可能 144Hz 或忽快忽慢。直接把最新状态画出来会"一顿一顿"。标准解法是保留上一步与
这一步，按 `alpha`（时钟剩余累加器 / 固定步，∈[0,1)）混合：

```text
display = lerp(RenderTransform2D, Transform2D, alpha)
```

`alpha` 由 `Application` 从 `DomainClock::Alpha()` 取出，通过 `Screen::BeginFrame(steps, fixed_dt, alpha)` 发布，
`GameScreen::InterpolationAlpha()` 保存并**钳制**到 [0, 0.999]（暂停或时钟未推进时可能越界，用越界值插值等于外推）。

**读 `Transform2D` 不违反"渲染不写模拟"**：混合结果只写进 draw item，两个组件都不被修改。反过来把混合值写回
`RenderTransform2D` 是错的——那会毁掉上一步状态，让下一帧从"已经插值过"的值继续插值，表现为逐渐滞后。

相机用同一个 `alpha` 混合，否则精灵平滑而相机跳步，整个世界会抖。

## 相机映射（G0.3）

`Renderer2D` 把世界映射到屏幕的规则是 `screen = (world - origin) * zoom`。把 `viewport/2` 对到相机中心，
解出唯一转换：

```text
origin = center - viewport / (2 * zoom)
```

`CameraView::Origin()` 是唯一的换算点。**P0 不支持 `Camera2D::rotation`**：`Renderer2D` 没有旋转视图变换，
所以这个字段被明确忽略，而不是悄悄生效一半。

## 排序（G0.3）

`Collect` 按 `(layer, sort_key, entity 索引)` 排序。`Renderer2D` 内部只按 layer 做 `stable_sort`，层内顺序
等于提交顺序，所以提交前必须排好——`Submit` 不会 PushLayer，也不会重排。

## 纹理解析

`ITextureResolver` 是唯一的 GPU 接缝：`Collect` 只经它要 `Renderer2D::Texture*`，因此整体可以 headless 测试。
`TextureCacheResolver` 是生产实现：

- 命中走一次哈希，缺失也记一次哈希——不会每帧重新打开 VFS 或重复解码；
- 自己持有 `TextureHandle`，所以纹理在还能被绘制期间不会被回收；析构后由 `TextureCache` 在帧边界延迟销毁。

## 诊断

`Scene2DRenderer::Stats()` 区分 `considered / drawn / unresolved / degenerate`，`LastUnresolvedUri()` 给出第一个
失败 URI。没有纹理时不画占位图，也不静默当成"场景很空"。

## 已知限制

- 稀疏：无旋转精灵、无最近邻采样器（像素风需要）、无纹理分组批处理优化；
- 旋转不可见（`Transform2D::rotation` 被插值了，但 `Renderer2D` 不能画旋转四边形，见 G0.3）。

## 测试

`Atom_Scene2D.SpriteRenderer`（`Test/Render/Scene2DRendererTest.cpp`）headless 断言：排序稳定性与并列时的实体序、
pivot/scale 目标矩形、图集子矩形、失效/缺失纹理诊断、相机取值与退化、以及"渲染不改模拟坐标"。
