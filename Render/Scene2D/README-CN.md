# Render/Scene2D

把 ECS 里的 2D 精灵与相机变成一个 `Renderer2D` 绘制帧。**不含**世界、主循环、帧节奏，也不含物理。
它依赖 `Atom_ECS`、`Atom_Renderer2D`、`Atom_Render_Resources`，且**不依赖 `Atom_Runtime`**——工具与测试可以
在没有帧循环的情况下用它。

## 四个步骤，一条分工线

```text
SnapshotTransforms   Transform2D      -> RenderTransform2D    （模拟 → 显示副本，每帧一次）
FindActiveCamera     Transform2D+Camera2D -> CameraView       （最低实体索引的那个相机）
Collect              RenderTransform2D+SpriteRenderer -> SpriteDraw[]（解析 URI、pivot、排序、诊断）
Submit               SpriteDraw[] -> Renderer2D                （唯一触碰 GPU 的一步，不做决策）
```

这条分工线就是"渲染只读显示状态"的执行方式：`Collect` 只读 `RenderTransform2D`，从不读 `Transform2D`，
所以渲染跑一次不可能改变模拟坐标；反过来，显示副本没被快照时，精灵停在旧位置而不是跳到模拟位置。

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
- `RenderTransform2D` 目前是纯拷贝，固定步与显示帧之间没有插值（需要 previous-state 缓冲与 alpha）。

## 测试

`Atom_Scene2D.SpriteRenderer`（`Test/Render/Scene2DRendererTest.cpp`）headless 断言：排序稳定性与并列时的实体序、
pivot/scale 目标矩形、图集子矩形、失效/缺失纹理诊断、相机取值与退化、以及"渲染不改模拟坐标"。
