# Atom TextLayout 落地执行计划

> 状态：**待 review，暂不实施**。本文把 `TextLayout-Design-CN.md` 的提案细化为可执行、
> 可逐阶段合入/回滚的分步任务，供 review 后决定是否启动。
>
> 优先级：**P3（备用）**。当前引擎重心在 ECS↔Physics、WorldGen、Animation、Particle 等
> 大头；本计划不阻塞它们，仅在文本多语言/静态 UI 缓存成为硬需求时启用。
>
> 与现状的关系：`Renderer2D` 内的 `text_layout_cache_` 是过渡件（透明字符串键 memoization），
> 本计划 P3 落地后由「调用方显式持有的 immutable `TextLayout`」取代。

## 目标（一句话）

把 Renderer2D 里的 FreeType 单字体排版路径，替换为：

```text
FontProvider → TextShaper(HarfBuzz) → immutable TextLayout → GlyphAtlas → TextRenderer
```

让 Renderer2D **只消费已定位好的 glyph quad**，不再解码 UTF-8、不再读字体、不再决定
fallback、不再算 kerning（`Text/README-CN.md` 的模块边界）。

## 阶段总览

每阶段**独立可验证、可合入、可回滚**；除 P4 外不破坏任何公开 API。

| 阶段 | 目标 | 关键产出 | 是否改公开 API |
|------|------|----------|----------------|
| P0 | 冻结后端无关契约 | `TextLayout/FontProvider/GlyphAtlas/TextRenderer` 头文件 | 新增，不破坏旧 |
| P1 | FreeType 包装为 provider | `FreeTypeFontProvider`，迁移 `Font` 内部 | 否（`Font` 保持） |
| P2 | HarfBuzz 接线 shaping | `HarfBuzzTextShaper` | 新增 |
| P3 | 不可变 TextLayout + 适配层 | `TextLayout` 实现 + `DrawText` 临时适配 | `DrawText` 语义不变 |
| P4 | atlas key 升级 + 基础 fallback | `(FontInstanceId, glyph_id, raster_px, render_mode)` | 内部 |
| P5 | golden tests + 切换 | 基线 + 移除 Renderer2D 内联排版 | 移除旧路径 |

## P0 — 冻结契约（先定接口，后动实现）

- **文件（新增）**：`Render/Text/TextLayout.hpp`、`FontProvider.hpp`、`GlyphAtlas.hpp`、
  `TextRenderer.hpp`（纯接口 + 值类型，不依赖 SDL_GPU/后端）。
- **最小公开面**（对齐 `TextLayout-Design-CN.md`）：
  - `FontInstanceId`（稳定句柄，替代裸 `Font*` 作为 atlas/shaping 的 key）
  - `PositionedGlyph{ FontInstanceId, glyph_id, cluster_byte_offset, x, y, x_advance, y_advance }`
  - `TextLine{ span<const PositionedGlyph>, baseline, width, ascent, descent }`
  - `TextLayout`（不可变：lines、bounds、source→cluster 映射）
  - `FontProvider` / `TextShaper` / `GlyphAtlas` / `TextRenderer` 的纯虚接口
- **验收**：用当前 `Font` 充当临时 `FreeTypeFontProvider` 能编译通过；`Render/Text` 单测覆盖
  空文本、非法 UTF-8、缺字 `.notdef`。
- **不做**：不动 `Renderer2D`；不接 HarfBuzz；不写 atlas LRU。

## P1 — FreeTypeFontProvider（把现有能力装箱）

- **文件**：`Render/Text/FreeTypeFontProvider.{hpp,cpp}`；`Font.cpp` 内部逻辑迁入，`Font` 变为薄适配。
- **步骤**：① `Font` 的 face/atlas 逻辑抽到 provider；② `FontProvider` 统一 `CreateInstance(family, size,
  variation)`；③ `Renderer2D::LoadFontFromMemory` 走 provider。
- **验收**：现有 `FontTest`、`Renderer2DTextTest` 全绿；`Font` 公开签名不变。
- **不做**：仍无 shaping/fallback；atlas key 仍是 `(Font*, size, codepoint)`。

## P2 — HarfBuzzTextShaper（shape 交给成熟库）

- **文件**：`Render/Text/HarfBuzzTextShaper.{hpp,cpp}`（`Atom_3rd_HarfBuzz` 已 vendored）。
- **步骤**：① `TextShaper::Shape(utf8, direction, script, lang, features) → glyph_id/cluster/advance/offset`；
  ② 先覆盖 **Latin + CJK + combining marks**，明确 RTL/Arabic/Indic/emoji 为后续增量。
- **验收**：新增 shaping golden test（固定字体/字号/文本 → 期望 glyph id 序列与 advance）。
- **不做**：不接 fallback；不改 `Renderer2D`；不处理 bidi 分段（属 Unicode 分段层，见 P4/后续 ICU）。

## P3 — immutable TextLayout + Renderer2D 适配层

- **文件**：`Render/Text/TextLayout.cpp`；`Renderer2D::ExpandTextOp` 改为消费 `TextLayout`。
- **步骤**：① `TextLayout::Build(utf8, width, align, ...)` 产出不可变 glyph/行盒；② `Renderer2D` 新增
  `DrawTextLayout(const TextLayout&, x, y, color)`；③ 旧 `DrawText` 内部临时 `Build`+缓存（等价当前
  interim 缓存），保证**业务 API 不破坏**；④ `MeasureText` 走 `TextLayout::Bounds`。
- **验收**：`Renderer2DTextTest` 全绿；MusicCard 长标题不再每帧重排（用 `DrawTextLayout` 持有 layout）。
- **点**：此阶段起，`text_layout_cache_` 过渡件可退役，由调用方显式持有 `TextLayout`。

## P4 — atlas key 升级 + 基础 fallback

- **文件**：`GlyphAtlas.cpp`、`FontProvider` fallback 链。
- **步骤**：① atlas key 从 `(Font*, size, codepoint)` → `(FontInstanceId, glyph_id, raster_px, render_mode)`；
  ② 基础 fallback（主字体缺字 → 后备字体）；③ 保留旧 key 兜底直到 P5 切换完成。
- **验收**：缺字 fallback golden test；同一 glyph_id 在不同 instance 下正确复用/隔离。
- **前置**：`IRender2DContext` 的 GPU 资源延迟释放协议完成（atlas LRU 依赖它）。

## P5 — golden tests + 切换

- **文件**：`Test/Render/Text*` golden 基线；删除 `Renderer2D` 内联 UTF-8 解码/kerning/wrap。
- **验收**：Latin/CJK/Arabic/组合字符/换行/裁剪/DPI/atlas 淘汰/字体销毁与在用 layout 生命周期
  （`Text/README-CN.md` 验收清单）全部有 golden 覆盖；`Renderer2D` 不再直接调用 FreeType。

## 明确不做 / 风险

- **不引入** Skia / Cosmic-Text / ICU（第一阶段）。ICU 仅作为未来「完整 Unicode 分段 + bidi」的可选增量。
- **P4 是唯一破坏性内部改动**（atlas key 语义变化），必须整阶段一次切、保留旧 key 兜底。
- **依赖前置**：`IRender2DContext` 延迟释放协议先落地，否则 atlas LRU（P4 的显存预算）无从谈起。
- **二进制/构建成本**：HarfBuzz 已 vendored，增量可控；不消除 atlas/GPU 上传成本（设计文档已述）。

## 与当前 interim 缓存的关系

`Renderer2D::text_layout_cache_`（透明字符串键缓存）是**过渡**：正确且已通过测试，但结构上把缓存
藏在 Renderer2D 内部。P3 落地后由调用方显式持有 `TextLayout`，本缓存退役。在此之前不动它。
